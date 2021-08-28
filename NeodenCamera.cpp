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

CCyUSBDevice* USBNeodenCamera;
HANDLE hDevice;
CCyUSBEndPoint* epControl;
CCyUSBEndPoint* epBulkIn;

int					QueueSize = 16;
const int				MAX_QUEUE_SZ = 64;
static int					PPX = 1024;
static int					TimeOut = 1500;


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

        for (int i = 0; i < n; i++) 
        {
            USBNeodenCamera->Open(i);
        }

        hDevice = USBNeodenCamera->DeviceHandle();
        epControl = USBNeodenCamera->EndPointOf((ULONG)0x00);
        epBulkIn = USBNeodenCamera->EndPointOf((ULONG)0x82);
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

    for (int i = 0; i < n; i++) 
    {
        USBNeodenCamera->Open(i);

        // Is it the Neoden?
        if (USBNeodenCamera->VendorID != 0x0828) 
        {
            continue;
        }

        string model = narrow(wstring(USBNeodenCamera->Product));
        string serial = narrow(wstring(USBNeodenCamera->SerialNumber));

        _RPT5(_CRT_WARN, "DID % 04x: % 04x, % s, % s, % s, % s\n", USBNeodenCamera->VendorID, USBNeodenCamera->ProductID, USBNeodenCamera->DeviceName, USBNeodenCamera->DevPath, model.c_str(), serial.c_str());

        unsigned char* buf = new unsigned char[1024 * 512]();
        hDevice = USBNeodenCamera->DeviceHandle();
        epControl = USBNeodenCamera->EndPointOf((ULONG)0x00);
        epBulkIn = USBNeodenCamera->EndPointOf((ULONG)0x82);

        img_init();
        img_set_speed(1, 2);
        img_set_exp(1, 25);
        img_set_gain(1, 8);
        img_get_exp(1);
        img_get_gain(1);
        img_setRoi(1, 212, 240, 800, 600);
        img_capture(0, buf);
        for (size_t i = 0; i < (1024 * 512); i++) {
        _RPT1(_CRT_WARN, "0x%02x ", buf[i]);
         }
        delete[] buf;
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

static void AbortXferLoop(int pending, PUCHAR* buffers, CCyIsoPktInfo** isoPktInfos, PUCHAR* contexts, OVERLAPPED inOvLap[])
{
    //EndPt->Abort(); - This is disabled to make sure that while application is doing IO and user unplug the device, this function hang the app.
    long len = epControl->MaxPktSize * PPX;
    epControl->Abort();

    for (int j = 0; j < QueueSize; j++)
    {
        if (j < pending)
        {
            epControl->WaitForXfer(&inOvLap[j], TimeOut);
            /*{
                EndPt->Abort();
                if (EndPt->LastError == ERROR_IO_PENDING)
                    WaitForSingleObject(inOvLap[j].hEvent,2000);
            }*/
            epControl->FinishDataXfer(buffers[j], len, &inOvLap[j], contexts[j]);
        }

        CloseHandle(inOvLap[j].hEvent);

        delete[] buffers[j];
        delete[] isoPktInfos[j];
    }

    delete[] buffers;
    delete[] isoPktInfos;
    delete[] contexts;
}



// This method executes in it's own thread.  The thread gets re-launched each time the
        // Start button is clicked (and the thread isn't already running).
static void XferInLoop()
{

    int i = 0;

    // Allocate the arrays needed for queueing

    PUCHAR* buffers = new PUCHAR[QueueSize];
    CCyIsoPktInfo** isoPktInfos = new CCyIsoPktInfo * [QueueSize];
    PUCHAR* contexts = new PUCHAR[QueueSize];
    OVERLAPPED		inOvLap[MAX_QUEUE_SZ];

    long len = epControl->MaxPktSize * PPX; // Each xfer request will get PPX isoc packets

    epControl->SetXferSize(len);

    // Allocate all the buffers for the queues
    for (i = 0; i < QueueSize; i++)
    {
        buffers[i] = new UCHAR[len];
        isoPktInfos[i] = new CCyIsoPktInfo[PPX];
        inOvLap[i].hEvent = CreateEvent(NULL, false, false, NULL);

        memset(buffers[i], 0xEF, len);
    }

    //DateTime t1 = DateTime::Now;	// For calculating xfer rate

    // Queue-up the first batch of transfer requests
    for (i = 0; i < QueueSize; i++)
    {
        contexts[i] = epControl->BeginDataXfer(buffers[i], len, &inOvLap[i]);
        if (epControl->NtStatus || epControl->UsbdStatus) // BeginDataXfer failed
        {
            //Display(String::Concat("Xfer request rejected. NTSTATUS = ", EndPt->NtStatus.ToString("x")));
            _RPT1(_CRT_WARN, "Xfer request rejected. NTSTATUS = %s\n", epControl->NtStatus.ToString("x"));
            AbortXferLoop(i + 1, buffers, isoPktInfos, contexts, inOvLap);
            return;
        }
    }

    i = 0;

    // The infinite xfer loop.
    while (1)
    {
        long rLen = len;	// Reset this each time through because
        // FinishDataXfer may modify it

        if (!epControl->WaitForXfer(&inOvLap[i], TimeOut))
        {
            epControl->Abort();
            if (epControl->LastError == ERROR_IO_PENDING)
                WaitForSingleObject(inOvLap[i].hEvent, 2000);
        }

        if (epControl->Attributes == 1) // ISOC Endpoint
        {
            if (epControl->FinishDataXfer(buffers[i], rLen, &inOvLap[i], contexts[i], isoPktInfos[i]))
            {
                CCyIsoPktInfo* pkts = isoPktInfos[i];
                for (int j = 0; j < PPX; j++)
                {
                    if ((pkts[j].Status == 0) && (pkts[j].Length <= epControl->MaxPktSize))
                    {

                    }


                    pkts[j].Length = 0;	// Reset to zero for re-use.
                    pkts[j].Status = 0;
                }

            }


        }

        else // BULK Endpoint
        {
            if (epControl->FinishDataXfer(buffers[i], rLen, &inOvLap[i], contexts[i]))
            {

            }


        }




        // Re-submit this queue element to keep the queue full
        contexts[i] = epControl->BeginDataXfer(buffers[i], len, &inOvLap[i]);
        if (epControl->NtStatus || epControl->UsbdStatus) // BeginDataXfer failed
        {
            //Display(String::Concat("Xfer request rejected. NTSTATUS = ", EndPt->NtStatus.ToString("x")));
            _RPT1(_CRT_WARN, "Xfer request rejected. NTSTATUS = %s\n", epControl->NtStatus.ToString("x"));
            AbortXferLoop(QueueSize, buffers, isoPktInfos, contexts, inOvLap);
            return;
        }

        i++;

        if (i == QueueSize) //Only update the display once each time through the Queue
        {
            i = 0;
        }

    }  // End of the infinite loop

    // Memory clean-up
    AbortXferLoop(QueueSize, buffers, isoPktInfos, contexts, inOvLap);
}



int _cdecl img_init_old()
{
    _RPT0(_CRT_WARN, "IMG INIT\n");

    long len = epControl->MaxPktSize * PPX; // Each xfer request will get PPX isoc packets

    epControl->SetXferSize(len);
    wait(500);

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;


    ept->Target = TGT_DEVICE; // byte 0
    ept->ReqType = REQ_VENDOR; // byte 0
    ept->ReqCode = 0xbc; // byte 1
    ept->Value = 0x0000; // byte 2,3
    ept->Index = 0x0000; // byte 4,5
    ept->TimeOut = 2000;

    unsigned char  buf2[512];

    LONG bytesToSend = 16;  // 38 + 16 = 54
    //ZeroMemory(buf2, bytesToSend);

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

    // changes byte 0 to 0x40
    ept->Write(buf2, bytesToSend);
    wait(500);

    ept->ReqCode = 0xb6; // byte 1
    ept->Value = 0x0000; // byte 2,3
    ept->Index = 0x0000; // byte 4,5

    bytesToSend = 10;  // 38 + 10 = 48

    //ZeroMemory(buf2, bytesToSend);

    // changes byte 0 to 0xc0
    ept->Read(buf2, bytesToSend);
    wait(500);

    ept->ReqCode = 0xbc; // byte 1
    ept->Value = 0x0000; // byte 2,3
    ept->Index = 0x0000; // byte 4,5

    bytesToSend = 16;  // 38 + 16 = 54

    //ZeroMemory(buf2, bytesToSend);

    // changes byte 0 to 0xc0
    ept->Read(buf2, bytesToSend);
    wait(500);


    ept->ReqCode = 0xb9; // byte 1
    ept->Value = 0x0000; // byte 2,3
    ept->Index = 0x0000; // byte 4,5

    bytesToSend = 0;  // 38 + 0 = 0
    // changes byte 0 to 0x40
    ept->Write(buf2, bytesToSend);
    wait(500);

    return USBNeodenCamera->DeviceCount();

}

int _cdecl img_init()
{
    _RPT0(_CRT_WARN, "IMG INIT\n");

    long len = epControl->MaxPktSize * PPX; // Each xfer request will get PPX isoc packets
    BOOL bXferCompleted = false;

    epControl->SetXferSize(len);
    wait(500);

    CCyControlEndPoint* cept = USBNeodenCamera->ControlEndPt;
    CCyBulkEndPoint* bept = USBNeodenCamera->BulkInEndPt;

    if(cept == NULL)
    {
        return 0;
    }


    unsigned char  buf2[512];
    LONG bytesToSend = 0;
    LONG rLen = 0;

    // *************************************************
    cept->Target = TGT_DEVICE; // byte 0
    cept->ReqType = REQ_VENDOR; // byte 0
    cept->ReqCode = 0xbc; // byte 1
    cept->Value = 0x0000; // byte 2,3
    cept->Index = 0x0000; // byte 4,5
    cept->Direction = DIR_TO_DEVICE; // 0x40
    cept->TimeOut = 2000;

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
    bXferCompleted = cept->XferData(buf2, rLen);

    // *************************************************
    cept->ReqCode = 0xb6; // byte 1
    cept->Value = 0x0000; // byte 2,3
    cept->Index = 0x0000; // byte 4,5
    cept->Direction = DIR_FROM_DEVICE; // 0xc0

    bytesToSend = 10;  // 38 + 10 = 48

    rLen = bytesToSend;
    bXferCompleted = cept->XferData(buf2, rLen);

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
    cept->ReqCode = 0xbc; // byte 1
    cept->Value = 0x0000; // byte 2,3
    cept->Index = 0x0000; // byte 4,5
    cept->Direction = DIR_FROM_DEVICE; // 0xc0

    bytesToSend = 16;  // 38 + 16 = 54

    rLen = bytesToSend;
    bXferCompleted = cept->XferData(buf2, rLen);

    //if (bept != NULL)
    //{
    //    rLen = bytesToSend;
    //    bXferCompleted = bept->XferData(buf2, rLen);
    //}

    //for (size_t i = 0; i < bytesToSend; i++) {
    //    _RPT2(_CRT_WARN, "i:%d:0x%02x\n", i, buf2[i]);
    //}

    // *************************************************
    cept->ReqCode = 0xb9; // byte 1
    cept->Value = 0x0000; // byte 2,3
    cept->Index = 0x0000; // byte 4,5
    cept->Direction = DIR_TO_DEVICE; // 0x40

    bytesToSend = 0;  // 38 + 0 = 0

    rLen = bytesToSend;
    bXferCompleted = cept->XferData(buf2, rLen);

    return USBNeodenCamera->DeviceCount();

}


int _cdecl img_capture(int which_camera, unsigned char* pFrameBuffer)
{
    _RPT0(_CRT_WARN, "IMG CAPTURE\n");

    CCyControlEndPoint* cept = USBNeodenCamera->ControlEndPt;
    CCyBulkEndPoint* bept = USBNeodenCamera->BulkInEndPt;
    BOOL bXferCompleted = false;
    unsigned char  buf2[1];
    LONG bytesToSend = 0;
    LONG rLen = 0;
    int retVal = 0;

    cept->Target = TGT_DEVICE;
    cept->ReqType = REQ_VENDOR;
    cept->ReqCode = 0xb8;
    cept->Value = 0x0000;
    cept->Index = 0x0000;
    cept->Direction = DIR_TO_DEVICE; // 0xc0

    bytesToSend = 0;
    rLen = bytesToSend;
    bXferCompleted = cept->XferData(buf2, rLen);

    //    unsigned char  buf[512];
    //    ZeroMemory(buf, 512);
    bytesToSend = 1024 * 512;
    rLen = bytesToSend;

    bXferCompleted = bept->XferData(pFrameBuffer, rLen);
    if (bXferCompleted)
    {
        retVal = 1;
    }

    _RPT1(_CRT_WARN, "capture: %d\n", retVal);

    return retVal;
}

BOOL _cdecl img_set(int which_camera, int speed, int16_t exposure, int16_t gain)
{
    _RPT0(_CRT_WARN, "IMG SET\n");

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
    _RPT0(_CRT_WARN, "IMG SET SPEED\n");

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->Target = TGT_DEVICE; // byte 0
    ept->ReqType = REQ_VENDOR; // byte 0
    ept->ReqCode = 0xb0; // byte 1
    ept->Value = 0x0000; // byte 2,3
    ept->Index = speed; // byte 4,5

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38
    //ZeroMemory(buf2, bytesToSend);

    // changes byte 0 to 0x40
    return ept->Write(buf2, bytesToSend);

}

BOOL _cdecl img_set_exp(int which_camera, int16_t exposure)
{
    _RPT0(_CRT_WARN, "IMG SET EXP\n");

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->Target = TGT_DEVICE; // byte 0
    ept->ReqType = REQ_VENDOR; // byte 0
    ept->ReqCode = 0xb7; // byte 1
    ept->Value = exposure; // byte 2,3
    ept->Index = 0x0009; // byte 4,5

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38
    //ZeroMemory(buf2, bytesToSend);

    // changes byte 0 to 0x40
    return ept->Write(buf2, bytesToSend);
}

BOOL _cdecl img_set_gain(int which_camera, int16_t gain)
{
    _RPT0(_CRT_WARN, "IMG SET GAIN\n");

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->Target = TGT_DEVICE; // byte 0
    ept->ReqType = REQ_VENDOR; // byte 0
    ept->ReqCode = 0xb7; // byte 1
    ept->Value = gain; // byte 2,3
    ept->Index = 0x0035; // byte 4,5

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38
    //ZeroMemory(buf2, bytesToSend);

    // changes byte 0 to 0x40
    return ept->Write(buf2, bytesToSend);

}

BOOL _cdecl img_get_exp(int which_camera)
{
    _RPT0(_CRT_WARN, "IMG GET exp\n");

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->Target = TGT_DEVICE; // byte 0
    ept->ReqType = REQ_VENDOR; // byte 0
    ept->ReqCode = 0xb6; // byte 1
    ept->Value = 0x0000; // byte 2,3
    ept->Index = 0x0009; // byte 4,5

    unsigned char  buf2[64];

    LONG bytesToSend = 64;  // 38 + 0 = 38
    //ZeroMemory(buf2, bytesToSend);

    // changes byte 0 to 0xc0
    return ept->Read(buf2, bytesToSend);
}

BOOL _cdecl img_get_gain(int which_camera)
{
    _RPT0(_CRT_WARN, "IMG GET GAIN\n");

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->Target = TGT_DEVICE; // byte 0
    ept->ReqType = REQ_VENDOR; // byte 0
    ept->ReqCode = 0xb6; // byte 1
    ept->Value = 0x0000; // byte 2,3
    ept->Index = 0x0035; // byte 4,5

    unsigned char  buf2[64];

    LONG bytesToSend = 64;  // 38 + 0 = 38
    //ZeroMemory(buf2, bytesToSend);

    // changes byte 0 to 0xc0
    return ept->Read(buf2, bytesToSend);
}


BOOL _cdecl img_setRoi(int which_camera, short a2, short a3, short w, short h)
{
    _RPT0(_CRT_WARN, "IMG SETROI\n");

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->Target = TGT_DEVICE; // byte 0
    ept->ReqType = REQ_VENDOR; // byte 0
    ept->ReqCode = 0xba; // byte 1


    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38
    //ZeroMemory(buf2, bytesToSend);

    ept->Value = ((a2 >> 1) << 1) + 12; // byte 2,3
    ept->Index = 0x0001; // byte 4,5

    // changes byte 0 to 0x40
    ept->Write(buf2, bytesToSend);

    ept->Value = ((a3 >> 1) << 1) + 20; // byte 2,3
    ept->Index = 0x0002; // byte 4,5

        // changes byte 0 to 0x40
    ept->Write(buf2, bytesToSend);

    ept->Value = h - 1; // byte 2,3
    ept->Index = 0x0003; // byte 4,5

        // changes byte 0 to 0x40
    ept->Write(buf2, bytesToSend);

    ept->Value = w - 1; // byte 2,3
    ept->Index = 0x0004; // byte 4,5

        // changes byte 0 to 0x40
    return ept->Write(buf2, bytesToSend);

}

BOOL _cdecl img_set_lt(int which_camera, int16_t a2, int16_t a3)
{
    _RPT0(_CRT_WARN, "IMG SET LT\n");

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->Target = TGT_DEVICE; // byte 0
    ept->ReqType = REQ_VENDOR; // byte 0
    ept->ReqCode = 0xba; // byte 1

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38
 /*   ZeroMemory(buf2, bytesToSend);*/

    ept->Value = ((a2 >> 1) << 1) + 12; // byte 2,3
    ept->Index = 0x0001; // byte 4,5

    // changes byte 0 to 0x40
    ept->Write(buf2, bytesToSend);

    ept->Value = ((a3 >> 1) << 1) + 20; // byte 2,3
    ept->Index = 0x0002; // byte 4,5

        // changes byte 0 to 0x40
    return ept->Write(buf2, bytesToSend);
}

BOOL _cdecl img_set_wh(int which_camera, int16_t w, int16_t h)
{
    _RPT0(_CRT_WARN, "IMG SETROI\n");

    CCyControlEndPoint* ept = USBNeodenCamera->ControlEndPt;

    ept->Target = TGT_DEVICE; // byte 0
    ept->ReqType = REQ_VENDOR; // byte 0
    ept->ReqCode = 0xba; // byte 1

    unsigned char  buf2[1];

    LONG bytesToSend = 0;  // 38 + 0 = 38
    //ZeroMemory(buf2, bytesToSend);

    ept->Value = h - 1; // byte 2,3
    ept->Index = 0x0003; // byte 4,5

        // changes byte 0 to 0x40
    ept->Write(buf2, bytesToSend);

    ept->Value = w - 1; // byte 2,3
    ept->Index = 0x0004; // byte 4,5

        // changes byte 0 to 0x40
    return ept->Write(buf2, bytesToSend);
}