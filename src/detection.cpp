#include "detection.h"

#define OBJECT_ITEM_LOCATION_ID "locationId"
#define OBJECT_ITEM_VENDOR_ID "vendorId"
#define OBJECT_ITEM_PRODUCT_ID "productId"
#define OBJECT_ITEM_DEVICE_NAME "deviceName"
#define OBJECT_ITEM_MANUFACTURER "manufacturer"
#define OBJECT_ITEM_SERIAL_NUMBER "serialNumber"
#define OBJECT_ITEM_DEVICE_ADDRESS "deviceAddress"

Detection::~Detection()
{
}

Napi::Value Detection::StartMonitoring(const Napi::CallbackInfo &args)
{
	Napi::Env env = args.Env();

	if (args.Length() == 0)
	{
		Napi::TypeError::New(env, "First argument must be a function").ThrowAsJavaScriptException();
		return env.Null();
	}

	// callback
	if (!args[0].IsFunction())
	{
		Napi::TypeError::New(env, "First argument must be a function").ThrowAsJavaScriptException();
		return env.Null();
	}

	if (!Start(env, args[0].As<Napi::Function>()))
	{
		Napi::Error::New(args.Env(), "Failed to start monitoring").ThrowAsJavaScriptException();
		return env.Null();
	}

	return env.Null();
}

void Detection::StopMonitoring(const Napi::CallbackInfo &args)
{
	Stop();
}

Napi::Value Detection::IsMonitoring(const Napi::CallbackInfo &args)
{
	Napi::Env env = args.Env();

	bool isRunning = IsRunning();
	return Napi::Boolean::From(env, isRunning);
}

Napi::Value DeviceItemToObject(const Napi::Env &env, std::shared_ptr<ListResultItem_t> it)
{
	Napi::Object item = Napi::Object::New(env);
	item.Set(Napi::String::New(env, OBJECT_ITEM_LOCATION_ID), Napi::Number::New(env, it->locationId));
	item.Set(Napi::String::New(env, OBJECT_ITEM_VENDOR_ID), Napi::Number::New(env, it->vendorId));
	item.Set(Napi::String::New(env, OBJECT_ITEM_PRODUCT_ID), Napi::Number::New(env, it->productId));
	item.Set(Napi::String::New(env, OBJECT_ITEM_DEVICE_NAME), Napi::String::New(env, it->deviceName.c_str()));
	item.Set(Napi::String::New(env, OBJECT_ITEM_MANUFACTURER), Napi::String::New(env, it->manufacturer.c_str()));
	item.Set(Napi::String::New(env, OBJECT_ITEM_SERIAL_NUMBER), Napi::String::New(env, it->serialNumber.c_str()));
	item.Set(Napi::String::New(env, OBJECT_ITEM_DEVICE_ADDRESS), Napi::Number::New(env, it->deviceAddress));
	return item;
}

// class FindWorker : public Napi::AsyncWorker
// {
// public:
// 	FindWorker(
// 		Napi::Env &env,
// 		int vid,
// 		int pid)
// 		: AsyncWorker(env),
// 		  deferred(Napi::Promise::Deferred::New(env)),
// 		  vid(vid),
// 		  pid(pid)
// 	{
// 	}

// 	~FindWorker()
// 	{
// 	}

// 	void Execute()
// 	{
// 		std::string err = DoCompress(this->props);
// 		if (!err.empty())
// 		{
// 			SetError(err);
// 		}
// 	}

// 	void OnOK()
// 	{
// 		deferred.Resolve(CompressResult(Env(), this->dstBuffer.Value(), this->props));
// 	}

// 	void OnError(Napi::Error const &error)
// 	{
// 		deferred.Reject(error.Value());
// 	}

// 	Napi::Promise GetPromise() const
// 	{
// 		return deferred.Promise();
// 	}

// private:
// 	Napi::Promise::Deferred deferred;
// 	int vid;
// 	int pid;
// };

// void Find(const Napi::CallbackInfo &args)
// {
// 	Napi::HandleScope scope(env);

// 	int vid = 0;
// 	int pid = 0;
// 	Napi::Function callback;

// 	if (args.Length() == 0)
// 	{
// 		Napi::TypeError::New(env, "First argument must be a function").ThrowAsJavaScriptException();
// 		return env.Null();
// 	}

// 	if (args.Length() >= 3)
// 	{
// 		if (args[0].IsNumber() && args[1].IsNumber())
// 		{
// 			vid = (int)args[0].As<Napi::Number>().Int32Value();
// 			pid = (int)args[1].As<Napi::Number>().Int32Value();
// 		}

// 		// callback
// 		if (!args[2]->IsFunction())
// 		{
// 			Napi::TypeError::New(env, "Third argument must be a function").ThrowAsJavaScriptException();
// 			return env.Null();
// 		}

// 		callback = args[2].As<Napi::Function>();
// 	}

// 	if (args.Length() == 2)
// 	{
// 		if (args[0].IsNumber())
// 		{
// 			vid = (int)args[0].As<Napi::Number>().Int32Value();
// 		}

// 		// callback
// 		if (!args[1]->IsFunction())
// 		{
// 			Napi::TypeError::New(env, "Second argument must be a function").ThrowAsJavaScriptException();
// 			return env.Null();
// 		}

// 		callback = args[1].As<Napi::Function>();
// 	}

// 	if (args.Length() == 1)
// 	{
// 		// callback
// 		if (!args[0]->IsFunction())
// 		{
// 			Napi::TypeError::New(env, "First argument must be a function").ThrowAsJavaScriptException();
// 			return env.Null();
// 		}

// 		callback = args[0].As<Napi::Function>();
// 	}

// 	// ListBaton *baton = new ListBaton();
// 	// strcpy(baton->errorString, "");
// 	// baton->callback = new Napi::FunctionReference(callback);
// 	// baton->vid = vid;
// 	// baton->pid = pid;

// 	// uv_work_t *req = new uv_work_t();
// 	// req->data = baton;
// 	// uv_queue_work(uv_default_loop(), req, EIO_Find, (uv_after_work_cb)EIO_AfterFind);
// }

// void EIO_AfterFind(uv_work_t *req)
// {
// 	Napi::HandleScope scope(env);

// 	ListBaton *data = static_cast<ListBaton *>(req->data);

// 	Napi::Value argv[2];
// 	if (data->errorString[0])
// 	{
// 		argv[0] = v8::Exception::Error(Napi::String::New(env, data->errorString));
// 		argv[1] = env.Undefined();
// 	}
// 	else
// 	{
// 		Napi::Array results = Napi::Array::New(env);
// 		int i = 0;
// 		for (std::list<ListResultItem_t *>::iterator it = data->results.begin(); it != data->results.end(); it++, i++)
// 		{
// 			Napi::Object item = Napi::Object::New(env);
// 			(item).Set(Napi::String::New(env, OBJECT_ITEM_LOCATION_ID), Napi::Number::New(env, (*it)->locationId));
// 			(item).Set(Napi::String::New(env, OBJECT_ITEM_VENDOR_ID), Napi::Number::New(env, (*it)->vendorId));
// 			(item).Set(Napi::String::New(env, OBJECT_ITEM_PRODUCT_ID), Napi::Number::New(env, (*it)->productId));
// 			(item).Set(Napi::String::New(env, OBJECT_ITEM_DEVICE_NAME), Napi::String::New(env, (*it)->deviceName.c_str()));
// 			(item).Set(Napi::String::New(env, OBJECT_ITEM_MANUFACTURER), Napi::String::New(env, (*it)->manufacturer.c_str()));
// 			(item).Set(Napi::String::New(env, OBJECT_ITEM_SERIAL_NUMBER), Napi::String::New(env, (*it)->serialNumber.c_str()));
// 			(item).Set(Napi::String::New(env, OBJECT_ITEM_DEVICE_ADDRESS), Napi::Number::New(env, (*it)->deviceAddress));
// 			(results).Set(i, item);
// 		}
// 		argv[0] = env.Undefined();
// 		argv[1] = results;
// 	}

// 	Napi::AsyncResource resource("usb-detection:EIO_AfterFind");
// 	data->callback->Call(2, argv, &resource);

// 	for (std::list<ListResultItem_t *>::iterator it = data->results.begin(); it != data->results.end(); it++)
// 	{
// 		delete *it;
// 	}
// 	delete data;
// 	delete req;
// }

Napi::Object init(Napi::Env env, Napi::Object exports)
{
	InitializeDetection(env, exports);
	return exports;
}

NODE_API_MODULE(detection, init);
