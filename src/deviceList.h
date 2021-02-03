#ifndef _DEVICE_LIST_H
#define _DEVICE_LIST_H

#include <string>
#include <list>

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

void AddItemToList(char *key, std::shared_ptr<ListResultItem_t> item);
std::shared_ptr<ListResultItem_t> PopItemFromList(char *key);
// ListResultItem_t *CopyElement(ListResultItem_t *item);
void CreateFilteredList(std::list<ListResultItem_t *> *filteredList, int vid, int pid);

#endif
