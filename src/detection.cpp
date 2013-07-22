#include "detection.h"


#define OBJECT_ITEM_LOCATION_ID     "locationId"
#define OBJECT_ITEM_VENDOR_ID       "vendorId"
#define OBJECT_ITEM_PRODUCT_ID      "productId"
#define OBJECT_ITEM_DEVICE_NAME     "deviceName"
#define OBJECT_ITEM_MANUFACTURER    "manufacturer"
#define OBJECT_ITEM_SERIAL_NUMBER   "serialNumber"
#define OBJECT_ITEM_DEVICE_ADDRESS  "deviceAddress"


NanCallback* addedCallback;
bool isAddedRegistered = false;

NanCallback* removedCallback;
bool isRemovedRegistered = false;

void NotifyAdded(ListResultItem_t* it) 
{
  if (isAddedRegistered) 
  {
    v8::Local<v8::Value> argv[1];
    v8::Local<v8::Object> item = v8::Object::New();
    item->Set(v8::String::New(OBJECT_ITEM_LOCATION_ID), v8::Number::New(it->locationId));
    item->Set(v8::String::New(OBJECT_ITEM_VENDOR_ID), v8::Number::New(it->vendorId));
    item->Set(v8::String::New(OBJECT_ITEM_PRODUCT_ID), v8::Number::New(it->productId));
    item->Set(v8::String::New(OBJECT_ITEM_DEVICE_NAME), v8::String::New(it->deviceName.c_str()));
    item->Set(v8::String::New(OBJECT_ITEM_MANUFACTURER), v8::String::New(it->manufacturer.c_str()));
    item->Set(v8::String::New(OBJECT_ITEM_SERIAL_NUMBER), v8::String::New(it->serialNumber.c_str()));
    item->Set(v8::String::New(OBJECT_ITEM_DEVICE_ADDRESS), v8::Number::New(it->deviceAddress));
    argv[0] = item;
    addedCallback->Call(1, argv);
  }
}

void NotifyRemoved(ListResultItem_t* it) 
{
  if (isRemovedRegistered) 
  {
    v8::Local<v8::Value> argv[1];
    v8::Local<v8::Object> item = v8::Object::New();
    item->Set(v8::String::New(OBJECT_ITEM_LOCATION_ID), v8::Number::New(it->locationId));
    item->Set(v8::String::New(OBJECT_ITEM_VENDOR_ID), v8::Number::New(it->vendorId));
    item->Set(v8::String::New(OBJECT_ITEM_PRODUCT_ID), v8::Number::New(it->productId));
    item->Set(v8::String::New(OBJECT_ITEM_DEVICE_NAME), v8::String::New(it->deviceName.c_str()));
    item->Set(v8::String::New(OBJECT_ITEM_MANUFACTURER), v8::String::New(it->manufacturer.c_str()));
    item->Set(v8::String::New(OBJECT_ITEM_SERIAL_NUMBER), v8::String::New(it->serialNumber.c_str()));
    item->Set(v8::String::New(OBJECT_ITEM_DEVICE_ADDRESS), v8::Number::New(it->deviceAddress));
    argv[0] = item;
    removedCallback->Call(1, argv);
  }
}

NAN_METHOD(RegisterAdded) {
  NanScope();

  NanCallback* callback = NULL;

  if (args.Length() == 0)
  {
    return NanThrowError("First argument must be a function");
  }

  if (args.Length() == 1) 
  {
    // callback
    if(!args[0]->IsFunction()) 
    {
      return NanThrowError("First argument must be a function");
    }

    callback = new NanCallback(v8::Local<v8::Function>::Cast(args[0]));
  }

  addedCallback = callback;
  isAddedRegistered = true;

  NanReturnUndefined();
}

NAN_METHOD(RegisterRemoved) {
  NanScope();

  NanCallback* callback = NULL;

  if (args.Length() == 0)
  {
    return NanThrowError("First argument must be a function");
  }

  if (args.Length() == 1) 
  {
    // callback
    if(!args[0]->IsFunction()) 
    {
      return NanThrowError("First argument must be a function");
    }

    callback = new NanCallback(v8::Local<v8::Function>::Cast(args[0]));
  }

  removedCallback = callback;
  isRemovedRegistered = true;

  NanReturnUndefined();
}

class FindWorker : public NanAsyncWorker {
 public:
  FindWorker(NanCallback *callback, int vid, int pid)
    : NanAsyncWorker(callback), vid(vid), pid(pid) {}
  ~FindWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    list = EIO_Find(vid, pid, errorString);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    NanScope();

    v8::Local<v8::Value> argv[2];
    if(errorString[0]) 
    {
      argv[0] = v8::Exception::Error(v8::String::New(errorString));
      argv[1] = v8::Local<v8::Value>::New(v8::Undefined());
    } 
    else 
    {
      v8::Local<v8::Array> results = v8::Array::New();
      int i = 0;
      for(std::list<ListResultItem_t*>::iterator it = list.begin(); it != list.end(); it++, i++) {
        v8::Local<v8::Object> item = v8::Object::New();
        item->Set(v8::String::New(OBJECT_ITEM_LOCATION_ID), v8::Number::New((*it)->locationId));
        item->Set(v8::String::New(OBJECT_ITEM_VENDOR_ID), v8::Number::New((*it)->vendorId));
        item->Set(v8::String::New(OBJECT_ITEM_PRODUCT_ID), v8::Number::New((*it)->productId));
        item->Set(v8::String::New(OBJECT_ITEM_DEVICE_NAME), v8::String::New((*it)->deviceName.c_str()));
        item->Set(v8::String::New(OBJECT_ITEM_MANUFACTURER), v8::String::New((*it)->manufacturer.c_str()));
        item->Set(v8::String::New(OBJECT_ITEM_SERIAL_NUMBER), v8::String::New((*it)->serialNumber.c_str()));
        item->Set(v8::String::New(OBJECT_ITEM_DEVICE_ADDRESS), v8::Number::New((*it)->deviceAddress));
        results->Set(i, item);
      }
      argv[0] = v8::Local<v8::Value>::New(v8::Undefined());
      argv[1] = results;
    }
    
    callback->Call(2, argv);

    for(std::list<ListResultItem_t*>::iterator it = list.begin(); it != list.end(); it++) 
    {
      delete *it;
    }
  };

 private:
  int vid;
  int pid;
  std::list<ListResultItem_t*> list;
  char errorString[1024];
};

NAN_METHOD(Find) {
  NanScope();

  int vid = 0;
  int pid = 0;
  NanCallback *callback;

  if (args.Length() == 0) 
  {
    return NanThrowError("First argument must be a function");
  }

  if (args.Length() == 3) 
  {
    if (args[0]->IsNumber() && args[1]->IsNumber()) 
    {
        vid = (int) args[0]->NumberValue();
        pid = (int) args[1]->NumberValue();
    }

    // callback
    if(!args[2]->IsFunction()) 
    {
      return NanThrowError("Third argument must be a function");
    }

    callback = new NanCallback(args[2].As<v8::Function>());
  }

  if (args.Length() == 2) 
  {
    if (args[0]->IsNumber()) 
    {
        vid = (int) args[0]->NumberValue();
    }

    // callback
    if(!args[1]->IsFunction()) 
    {
      return NanThrowError("Second argument must be a function");
    }

    callback = new NanCallback(args[1].As<v8::Function>());
  }

  if (args.Length() == 1) 
  {
    // callback
    if(!args[0]->IsFunction()) 
    {
      return NanThrowError("First argument must be a function");
    }

    callback = new NanCallback(args[1].As<v8::Function>());
  }

  NanAsyncQueueWorker(new FindWorker(callback, vid, pid));
  NanReturnUndefined();
}

NAN_METHOD(StartMonitoring) {
  NanScope();

  Start();

  NanReturnUndefined();
}

NAN_METHOD(StopMonitoring) {
  NanScope();

  Stop();

  NanReturnUndefined();
}

extern "C" {
  void init (v8::Handle<v8::Object> target) 
  {
    NODE_SET_METHOD(target, "find", Find);
    NODE_SET_METHOD(target, "registerAdded", RegisterAdded);
    NODE_SET_METHOD(target, "registerRemoved", RegisterRemoved);
    NODE_SET_METHOD(target, "startMonitoring", StartMonitoring);
    NODE_SET_METHOD(target, "stopMonitoring", StopMonitoring);
    InitDetection();
  }
}

NODE_MODULE(detection, init);
