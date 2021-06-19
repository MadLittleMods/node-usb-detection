#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <sys/param.h>
#include <poll.h>
#include <thread>

#include "detection.h"
#include "deviceList.h"

struct NotificationDeviceItem
{
	io_object_t notification;
	IOUSBDeviceInterface **deviceInterface;
	std::shared_ptr<ListResultItem_t> deviceItem;
	Detection *parent;
};

/**********************************
 * Local Helper Functions protoypes
 **********************************/

//================================================================================================
//
//  DeviceRemoved
//
//  This routine will get called whenever any kIOGeneralInterest notification happens.  We are
//  interested in the kIOMessageServiceIsTerminated message so that's what we look for.  Other
//  messages are defined in IOMessage.h.
//
//================================================================================================
static void DeviceRemovedOuter(void *refCon, io_service_t service, natural_t messageType, void *messageArgument);

//================================================================================================
//
//  DeviceAdded
//
//  This routine is the callback for our IOServiceAddMatchingNotification.  When we get called
//  we will look at all the devices that were added and we will:
//
//  1.  Create some private data to relate to each device (in this case we use the service's name
//      and the location ID of the device
//  2.  Submit an IOServiceAddInterestNotification of type kIOGeneralInterest for this device,
//      using the refCon field to store a pointer to our private data.  When we get called with
//      this interest notification, we can grab the refCon and access our private data.
//
//================================================================================================
static void DeviceAddedOuter(void *refCon, io_iterator_t iterator);

void FreeNotificationItem(NotificationDeviceItem *notifyItem)
{
	if (notifyItem->deviceInterface)
	{
		(*notifyItem->deviceInterface)->Release(notifyItem->deviceInterface);
		notifyItem->deviceInterface = nullptr;
	}

	IOObjectRelease(notifyItem->notification);
	notifyItem->notification = 0;

	notifyItem->deviceItem->data = nullptr;
}

/**********************************
 * Public Functions
 **********************************/

class MacDetection : public Detection, public Napi::ObjectWrap<MacDetection>
{
public:
	MacDetection(const Napi::CallbackInfo &info)
		: Napi::ObjectWrap<MacDetection>(info)
	{
	}

	static void Initialize(Napi::Env &env, Napi::Object &target)
	{
		Napi::Function ctor = DefineClass(env, "Detection",
										  {
											  InstanceMethod("isMonitoring", &Detection::IsMonitoring),
											  InstanceMethod("startMonitoring", &Detection::StartMonitoring),
											  InstanceMethod("stopMonitoring", &Detection::StopMonitoring),
											  InstanceMethod("findDevices", &Detection::FindDevices),
										  });

		target.Set("Detection", ctor);
		// constructor = Napi::Persistent(ctor);
		// constructor.SuppressDestruct();
	}

	bool IsRunning()
	{
		return isRunning;
	}
	bool Start(const Napi::Env &env, const Napi::Function &callback)
	{
		if (isRunning)
		{
			return true;
		}

		isRunning = true;

		kern_return_t kr;

		// Set up the matching criteria for the devices we're interested in. The matching criteria needs to follow
		// the same rules as kernel drivers: mainly it needs to follow the USB Common Class Specification, pp. 6-7.
		// See also Technical Q&A QA1076 "Tips on USB driver matching on Mac OS X"
		// <http://developer.apple.com/qa/qa2001/qa1076.html>.
		// One exception is that you can use the matching dictionary "as is", i.e. without adding any matching
		// criteria to it and it will match every IOUSBDevice in the system. IOServiceAddMatchingNotification will
		// consume this dictionary reference, so there is no need to release it later on.

		// Interested in instances of class
		// IOUSBDevice and its subclasses
		matchingDict = IOServiceMatching(kIOUSBDeviceClassName);

		if (matchingDict == NULL)
		{
			DEBUG_LOG("IOServiceMatching returned NULL.\n");
		}

		// Create a notification port and add its run loop event source to our run loop
		// This is how async notifications get set up.

		gNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);

		// Now set up a notification to be called when a device is first matched by I/O Kit.
		kr = IOServiceAddMatchingNotification(
			gNotifyPort,			   // notifyPort
			kIOFirstMatchNotification, // notificationType
			matchingDict,			   // matching
			DeviceAddedOuter,		   // callback
			this,					   // refCon
			&gAddedIter				   // notification
		);

		if (KERN_SUCCESS != kr)
		{
			DEBUG_LOG("IOServiceAddMatchingNotification returned 0x%08x.\n", kr);
		}

		// Iterate once to get already-present devices, skipping the notify
		IterateFoundDevices(gAddedIter, false);

		notify_func = Napi::ThreadSafeFunction::New(
			env,
			callback,		 // JavaScript function called asynchronously
			"Resource Name", // Name
			0,				 // Unlimited queue
			1,				 // Only one thread will use this initially
			[=](Napi::Env) { // Finalizer used to clean threads up
				// Wait for end of the thread, if it wasnt the one to close up
				if (poll_thread.joinable())
				{
					poll_thread.join();
				}
			});

		poll_thread = std::thread([=]()
								  {
									  // We have this check in case we `Stop` before this thread starts,
									  // otherwise the process will hang
									  if (isRunning)
									  {

										  CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);

										  gRunLoop = CFRunLoopGetCurrent();
										  CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);

										  // Creating `gRunLoop` can take some cycles so we also need this second
										  // `isRunning` check here because it happens at a future time
										  if (isRunning)
										  {
											  // Start the run loop. Now we'll receive notifications.
											  CFRunLoopRun();
										  }

										  // The `CFRunLoopRun` is a blocking call so we also need this second
										  // `isRunning` check here because it happens at a future time
										  if (isRunning)
										  {
											  // We should never get here while running
											  DEBUG_LOG("Unexpectedly back from CFRunLoopRun()!\n");
										  }

										  // Remove the runloop source
										  CFRunLoopRemoveSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);
									  }

									  // Destroy the port
									  IOObjectRelease(gAddedIter);
									  IONotificationPortDestroy(gNotifyPort);

									  // free the listeners allocated for each device
									  auto remainingDevices = deviceMap.popAll();
									  for (auto it = remainingDevices.begin(); it != remainingDevices.end(); ++it)
									  {
										  std::shared_ptr<ListResultItem_t> item = *it;
										  if (item->data != nullptr)
										  {
											  NotificationDeviceItem *notifyItem = (NotificationDeviceItem *)item->data;
											  FreeNotificationItem(notifyItem);
											  delete notifyItem;
										  }
									  }

									  notify_func.Release();
								  });

		return true;
	}
	void Stop()
	{
		if (isRunning)
		{
			isRunning = false;

			if (gRunLoop)
			{
				CFRunLoopStop(gRunLoop);
			}

			poll_thread.join();
		}
	}

	void IterateFoundDevices(io_iterator_t iterator, bool notify)
	{
		kern_return_t kr;
		io_service_t usbDevice;
		IOCFPlugInInterface **plugInInterface = NULL;
		SInt32 score;
		HRESULT res;

		while ((usbDevice = IOIteratorNext(iterator)))
		{
			io_name_t deviceName;
			CFStringRef deviceNameAsCFString;
			UInt32 locationID;
			UInt16 vendorId;
			UInt16 productId;
			UInt16 addr;

			std::shared_ptr<ListResultItem_t> item(new ListResultItem_t);

			// Get the USB device's name.
			kr = IORegistryEntryGetName(usbDevice, deviceName);
			if (KERN_SUCCESS != kr)
			{
				deviceName[0] = '\0';
			}

			deviceNameAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, deviceName, kCFStringEncodingASCII);

			if (deviceNameAsCFString)
			{
				Boolean result;
				char deviceName[MAXPATHLEN];

				// Convert from a CFString to a C (NUL-terminated)
				result = CFStringGetCString(deviceNameAsCFString,
											deviceName,
											sizeof(deviceName),
											kCFStringEncodingUTF8);

				if (result)
				{
					item->deviceName = deviceName;
				}

				CFRelease(deviceNameAsCFString);
			}

			CFStringRef manufacturerAsCFString = (CFStringRef)IORegistryEntrySearchCFProperty(
				usbDevice,
				kIOServicePlane,
				CFSTR(kUSBVendorString),
				kCFAllocatorDefault,
				kIORegistryIterateRecursively);

			if (manufacturerAsCFString)
			{
				Boolean result;
				char manufacturer[MAXPATHLEN];

				// Convert from a CFString to a C (NUL-terminated)
				result = CFStringGetCString(
					manufacturerAsCFString,
					manufacturer,
					sizeof(manufacturer),
					kCFStringEncodingUTF8);

				if (result)
				{
					item->manufacturer = manufacturer;
				}

				CFRelease(manufacturerAsCFString);
			}

			CFStringRef serialNumberAsCFString = (CFStringRef)IORegistryEntrySearchCFProperty(
				usbDevice,
				kIOServicePlane,
				CFSTR(kUSBSerialNumberString),
				kCFAllocatorDefault,
				kIORegistryIterateRecursively);

			if (serialNumberAsCFString)
			{
				Boolean result;
				char serialNumber[MAXPATHLEN];

				// Convert from a CFString to a C (NUL-terminated)
				result = CFStringGetCString(
					serialNumberAsCFString,
					serialNumber,
					sizeof(serialNumber),
					kCFStringEncodingUTF8);

				if (result)
				{
					item->serialNumber = serialNumber;
				}

				CFRelease(serialNumberAsCFString);
			}

			// Now, get the locationID of this device. In order to do this, we need to create an IOUSBDeviceInterface
			// for our device. This will create the necessary connections between our userland application and the
			// kernel object for the USB Device.
			kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);

			if ((kIOReturnSuccess != kr) || !plugInInterface)
			{
				DEBUG_LOG("IOCreatePlugInInterfaceForService returned 0x%08x.\n", kr);
				continue;
			}

			NotificationDeviceItem *notificationItem = new NotificationDeviceItem();
			notificationItem->parent = this;

			// Use the plugin interface to retrieve the device interface.
			res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&notificationItem->deviceInterface);

			// Now done with the plugin interface.
			(*plugInInterface)->Release(plugInInterface);

			if (res || notificationItem->deviceInterface == NULL)
			{
				DEBUG_LOG("QueryInterface returned %d.\n", (int)res);
				continue;
			}

			// Now that we have the IOUSBDeviceInterface, we can call the routines in IOUSBLib.h.
			// In this case, fetch the locationID. The locationID uniquely identifies the device
			// and will remain the same, even across reboots, so long as the bus topology doesn't change.

			kr = (*notificationItem->deviceInterface)->GetLocationID(notificationItem->deviceInterface, &locationID);
			if (KERN_SUCCESS != kr)
			{
				DEBUG_LOG("GetLocationID returned 0x%08x.\n", kr);
				continue;
			}
			item->locationId = locationID;

			kr = (*notificationItem->deviceInterface)->GetDeviceAddress(notificationItem->deviceInterface, &addr);
			if (KERN_SUCCESS != kr)
			{
				DEBUG_LOG("GetDeviceAddress returned 0x%08x.\n", kr);
				continue;
			}
			item->deviceAddress = addr;

			kr = (*notificationItem->deviceInterface)->GetDeviceVendor(notificationItem->deviceInterface, &vendorId);
			if (KERN_SUCCESS != kr)
			{
				DEBUG_LOG("GetDeviceVendor returned 0x%08x.\n", kr);
				continue;
			}
			item->vendorId = vendorId;

			kr = (*notificationItem->deviceInterface)->GetDeviceProduct(notificationItem->deviceInterface, &productId);
			if (KERN_SUCCESS != kr)
			{
				DEBUG_LOG("GetDeviceProduct returned 0x%08x.\n", kr);
				continue;
			}
			item->productId = productId;

			// Extract path name as unique key
			io_string_t pathName;
			IORegistryEntryGetPath(usbDevice, kIOServicePlane, pathName);
			deviceNameAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, pathName, kCFStringEncodingASCII);
			char cPathName[MAXPATHLEN];

			if (deviceNameAsCFString)
			{
				Boolean result;

				// Convert from a CFString to a C (NUL-terminated)
				result = CFStringGetCString(
					deviceNameAsCFString,
					cPathName,
					sizeof(cPathName),
					kCFStringEncodingUTF8);

				CFRelease(deviceNameAsCFString);
			}

			notificationItem->deviceItem = item;
			item->data = notificationItem;
			deviceMap.addItem(cPathName, item);
			// notificationMap.insert(notificationItem, item);

			if (notify)
			{
				DeviceAdded(item);
			}

			// Register for an interest notification of this device being removed. Use a reference to our
			// private data as the refCon which will be passed to the notification callback.
			kr = IOServiceAddInterestNotification(
				gNotifyPort,					  // notifyPort
				usbDevice,						  // service
				kIOGeneralInterest,				  // interestType
				DeviceRemovedOuter,				  // callback
				notificationItem,				  // refCon
				&(notificationItem->notification) // notification
			);

			if (KERN_SUCCESS != kr)
			{
				DEBUG_LOG("IOServiceAddInterestNotification returned 0x%08x.\n", kr);
			}

			// Done with this USB device; release the reference added by IOIteratorNext
			kr = IOObjectRelease(usbDevice);
		}
	}

private:
	/**********************************
	 * Local Variables
	 **********************************/
	std::thread poll_thread;

	IONotificationPortRef gNotifyPort;
	io_iterator_t gAddedIter;
	CFRunLoopRef gRunLoop;

	CFMutableDictionaryRef matchingDict;
	std::map<ListResultItem_t *, std::unique_ptr<NotificationDeviceItem> > notificationMap;

	std::atomic<bool> isRunning = {false};
};

void InitializeDetection(Napi::Env &env, Napi::Object &target)
{
	MacDetection::Initialize(env, target);
}

static void DeviceAddedOuter(void *refCon, io_iterator_t iterator)
{
	MacDetection *parent = (MacDetection *)refCon;
	parent->IterateFoundDevices(iterator, true);
}

static void DeviceRemovedOuter(void *refCon, io_service_t service, natural_t messageType, void *messageArgument)
{
	NotificationDeviceItem *notifyItem = (NotificationDeviceItem *)refCon;

	if (messageType == kIOMessageServiceIsTerminated)
	{
		FreeNotificationItem(notifyItem);

		notifyItem->parent->DeviceRemoved(notifyItem->deviceItem);

		delete notifyItem;
	}
}
