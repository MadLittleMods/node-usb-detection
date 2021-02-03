#include <map>
#include <string.h>
#include <stdio.h>
#include <memory>

#include "deviceList.h"

std::map<std::string, std::shared_ptr<ListResultItem_t>> deviceMap;

void AddItemToList(char *key, std::shared_ptr<ListResultItem_t> item)
{
	deviceMap.insert(std::pair<std::string, std::shared_ptr<ListResultItem_t>>(key, item));
}

std::shared_ptr<ListResultItem_t> PopItemFromList(char *key)
{
	auto it = deviceMap.find(key);
	if (it == deviceMap.end())
	{
		return nullptr;
	}
	else
	{
		std::shared_ptr<ListResultItem_t> item = it->second;
		deviceMap.erase(it);
		return item;
	}
}

// ListResultItem_t *CopyElement(ListResultItem_t *item)
// {
// 	ListResultItem_t *dst = new ListResultItem_t();
// 	dst->locationId = item->locationId;
// 	dst->vendorId = item->vendorId;
// 	dst->productId = item->productId;
// 	dst->deviceName = item->deviceName;
// 	dst->manufacturer = item->manufacturer;
// 	dst->serialNumber = item->serialNumber;
// 	dst->deviceAddress = item->deviceAddress;

// 	return dst;
// }

void CreateFilteredList(std::list<std::shared_ptr<ListResultItem_t>> *filteredList, int vid, int pid)
{
	for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it)
	{
		std::shared_ptr<ListResultItem_t> item = it->second;

		if (
			((vid != 0 && pid != 0) && (vid == item->vendorId && pid == item->productId)) || ((vid != 0 && pid == 0) && vid == item->vendorId) || (vid == 0 && pid == 0))
		{
			(*filteredList).push_back(item);
		}
	}
}
