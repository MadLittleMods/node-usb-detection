
#ifndef _USB_DETECTION_H
#define _USB_DETECTION_H

#include <napi.h>
#include <list>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deviceList.h"

// void EIO_Find(uv_work_t *req);
// void InitDetection();
bool IsRunning();
bool Start(const Napi::Env &env, const Napi::Function &callback);
void Stop();

Napi::Value DeviceItemToObject(const Napi::Env &env, std::shared_ptr<ListResultItem_t> it);

struct ListBaton
{
public:
	Napi::FunctionReference *callback;
	std::list<ListResultItem_t *> results;
	char errorString[1024];
	int vid;
	int pid;
};

void NotifyAdded(ListResultItem_t *it);
void NotifyRemoved(ListResultItem_t *it);

#endif

#ifdef DEBUG
#define DEBUG_HEADER fprintf(stderr, "node-usb-detection [%s:%s() %d]: ", __FILE__, __FUNCTION__, __LINE__);
#define DEBUG_FOOTER fprintf(stderr, "\n");
#define DEBUG_LOG(...)                         \
	DEBUG_HEADER fprintf(stderr, __VA_ARGS__); \
	DEBUG_FOOTER
#else
#define DEBUG_LOG(...)
#endif