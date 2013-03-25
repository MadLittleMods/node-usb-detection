
#ifndef _serialport_h_
#define _serialport_h_

#include <node.h>
#include <v8.h>
#include <list>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

v8::Handle<v8::Value> Find(const v8::Arguments& args);
void EIO_Find(uv_work_t* req);
void EIO_AfterFind(uv_work_t* req);
void InitDetection();

struct ListResultItem {
public:
  int locationId;
  int vendorId;
  int productId;
  std::string deviceName;
  std::string manufacturer;
  std::string serialNumber;
  int deviceAddress;
};

struct ListBaton {
public:
  v8::Persistent<v8::Value> callback;
  std::list<ListResultItem*> results;
  char errorString[1024];
  int vid;
  int pid;
};

v8::Handle<v8::Value> RegisterAdded(const v8::Arguments& args);
void NotifyAdded(ListResultItem* it);
v8::Handle<v8::Value> RegisterRemoved(const v8::Arguments& args);
void NotifyRemoved(ListResultItem* it);

#endif
