
#ifndef _USB_DETECTION_H
#define _USB_DETECTION_H

#include <node.h>
#include <v8.h>
#include <uv.h>
#include <list>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nan.h>

#include "deviceList.h"

void Find(const Nan::FunctionCallbackInfo<v8::Value>& args);
void EIO_Find(uv_work_t* req);
void EIO_AfterFind(uv_work_t* req);
void InitDetection();
void StartMonitoring(const Nan::FunctionCallbackInfo<v8::Value>& args);
void Start();
void StopMonitoring(const Nan::FunctionCallbackInfo<v8::Value>& args);
void Stop();


struct ListBaton {
	public:
		//v8::Persistent<v8::Function> callback;
		Nan::Callback* callback;
		std::list<ListResultItem_t*> results;
		char errorString[1024];
		int vid;
		int pid;
};

void RegisterAdded(const Nan::FunctionCallbackInfo<v8::Value>& args);
void NotifyAdded(ListResultItem_t* it);
void RegisterRemoved(const Nan::FunctionCallbackInfo<v8::Value>& args);
void NotifyRemoved(ListResultItem_t* it);

#endif

#ifdef DEBUG
  #define DEBUG_HEADER fprintf(stderr, "node-usb-detection [%s:%s() %d]: ", __FILE__, __FUNCTION__, __LINE__);
  #define DEBUG_FOOTER fprintf(stderr, "\n");
  #define DEBUG_LOG(...) DEBUG_HEADER fprintf(stderr, __VA_ARGS__); DEBUG_FOOTER
#else
  #define DEBUG_LOG(...)
#endif