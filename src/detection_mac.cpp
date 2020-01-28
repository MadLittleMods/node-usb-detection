#include "detection.h"
#include "deviceList.h"

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <sys/param.h>
#include <poll.h>

#include <uv.h>

/**********************************
 * Local typedefs
 **********************************/
typedef struct DeviceListItem {
	io_object_t notification;
	IOUSBDeviceInterface** deviceInterface;
	DeviceItem_t* deviceItem;
} stDeviceListItem;

/**********************************
 * Local Variables
 **********************************/
static ListResultItem_t* currentItem;
static bool isAdded = false;
static bool initialDeviceImport = true;

static IONotificationPortRef gNotifyPort;
static io_iterator_t gAddedIter;
static CFRunLoopRef gRunLoop;

static CFMutableDictionaryRef matchingDict;
static CFRunLoopSourceRef runLoopSource;

static uv_work_t work_req;
static uv_signal_t term_signal;
static uv_signal_t int_signal;

static uv_async_t async_handler;
static uv_mutex_t notify_mutex;
static uv_cond_t notifyDeviceHandled;

static bool deviceHandled = true;

static bool isRunning = false;

/**********************************
 * Local Helper Functions protoypes
 **********************************/

static void WaitForDeviceHandled();
static void SignalDeviceHandled();
static void cbTerminate(uv_signal_t *handle, int signum);
static void cbWork(uv_work_t *req);
static void cbAfter(uv_work_t *req, int status);
static void cbAsync(uv_async_t *handle);


/**********************************
 * Public Functions
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
static void DeviceRemoved(void *refCon, io_service_t service, natural_t messageType, void *messageArgument) {
	kern_return_t kr;
	stDeviceListItem* deviceListItem = (stDeviceListItem *) refCon;
	DeviceItem_t* deviceItem = deviceListItem->deviceItem;

	if(messageType == kIOMessageServiceIsTerminated) {
		if(deviceListItem->deviceInterface) {
			kr = (*deviceListItem->deviceInterface)->Release(deviceListItem->deviceInterface);
		}

		kr = IOObjectRelease(deviceListItem->notification);


		ListResultItem_t* item = NULL;
		if(deviceItem) {
			item = CopyElement(&deviceItem->deviceParams);
			RemoveItemFromList(deviceItem);
			delete deviceItem;
		}
		else {
			item = new ListResultItem_t();
		}

		WaitForDeviceHandled();
		currentItem = item;
		isAdded = false;
		uv_async_send(&async_handler);
	}
}

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
static void DeviceAdded(void *refCon, io_iterator_t iterator) {
	kern_return_t kr;
	io_service_t usbDevice;
	IOCFPlugInInterface **plugInInterface = NULL;
	SInt32 score;
	HRESULT res;

	while((usbDevice = IOIteratorNext(iterator))) {
		io_name_t deviceName;
		CFStringRef deviceNameAsCFString;
		UInt32 locationID;
		UInt16 vendorId;
		UInt16 productId;
		UInt16 addr;

		DeviceItem_t* deviceItem = new DeviceItem_t();

		// Get the USB device's name.
		kr = IORegistryEntryGetName(usbDevice, deviceName);
		if(KERN_SUCCESS != kr) {
			deviceName[0] = '\0';
		}

		deviceNameAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, deviceName, kCFStringEncodingASCII);


		if(deviceNameAsCFString) {
			Boolean result;
			char    deviceName[MAXPATHLEN];

			// Convert from a CFString to a C (NUL-terminated)
			result = CFStringGetCString(deviceNameAsCFString,
										deviceName,
										sizeof(deviceName),
										kCFStringEncodingUTF8);

			if(result) {
				deviceItem->deviceParams.deviceName = deviceName;
			}

			CFRelease(deviceNameAsCFString);
		}

		CFStringRef manufacturerAsCFString = (CFStringRef)IORegistryEntrySearchCFProperty(
				usbDevice,
				kIOServicePlane,
				CFSTR(kUSBVendorString),
				kCFAllocatorDefault,
				kIORegistryIterateRecursively
			);

		if(manufacturerAsCFString) {
			Boolean result;
			char    manufacturer[MAXPATHLEN];

			// Convert from a CFString to a C (NUL-terminated)
			result = CFStringGetCString(
					manufacturerAsCFString,
					manufacturer,
					sizeof(manufacturer),
					kCFStringEncodingUTF8
				);

			if(result) {
				deviceItem->deviceParams.manufacturer = manufacturer;
			}

			CFRelease(manufacturerAsCFString);
		}

		CFStringRef serialNumberAsCFString = (CFStringRef) IORegistryEntrySearchCFProperty(
				usbDevice,
				kIOServicePlane,
				CFSTR(kUSBSerialNumberString),
				kCFAllocatorDefault,
				kIORegistryIterateRecursively
			);

		if(serialNumberAsCFString) {
			Boolean result;
			char    serialNumber[MAXPATHLEN];

			// Convert from a CFString to a C (NUL-terminated)
			result = CFStringGetCString(
					serialNumberAsCFString,
					serialNumber,
					sizeof(serialNumber),
					kCFStringEncodingUTF8
				);

			if(result) {
				deviceItem->deviceParams.serialNumber = serialNumber;
			}

			CFRelease(serialNumberAsCFString);
		}


		// Now, get the locationID of this device. In order to do this, we need to create an IOUSBDeviceInterface
		// for our device. This will create the necessary connections between our userland application and the
		// kernel object for the USB Device.
		kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);

		if((kIOReturnSuccess != kr) || !plugInInterface) {
			DEBUG_LOG("IOCreatePlugInInterfaceForService returned 0x%08x.\n", kr);
			continue;
		}

		stDeviceListItem *deviceListItem = new stDeviceListItem();

		// Use the plugin interface to retrieve the device interface.
		res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID*) &deviceListItem->deviceInterface);

		// Now done with the plugin interface.
		(*plugInInterface)->Release(plugInInterface);

		if(res || deviceListItem->deviceInterface == NULL) {
			DEBUG_LOG("QueryInterface returned %d.\n", (int) res);
			continue;
		}

		// Now that we have the IOUSBDeviceInterface, we can call the routines in IOUSBLib.h.
		// In this case, fetch the locationID. The locationID uniquely identifies the device
		// and will remain the same, even across reboots, so long as the bus topology doesn't change.

		kr = (*deviceListItem->deviceInterface)->GetLocationID(deviceListItem->deviceInterface, &locationID);
		if(KERN_SUCCESS != kr) {
			DEBUG_LOG("GetLocationID returned 0x%08x.\n", kr);
			continue;
		}
		deviceItem->deviceParams.locationId = locationID;


		kr = (*deviceListItem->deviceInterface)->GetDeviceAddress(deviceListItem->deviceInterface, &addr);
		if(KERN_SUCCESS != kr) {
			DEBUG_LOG("GetDeviceAddress returned 0x%08x.\n", kr);
			continue;
		}
		deviceItem->deviceParams.deviceAddress = addr;


		kr = (*deviceListItem->deviceInterface)->GetDeviceVendor(deviceListItem->deviceInterface, &vendorId);
		if(KERN_SUCCESS != kr) {
			DEBUG_LOG("GetDeviceVendor returned 0x%08x.\n", kr);
			continue;
		}
		deviceItem->deviceParams.vendorId = vendorId;

		kr = (*deviceListItem->deviceInterface)->GetDeviceProduct(deviceListItem->deviceInterface, &productId);
		if(KERN_SUCCESS != kr) {
			DEBUG_LOG("GetDeviceProduct returned 0x%08x.\n", kr);
			continue;
		}
		deviceItem->deviceParams.productId = productId;


		// Extract path name as unique key
		io_string_t pathName;
		IORegistryEntryGetPath(usbDevice, kIOServicePlane, pathName);
		deviceNameAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, pathName, kCFStringEncodingASCII);
		char cPathName[MAXPATHLEN];

		if(deviceNameAsCFString) {
			Boolean result;

			// Convert from a CFString to a C (NUL-terminated)
			result = CFStringGetCString(
					deviceNameAsCFString,
					cPathName,
					sizeof(cPathName),
					kCFStringEncodingUTF8
				);


			CFRelease(deviceNameAsCFString);
		}

		AddItemToList(cPathName, deviceItem);
		deviceListItem->deviceItem = deviceItem;

		if(initialDeviceImport == false) {
			WaitForDeviceHandled();
			currentItem = &deviceItem->deviceParams;
			isAdded = true;
			uv_async_send(&async_handler);
		}

		// Register for an interest notification of this device being removed. Use a reference to our
		// private data as the refCon which will be passed to the notification callback.
		kr = IOServiceAddInterestNotification(
				gNotifyPort, // notifyPort
				usbDevice, // service
				kIOGeneralInterest, // interestType
				DeviceRemoved, // callback
				deviceListItem, // refCon
				&(deviceListItem->notification) // notification
			);

		if(KERN_SUCCESS != kr) {
			DEBUG_LOG("IOServiceAddInterestNotification returned 0x%08x.\n", kr);
		}

		// Done with this USB device; release the reference added by IOIteratorNext
		kr = IOObjectRelease(usbDevice);
	}
}

void Start() {
	if(isRunning) {
		return;
	}

	isRunning = true;

	uv_mutex_init(&notify_mutex);
	uv_async_init(uv_default_loop(), &async_handler, cbAsync);
	uv_signal_init(uv_default_loop(), &term_signal);
	uv_signal_init(uv_default_loop(), &int_signal);
	uv_cond_init(&notifyDeviceHandled);

	uv_queue_work(uv_default_loop(), &work_req, cbWork, cbAfter);
}

void Stop() {
	if(!isRunning) {
		return;
	}

	isRunning = false;

	uv_mutex_destroy(&notify_mutex);
	uv_signal_stop(&int_signal);
	uv_signal_stop(&term_signal);
	uv_close((uv_handle_t *) &async_handler, NULL);
	uv_cond_destroy(&notifyDeviceHandled);

	if (gRunLoop) {
		CFRunLoopStop(gRunLoop);
	}
}

void InitDetection() {
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

	if (matchingDict == NULL) {
		DEBUG_LOG("IOServiceMatching returned NULL.\n");
	}

	// Create a notification port and add its run loop event source to our run loop
	// This is how async notifications get set up.

	gNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);

	// Now set up a notification to be called when a device is first matched by I/O Kit.
	kr = IOServiceAddMatchingNotification(
			gNotifyPort, // notifyPort
			kIOFirstMatchNotification, // notificationType
			matchingDict, // matching
			DeviceAdded, // callback
			NULL, // refCon
			&gAddedIter // notification
		);

	if (KERN_SUCCESS != kr) {
		DEBUG_LOG("IOServiceAddMatchingNotification returned 0x%08x.\n", kr);
	}

	// Iterate once to get already-present devices and arm the notification
	DeviceAdded(NULL, gAddedIter);
	initialDeviceImport = false;
}

void EIO_Find(uv_work_t* req) {
	ListBaton* data = static_cast<ListBaton*>(req->data);

	CreateFilteredList(&data->results, data->vid, data->pid);
}

static void WaitForDeviceHandled() {
	uv_mutex_lock(&notify_mutex);
	if(deviceHandled == false) {
		uv_cond_wait(&notifyDeviceHandled, &notify_mutex);
	}
	deviceHandled = false;
	uv_mutex_unlock(&notify_mutex);
}

static void SignalDeviceHandled() {
	uv_mutex_lock(&notify_mutex);
	deviceHandled = true;
	uv_cond_signal(&notifyDeviceHandled);
	uv_mutex_unlock(&notify_mutex);
}

static void cbWork(uv_work_t *req) {
	// We have this check in case we `Stop` before this thread starts,
	// otherwise the process will hang
	if(!isRunning) {
		return;
	}

	uv_signal_start(&int_signal, cbTerminate, SIGINT);
	uv_signal_start(&term_signal, cbTerminate, SIGTERM);

	runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);

	gRunLoop = CFRunLoopGetCurrent();
	CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);

	// Creating `gRunLoop` can take some cycles so we also need this second
	// `isRunning` check here because it happens at a future time
	if(isRunning) {
		// Start the run loop. Now we'll receive notifications.
		CFRunLoopRun();
	}

	// The `CFRunLoopRun` is a blocking call so we also need this second
	// `isRunning` check here because it happens at a future time
	if(isRunning) {
		// We should never get here while running
		DEBUG_LOG("Unexpectedly back from CFRunLoopRun()!\n");
	}
}

static void cbAfter(uv_work_t *req, int status) {
	Stop();
}

static void cbAsync(uv_async_t *handle) {
	if(!isRunning) {
		return;
	}

	if(isAdded) {
		NotifyAdded(currentItem);
	}
	else {
		NotifyRemoved(currentItem);
	}

	// Delete Item in case of removal
	if(isAdded == false) {
		delete currentItem;
	}

	SignalDeviceHandled();
}


static void cbTerminate(uv_signal_t *handle, int signum) {
	Stop();
}
