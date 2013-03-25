#include "detection.h"

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <sys/param.h>
#include <pthread.h>

#include <uv.h>

typedef struct UsbDevice {
    char deviceName[MAXPATHLEN];
    UInt32 locationId;
    UInt16 deviceAddress;
    UInt16 vendorId;
    UInt16 productId;
    char manufacturer[MAXPATHLEN];
    char serialNumber[MAXPATHLEN];
} stUsbDevice;

typedef struct DeviceListItem {
    io_object_t       notification;
    IOUSBDeviceInterface  **deviceInterface;
    struct UsbDevice value;
    struct DeviceListItem *prev;
    struct DeviceListItem *next;
    int* length;
} stDeviceListItem;

static IONotificationPortRef    gNotifyPort;
static io_iterator_t            gAddedIter;
static CFRunLoopRef             gRunLoop;

static stDeviceListItem* devices = NULL;
static stDeviceListItem* lastDevice = NULL;
static int length = 0;

CFMutableDictionaryRef  matchingDict;
CFRunLoopSourceRef      runLoopSource;

static pthread_t lookupThread;

pthread_mutex_t notify_mutex;
pthread_cond_t notify_cv;
ListResultItem* notify_item;
bool isAdded = false;

//================================================================================================
//
//  DeviceRemoved
//
//  This routine will get called whenever any kIOGeneralInterest notification happens.  We are
//  interested in the kIOMessageServiceIsTerminated message so that's what we look for.  Other
//  messages are defined in IOMessage.h.
//
//================================================================================================
void DeviceRemoved(void *refCon, io_service_t service, natural_t messageType, void *messageArgument)
{
    kern_return_t   kr;
    stDeviceListItem* deviceListItem = (stDeviceListItem *) refCon;
    stUsbDevice *usbDev = &(deviceListItem->value);
    
    if (messageType == kIOMessageServiceIsTerminated) {
        if (deviceListItem->deviceInterface) {
            kr = (*deviceListItem->deviceInterface)->Release(deviceListItem->deviceInterface);
        }
        
        kr = IOObjectRelease(deviceListItem->notification);
        
        if (deviceListItem->prev && deviceListItem->next) {
            deviceListItem->prev->next = deviceListItem->next;
            deviceListItem->next->prev = deviceListItem->prev;
        }
        
        if (deviceListItem->prev && !deviceListItem->next) {
            deviceListItem->prev->next = NULL;
        }
        
        if (!deviceListItem->prev && deviceListItem->next) {
            deviceListItem->next->prev = NULL;
        }
        
        length--;


        pthread_mutex_lock(&notify_mutex);

        notify_item = new ListResultItem();
        if (usbDev->deviceName != NULL) {
          notify_item->deviceName = usbDev->deviceName;
        }
        if (usbDev->locationId != 0) {
          notify_item->locationId = usbDev->locationId;
        }
        if (usbDev->vendorId != 0) {
          notify_item->vendorId = usbDev->vendorId;
        }
        if (usbDev->productId != 0) {
          notify_item->productId = usbDev->productId;
        }
        if (usbDev->manufacturer != NULL) {
          notify_item->manufacturer = usbDev->manufacturer;
        }
        if (usbDev->serialNumber != NULL) {
          notify_item->serialNumber = usbDev->serialNumber;
        }
        if (usbDev->deviceAddress != 0) {
          notify_item->deviceAddress = usbDev->deviceAddress;
        }
        isAdded = false;
        pthread_cond_signal(&notify_cv);

        pthread_mutex_unlock(&notify_mutex);
        
        // free(deviceListItem);
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
void DeviceAdded(void *refCon, io_iterator_t iterator)
{
    kern_return_t       kr;
    io_service_t        usbDevice;
    IOCFPlugInInterface **plugInInterface = NULL;
    SInt32              score;
    HRESULT             res;
    
    while ((usbDevice = IOIteratorNext(iterator))) {
        io_name_t       deviceName;
        CFStringRef     deviceNameAsCFString;   
        stUsbDevice     *usbDev = NULL;
        UInt32          locationID;
        UInt16          vendorId;
        UInt16          productId;
        UInt16          addr;
        
        
        // Add some app-specific information about this device.
        // Create a buffer to hold the data.
        usbDev = (stUsbDevice *) malloc(sizeof(stUsbDevice));
        bzero(usbDev, sizeof(stUsbDevice));
        
        // Get the USB device's name.
        kr = IORegistryEntryGetName(usbDevice, deviceName);
        if (KERN_SUCCESS != kr) {
            deviceName[0] = '\0';
        }
        
        deviceNameAsCFString = CFStringCreateWithCString(kCFAllocatorDefault, deviceName, 
                                                         kCFStringEncodingASCII);
        
        
        if (deviceNameAsCFString)
        {
            Boolean result;
            char    deviceName[MAXPATHLEN];
            
            // Convert from a CFString to a C (NUL-terminated)
            result = CFStringGetCString(deviceNameAsCFString,
                                        deviceName,
                                        sizeof(deviceName),
                                        kCFStringEncodingUTF8);
            
            if (result) {
                strcpy(usbDev->deviceName, deviceName);
            }
            
            CFRelease(deviceNameAsCFString);
        }
        
        CFStringRef manufacturerAsCFString = (CFStringRef) IORegistryEntrySearchCFProperty(usbDevice,
                                                                                           kIOServicePlane,
                                                                                           CFSTR(kUSBVendorString),
                                                                                           kCFAllocatorDefault,
                                                                                           kIORegistryIterateRecursively);
        
        if (manufacturerAsCFString)
        {
            Boolean result;
            char    manufacturer[MAXPATHLEN];
            
            // Convert from a CFString to a C (NUL-terminated)
            result = CFStringGetCString(manufacturerAsCFString,
                                        manufacturer,
                                        sizeof(manufacturer),
                                        kCFStringEncodingUTF8);
            
            if (result) {
                strcpy(usbDev->manufacturer, manufacturer);
            }
            
            CFRelease(manufacturerAsCFString);
        }
        
        CFStringRef serialNumberAsCFString = (CFStringRef) IORegistryEntrySearchCFProperty(usbDevice,
                                                                                           kIOServicePlane,
                                                                                           CFSTR(kUSBSerialNumberString),
                                                                                           kCFAllocatorDefault,
                                                                                           kIORegistryIterateRecursively);
        
        if (serialNumberAsCFString)
        {
            Boolean result;
            char    serialNumber[MAXPATHLEN];
            
            // Convert from a CFString to a C (NUL-terminated)
            result = CFStringGetCString(serialNumberAsCFString,
                                        serialNumber,
                                        sizeof(serialNumber),
                                        kCFStringEncodingUTF8);
            
            if (result) {
                strcpy(usbDev->serialNumber, serialNumber);
            }
            
            CFRelease(serialNumberAsCFString);
        }
        
                                                
        // Now, get the locationID of this device. In order to do this, we need to create an IOUSBDeviceInterface 
        // for our device. This will create the necessary connections between our userland application and the 
        // kernel object for the USB Device.
        kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
                                               &plugInInterface, &score);

        if ((kIOReturnSuccess != kr) || !plugInInterface) {
            fprintf(stderr, "IOCreatePlugInInterfaceForService returned 0x%08x.\n", kr);
            continue;
        }
        
        stDeviceListItem *deviceListItem = (stDeviceListItem*) malloc(sizeof(stDeviceListItem));

        // Use the plugin interface to retrieve the device interface.
        res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                                 (LPVOID*) &deviceListItem->deviceInterface);
        
        // Now done with the plugin interface.
        (*plugInInterface)->Release(plugInInterface);
                    
        if (res || deviceListItem->deviceInterface == NULL) {
            fprintf(stderr, "QueryInterface returned %d.\n", (int) res);
            continue;
        }

        // Now that we have the IOUSBDeviceInterface, we can call the routines in IOUSBLib.h.
        // In this case, fetch the locationID. The locationID uniquely identifies the device
        // and will remain the same, even across reboots, so long as the bus topology doesn't change.
        
        kr = (*deviceListItem->deviceInterface)->GetLocationID(deviceListItem->deviceInterface, &locationID);
        if (KERN_SUCCESS != kr) {
            fprintf(stderr, "GetLocationID returned 0x%08x.\n", kr);
            continue;
        }
        usbDev->locationId = locationID;

        
        kr = (*deviceListItem->deviceInterface)->GetDeviceAddress(deviceListItem->deviceInterface, &addr);
        if (KERN_SUCCESS != kr) {
            fprintf(stderr, "GetDeviceAddress returned 0x%08x.\n", kr);
            continue;
        }
        usbDev->deviceAddress = addr;

        // UInt16 realLocId = (locationID >> 16 & 0xff00) + addr;
        // usbDev->locationID = realLocId;
        
        
        kr = (*deviceListItem->deviceInterface)->GetDeviceVendor(deviceListItem->deviceInterface, &vendorId);
        if (KERN_SUCCESS != kr) {
            fprintf(stderr, "GetDeviceVendor returned 0x%08x.\n", kr);
            continue;
        }
        usbDev->vendorId = vendorId;
        
        kr = (*deviceListItem->deviceInterface)->GetDeviceProduct(deviceListItem->deviceInterface, &productId);
        if (KERN_SUCCESS != kr) {
            fprintf(stderr, "GetDeviceProduct returned 0x%08x.\n", kr);
            continue;
        }
        usbDev->productId = productId;
        

        deviceListItem->value = *usbDev;
        deviceListItem->next = NULL;
        deviceListItem->prev = lastDevice;
        deviceListItem->length = &length;
        
        if (devices == NULL) {
            devices = deviceListItem;
        }
        else {
            lastDevice->next = deviceListItem;
        }
        
        lastDevice = deviceListItem;
        length++;

        pthread_mutex_lock(&notify_mutex);

        notify_item = new ListResultItem();
        if (usbDev->deviceName != NULL) {
          notify_item->deviceName = usbDev->deviceName;
        }
        if (usbDev->locationId != 0) {
          notify_item->locationId = usbDev->locationId;
        }
        if (usbDev->vendorId != 0) {
          notify_item->vendorId = usbDev->vendorId;
        }
        if (usbDev->productId != 0) {
          notify_item->productId = usbDev->productId;
        }
        if (usbDev->manufacturer != NULL) {
          notify_item->manufacturer = usbDev->manufacturer;
        }
        if (usbDev->serialNumber != NULL) {
          notify_item->serialNumber = usbDev->serialNumber;
        }
        if (usbDev->deviceAddress != 0) {
          notify_item->deviceAddress = usbDev->deviceAddress;
        }
        isAdded = true;
        pthread_cond_signal(&notify_cv);

        pthread_mutex_unlock(&notify_mutex);

        // Register for an interest notification of this device being removed. Use a reference to our
        // private data as the refCon which will be passed to the notification callback.
        kr = IOServiceAddInterestNotification(gNotifyPort,                      // notifyPort
                                              usbDevice,                        // service
                                              kIOGeneralInterest,               // interestType
                                              DeviceRemoved,                    // callback
                                              deviceListItem,                   // refCon
                                              &(deviceListItem->notification)   // notification
                                              );
                                                
        if (KERN_SUCCESS != kr) {
            printf("IOServiceAddInterestNotification returned 0x%08x.\n", kr);
        }
        
        // Done with this USB device; release the reference added by IOIteratorNext
        kr = IOObjectRelease(usbDevice);
    }
}

void *RunLoop(void * arg)
{

    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    
    gRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);      

    // Start the run loop. Now we'll receive notifications.
    CFRunLoopRun();

    // We should never get here
    fprintf(stderr, "Unexpectedly back from CFRunLoopRun()!\n");

    return NULL;
}

void NotifyAsync(uv_work_t* req)
{
    pthread_mutex_lock(&notify_mutex);

    pthread_cond_wait(&notify_cv, &notify_mutex);

    pthread_mutex_unlock(&notify_mutex);
}

void NotifyFinished(uv_work_t* req)
{
    pthread_mutex_lock(&notify_mutex);

    if (isAdded) {
        NotifyAdded(notify_item);
    } else {
        NotifyRemoved(notify_item);
    }

    pthread_mutex_unlock(&notify_mutex);

    uv_queue_work(uv_default_loop(), req, NotifyAsync, (uv_after_work_cb)NotifyFinished);
}

void InitDetection() {

    kern_return_t           kr;

    // Set up the matching criteria for the devices we're interested in. The matching criteria needs to follow
    // the same rules as kernel drivers: mainly it needs to follow the USB Common Class Specification, pp. 6-7.
    // See also Technical Q&A QA1076 "Tips on USB driver matching on Mac OS X" 
    // <http://developer.apple.com/qa/qa2001/qa1076.html>.
    // One exception is that you can use the matching dictionary "as is", i.e. without adding any matching 
    // criteria to it and it will match every IOUSBDevice in the system. IOServiceAddMatchingNotification will 
    // consume this dictionary reference, so there is no need to release it later on.
    
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);    // Interested in instances of class
                                                                // IOUSBDevice and its subclasses
    
    if (matchingDict == NULL) {
        fprintf(stderr, "IOServiceMatching returned NULL.\n");
    }

    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.
    
    gNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);

    // Now set up a notification to be called when a device is first matched by I/O Kit.
    kr = IOServiceAddMatchingNotification(gNotifyPort,                  // notifyPort
                                          kIOFirstMatchNotification,    // notificationType
                                          matchingDict,                 // matching
                                          DeviceAdded,                  // callback
                                          NULL,                         // refCon
                                          &gAddedIter                   // notification
                                          );        
    
    if (KERN_SUCCESS != kr) {
        printf("IOServiceAddMatchingNotification returned 0x%08x.\n", kr);
    }

    // Iterate once to get already-present devices and arm the notification
    DeviceAdded(NULL, gAddedIter);


    pthread_mutex_init(&notify_mutex, NULL);
    pthread_cond_init(&notify_cv, NULL);


    int rc = pthread_create(&lookupThread, NULL, RunLoop, NULL);
    if (rc){
         printf("ERROR; return code from pthread_create() is %d\n", rc);
         exit(-1);
    }

    uv_work_t* req = new uv_work_t();
    uv_queue_work(uv_default_loop(), req, NotifyAsync, (uv_after_work_cb)NotifyFinished);
}

void EIO_Find(uv_work_t* req) {

    ListBaton* data = static_cast<ListBaton*>(req->data);

    if (*(devices->length) > 0)
    {    
        stDeviceListItem* next = devices;
        
        for (int i = 0, len = *(devices->length); i < len; i++) {
            stUsbDevice device = (* next).value;

            if (((data->vid != 0 && data->pid != 0) &&
                (data->vid == device.vendorId && data->pid == device.productId))
                ||
                ((data->vid != 0 && data->pid == 0) &&
                data->vid == device.vendorId)
                ||
                (data->vid == 0 && data->pid == 0))
            {
                ListResultItem* resultItem = new ListResultItem();

                if (device.deviceName != NULL) {
                  resultItem->deviceName = device.deviceName;
                }
                if (device.locationId != 0) {
                  resultItem->locationId = device.locationId;
                }
                if (device.vendorId != 0) {
                  resultItem->vendorId = device.vendorId;
                }
                if (device.productId != 0) {
                  resultItem->productId = device.productId;
                }
                if (device.manufacturer != NULL) {
                  resultItem->manufacturer = device.manufacturer;
                }
                if (device.serialNumber != NULL) {
                  resultItem->serialNumber = device.serialNumber;
                }
                data->results.push_back(resultItem);
            }

            stDeviceListItem* current = next;

            if (next->next != NULL)
            {
              next = next->next;
            }

            // free(current);
        }

    }

}
