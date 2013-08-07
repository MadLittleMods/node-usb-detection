
#ifndef _USB_DETECTION_H
#define _USB_DETECTION_H

#include <node.h>
#include <v8.h>
#include <list>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deviceList.h"

v8::Handle<v8::Value> Find(const v8::Arguments& args);
void EIO_Find(uv_work_t* req);
void EIO_AfterFind(uv_work_t* req);
void InitDetection();
v8::Handle<v8::Value> StartMonitoring(const v8::Arguments& args);
void Start();
v8::Handle<v8::Value> StopMonitoring(const v8::Arguments& args);
void Stop();


struct ListBaton 
{
public:
  v8::Persistent<v8::Value> callback;
  std::list<ListResultItem_t*> results;
  char errorString[1024];
  int vid;
  int pid;
};

v8::Handle<v8::Value> RegisterAdded(const v8::Arguments& args);
void NotifyAdded(ListResultItem_t* it);
v8::Handle<v8::Value> RegisterRemoved(const v8::Arguments& args);
void NotifyRemoved(ListResultItem_t* it);

#endif
