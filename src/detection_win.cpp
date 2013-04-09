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


/**********************************
 * Local typedefs
 **********************************/



/**********************************
 * Local Variables
 **********************************/
GUID GUID_DEVINTERFACE_USB_DEVICE = { 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED};

HWND handle;
DWORD threadId;
HANDLE threadHandle;

HANDLE deviceChangedEvent; 

ListResultItem_t* currentDevice;
bool isAdded;

/**********************************
 * Local Helper Functions protoypes
 **********************************/
void UpdateDevice(PDEV_BROADCAST_DEVICEINTERFACE pDevInf, WPARAM wParam, DeviceState_t state);
DWORD WINAPI ListenerThread( LPVOID lpParam );

void BuildInitialDeviceList();

void NotifyAsync(uv_work_t* req);
void NotifyFinished(uv_work_t* req);

void ExtractDeviceInfo(HDEVINFO hDevInfo, SP_DEVINFO_DATA* pspDevInfoData, TCHAR* buf, DWORD buffSize, ListResultItem_t* resultItem);



/**********************************
 * Public Functions
 **********************************/
void NotifyAsync(uv_work_t* req)
{
    WaitForSingleObject(deviceChangedEvent, INFINITE);
}


void NotifyFinished(uv_work_t* req)
{

    if(isAdded)
    {
        NotifyAdded(currentDevice);
    }
    else
    {
        NotifyRemoved(currentDevice);
        delete currentDevice;
    }

    uv_queue_work(uv_default_loop(), req, NotifyAsync, (uv_after_work_cb)NotifyFinished);
}


void InitDetection()
{

    threadHandle = CreateThread( 
        NULL,                   // default security attributes
        0,                      // use default stack size  
        ListenerThread,         // thread function name
        NULL,                   // argument to thread function 
        0,                      // use default creation flags 
        &threadId);   

    deviceChangedEvent = CreateEvent(NULL, false /* auto-reset event */, false /* non-signalled state */, "");

    BuildInitialDeviceList();

    uv_work_t* req = new uv_work_t();
    uv_queue_work(uv_default_loop(), req, NotifyAsync, (uv_after_work_cb)NotifyFinished);
}


void EIO_Find(uv_work_t* req)
{

    ListBaton* data = static_cast<ListBaton*>(req->data);

    CreateFilteredList(&data->results, data->vid, data->pid);
}


/**********************************
 * Local Functions
 **********************************/
void extractVidPid(char * buf, ListResultItem_t * item)
{
    if(buf == NULL)
    {
        return;
    }

    char* string;
    char* temp;
    char* pidStr, *vidStr;
    int vid = 0;
    int pid = 0;

    string = new char[strlen(buf) + 1];
    memcpy(string, buf, strlen(buf) + 1);

    vidStr = strstr(string, VID_TAG);
    pidStr = strstr(string, PID_TAG);
    
    if(vidStr != NULL)
    {
        temp = (char*) (vidStr + strlen(VID_TAG));
        temp[4] = '\0';
        vid = strtol (temp, NULL, 16);
    }

    if(pidStr != NULL)
    {
        temp = (char*) (pidStr + strlen(PID_TAG));
        temp[4] = '\0';
        pid = strtol (temp, NULL, 16);
    }
    item->vendorId = vid;
    item->productId = pid;

    delete string;
}


LRESULT CALLBACK DetectCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DEVICECHANGE)
    {
        if ( DBT_DEVICEARRIVAL == wParam || DBT_DEVICEREMOVECOMPLETE == wParam ) 
        {
            PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;  
            PDEV_BROADCAST_DEVICEINTERFACE pDevInf;  
            PDEV_BROADCAST_PORT pDevPort; 

            if(pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
            {
                pDevInf = (PDEV_BROADCAST_DEVICEINTERFACE)pHdr; 
                if(DBT_DEVICEARRIVAL == wParam)
                {
                    UpdateDevice(pDevInf, wParam, DeviceState_Connect);
                }
                else
                {
                    UpdateDevice(pDevInf, wParam, DeviceState_Disconnect);
                }
            }
        }
    }

    return 1;
}


DWORD WINAPI ListenerThread( LPVOID lpParam ) 
{ 
    const char *className = "ListnerThread";

    WNDCLASSA wincl = {0};
    wincl.hInstance = GetModuleHandle(0);
    wincl.lpszClassName = className;
    wincl.lpfnWndProc = DetectCallback;

    if (!RegisterClassA(&wincl))
    {
        DWORD le = GetLastError();
        printf("RegisterClassA() failed [Error: %x]\r\n", le);
        return 1;
    }//if

    
    HWND hwnd = CreateWindowExA(WS_EX_TOPMOST, className, className, 0, 0, 0, 0, 0, NULL, 0, 0, 0);
    if (!hwnd)
    {
        DWORD le = GetLastError();
        printf("CreateWindowExA() failed [Error: %x]\r\n", le);
        return 1;
    }//if

    DEV_BROADCAST_DEVICEINTERFACE_A notifyFilter = {0};
    notifyFilter.dbcc_size = sizeof(notifyFilter);
    notifyFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    notifyFilter.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

    HDEVNOTIFY hDevNotify = RegisterDeviceNotificationA(hwnd, &notifyFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!hDevNotify)
    {
        DWORD le = GetLastError();
        printf("RegisterDeviceNotificationA() failed [Error: %x]\r\n", le);
        return 1;
    }//if

    MSG msg;
    for (;;) // ctrl-c to exit ;)
    {
        BOOL bRet = GetMessage(&msg, hwnd, 0, 0);
        if ((bRet == 0) || (bRet == -1))
        {
            break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }//while

    return 0;
} 


void BuildInitialDeviceList()
{
    DWORD dwFlag = (DIGCF_ALLCLASSES | DIGCF_PRESENT);
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, "USB", NULL, dwFlag);

    if( INVALID_HANDLE_VALUE == hDevInfo )
    {
        return;
    }

    SP_DEVINFO_DATA* pspDevInfoData = (SP_DEVINFO_DATA*) HeapAlloc(GetProcessHeap(), 0, sizeof(SP_DEVINFO_DATA));
    pspDevInfoData->cbSize = sizeof(SP_DEVINFO_DATA);
    for(int i=0; SetupDiEnumDeviceInfo(hDevInfo, i, pspDevInfoData); i++)
    {
        DWORD nSize=0 ;
        TCHAR buf[MAX_PATH];

        if ( !SetupDiGetDeviceInstanceId(hDevInfo, pspDevInfoData, buf, sizeof(buf), &nSize) )
        {
            break;
        }

        DeviceItem_t* item = new DeviceItem_t();
        item->deviceState = DeviceState_Connect;

        DWORD DataT;
        SetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_HARDWAREID, &DataT, (PBYTE)buf, MAX_PATH, &nSize);

        AddItemToList(buf, item);
        ExtractDeviceInfo(hDevInfo, pspDevInfoData, buf, MAX_PATH, &item->deviceParams);
    }

    if ( pspDevInfoData ) HeapFree(GetProcessHeap(), 0, pspDevInfoData);
    SetupDiDestroyDeviceInfoList(hDevInfo);

}


void ExtractDeviceInfo(HDEVINFO hDevInfo, SP_DEVINFO_DATA* pspDevInfoData, TCHAR* buf, DWORD buffSize, ListResultItem_t* resultItem)
{
    DWORD DataT;
    DWORD nSize;
    static int dummy = 1;

    resultItem->locationId = 0;
    resultItem->deviceAddress = dummy++;

    // device found
    if ( SetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_FRIENDLYNAME, &DataT, (PBYTE)buf, buffSize, &nSize) ) 
    {
        resultItem->deviceName = buf;
    } 
    else if ( SetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_DEVICEDESC, &DataT, (PBYTE)buf, buffSize, &nSize) ) 
    {
        resultItem->deviceName = buf;
    } 
    if ( SetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_MFG, &DataT, (PBYTE)buf, buffSize, &nSize) ) 
    {
        resultItem->manufacturer = buf;
    }  
    if ( SetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_HARDWAREID, &DataT, (PBYTE)buf, buffSize, &nSize) ) 
    {
        // Use this to extract VID / PID
        extractVidPid(buf, resultItem);
    } 
}

 
void UpdateDevice(PDEV_BROADCAST_DEVICEINTERFACE pDevInf, WPARAM wParam, DeviceState_t state)
{
    // dbcc_name:
    // \\?\USB#Vid_04e8&Pid_503b#0002F9A9828E0F06#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
    // convert to
    // USB\Vid_04e8&Pid_503b\0002F9A9828E0F06
    CString szDevId = pDevInf->dbcc_name+4;
    int idx = szDevId.ReverseFind(_T('#'));

    szDevId.Truncate(idx);
    szDevId.Replace(_T('#'), _T('\\'));
    szDevId.MakeUpper();

    CString szClass;
    idx = szDevId.Find(_T('\\'));
    szClass = szDevId.Left(idx);

    // if we are adding device, we only need present devices
    // otherwise, we need all devices
    DWORD dwFlag = DBT_DEVICEARRIVAL != wParam ? DIGCF_ALLCLASSES : (DIGCF_ALLCLASSES | DIGCF_PRESENT);
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, szClass, NULL, dwFlag);
    if( INVALID_HANDLE_VALUE == hDevInfo )
    {
        return;
    }

    SP_DEVINFO_DATA* pspDevInfoData = (SP_DEVINFO_DATA*) HeapAlloc(GetProcessHeap(), 0, sizeof(SP_DEVINFO_DATA));
    pspDevInfoData->cbSize = sizeof(SP_DEVINFO_DATA);
    for(int i=0; SetupDiEnumDeviceInfo(hDevInfo, i, pspDevInfoData); i++)
    {
        DWORD nSize=0 ;
        TCHAR buf[MAX_PATH];

        if ( !SetupDiGetDeviceInstanceId(hDevInfo, pspDevInfoData, buf, sizeof(buf), &nSize) )
        {
            break;
        }

        if ( szDevId == buf )
        {
            DWORD DataT;
            DWORD nSize;
            SetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_HARDWAREID, &DataT, (PBYTE)buf, MAX_PATH, &nSize);

            if(state == DeviceState_Connect)
            {
                DeviceItem_t* device = new DeviceItem_t();

                AddItemToList(buf, device);
                ExtractDeviceInfo(hDevInfo, pspDevInfoData, buf, MAX_PATH, &device->deviceParams);

                currentDevice = &device->deviceParams;
                isAdded = true;
            }
            else
            {
                ListResultItem_t* item = NULL;
                if(IsItemAlreadyStored(buf))
                {
                    DeviceItem_t* deviceItem = GetItemFromList(buf);
                    if(deviceItem)
                    {
                        item = CopyElement(&deviceItem->deviceParams);
                    }
                    RemoveItemFromList(deviceItem);
                    delete deviceItem;
                }

                if(item == NULL)
                {
                    item = new ListResultItem_t();
                    ExtractDeviceInfo(hDevInfo, pspDevInfoData, buf, MAX_PATH, item);
                }
                currentDevice = item;
                isAdded = false;
            }

            break;
        }
    }

    if ( pspDevInfoData ) HeapFree(GetProcessHeap(), 0, pspDevInfoData);
    SetupDiDestroyDeviceInfoList(hDevInfo);

    SetEvent(deviceChangedEvent);
}