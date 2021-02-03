#include <map>
#include <string.h>
#include <stdio.h>

#include "deviceList.h"

using namespace std;

std::map<string, ListResultItem_t *> deviceMap;

void AddItemToList(char *key, ListResultItem_t *item)
{
	deviceMap.insert(std::pair<string, ListResultItem_t *>(key, item));
}

ListResultItem_t *PopItemFromList(char *key)
{
	auto it = deviceMap.find(key);
	if (it == deviceMap.end())
	{
		return NULL;
	}
	else
	{
		ListResultItem_t *item = it->second;
		deviceMap.erase(it);
		return item;
	}
}

bool IsItemAlreadyStored(char *key)
{
	auto it = deviceMap.find(key);
	return it != deviceMap.end();
}

ListResultItem_t *CopyElement(ListResultItem_t *item)
{
	ListResultItem_t *dst = new ListResultItem_t();
	dst->locationId = item->locationId;
	dst->vendorId = item->vendorId;
	dst->productId = item->productId;
	dst->deviceName = item->deviceName;
	dst->manufacturer = item->manufacturer;
	dst->serialNumber = item->serialNumber;
	dst->deviceAddress = item->deviceAddress;

	return dst;
}

void CreateFilteredList(list<ListResultItem_t *> *filteredList, int vid, int pid)
{
	map<string, ListResultItem_t *>::iterator it;

	for (it = deviceMap.begin(); it != deviceMap.end(); ++it)
	{
		ListResultItem_t *item = it->second;

		if (
			((vid != 0 && pid != 0) && (vid == item->vendorId && pid == item->productId)) || ((vid != 0 && pid == 0) && vid == item->vendorId) || (vid == 0 && pid == 0))
		{
			(*filteredList).push_back(CopyElement(item));
		}
	}
}
