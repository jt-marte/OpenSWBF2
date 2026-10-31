#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <hidclass.h>
#include <hidsdi.h>
#include <stdio.h>
#include <string.h>

#include "pcSonyHid.h"
#include "pcSonyPad.h"
#include "RedEngineFL/Util/RedLog.h"

namespace SonyHid
{

// Largest report handled. Bluetooth pads declare reports up to 547 bytes
// (audio), and Windows pads every read and write to the largest one.
const unsigned int kMaxReport = 1024;

// Reports drained per pad per frame: more than Windows' 32-report input
// buffer, so a long frame still ends on the newest state.
const int kMaxReadsPerFrame = 128;

// How often to check the device list for a new pad, and how many scans a pad
// that wouldn't open (another program holds it, or it was still starting up)
// is retried on before waiting for the next device change.
const float kScanInterval = 1.0f;
const int kOpenRetries = 3;

// A feature read is a round trip to the pad; one that doesn't answer is given
// up on rather than stalling the frame.
const DWORD kFeatureTimeoutMs = 250;

struct Device
{
    HANDLE hFile;
    HANDLE hReadEvent;
    HANDLE hWriteEvent;
    OVERLAPPED kRead;
    OVERLAPPED kWrite;
    bool bReadPending;
    bool bWritePending;
    bool bCanWrite;
    bool bBluetooth;
    bool bEnhancedRumble;
    bool bLoggedReport;
    bool bLoggedWriteError;
    SonyPad::Model eModel;
    unsigned short usPid;
    unsigned char ucSequence;
    unsigned int uiInSize;
    unsigned int uiOutSize;
    XInputPad::Pad kPad;
    char acPath[512];
    unsigned char aucIn[kMaxReport];
    unsigned char aucOut[kMaxReport];
};

static Device sDevices[SonyPad::kMaxPads];
static int sSlots;
static float sScanTimer;
static int sDeviceCount = -1;
static int sRetries;
static char sacIgnore[512];

// Paths already reported as failing to open, so a retry doesn't log again.
static char sacFailedPath[SonyPad::kMaxPads][512];
static int sFailedNext;

static bool LoggedFailure(const char* pcPath)
{
    for (int i = 0; i < SonyPad::kMaxPads; ++i)
    {
        if (strcmp(sacFailedPath[i], pcPath) == 0)
        {
            return true;
        }
    }
    strncpy(sacFailedPath[sFailedNext], pcPath, sizeof(sacFailedPath[0]) - 1);
    sFailedNext = (sFailedNext + 1) % SonyPad::kMaxPads;
    return false;
}

static void Close(Device& d, bool bFlushWrite)
{
    if (!d.hFile)
    {
        return;
    }
    if (d.bWritePending && bFlushWrite)
    {
        // let a last rumble-off report reach the pad
        WaitForSingleObject(d.hWriteEvent, 100);
    }
    if (d.bReadPending || d.bWritePending)
    {
        CancelIoEx(d.hFile, 0);
        DWORD dwDone;
        if (d.bReadPending)
        {
            GetOverlappedResult(d.hFile, &d.kRead, &dwDone, TRUE);
        }
        if (d.bWritePending)
        {
            GetOverlappedResult(d.hFile, &d.kWrite, &dwDone, TRUE);
        }
    }
    CloseHandle(d.hFile);
    if (d.hReadEvent)
    {
        CloseHandle(d.hReadEvent);
    }
    if (d.hWriteEvent)
    {
        CloseHandle(d.hWriteEvent);
    }
    memset(&d, 0, sizeof(d));
}

static void Lost(int iSlot)
{
    LOG(LogCore, Inform, "Sony pad %d: %s disconnected", iSlot, SonyPad::ModelName(sDevices[iSlot].eModel));
    Close(sDevices[iSlot], false);
}

static bool IsOpen(const char* pcPath)
{
    for (int i = 0; i < sSlots; ++i)
    {
        if (sDevices[i].hFile && _stricmp(sDevices[i].acPath, pcPath) == 0)
        {
            return true;
        }
    }
    return false;
}

static int FreeSlot()
{
    for (int i = 0; i < sSlots; ++i)
    {
        if (!sDevices[i].hFile)
        {
            return i;
        }
    }
    return -1;
}

// Feature report into pucBuf (report ID first; uiSize is the device's largest
// feature report, which Windows wants). The handle is overlapped, which
// HidD_GetFeature doesn't support, so this is its ioctl with a time limit.
// Only called while no read or write is in flight.
static bool GetFeature(Device& d, unsigned char ucId, unsigned char* pucBuf, unsigned int uiSize)
{
    memset(pucBuf, 0, uiSize);
    pucBuf[0] = ucId;
    OVERLAPPED kOverlapped;
    memset(&kOverlapped, 0, sizeof(kOverlapped));
    kOverlapped.hEvent = d.hReadEvent;
    DWORD dwGot = 0;
    if (!DeviceIoControl(d.hFile, IOCTL_HID_GET_FEATURE, pucBuf, uiSize, pucBuf, uiSize, 0, &kOverlapped) &&
        GetLastError() != ERROR_IO_PENDING)
    {
        return false;
    }
    if (WaitForSingleObject(d.hReadEvent, kFeatureTimeoutMs) != WAIT_OBJECT_0)
    {
        CancelIoEx(d.hFile, &kOverlapped);
    }
    return GetOverlappedResult(d.hFile, &kOverlapped, &dwGot, TRUE) != FALSE;
}

static void LogWriteError(Device& d, DWORD dwError)
{
    if (!d.bLoggedWriteError)
    {
        d.bLoggedWriteError = true;
        LOG(LogCore, Warning, "Sony pad: %s rumble write of %u bytes failed (error %lu)", SonyPad::ModelName(d.eModel),
            d.uiOutSize, dwError);
    }
}

static bool WriteRumble(Device& d, unsigned short usHeavy, unsigned short usLight)
{
    const unsigned int uiLength = SonyPad::BuildRumbleReport(d.eModel, d.bBluetooth, d.bEnhancedRumble, d.ucSequence,
                                                             usHeavy, usLight, d.aucOut, d.uiOutSize);
    if (uiLength == 0)
    {
        return false;
    }
    memset(&d.kWrite, 0, sizeof(d.kWrite));
    d.kWrite.hEvent = d.hWriteEvent;
    if (!WriteFile(d.hFile, d.aucOut, d.uiOutSize, 0, &d.kWrite) && GetLastError() != ERROR_IO_PENDING)
    {
        LogWriteError(d, GetLastError());
        return false;
    }
    d.bWritePending = true;
    d.ucSequence = (unsigned char)((d.ucSequence + 1) & 0x0F);
    return true;
}

static bool Open(int iSlot, const char* pcPath)
{
    Device& d = sDevices[iSlot];
    memset(&d, 0, sizeof(d));

    bool bCanWrite = true;
    HANDLE hFile = CreateFileA(pcPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
                               OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    if (hFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED)
    {
        bCanWrite = false;
        hFile = CreateFileA(pcPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
                            FILE_FLAG_OVERLAPPED, 0);
    }
    if (hFile == INVALID_HANDLE_VALUE)
    {
        const DWORD dwError = GetLastError();
        if (!LoggedFailure(pcPath))
        {
            LOG(LogCore, Warning, "Sony pad: can't open %s (error %lu%s)", pcPath, dwError,
                dwError == ERROR_SHARING_VIOLATION ? ", another program such as DS4Windows holds it" : "");
        }
        return false;
    }

    HIDD_ATTRIBUTES kAttributes;
    kAttributes.Size = sizeof(kAttributes);
    PHIDP_PREPARSED_DATA pPreparsed = 0;
    HIDP_CAPS kCaps;
    memset(&kCaps, 0, sizeof(kCaps));
    bool bCaps = false;
    if (HidD_GetPreparsedData(hFile, &pPreparsed))
    {
        bCaps = HidP_GetCaps(pPreparsed, &kCaps) == HIDP_STATUS_SUCCESS;
        HidD_FreePreparsedData(pPreparsed);
    }
    const SonyPad::Model eModel = HidD_GetAttributes(hFile, &kAttributes)
                                      ? SonyPad::ModelFor(kAttributes.VendorID, kAttributes.ProductID)
                                      : SonyPad::kModelNone;
    // the pad's gamepad collection, with reports that fit the buffers
    if (eModel == SonyPad::kModelNone || !bCaps || kCaps.UsagePage != 0x01 ||
        (kCaps.Usage != 0x04 && kCaps.Usage != 0x05) || kCaps.InputReportByteLength < SonyPad::kSimpleInputSize ||
        kCaps.InputReportByteLength > kMaxReport || kCaps.OutputReportByteLength > kMaxReport ||
        kCaps.FeatureReportByteLength > kMaxReport)
    {
        CloseHandle(hFile);
        return false;
    }

    d.hFile = hFile;
    d.hReadEvent = CreateEventA(0, TRUE, FALSE, 0);
    d.hWriteEvent = CreateEventA(0, TRUE, FALSE, 0);
    if (!d.hReadEvent || !d.hWriteEvent)
    {
        Close(d, false);
        return false;
    }
    strncpy(d.acPath, pcPath, sizeof(d.acPath) - 1);
    d.eModel = eModel;
    d.usPid = kAttributes.ProductID;
    d.uiInSize = kCaps.InputReportByteLength;
    d.uiOutSize = kCaps.OutputReportByteLength;
    // USB reports are 64 bytes; over Bluetooth the device declares bigger
    // ones (DS4Windows tells the two apart the same way).
    d.bBluetooth = d.uiInSize != SonyPad::kUsbInputSize;
    d.bCanWrite = bCanWrite && d.uiOutSize >= (d.bBluetooth ? SonyPad::kBtReportSize
                                                            : (eModel == SonyPad::kModelDualSense
                                                                   ? SonyPad::kDualSenseUsbOutputSize
                                                                   : SonyPad::kDs4UsbOutputSize));

    // Feature reads: the DualSense's firmware version picks its rumble mode,
    // and over Bluetooth a feature read also moves either pad from simple
    // reports to full ones (the rumble report below does too).
    unsigned short usFirmware = 0;
    bool bFirmware = false;
    if (kCaps.FeatureReportByteLength > 0)
    {
        unsigned char* pucFeature = d.aucIn;
        if (eModel == SonyPad::kModelDualSense)
        {
            bFirmware = GetFeature(d, 0x20, pucFeature, kCaps.FeatureReportByteLength) &&
                        SonyPad::ParseFirmwareVersion(pucFeature, kCaps.FeatureReportByteLength, usFirmware);
            if (d.bBluetooth)
            {
                GetFeature(d, 0x09, pucFeature, kCaps.FeatureReportByteLength);
            }
        }
        else if (d.bBluetooth)
        {
            GetFeature(d, 0x05, pucFeature, kCaps.FeatureReportByteLength);
        }
    }
    d.bEnhancedRumble = SonyPad::UseEnhancedRumble(d.usPid, bFirmware, usFirmware);

    if (d.bCanWrite)
    {
        WriteRumble(d, 0, 0);
    }

    char acFirmware[32] = "";
    if (bFirmware)
    {
        sprintf(acFirmware, ", firmware %x.%02x", usFirmware >> 8, usFirmware & 0xFF);
    }
    LOG(LogCore, Inform, "Sony pad %d: %s (PID %04X) connected over %s%s, rumble %s", iSlot,
        SonyPad::ModelName(eModel), d.usPid, d.bBluetooth ? "Bluetooth" : "USB", acFirmware,
        !d.bCanWrite ? "unavailable" : (eModel == SonyPad::kModelDualSense && !d.bEnhancedRumble ? "compatible" : "on"));
    return true;
}

// Opens every Sony pad not open yet, while slots last. True if one wouldn't open.
static bool Scan()
{
    GUID kHidGuid;
    HidD_GetHidGuid(&kHidGuid);
    HDEVINFO hInfo = SetupDiGetClassDevsA(&kHidGuid, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hInfo == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    bool bFailed = false;
    SP_DEVICE_INTERFACE_DATA kInterface;
    kInterface.cbSize = sizeof(kInterface);
    for (DWORD dwIndex = 0; FreeSlot() >= 0 && SetupDiEnumDeviceInterfaces(hInfo, 0, &kHidGuid, dwIndex, &kInterface);
         ++dwIndex)
    {
        DWORD adwDetail[(sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A) + 512 + sizeof(DWORD)) / sizeof(DWORD)];
        SP_DEVICE_INTERFACE_DETAIL_DATA_A* pDetail = (SP_DEVICE_INTERFACE_DETAIL_DATA_A*)adwDetail;
        pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        if (!SetupDiGetDeviceInterfaceDetailA(hInfo, &kInterface, pDetail, sizeof(adwDetail), 0, 0))
        {
            continue;
        }
        unsigned short usVid;
        unsigned short usPid;
        if (!SonyPad::ParseHidPathIds(pDetail->DevicePath, usVid, usPid) ||
            SonyPad::ModelFor(usVid, usPid) == SonyPad::kModelNone || IsOpen(pDetail->DevicePath))
        {
            continue;
        }
        if (SonyPad::IdListHas(sacIgnore, usVid, usPid))
        {
            if (!LoggedFailure(pDetail->DevicePath))
            {
                LOG(LogCore, Inform, "Sony pad: %04X:%04X left to Steam Input, which already plays it as an XInput pad",
                    usVid, usPid);
            }
            continue;
        }
        if (!Open(FreeSlot(), pDetail->DevicePath))
        {
            bFailed = true;
        }
    }
    SetupDiDestroyDeviceInfoList(hInfo);
    return bFailed;
}

void Init(int iSlots)
{
    sSlots = iSlots < SonyPad::kMaxPads ? iSlots : SonyPad::kMaxPads;
    sScanTimer = 0.0f;
    sDeviceCount = -1;
    sRetries = 0;
    sacIgnore[0] = '\0';
    const DWORD dwLen = GetEnvironmentVariableA("SDL_GAMECONTROLLER_IGNORE_DEVICES", sacIgnore, sizeof(sacIgnore));
    if (dwLen >= sizeof(sacIgnore))
    {
        sacIgnore[0] = '\0';
    }
}

static void Pump(int iSlot)
{
    Device& d = sDevices[iSlot];
    for (int i = 0; i < kMaxReadsPerFrame && d.hFile; ++i)
    {
        if (!d.bReadPending)
        {
            memset(&d.kRead, 0, sizeof(d.kRead));
            d.kRead.hEvent = d.hReadEvent;
            if (!ReadFile(d.hFile, d.aucIn, d.uiInSize, 0, &d.kRead) && GetLastError() != ERROR_IO_PENDING)
            {
                Lost(iSlot);
                return;
            }
            d.bReadPending = true;
        }
        DWORD dwGot = 0;
        if (!GetOverlappedResult(d.hFile, &d.kRead, &dwGot, FALSE))
        {
            if (GetLastError() == ERROR_IO_INCOMPLETE)
            {
                return;
            }
            d.bReadPending = false;
            Lost(iSlot);
            return;
        }
        d.bReadPending = false;
        if (SonyPad::ParseReport(d.eModel, d.aucIn, dwGot, d.kPad) && !d.bLoggedReport)
        {
            d.bLoggedReport = true;
            LOG(LogCore, Inform, "Sony pad %d: first report 0x%02X, %lu bytes", iSlot, d.aucIn[0], dwGot);
        }
    }
}

void Update(float fDeltaTime)
{
    if (sSlots <= 0)
    {
        return;
    }
    for (int i = 0; i < sSlots; ++i)
    {
        Pump(i);
    }

    if (FreeSlot() < 0)
    {
        return;
    }
    sScanTimer -= fDeltaTime;
    if (sScanTimer > 0.0f)
    {
        return;
    }
    sScanTimer = kScanInterval;
    // A device arriving or leaving changes the raw input list; enumerating
    // HID devices is too slow to do every second for nothing.
    UINT uiCount = 0;
    GetRawInputDeviceList(0, &uiCount, sizeof(RAWINPUTDEVICELIST));
    const bool bChanged = (int)uiCount != sDeviceCount;
    if (!bChanged && sRetries == 0)
    {
        return;
    }
    sDeviceCount = (int)uiCount;
    sRetries = bChanged ? 0 : sRetries - 1;
    if (Scan() && bChanged)
    {
        sRetries = kOpenRetries;
    }
}

bool GetState(int iSlot, XInputPad::Pad& rPad)
{
    if (iSlot < 0 || iSlot >= sSlots || !sDevices[iSlot].hFile)
    {
        return false;
    }
    rPad = sDevices[iSlot].kPad;
    return true;
}

bool SetRumble(int iSlot, unsigned short usHeavy, unsigned short usLight)
{
    if (iSlot < 0 || iSlot >= sSlots || !sDevices[iSlot].hFile)
    {
        return false;
    }
    Device& d = sDevices[iSlot];
    if (!d.bCanWrite)
    {
        return true;
    }
    if (d.bWritePending)
    {
        DWORD dwDone;
        if (!GetOverlappedResult(d.hFile, &d.kWrite, &dwDone, FALSE))
        {
            const DWORD dwError = GetLastError();
            if (dwError == ERROR_IO_INCOMPLETE)
            {
                return false;
            }
            LogWriteError(d, dwError);
        }
        d.bWritePending = false;
    }
    // a failed write is dropped; a pad that's gone shows up as a failed read
    WriteRumble(d, usHeavy, usLight);
    return true;
}

void Shutdown()
{
    for (int i = 0; i < SonyPad::kMaxPads; ++i)
    {
        Close(sDevices[i], true);
    }
    sSlots = 0;
}

}
