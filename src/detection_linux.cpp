#include <libudev.h>
#include <poll.h>
#include <thread>
#include <atomic>

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
 * Public Functions
 **********************************/

class LinuxDetection : public Detection, public Napi::ObjectWrap<LinuxDetection>
{
public:
	LinuxDetection(const Napi::CallbackInfo &info)
		: Napi::ObjectWrap<LinuxDetection>(info)
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

		/* Create the udev object */
		udev *udevHandle = udev_new();
		if (!udevHandle)
		{
			printf("Can't create udev\n");
			isRunning = false;
			return false;
		}

		/* Set up a monitor to monitor devices */
		udev_monitor *mon = udev_monitor_new_from_netlink(udevHandle, "udev");
		if (!mon)
		{
			printf("Can't create udev monitor\n");
			udev_unref(udevHandle);
			isRunning = false;
			return false;
		}

		udev_monitor_enable_receiving(mon);

		/* Get the file descriptor (fd) for the monitor.
		 This fd will get passed to select() */
		int fd = udev_monitor_get_fd(mon);

		// Do initial scan
		BuildInitialDeviceList(udevHandle);

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
									  if (!isRunning)
									  {
										  return;
									  }

									  // uv_signal_start(&int_signal, cbTerminate, SIGINT);
									  // uv_signal_start(&term_signal, cbTerminate, SIGTERM);

									  udev_device *dev;

									  pollfd fds = {fd, POLLIN, 0};
									  while (isRunning)
									  {
										  int ret = poll(&fds, 1, 100);
										  if (!ret)
											  continue;
										  if (ret < 0)
											  break;

										  dev = udev_monitor_receive_device(mon);
										  if (dev)
										  {
											  if (udev_device_get_devtype(dev) && strcmp(udev_device_get_devtype(dev), DEVICE_TYPE_DEVICE) == 0)
											  {
												  if (strcmp(udev_device_get_action(dev), DEVICE_ACTION_ADDED) == 0)
												  {
													  std::shared_ptr<ListResultItem_t> item = GetProperties(dev);

													  deviceMap.addItem(udev_device_get_devnode(dev), item);

													  DeviceAdded(item);
												  }
												  else if (strcmp(udev_device_get_action(dev), DEVICE_ACTION_REMOVED) == 0)
												  {
													  std::shared_ptr<ListResultItem_t> item = deviceMap.popItem(udev_device_get_devnode(dev));
													  if (item == nullptr)
													  {
														  item = GetProperties(dev);
													  }

													  DeviceRemoved(item);
												  }
											  }
											  udev_device_unref(dev);
										  }
									  }

									  udev_monitor_unref(mon);
									  udev_unref(udevHandle);

									  notify_func.Release();
								  });

		return true;
	}
	void Stop()
	{
		if (isRunning)
		{
			isRunning = false;

			poll_thread.join();
		}
	}

private:
	/**********************************
 	 * Local Functions
	 **********************************/
	std::shared_ptr<ListResultItem_t> GetProperties(struct udev_device *dev)
	{
		std::shared_ptr<ListResultItem_t> item(new ListResultItem_t);

		auto sysattrs = udev_device_get_properties_list_entry(dev);
		struct udev_list_entry *entry;
		udev_list_entry_foreach(entry, sysattrs)
		{
			const char *name = udev_list_entry_get_name(entry);
			const char *value = udev_list_entry_get_value(entry);

			if (strcmp(name, DEVICE_PROPERTY_NAME) == 0)
			{
				item->deviceName = value;
			}
			else if (strcmp(name, DEVICE_PROPERTY_SERIAL) == 0)
			{
				item->serialNumber = value;
			}
			else if (strcmp(name, DEVICE_PROPERTY_VENDOR) == 0)
			{
				item->manufacturer = value;
			}
		}

		const char *vendor = udev_device_get_sysattr_value(dev, "idVendor");
		if (vendor)
		{
			item->vendorId = strtol(vendor, NULL, 16);
		}
		const char *product = udev_device_get_sysattr_value(dev, "idVendor");
		if (product)
		{
			item->productId = strtol(product, NULL, 16);
		}
		item->deviceAddress = 0;
		item->locationId = 0;

		return item;
	}

	void BuildInitialDeviceList(udev *udevHandle)
	{
		/* Create a list of the devices */
		auto enumerate = udev_enumerate_new(udevHandle);
		udev_enumerate_scan_devices(enumerate);
		auto devices = udev_enumerate_get_list_entry(enumerate);
		/* For each item enumerated, print out its information.
	     udev_list_entry_foreach is a macro which expands to
	     a loop. The loop will be executed for each member in
	     devices, setting dev_list_entry to a list entry
	     which contains the device's path in /sys. */
		udev_list_entry *dev_list_entry;
		udev_device *dev;
		udev_list_entry_foreach(dev_list_entry, devices)
		{
			const char *path;

			/* Get the filename of the /sys entry for the device
		   and create a udev_device object (dev) representing it */
			path = udev_list_entry_get_name(dev_list_entry);
			dev = udev_device_new_from_syspath(udevHandle, path);

			/* usb_device_get_devnode() returns the path to the device node
		     itself in /dev. */
			if (udev_device_get_devnode(dev) == NULL || udev_device_get_sysattr_value(dev, "idVendor") == NULL)
			{
				continue;
			}

			/* From here, we can call get_sysattr_value() for each file
		     in the device's /sys entry. The strings passed into these
		     functions (idProduct, idVendor, serial, etc.) correspond
		     directly to the files in the /sys directory which
		     represents the USB device. Note that USB strings are
		     Unicode, UCS2 encoded, but the strings returned from
		     udev_device_get_sysattr_value() are UTF-8 encoded. */

			// ListResultItem_t *item = new ListResultItem_t();
			// item->vendorId = strtol(udev_device_get_sysattr_value(dev, "idVendor"), NULL, 16);
			// item->productId = strtol(udev_device_get_sysattr_value(dev, "idProduct"), NULL, 16);
			// if (udev_device_get_sysattr_value(dev, "product") != NULL)
			// {
			// 	item->deviceName = udev_device_get_sysattr_value(dev, "product");
			// }
			// if (udev_device_get_sysattr_value(dev, "manufacturer") != NULL)
			// {
			// 	item->manufacturer = udev_device_get_sysattr_value(dev, "manufacturer");
			// }
			// if (udev_device_get_sysattr_value(dev, "serial") != NULL)
			// {
			// 	item->serialNumber = udev_device_get_sysattr_value(dev, "serial");
			// }
			// item->deviceAddress = 0;
			// item->locationId = 0;
			auto item = GetProperties(dev);

			deviceMap.addItem(udev_device_get_devnode(dev), item);

			udev_device_unref(dev);
		}
		/* Free the enumerator object */
		udev_enumerate_unref(enumerate);
	}

	/**********************************
	 * Local Variables
	 **********************************/
	std::thread poll_thread;

	std::atomic<bool> isRunning = {false};
};

void InitializeDetection(Napi::Env &env, Napi::Object &target)
{
	LinuxDetection::Initialize(env, target);
}