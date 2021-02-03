#ifndef _DEVICE_LIST_H
#define _DEVICE_LIST_H

#include <string>
#include <list>
#include <map>
#include <memory>
typedef struct
{
public:
	int locationId;
	int vendorId;
	int productId;
	std::string deviceName;
	std::string manufacturer;
	std::string serialNumber;
	int deviceAddress;
} ListResultItem_t;

typedef enum _DeviceState_t
{
	DeviceState_Connect,
	DeviceState_Disconnect,
} DeviceState_t;

class DeviceMap
{
public:
	void addItem(std::string key, std::shared_ptr<ListResultItem_t> item);
	std::shared_ptr<ListResultItem_t> popItem(std::string key);

	std::list<std::shared_ptr<ListResultItem_t>> filterItems(int vid, int pid);

private:
	std::map<std::string, std::shared_ptr<ListResultItem_t>> deviceMap;
};

#endif
