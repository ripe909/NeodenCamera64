#include "stdafx.h"
#include <windows.h>


// windows.h includes WINGDI.h which
// defines GetObject as GetObjectA, breaking
// System::Resources::ResourceManager::GetObject.
// So, we undef here.
#undef GetObject

#include "NeodenCamera.h"
#include <time.h>


using namespace std;

CCyUSBDevice* USBDevice;
HANDLE hDevice[2];
CCyControlEndPoint* epControl[2];
CCyBulkEndPoint* epBulkIn[2];

int					QueueSize = 16;
const int			MAX_QUEUE_SZ = 64;
static int			PPX = 8192;
static int			TimeOut = 1500;


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
        USBDevice = new CCyUSBDevice(0, CYUSBDRV_GUID, true);
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
    USBDevice = new CCyUSBDevice(0, CYUSBDRV_GUID, true);

    if (img_init() == 2)
    {
        for (int i = 0; i < 2; i++)
        {
           
            //img_set_speed(i, 2);
            //img_set_exp(i, 25);
            //img_set_gain(i, 8);
            //img_get_exp(i);
            //img_get_gain(i);
            //img_setRoi(i, 212, 240, 800, 600);
            //img_capture(0, buf);
            //for (size_t i = 0; i < (1024 * 512); i++) {
            //_RPT1(_CRT_WARN, "0x%02x ", buf[i]);
            // }
            //delete[] buf;
        }
    }

    delete USBDevice;
    return 0;
}

BOOL _cdecl img_led(int which_camera, int16_t mode)
{
    DWORD dwBytes = 0;
    UCHAR Address = 0x82;

    _RPT1(_CRT_WARN, "IMG LED: %d\n", which_camera);
    BOOL RetVal = (DeviceIoControl(hDevice, IOCTL_ADAPT_ABORT_PIPE, &Address, sizeof(UCHAR), NULL, 0, &dwBytes, NULL) != 0);

    return RetVal;
}

int _cdecl img_init()
{
    _RPT0(_CRT_WARN, "IMG INIT\n");

    BOOL bXferCompleted = false;
    int n = USBDevice->DeviceCount();
    long len = 0; // Each xfer request will get PPX isoc packets

    unsigned char  buf2[512];
    LONG bytesToSend = 0;
    LONG rLen = 0;

    for (int i = 0; i < n; i++)
    {
        if (USBDevice->IsOpen() == true) USBDevice->Close();
        USBDevice->Open(i);
        // Is it the Neoden?
        if (USBDevice->VendorID != 0x0828)
        {
            continue;
        }

        string model = narrow(wstring(USBDevice->Product));
        string serial = narrow(wstring(USBDevice->SerialNumber));

        _RPT5(_CRT_WARN, "DID % 04x: % 04x, % s, % s, % s, % s\n", USBDevice->VendorID, USBDevice->ProductID, USBDevice->DeviceName, USBDevice->DevPath, model.c_str(), serial.c_str());
        hDevice[i] = USBDevice->DeviceHandle();
        epControl[i] = USBDevice->ControlEndPt;
        _RPT1(_CRT_WARN, "epcontrol init: 0x%x\n", epControl[i]);
        epBulkIn[i] = USBDevice->BulkInEndPt;

        len = epControl[i]->MaxPktSize * PPX;
        epControl[i]->SetXferSize(len);

        // *************************************************
        epControl[i]->Target = TGT_DEVICE; // byte 0
        epControl[i]->ReqType = REQ_VENDOR; // byte 0
        epControl[i]->ReqCode = 0xbc; // byte 1
        epControl[i]->Value = 0x0000; // byte 2,3
        epControl[i]->Index = 0x0000; // byte 4,5
        epControl[i]->Direction = DIR_TO_DEVICE; // 0x40
        epControl[i]->TimeOut = 2000;

        bytesToSend = 16;  // 38 + 16 = 54

        buf2[0] = 0x28;
        buf2[1] = 0x74;
        buf2[2] = 0x00;
        buf2[3] = 0x00;

        buf2[4] = 0x38;
        buf2[5] = 0x18;
        buf2[6] = 0x00;
        buf2[7] = 0x00;

        buf2[8] = 0xb0;
        buf2[9] = 0xd3;
        buf2[10] = 0xb9;
        buf2[11] = 0xdb;

        buf2[12] = 0x64;
        buf2[13] = 0x24;
        buf2[14] = 0xcf;
        buf2[15] = 0x77;

        rLen = bytesToSend;
        bXferCompleted = epControl[i]->XferData(buf2, rLen);

        // *************************************************
        epControl[i]->ReqCode = 0xb6; // byte 1
        epControl[i]->Value = 0x0000; // byte 2,3
        epControl[i]->Index = 0x0000; // byte 4,5
        epControl[i]->Direction = DIR_FROM_DEVICE; // 0xc0

        bytesToSend = 10;  // 38 + 10 = 48

        rLen = bytesToSend;
        bXferCompleted = epControl[i]->XferData(buf2, rLen);

        //if (bept != NULL)
        //{
        //    rLen = bytesToSend;
        //    bXferCompleted = bept->XferData(buf2, rLen);
        //}

        ////_RPT0(_CRT_WARN, "IMG INIT:4\n");
        //for (size_t i = 0; i < bytesToSend; i++) {
        //    _RPT2(_CRT_WARN, "i:%d:0x%02x\n", i, buf2[i]);
        //}

        // *************************************************
        epControl[i]->ReqCode = 0xbc; // byte 1
        epControl[i]->Value = 0x0000; // byte 2,3
        epControl[i]->Index = 0x0000; // byte 4,5
        epControl[i]->Direction = DIR_FROM_DEVICE; // 0xc0

        bytesToSend = 16;  // 38 + 16 = 54

        rLen = bytesToSend;
        bXferCompleted = epControl[i]->XferData(buf2, rLen);

        //if (bept != NULL)
        //{
        //    rLen = bytesToSend;
        //    bXferCompleted = bept->XferData(buf2, rLen);
        //}

        //for (size_t i = 0; i < bytesToSend; i++) {
        //    _RPT2(_CRT_WARN, "i:%d:0x%02x\n", i, buf2[i]);
        //}

        // *************************************************
        epControl[i]->ReqCode = 0xb9; // byte 1
        epControl[i]->Value = 0x0000; // byte 2,3
        epControl[i]->Index = 0x0000; // byte 4,5
        epControl[i]->Direction = DIR_TO_DEVICE; // 0x40

        bytesToSend = 0;  // 38 + 0 = 0

        rLen = bytesToSend;
        bXferCompleted = epControl[i]->XferData(buf2, rLen);
    }

    return n;
}


int _cdecl img_capture(int which_camera, unsigned char* pFrameBuffer)
{
    _RPT1(_CRT_WARN, "IMG CAPTURE: %d\n", which_camera);

    BOOL bXferCompleted = false;
    unsigned char  buf2[1];
    LONG bytesToSend = 0;
    LONG rLen = 0;
    int retVal = 0;

    epControl[which_camera]->Target = TGT_DEVICE;
    epControl[which_camera]->ReqType = REQ_VENDOR;
    epControl[which_camera]->ReqCode = 0xb8;
    epControl[which_camera]->Value = 0x0000;
    epControl[which_camera]->Index = 0x0000;
    epControl[which_camera]->Direction = DIR_TO_DEVICE; // 0xc0

    // ************************************************
    bytesToSend = 0;
    rLen = bytesToSend;
    bXferCompleted = epControl[which_camera]->XferData(buf2, rLen);

    // ************************************************
    bytesToSend = 1024 * 1024 * 3;
    rLen = bytesToSend;
    bXferCompleted = epBulkIn[which_camera]->XferData(pFrameBuffer, rLen);
    if (bXferCompleted)
    {
        retVal = 1;
    }

    return retVal;
}

BOOL _cdecl img_set(int which_camera, int speed, int16_t exposure, int16_t gain)
{
    _RPT1(_CRT_WARN, "IMG SET: %d\n", which_camera);

    BOOL retVal = true;

    if (img_set_speed(which_camera, speed))
    {
        if (img_set_gain(which_camera, gain))
        {
            if (!img_set_exp(which_camera, exposure))
            {
                retVal = false;
            }
        }
        else
        {
            retVal = false;
        }
    }
    else
    {
        retVal = false;
    }

    return retVal;
}

BOOL _cdecl img_set_speed(int which_camera, int16_t speed)
{
    _RPT1(_CRT_WARN, "IMG SET SPEED: %d\n", which_camera);

    epControl[which_camera]->Target = TGT_DEVICE; // byte 0
    epControl[which_camera]->ReqType = REQ_VENDOR; // byte 0
    epControl[which_camera]->ReqCode = 0xb0; // byte 1
    epControl[which_camera]->Value = 0x0000; // byte 2,3
    epControl[which_camera]->Index = speed; // byte 4,5

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38
    return epControl[which_camera]->Write(buf2, bytesToSend);
}

BOOL _cdecl img_set_exp(int which_camera, int16_t exposure)
{
    _RPT1(_CRT_WARN, "IMG SET EXP: %d\n", which_camera);

    epControl[which_camera]->Target = TGT_DEVICE; // byte 0
    epControl[which_camera]->ReqType = REQ_VENDOR; // byte 0
    epControl[which_camera]->ReqCode = 0xb7; // byte 1
    epControl[which_camera]->Value = exposure; // byte 2,3
    epControl[which_camera]->Index = 0x0009; // byte 4,5

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38
    return epControl[which_camera]->Write(buf2, bytesToSend);
}

BOOL _cdecl img_set_gain(int which_camera, int16_t gain)
{
    _RPT1(_CRT_WARN, "IMG SET GAIN: %d\n", which_camera);

    epControl[which_camera]->Target = TGT_DEVICE; // byte 0
    epControl[which_camera]->ReqType = REQ_VENDOR; // byte 0
    epControl[which_camera]->ReqCode = 0xb7; // byte 1
    epControl[which_camera]->Value = gain; // byte 2,3
    epControl[which_camera]->Index = 0x0035; // byte 4,5

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38
    return epControl[which_camera]->Write(buf2, bytesToSend);
}

BOOL _cdecl img_get_exp(int which_camera)
{
    _RPT1(_CRT_WARN, "IMG GET EXP: %d\n", which_camera);

    epControl[which_camera]->Target = TGT_DEVICE; // byte 0
    epControl[which_camera]->ReqType = REQ_VENDOR; // byte 0
    epControl[which_camera]->ReqCode = 0xb6; // byte 1
    epControl[which_camera]->Value = 0x0000; // byte 2,3
    epControl[which_camera]->Index = 0x0009; // byte 4,5

    unsigned char  buf2[64];

    LONG bytesToSend = 64;  // 38 + 64 = 102
    return epControl[which_camera]->Read(buf2, bytesToSend);
}

BOOL _cdecl img_get_gain(int which_camera)
{
    _RPT1(_CRT_WARN, "IMG GET GAIN: %d\n", which_camera);

    epControl[which_camera]->Target = TGT_DEVICE; // byte 0
    epControl[which_camera]->ReqType = REQ_VENDOR; // byte 0
    epControl[which_camera]->ReqCode = 0xb6; // byte 1
    epControl[which_camera]->Value = 0x0000; // byte 2,3
    epControl[which_camera]->Index = 0x0035; // byte 4,5

    unsigned char  buf2[64];

    LONG bytesToSend = 64;  // 38 + 64 = 102
    return epControl[which_camera]->Read(buf2, bytesToSend);
}


BOOL _cdecl img_setRoi(int which_camera, short a2, short a3, short w, short h)
{
    _RPT1(_CRT_WARN, "IMG SET ROI: %d\n", which_camera);

    BOOL retVal = false;

    if (img_set_lt(which_camera, a2, a3))
    {
        retVal = img_set_wh(which_camera, w, h);
    }

    return retVal;
}

BOOL _cdecl img_set_lt(int which_camera, int16_t a2, int16_t a3)
{
    _RPT1(_CRT_WARN, "IMG SET LT: %d\n", which_camera);

    epControl[which_camera]->Target = TGT_DEVICE; // byte 0
    epControl[which_camera]->ReqType = REQ_VENDOR; // byte 0
    epControl[which_camera]->ReqCode = 0xba; // byte 1

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38

    // ************************************************
    epControl[which_camera]->Value = ((a2 >> 1) << 1) + 12; // byte 2,3
    epControl[which_camera]->Index = 0x0001; // byte 4,5
    epControl[which_camera]->Write(buf2, bytesToSend);

    // ************************************************
    epControl[which_camera]->Value = ((a3 >> 1) << 1) + 20; // byte 2,3
    epControl[which_camera]->Index = 0x0002; // byte 4,5
    return epControl[which_camera]->Write(buf2, bytesToSend);
}

BOOL _cdecl img_set_wh(int which_camera, int16_t w, int16_t h)
{
    _RPT1(_CRT_WARN, "IMG SET WH: %d\n", which_camera);

    epControl[which_camera]->Target = TGT_DEVICE; // byte 0
    epControl[which_camera]->ReqType = REQ_VENDOR; // byte 0
    epControl[which_camera]->ReqCode = 0xba; // byte 1

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38

    // ************************************************
    epControl[which_camera]->Value = h - 1; // byte 2,3
    epControl[which_camera]->Index = 0x0003; // byte 4,5
    epControl[which_camera]->Write(buf2, bytesToSend);

    // ************************************************
    epControl[which_camera]->Value = w - 1; // byte 2,3
    epControl[which_camera]->Index = 0x0004; // byte 4,5
    return epControl[which_camera]->Write(buf2, bytesToSend);
}