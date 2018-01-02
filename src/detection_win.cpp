#include <uv.h>
#include <dbt.h>
#include <iostream>
#include <iomanip>
#include <atlstr.h>


// Include Windows headers
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <Setupapi.h>

#include "detection.h"
#include "deviceList.h"

using namespace std;

/**********************************
 * Local defines
 **********************************/
#define VID_TAG "VID_"
#define PID_TAG "PID_"

#define LIBRARY_NAME ("setupapi.dll")


#define DllImport __declspec(dllimport)

#define MAX_THREAD_WINDOW_NAME 64

/**********************************
 * Local typedefs
 **********************************/



/**********************************
 * Local Variables
 **********************************/
GUID GUID_DEVINTERFACE_USB_DEVICE = {
	0xA5DCBF10L,
	0x6530,
	0x11D2,
	0x90,
	0x1F,
	0x00,
	0xC0,
	0x4F,
	0xB9,
	0x51,
	0xED
};

HWND handle;
DWORD threadId;
HANDLE threadHandle;

HANDLE deviceChangedRegisteredEvent;
HANDLE deviceChangedSentEvent;

ListResultItem_t* currentDevice;
bool isAdded;
bool isRunning = false;

HINSTANCE hinstLib;


typedef BOOL (WINAPI *_SetupDiEnumDeviceInfo) (HDEVINFO DeviceInfoSet, DWORD MemberIndex, PSP_DEVINFO_DATA DeviceInfoData);
typedef HDEVINFO (WINAPI *_SetupDiGetClassDevs) (const GUID *ClassGuid, PCTSTR Enumerator, HWND hwndParent, DWORD Flags);
typedef BOOL (WINAPI *_SetupDiDestroyDeviceInfoList) (HDEVINFO DeviceInfoSet);
typedef BOOL (WINAPI *_SetupDiGetDeviceInstanceId) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, PTSTR DeviceInstanceId, DWORD DeviceInstanceIdSize, PDWORD RequiredSize);
typedef BOOL (WINAPI *_SetupDiGetDeviceRegistryProperty) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, DWORD Property, PDWORD PropertyRegDataType, PBYTE PropertyBuffer, DWORD PropertyBufferSize, PDWORD RequiredSize);
typedef BOOL (WINAPI *_SetupDiEnumDeviceInterfaces) (HDEVINFO DeviceInfoSet, PSP_DEVINFO_DATA DeviceInfoData, const GUID *InterfaceClassGuid, DWORD MemberIndex, PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData);
typedef BOOL (WINAPI *_SetupDiGetDeviceInterfaceDetail) (HDEVINFO DeviceInfoSet, PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData, PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData, DWORD DeviceInterfaceDetailDataSize, PDWORD RequiredSize, PSP_DEVINFO_DATA DeviceInfoData);


_SetupDiEnumDeviceInfo DllSetupDiEnumDeviceInfo;
_SetupDiGetClassDevs DllSetupDiGetClassDevs;
_SetupDiDestroyDeviceInfoList DllSetupDiDestroyDeviceInfoList;
_SetupDiGetDeviceInstanceId DllSetupDiGetDeviceInstanceId;
_SetupDiGetDeviceRegistryProperty DllSetupDiGetDeviceRegistryProperty;
_SetupDiEnumDeviceInterfaces DllSetupDiEnumDeviceInterfaces;
_SetupDiGetDeviceInterfaceDetail DllSetupDiGetDeviceInterfaceDetail;


/**********************************
 * Local Helper Functions protoypes
 **********************************/
void HandleDeviceConnect(PDEV_BROADCAST_DEVICEINTERFACE pDevInf);
void HandleDeviceDisconnect(PDEV_BROADCAST_DEVICEINTERFACE pDevInf);
DWORD WINAPI ListenerThread(LPVOID lpParam);

void BuildInitialDeviceList();

void NotifyAsync(uv_work_t* req);
void NotifyFinished(uv_work_t* req);

void ExtractDeviceInfo(HDEVINFO hDevInfo, SP_DEVINFO_DATA* pspDevInfoData, TCHAR* devicePath, ListResultItem_t* resultItem);
bool CheckValidity(ListResultItem_t* item);


/**********************************
 * Public Functions
 **********************************/
void NotifyAsync(uv_work_t* req) {
	WaitForSingleObject(deviceChangedRegisteredEvent, INFINITE);
}


void NotifyFinished(uv_work_t* req) {
	if (isRunning) {
		if(isAdded) {
			NotifyAdded(currentDevice);
		}
		else {
			NotifyRemoved(currentDevice);
		}
	}

	// Delete Item in case of removal
	if(isAdded == false) {
		delete currentDevice;
	}

	SetEvent(deviceChangedSentEvent);

	currentDevice = NULL;
	uv_queue_work(uv_default_loop(), req, NotifyAsync, (uv_after_work_cb)NotifyFinished);
}

void LoadFunctions() {

	bool success;

	hinstLib = LoadLibrary(LIBRARY_NAME);

	if (hinstLib != NULL) {
		DllSetupDiEnumDeviceInfo = (_SetupDiEnumDeviceInfo) GetProcAddress(hinstLib, "SetupDiEnumDeviceInfo");

		DllSetupDiGetClassDevs = (_SetupDiGetClassDevs) GetProcAddress(hinstLib, "SetupDiGetClassDevsA");

		DllSetupDiDestroyDeviceInfoList = (_SetupDiDestroyDeviceInfoList) GetProcAddress(hinstLib, "SetupDiDestroyDeviceInfoList");

		DllSetupDiGetDeviceInstanceId = (_SetupDiGetDeviceInstanceId) GetProcAddress(hinstLib, "SetupDiGetDeviceInstanceIdA");

		DllSetupDiGetDeviceRegistryProperty = (_SetupDiGetDeviceRegistryProperty) GetProcAddress(hinstLib, "SetupDiGetDeviceRegistryPropertyA");

		DllSetupDiEnumDeviceInterfaces = (_SetupDiEnumDeviceInterfaces) GetProcAddress(hinstLib, "SetupDiEnumDeviceInterfaces");

		DllSetupDiGetDeviceInterfaceDetail = (_SetupDiGetDeviceInterfaceDetail) GetProcAddress(hinstLib, "SetupDiGetDeviceInterfaceDetailA");

		success = (
			DllSetupDiEnumDeviceInfo != NULL &&
			DllSetupDiGetClassDevs != NULL &&
			DllSetupDiDestroyDeviceInfoList != NULL &&
			DllSetupDiGetDeviceInstanceId != NULL &&
			DllSetupDiGetDeviceRegistryProperty != NULL &&
			DllSetupDiEnumDeviceInterfaces != NULL &&
			DllSetupDiGetDeviceInterfaceDetail != NULL
		);
	}
	else {
		success = false;
	}

	if(!success) {
		printf("Could not load library functions from dll -> abort (Check if %s is available)\r\n", LIBRARY_NAME);
		exit(1);
	}
}

void Start() {
	isRunning = true;
}

void Stop() {
	isRunning = false;
	SetEvent(deviceChangedRegisteredEvent);
}

void InitDetection() {

	LoadFunctions();

	deviceChangedRegisteredEvent = CreateEvent(NULL, false /* auto-reset event */, false /* non-signalled state */, "");
	deviceChangedSentEvent = CreateEvent(NULL, false /* auto-reset event */, true /* non-signalled state */, "");

	BuildInitialDeviceList();

	threadHandle = CreateThread(
			NULL,				// default security attributes
			0,					// use default stack size
			ListenerThread,		// thread function name
			NULL,				// argument to thread function
			0,					// use default creation flags
			&threadId
		);

	uv_work_t* req = new uv_work_t();
	uv_queue_work(uv_default_loop(), req, NotifyAsync, (uv_after_work_cb)NotifyFinished);
}


void EIO_Find(uv_work_t* req) {

	ListBaton* data = static_cast<ListBaton*>(req->data);

	CreateFilteredList(&data->results, data->vid, data->pid);
}


/**********************************
 * Local Functions
 **********************************/

void ExtractDetails(TCHAR* devicePath, ListResultItem_t* item) {
	// expected format:
	// \\?\USB#Vid_04e8&Pid_503b#0002F9A9828E0F06#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
	CString dpcs = devicePath + 4;

	CString temp = devicePath;
	temp = temp.Left(temp.ReverseFind(_T('#')));
	temp = temp.Mid(temp.ReverseFind(_T('#')) + 1);
	item->serialNumber = temp;

	temp = devicePath;
	temp.MakeUpper();
	item->vendorId = strtol(temp.Mid(temp.Find(VID_TAG) + strlen(VID_TAG), 4), NULL, 16);
	item->productId = strtol(temp.Mid(temp.Find(PID_TAG) + strlen(PID_TAG), 4), NULL, 16);
}


LRESULT CALLBACK DetectCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_DEVICECHANGE) {
		if ( DBT_DEVICEARRIVAL == wParam || DBT_DEVICEREMOVECOMPLETE == wParam ) {
			PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
			PDEV_BROADCAST_DEVICEINTERFACE pDevInf;

			if(pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
				pDevInf = (PDEV_BROADCAST_DEVICEINTERFACE)pHdr;
				if (DBT_DEVICEARRIVAL == wParam) {
					HandleDeviceConnect(pDevInf);
				} else {
					HandleDeviceDisconnect(pDevInf);
				}
			}
		}
	}

	return 1;
}


DWORD WINAPI ListenerThread( LPVOID lpParam ) {
	char className[MAX_THREAD_WINDOW_NAME];
	_snprintf_s(className, MAX_THREAD_WINDOW_NAME, "ListnerThreadUsbDetection_%d", GetCurrentThreadId());

	WNDCLASSA wincl = {0};
	wincl.hInstance = GetModuleHandle(0);
	wincl.lpszClassName = className;
	wincl.lpfnWndProc = DetectCallback;

	if (!RegisterClassA(&wincl)) {
		DWORD le = GetLastError();
		printf("RegisterClassA() failed [Error: %x]\r\n", le);
		return 1;
	}


	HWND hwnd = CreateWindowExA(WS_EX_TOPMOST, className, className, 0, 0, 0, 0, 0, NULL, 0, 0, 0);
	if (!hwnd) {
		DWORD le = GetLastError();
		printf("CreateWindowExA() failed [Error: %x]\r\n", le);
		return 1;
	}

	DEV_BROADCAST_DEVICEINTERFACE_A notifyFilter = {0};
	notifyFilter.dbcc_size = sizeof(notifyFilter);
	notifyFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	notifyFilter.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

	HDEVNOTIFY hDevNotify = RegisterDeviceNotificationA(hwnd, &notifyFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
	if (!hDevNotify) {
		DWORD le = GetLastError();
		printf("RegisterDeviceNotificationA() failed [Error: %x]\r\n", le);
		return 1;
	}

	MSG msg;
	while(TRUE) {
		BOOL bRet = GetMessage(&msg, hwnd, 0, 0);
		if ((bRet == 0) || (bRet == -1)) {
			break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

BOOL GetDeviceDetails(HDEVINFO hDevInfo, SP_DEVINFO_DATA* pspDevInfoData, int index, PSP_DEVICE_INTERFACE_DETAIL_DATA* pspDeviceIntDetail) {
	SP_DEVICE_INTERFACE_DATA spDevIntData;
	spDevIntData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	if (!DllSetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_USB_DEVICE, index, &spDevIntData)) {
		return false;
	}

	DWORD dwSize = 0;
	if (DllSetupDiGetDeviceInterfaceDetail(hDevInfo, &spDevIntData, NULL, 0, &dwSize, NULL)) {
		return false;
	}

	if (ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
		return false;
	}

	*pspDeviceIntDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(dwSize);
	if (NULL == pspDeviceIntDetail) {
		return false;
	}
	(*pspDeviceIntDetail)->cbSize = sizeof(PSP_DEVICE_INTERFACE_DETAIL_DATA);

	if (!DllSetupDiGetDeviceInterfaceDetail(hDevInfo, &spDevIntData, *pspDeviceIntDetail, dwSize, NULL, NULL)) {
		free(pspDeviceIntDetail);
		pspDeviceIntDetail = NULL;
		return false;
	}
	return true;
}


void BuildInitialDeviceList() {
	TCHAR buf[MAX_PATH];
	DWORD dwFlag = (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	HDEVINFO hDevInfo = DllSetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL, dwFlag);

	if(INVALID_HANDLE_VALUE == hDevInfo) {
		return;
	}

	SP_DEVINFO_DATA* pspDevInfoData = (SP_DEVINFO_DATA*) HeapAlloc(GetProcessHeap(), 0, sizeof(SP_DEVINFO_DATA));
	pspDevInfoData->cbSize = sizeof(SP_DEVINFO_DATA);
	for(int i=0; DllSetupDiEnumDeviceInfo(hDevInfo, i, pspDevInfoData); i++) {
		if (!DllSetupDiGetDeviceInstanceId(hDevInfo, pspDevInfoData, buf, sizeof(buf), NULL)) {
			break;
		}

		PSP_DEVICE_INTERFACE_DETAIL_DATA pspDeviceIntDetail = NULL;
		if (!GetDeviceDetails(hDevInfo, pspDevInfoData, i, &pspDeviceIntDetail)) {
			continue;
		}

		// Make sure we can actually talk to this device before adding
		// it to the list of connected devices.
		HANDLE devHandle = CreateFile(pspDeviceIntDetail->DevicePath,
									GENERIC_READ | GENERIC_WRITE,
									FILE_SHARE_READ | FILE_SHARE_WRITE,
									NULL, OPEN_EXISTING,
									FILE_FLAG_OVERLAPPED, NULL);
		if (INVALID_HANDLE_VALUE != devHandle) {
			CloseHandle(devHandle);

			DeviceItem_t* item = new DeviceItem_t();
			item->deviceState = DeviceState_Connect;

			ExtractDeviceInfo(hDevInfo, pspDevInfoData, pspDeviceIntDetail->DevicePath, &item->deviceParams);
			AddItemToList(buf, item);
		}
		free(pspDeviceIntDetail);
	}

	if(pspDevInfoData) {
		HeapFree(GetProcessHeap(), 0, pspDevInfoData);
	}

	if(hDevInfo) {
		DllSetupDiDestroyDeviceInfoList(hDevInfo);
	}
}


void ExtractDeviceInfo(HDEVINFO hDevInfo, SP_DEVINFO_DATA* pspDevInfoData, TCHAR* devicePath, ListResultItem_t* resultItem) {

	TCHAR buf[MAX_PATH];
	DWORD DataT;
	DWORD nSize;
	static int dummy = 1;

	resultItem->locationId = 0;
	resultItem->deviceAddress = dummy++;

	// device found
	if (DllSetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_FRIENDLYNAME, &DataT, (PBYTE)buf, MAX_PATH, &nSize)) {
		resultItem->deviceName = buf;
	}
	else if ( DllSetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_DEVICEDESC, &DataT, (PBYTE)buf, MAX_PATH, &nSize))
	{
		resultItem->deviceName = buf;
	}
	if (DllSetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_MFG, &DataT, (PBYTE)buf, MAX_PATH, &nSize)) {
		resultItem->manufacturer = buf;
	}
	ExtractDetails(devicePath, resultItem);
}

void HandleDeviceConnect(PDEV_BROADCAST_DEVICEINTERFACE pDevInf) {
	HDEVINFO hDevInfo = DllSetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (INVALID_HANDLE_VALUE == hDevInfo) {
		return;
	}

	SP_DEVINFO_DATA* pspDevInfoData = (SP_DEVINFO_DATA*) HeapAlloc(GetProcessHeap(), 0, sizeof(SP_DEVINFO_DATA));
	pspDevInfoData->cbSize = sizeof(SP_DEVINFO_DATA);
	for(int i=0; DllSetupDiEnumDeviceInfo(hDevInfo, i, pspDevInfoData); i++) {
		PSP_DEVICE_INTERFACE_DETAIL_DATA pspDeviceIntDetail = NULL;
		if (!GetDeviceDetails(hDevInfo, pspDevInfoData, i, &pspDeviceIntDetail)) {
			continue;
		}

		if (0 == _strnicmp(pDevInf->dbcc_name, pspDeviceIntDetail->DevicePath, strlen(pDevInf->dbcc_name))) {
			TCHAR buf[MAX_PATH];
			if (!DllSetupDiGetDeviceInstanceId(hDevInfo, pspDevInfoData, buf, sizeof(buf), NULL)) {
				break;
			}

			WaitForSingleObject(deviceChangedSentEvent, INFINITE);

			DeviceItem_t* device = new DeviceItem_t();

			ExtractDeviceInfo(hDevInfo, pspDevInfoData, pDevInf->dbcc_name, &device->deviceParams);
			AddItemToList(buf, device);

			currentDevice = &device->deviceParams;
			isAdded = true;

			break;
		}
	}

	if (pspDevInfoData) {
		HeapFree(GetProcessHeap(), 0, pspDevInfoData);
	}

	if (hDevInfo) {
		DllSetupDiDestroyDeviceInfoList(hDevInfo);
	}

	SetEvent(deviceChangedRegisteredEvent);
}

void HandleDeviceDisconnect(PDEV_BROADCAST_DEVICEINTERFACE pDevInf) {
	// dbcc_name:
	// \\?\USB#Vid_04e8&Pid_503b#0002F9A9828E0F06#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
	// convert to
	// USB\Vid_04e8&Pid_503b\0002F9A9828E0F06
	CString szDevId = pDevInf->dbcc_name + 4;
	szDevId.Truncate(szDevId.ReverseFind(_T('#')));
	szDevId.Replace(_T('#'), _T('\\'));

	HDEVINFO hDevInfo = DllSetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, NULL, NULL, DIGCF_ALLCLASSES);
	if (INVALID_HANDLE_VALUE == hDevInfo) {
		return;
	}

	SP_DEVINFO_DATA* pspDevInfoData = (SP_DEVINFO_DATA*) HeapAlloc(GetProcessHeap(), 0, sizeof(SP_DEVINFO_DATA));
	pspDevInfoData->cbSize = sizeof(SP_DEVINFO_DATA);
	for(int i=0; DllSetupDiEnumDeviceInfo(hDevInfo, i, pspDevInfoData); i++) {
		TCHAR buf[MAX_PATH];
		if (!DllSetupDiGetDeviceInstanceId(hDevInfo, pspDevInfoData, buf, sizeof(buf), NULL)) {
			break;
		}

		if (0 == _strnicmp(szDevId, buf, strlen(szDevId))) {
			ListResultItem_t* item = NULL;
			if (IsItemAlreadyStored(buf)) {
				DeviceItem_t* deviceItem = GetItemFromList(buf);
				if (deviceItem) {
					item = CopyElement(&deviceItem->deviceParams);
				}
				RemoveItemFromList(deviceItem);
				delete deviceItem;
			}

			if (item == NULL) {
				item = new ListResultItem_t();
				ExtractDeviceInfo(hDevInfo, pspDevInfoData, pDevInf->dbcc_name, item);
			}
			currentDevice = item;
			isAdded = false;
		}
	}

	if (pspDevInfoData) {
		HeapFree(GetProcessHeap(), 0, pspDevInfoData);
	}

	if (hDevInfo) {
		DllSetupDiDestroyDeviceInfoList(hDevInfo);
	}

	SetEvent(deviceChangedRegisteredEvent);
}
