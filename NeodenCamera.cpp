#include "stdafx.h"
#include <windows.h>


// windows.h includes WINGDI.h which
// defines GetObject as GetObjectA, breaking
// System::Resources::ResourceManager::GetObject.
// So, we undef here.
#undef GetObject

#include "NeodenCamera.h"



using namespace std;

CCyUSBDevice* USBNeodenCamera;

HANDLE hDevice;

#include <time.h>
void wait(unsigned timeout)
{
    timeout += clock();
    while (clock() < timeout) continue;
}

string narrow(const wstring& wstr)
{
    string str(wstr.length(), ' ');
    copy(wstr.begin(), wstr.end(), str.begin());
    return str;
}







BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        // Create the CyUSBDevice

        USBNeodenCamera = new CCyUSBDevice(0, CYUSBDRV_GUID, true);

        int n = USBNeodenCamera->DeviceCount();



        //_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);

        // for all Cypress devices found

        for (int i = 0; i < n; i++) {

            USBNeodenCamera->Open(i);
        }

        hDevice = USBNeodenCamera->DeviceHandle();
    }
    break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


int APIENTRY _tWinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR    lpCmdLine,
    int       nCmdShow)
{


    // Create the CyUSBDevice

    USBNeodenCamera = new CCyUSBDevice(0, CYUSBDRV_GUID, true);


    int n = USBNeodenCamera->DeviceCount();


    //_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);

    // for all Cypress devices found

    for (int i = 0; i < 1; i++) {

        USBNeodenCamera->Open(i);

        // Is it the Neoden?
        if (USBNeodenCamera->VendorID != 0x0828) {
            continue;
        }

        std::string model = narrow(std::wstring(USBNeodenCamera->Product));
        std::string serial = narrow(std::wstring(USBNeodenCamera->SerialNumber));

        _RPT5(_CRT_WARN, "DID % 04x: % 04x, % s, % s, % s, % s\n", USBNeodenCamera->VendorID, USBNeodenCamera->ProductID, USBNeodenCamera->DeviceName, USBNeodenCamera->DevPath, model.c_str(), serial.c_str());

        //unsigned char* buf = new unsigned char[1024 * 512]();
        hDevice = USBNeodenCamera->DeviceHandle();
        img_init();
        //img_capture(0, buf);
        //delete[] buf;

    }


    while (1)
    {
        unsigned char* buf = new unsigned char[1024 * 512]();
        //img_capture(0, buf);
        delete[] buf;
        wait(2000);
    }

    delete USBNeodenCamera;


    return 0;
}

BOOL _cdecl img_led(int which_camera, int16_t mode)
{

    DWORD dwBytes = 0;
    UCHAR Address = 0x82;


    _RPT0(_CRT_WARN, "IMG LED\n");
    BOOL RetVal = (DeviceIoControl(hDevice, IOCTL_ADAPT_ABORT_PIPE, &Address, sizeof(UCHAR), NULL, 0, &dwBytes, NULL) != 0);

    return RetVal;
}

int _cdecl img_init()
{
    DWORD dwBytes = 0;
    UCHAR Address = 0x82;

    unsigned char* buf = new unsigned char[5]();

    _RPT0(_CRT_WARN, "IMG INIT\n");
    //USBNeodenCamera->CCyUSBDevice::Reset();
    //DeviceIoControl(hDevice, IOCTL_ADAPT_SET_TRANSFER_SIZE, buf, 5, buf, 5, &dwBytes, NULL);

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->SetXferSize((ULONG)524288);

    ept->Target = TGT_DEVICE;
    ept->ReqType = REQ_VENDOR;
    ept->ReqCode = IOCTL_ADAPT_SEND_EP0_CONTROL_TRANSFER;
    ept->Value = 1;
    ept->Index = 0;

    unsigned char  buf2[54];
    ZeroMemory(buf2, 54);
    LONG bytesToSend = 16;

    ept->Write(buf2, bytesToSend);

    bytesToSend = 10;
    ept->Write(buf2, bytesToSend);
    return USBNeodenCamera->DeviceCount();

}

int _cdecl img_capture(int which_camera, unsigned char* pFrameBuffer)
{
    _RPT0(_CRT_WARN, "IMG CAPTURE\n");

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->Target = TGT_DEVICE;
    ept->ReqType = REQ_VENDOR;
    ept->ReqCode = IOCTL_ADAPT_SEND_NON_EP0_TRANSFER;
    ept->Value = 1;
    ept->Index = 0;

    //    unsigned char  buf[512];
    //    ZeroMemory(buf, 512);
    LONG bytesToSend = 1024 * 512;

    BOOL retVal = ept->Write(pFrameBuffer, bytesToSend);

    _RPT1(_CRT_WARN, "capture: %d\n", retVal);

    return retVal;
}

BOOL _cdecl img_set(int which_camera, int speed, int16_t exposure, int16_t gain)
{
    _RPT0(_CRT_WARN, "IMG SET\n");
    return 0;
}


BOOL _cdecl img_setRoi(int which_camera, short a2, short a3, short w, short h)
{
    _RPT0(_CRT_WARN, "IMG SETROI\n");
    return 0;
}
