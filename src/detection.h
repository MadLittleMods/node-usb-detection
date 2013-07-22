
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
#include "nan.h"

std::list<ListResultItem_t*> EIO_Find(int vid, int pid, char errorString[1024]);
void InitDetection();
void Start();
void Stop();

void NotifyAdded(ListResultItem_t* it);
void NotifyRemoved(ListResultItem_t* it);

#endif
