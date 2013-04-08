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

using namespace std;


#define VID_TAG "VID_"
#define PID_TAG "PID_"

// Linked libraries
#pragma comment (lib , "setupapi.lib" )


typedef enum  _DeviceState_t
{
    DeviceState_Connect,
    DeviceState_Disconnect,
} DeviceState_t;

typedef struct _DeviceData_t
{
    ListResultItem * item;
    DeviceState_t state;
} DeviceData_t;

typedef struct _DeviceListItem_t
{
    DeviceData_t* device;
    char* identifier;
    _DeviceListItem_t* next;

    _DeviceListItem_t()
    {
        next = NULL;
    }

} DeviceListItem_t;


GUID GUID_DEVINTERFACE_USB_DEVICE = { 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED};
//GUID GUID_DEVINTERFACE_USB_DEVICE = { 0x25dbce51, 0x6c8f, 0x4a72, 0x8a,0x6d,0xb5,0x4c,0x2b,0x4f,0xc8,0x35 };

HWND handle;
DWORD threadId;
HANDLE threadHandle;

HANDLE deviceChangedEvent; 

DeviceData_t* currentDevice;
DeviceListItem_t* deviceItemList = NULL;
DeviceListItem_t* currentListItem = NULL;


void UpdateDevice(PDEV_BROADCAST_DEVICEINTERFACE pDevInf, WPARAM wParam, DeviceState_t state);
DWORD WINAPI ListenerThread( LPVOID lpParam );

void BuildInitialDeviceList();

void NotifyAsync(uv_work_t* req);
void NotifyFinished(uv_work_t* req);

void AddRemoveDevice(DeviceData_t* device , DeviceState_t state, TCHAR* identifier);
DeviceData_t* GetDeviceForIdentifier(TCHAR* identifier);
void ExtractDeviceInfo(HDEVINFO hDevInfo, SP_DEVINFO_DATA* pspDevInfoData, TCHAR* buf, DWORD buffSize, ListResultItem* resultItem);


void extractVidPid(char * buf, ListResultItem * item)
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
}//WinProc


void NotifyAsync(uv_work_t* req)
{
    WaitForSingleObject(deviceChangedEvent, INFINITE);
}


void NotifyFinished(uv_work_t* req)
{

    if(currentDevice->state == DeviceState_Connect)
    {
        NotifyAdded(currentDevice->item);
    }
    else
    {
        NotifyRemoved(currentDevice->item);
        delete currentDevice->item;
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


void EIO_Find(uv_work_t* req)
{

    ListBaton* data = static_cast<ListBaton*>(req->data);

    DeviceListItem_t* current = deviceItemList;
    while(current != NULL)
    {
        if(current->device != NULL)
        {
            ListResultItem * item = current->device->item;

            if (((data->vid != 0 && data->pid != 0) && (data->vid == item->vendorId && data->pid == item->productId))
                || ((data->vid != 0 && data->pid == 0) && data->vid == item->vendorId)
                || (data->vid == 0 && data->pid == 0))
            {
                data->results.push_back(item);
            }
        }
        current = current->next;
    }
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

        DeviceData_t* device;
        device = new DeviceData_t();
        device->state = DeviceState_Connect;
        device->item = new ListResultItem();

        DWORD DataT;
        SetupDiGetDeviceRegistryProperty(hDevInfo, pspDevInfoData, SPDRP_HARDWAREID, &DataT, (PBYTE)buf, MAX_PATH, &nSize);

        AddRemoveDevice(device, DeviceState_Connect, buf);
        ExtractDeviceInfo(hDevInfo, pspDevInfoData, buf, MAX_PATH, device->item);
    }

    if ( pspDevInfoData ) HeapFree(GetProcessHeap(), 0, pspDevInfoData);
    SetupDiDestroyDeviceInfoList(hDevInfo);

}


void ExtractDeviceInfo(HDEVINFO hDevInfo, SP_DEVINFO_DATA* pspDevInfoData, TCHAR* buf, DWORD buffSize, ListResultItem* resultItem)
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


void AddRemoveDevice(DeviceData_t* device , DeviceState_t state, TCHAR* identifier)
{
    if(state == DeviceState_Connect)
    {
        DeviceListItem_t* last;

        if(deviceItemList == NULL)
        {
            deviceItemList = new DeviceListItem_t();
            last = deviceItemList;
        }
        else
        {
            last = deviceItemList;

            while(last->next != NULL)
            {
                last = last->next;
            }
            
            last->next = new DeviceListItem_t();
            last = last->next;
        }
        last->device = device;

        last->identifier = new char[strlen(identifier) + 1];
        memcpy(last->identifier, identifier, strlen(identifier) + 1);

        printf("Add device [%s]\r\n", last->identifier);
    }
    else
    {
        DeviceListItem_t* current = deviceItemList;
        DeviceListItem_t* last = deviceItemList;

        while(current != NULL)
        {
            if(current->device == device)
            {
                printf("Found for Remove: %s\r\n", current->identifier);
                if(current == deviceItemList)
                {
                    deviceItemList = deviceItemList->next;
                }
                else
                {
                    last->next = current->next;
                }

                delete current->identifier;
                delete current;

                return;
            }

            last = current;
            current = current->next;
        }

        printf("device [%s] not found!\r\n", identifier);
    }
}


DeviceData_t* GetDeviceForIdentifier(TCHAR* identifier)
{
    DeviceListItem_t* current = deviceItemList;

    while(current != NULL)
    {
        if(strcmp(current->identifier, identifier) == 0)
        {
            return current->device;
        }

        current = current->next;
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
                currentDevice = new DeviceData_t();
                currentDevice->item = new ListResultItem();
                ExtractDeviceInfo(hDevInfo, pspDevInfoData, buf, MAX_PATH, currentDevice->item);
            }
            else
            {
                currentDevice = GetDeviceForIdentifier(buf);
            }
            currentDevice->state = state;

            AddRemoveDevice(currentDevice, state, buf);
            break;
        }
    }

    if ( pspDevInfoData ) HeapFree(GetProcessHeap(), 0, pspDevInfoData);
    SetupDiDestroyDeviceInfoList(hDevInfo);

    SetEvent(deviceChangedEvent);
}