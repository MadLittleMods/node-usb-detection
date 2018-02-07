#include <libudev.h>
#include <poll.h>

#include "detection.h"
#include "deviceList.h"

using namespace std;



/**********************************
 * Local defines
 **********************************/
#define DEVICE_ACTION_ADDED "add"
#define DEVICE_ACTION_REMOVED "remove"

#define DEVICE_TYPE_DEVICE "usb_device"

#define DEVICE_PROPERTY_NAME "ID_MODEL"
#define DEVICE_PROPERTY_SERIAL "ID_SERIAL_SHORT"
#define DEVICE_PROPERTY_VENDOR "ID_VENDOR"


/**********************************
 * Local typedefs
 **********************************/



/**********************************
 * Local Variables
 **********************************/
static ListResultItem_t* currentItem;
static bool isAdded;

static udev *udev;
static udev_enumerate *enumerate;
static udev_list_entry *devices, *dev_list_entry;
static udev_device *dev;

static udev_monitor *mon;
static int fd;

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
static void BuildInitialDeviceList();

static void WaitForDeviceHandled();
static void SignalDeviceHandled();
static void cbTerminate(uv_signal_t *handle, int signum);
static void cbWork(uv_work_t *req);
static void cbAfter(uv_work_t *req, int status);
static void cbAsync(uv_async_t *handle);

/**********************************
 * Public Functions
 **********************************/
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

	udev_monitor_unref(mon);
	udev_unref(udev);
}

void InitDetection() {
	/* Create the udev object */
	udev = udev_new();
	if (!udev)
	{
		printf("Can't create udev\n");
		return;
	}

	/* Set up a monitor to monitor devices */
	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_enable_receiving(mon);

	/* Get the file descriptor (fd) for the monitor.
	   This fd will get passed to select() */
	fd = udev_monitor_get_fd(mon);

	BuildInitialDeviceList();
}


void EIO_Find(uv_work_t* req) {
	ListBaton* data = static_cast<ListBaton*>(req->data);

	CreateFilteredList(&data->results, data->vid, data->pid);
}

/**********************************
 * Local Functions
 **********************************/
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

static ListResultItem_t* GetProperties(struct udev_device* dev, ListResultItem_t* item) {
	struct udev_list_entry* sysattrs;
	struct udev_list_entry* entry;
	sysattrs = udev_device_get_properties_list_entry(dev);
	udev_list_entry_foreach(entry, sysattrs) {
		const char *name, *value;
		name = udev_list_entry_get_name(entry);
		value = udev_list_entry_get_value(entry);

		if(strcmp(name, DEVICE_PROPERTY_NAME) == 0) {
			item->deviceName = value;
		}
		else if(strcmp(name, DEVICE_PROPERTY_SERIAL) == 0) {
			item->serialNumber = value;
		}
		else if(strcmp(name, DEVICE_PROPERTY_VENDOR) == 0) {
			item->manufacturer = value;
		}
	}
	item->vendorId = strtol(udev_device_get_sysattr_value(dev,"idVendor"), NULL, 16);
	item->productId = strtol(udev_device_get_sysattr_value(dev,"idProduct"), NULL, 16);
	item->deviceAddress = 0;
	item->locationId = 0;

	return item;
}

static void DeviceAdded(struct udev_device* dev) {
	DeviceItem_t* item = new DeviceItem_t();
	GetProperties(dev, &item->deviceParams);

	AddItemToList((char *)udev_device_get_devnode(dev), item);

	currentItem = &item->deviceParams;
	isAdded = true;

	uv_async_send(&async_handler);
}

static void DeviceRemoved(struct udev_device* dev) {
	ListResultItem_t* item = NULL;

	if(IsItemAlreadyStored((char *)udev_device_get_devnode(dev))) {
		DeviceItem_t* deviceItem = GetItemFromList((char *)udev_device_get_devnode(dev));
		if(deviceItem) {
			item = CopyElement(&deviceItem->deviceParams);
		}
		RemoveItemFromList(deviceItem);
		delete deviceItem;
	}

	if(item == NULL) {
		item = new ListResultItem_t();
		GetProperties(dev, item);
	}

	currentItem = item;
	isAdded = false;

	uv_async_send(&async_handler);
}


static void cbWork(uv_work_t *req) {
	// We have this check in case we `Stop` before this thread starts,
	// otherwise the process will hang
	if(!isRunning) {
		return;
	}

	uv_signal_start(&int_signal, cbTerminate, SIGINT);
	uv_signal_start(&term_signal, cbTerminate, SIGTERM);

	pollfd fds = {fd, POLLIN, 0};
	while (isRunning) {
		int ret = poll(&fds, 1, 100);
		if (!ret) continue;
		if (ret < 0) break;

		dev = udev_monitor_receive_device(mon);
		if (dev) {
			if(udev_device_get_devtype(dev) && strcmp(udev_device_get_devtype(dev), DEVICE_TYPE_DEVICE) == 0) {
				if(strcmp(udev_device_get_action(dev), DEVICE_ACTION_ADDED) == 0) {
					WaitForDeviceHandled();
					DeviceAdded(dev);
				}
				else if(strcmp(udev_device_get_action(dev), DEVICE_ACTION_REMOVED) == 0) {
					WaitForDeviceHandled();
					DeviceRemoved(dev);
				}
			}
			udev_device_unref(dev);
		}
	}
}

static void cbAfter(uv_work_t *req, int status) {
	Stop();
}

static void cbAsync(uv_async_t *handle) {
	if(!isRunning) {
		return;
	}

	if (isAdded) {
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


static void BuildInitialDeviceList() {
	/* Create a list of the devices */
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);
	/* For each item enumerated, print out its information.
	   udev_list_entry_foreach is a macro which expands to
	   a loop. The loop will be executed for each member in
	   devices, setting dev_list_entry to a list entry
	   which contains the device's path in /sys. */
	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;

		/* Get the filename of the /sys entry for the device
		   and create a udev_device object (dev) representing it */
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);

		/* usb_device_get_devnode() returns the path to the device node
		   itself in /dev. */
		if(udev_device_get_devnode(dev) == NULL || udev_device_get_sysattr_value(dev,"idVendor") == NULL) {
			continue;
		}

		/* From here, we can call get_sysattr_value() for each file
		   in the device's /sys entry. The strings passed into these
		   functions (idProduct, idVendor, serial, etc.) correspond
		   directly to the files in the /sys directory which
		   represents the USB device. Note that USB strings are
		   Unicode, UCS2 encoded, but the strings returned from
		   udev_device_get_sysattr_value() are UTF-8 encoded. */

		DeviceItem_t* item = new DeviceItem_t();
		item->deviceParams.vendorId = strtol (udev_device_get_sysattr_value(dev,"idVendor"), NULL, 16);
		item->deviceParams.productId = strtol (udev_device_get_sysattr_value(dev,"idProduct"), NULL, 16);
		if(udev_device_get_sysattr_value(dev,"product") != NULL) {
			item->deviceParams.deviceName = udev_device_get_sysattr_value(dev,"product");
		}
		if(udev_device_get_sysattr_value(dev,"manufacturer") != NULL) {
			item->deviceParams.manufacturer = udev_device_get_sysattr_value(dev,"manufacturer");
		}
		if(udev_device_get_sysattr_value(dev,"serial") != NULL) {
			item->deviceParams.serialNumber = udev_device_get_sysattr_value(dev, "serial");
		}
		item->deviceParams.deviceAddress = 0;
		item->deviceParams.locationId = 0;

		item->deviceState = DeviceState_Connect;

		AddItemToList((char *)udev_device_get_devnode(dev), item);

		udev_device_unref(dev);
	}
	/* Free the enumerator object */
	udev_enumerate_unref(enumerate);
}
