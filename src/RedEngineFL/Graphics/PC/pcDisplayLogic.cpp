#include <math.h>
#include <stdio.h>
#include <string.h>

#include <d3d9.h>

#include "pcDisplayLogic.h"

#include "RedEngineFL/Util/RedLog.h"

namespace DisplayLogic
{

static bool FindOrdinal(const AdapterRec* pAdapters, unsigned int uiCount, unsigned int uiOrdinal)
{
    for (unsigned int i = 0; i < uiCount; i++)
    {
        if (pAdapters[i].ordinal == uiOrdinal)
        {
            return true;
        }
    }
    return false;
}

unsigned int BuildModeList(const DisplayModeRec* pSrc, unsigned int uiSrc, unsigned int uiAdapter,
                           DisplayModeRec* pOut, unsigned int uiOutCap)
{
    unsigned int uiTotal = 0;
    DisplayModeRec kLast = {0, 0, 0, 0};
    for (unsigned int i = 0; i < uiSrc; i++)
    {
        if (pSrc[i].adapter != uiAdapter)
        {
            continue;
        }
        bool bDuplicate = uiTotal > 0 && pSrc[i].w == kLast.w && pSrc[i].h == kLast.h
            && pSrc[i].refresh == kLast.refresh;
        if (bDuplicate)
        {
            continue;
        }
        if (pOut && uiTotal < uiOutCap)
        {
            pOut[uiTotal] = pSrc[i];
        }
        kLast = pSrc[i];
        uiTotal++;
    }
    return uiTotal;
}

unsigned int UniqueResolutions(const DisplayModeRec* pList, unsigned int uiCount, unsigned int uiMinW,
                               unsigned int uiMinH, DisplayModeRec* pOut, unsigned int uiOutCap)
{
    unsigned int uiTotal = 0;
    for (unsigned int i = 0; i < uiCount; i++)
    {
        if (pList[i].w < uiMinW || pList[i].h < uiMinH)
        {
            continue;
        }
        bool bSeen = false;
        for (unsigned int j = 0; j < i && !bSeen; j++)
        {
            bSeen = pList[j].w == pList[i].w && pList[j].h == pList[i].h;
        }
        if (bSeen)
        {
            continue;
        }
        if (pOut && uiTotal < uiOutCap)
        {
            pOut[uiTotal] = pList[i];
        }
        uiTotal++;
    }
    return uiTotal;
}

void DesktopDefault(const DisplayModeRec* pList, unsigned int uiCount, unsigned int uiDeskW,
                    unsigned int uiDeskH, unsigned int& uiW, unsigned int& uiH)
{
    if (uiDeskW == 0 || uiDeskH == 0 || uiCount == 0)
    {
        uiW = kFallbackWidth;
        uiH = kFallbackHeight;
        return;
    }
    uiW = 0;
    uiH = 0;
    for (unsigned int i = 0; i < uiCount; i++)
    {
        if (pList[i].w > uiDeskW || pList[i].h > uiDeskH)
        {
            continue;
        }
        unsigned long long ullArea = (unsigned long long)pList[i].w * pList[i].h;
        unsigned long long ullBest = (unsigned long long)uiW * uiH;
        if (ullArea > ullBest || (ullArea == ullBest && pList[i].w > uiW))
        {
            uiW = pList[i].w;
            uiH = pList[i].h;
        }
    }
    if (uiW == 0 && uiH == 0 && uiCount != 0 && uiDeskW != 0 && uiDeskH != 0)
    {
        uiW = kFallbackWidth;
        uiH = kFallbackHeight;
    }
}

unsigned int PickRefresh(const DisplayModeRec* pList, unsigned int uiCount, unsigned int uiW,
                         unsigned int uiH, unsigned int uiRequested, bool bWindowed,
                         unsigned int uiDesktopHz)
{
    if (bWindowed)
    {
        return 0;
    }
    unsigned int uiWanted = uiRequested != 0 ? uiRequested : uiDesktopHz;
    unsigned int uiHighest = 0;
    for (unsigned int i = 0; i < uiCount; i++)
    {
        if (pList[i].w != uiW || pList[i].h != uiH)
        {
            continue;
        }
        if (uiWanted != 0 && pList[i].refresh == uiWanted)
        {
            return uiWanted;
        }
        if (pList[i].refresh > uiHighest)
        {
            uiHighest = pList[i].refresh;
        }
    }
    if (uiHighest == 0)
    {
        return 0;
    }
    if (uiRequested != 0)
    {
        LOG(LogRender, Warning, "display: refresh %uHz not listed for %ux%u, using %uHz", uiRequested,
            uiW, uiH, uiHighest);
    }
    return uiHighest;
}

unsigned int ResolveAdapter(unsigned int uiReqOrdinal, bool bOrdinalFromCli, const char* pcReqName,
                            const AdapterRec* pAdapters, unsigned int uiCount, bool bNoRender)
{
    if (bNoRender)
    {
        return kPrimaryAdapter;
    }
    bool bNameTried = !bOrdinalFromCli && pcReqName && pcReqName[0] != '\0';
    if (bNameTried)
    {
        for (unsigned int i = 0; i < uiCount; i++)
        {
            if (pAdapters[i].name && _stricmp(pAdapters[i].name, pcReqName) == 0)
            {
                return pAdapters[i].ordinal;
            }
        }
    }
    if (FindOrdinal(pAdapters, uiCount, uiReqOrdinal))
    {
        if (bNameTried)
        {
            LOG(LogRender, Warning, "display: adapter name '%.64s' not found, using ordinal %u",
                pcReqName, uiReqOrdinal);
        }
        return uiReqOrdinal;
    }
    LOG(LogRender, Warning, "display: adapter %u%s%.64s%s not found, using adapter %u", uiReqOrdinal,
        bNameTried ? " / '" : "", bNameTried ? pcReqName : "", bNameTried ? "'" : "", kPrimaryAdapter);
    return kPrimaryAdapter;
}

bool UsableUncached(bool bInList, bool bIsDesktop, bool bMsNone, unsigned int uiMsLevels,
                    unsigned int uiQuality)
{
    if (!bInList && !bIsDesktop)
    {
        return false;
    }
    if (bMsNone)
    {
        return true;
    }
    return uiMsLevels > uiQuality;
}

unsigned int EffectiveVsync(bool bKeySet, unsigned int uiKeyValue, bool bProfile)
{
    return bKeySet ? uiKeyValue : (bProfile ? 1u : 0u);
}

unsigned int ResolveVsyncInterval(unsigned int uiRequested, unsigned long ulCaps, bool bWindowed,
                                  bool* pbFellBack)
{
    if (pbFellBack)
    {
        *pbFellBack = false;
    }
    if (uiRequested == 0)
    {
        if (ulCaps != 0 && !(ulCaps & kIntervalImmediate))
        {
            if (pbFellBack)
            {
                *pbFellBack = true;
            }
            return kIntervalDefault;
        }
        return kIntervalImmediate;
    }
    if (uiRequested == 2)
    {
        if (!bWindowed && (ulCaps & kIntervalTwo))
        {
            return kIntervalTwo;
        }
        if (pbFellBack)
        {
            *pbFellBack = true;
        }
        return kIntervalDefault;
    }
    return kIntervalDefault;
}

const ResetBounds kStartupBounds = {0, 30000};
const ResetBounds kRuntimeBounds = {20, 0};

static ResetAction RevertOrFatal(bool bRevertAvailable)
{
    if (bRevertAvailable)
    {
        return kResetRevert;
    }
    return kResetFatal;
}

static bool OverBounds(unsigned int uiFailures, unsigned int uiElapsedMs, const ResetBounds& bounds)
{
    if (bounds.uiMaxFailures != 0 && uiFailures >= bounds.uiMaxFailures)
    {
        return true;
    }
    if (bounds.uiMaxMs != 0 && uiElapsedMs >= bounds.uiMaxMs)
    {
        return true;
    }
    return false;
}

ResetAction CoopStep(long hrCoop, unsigned int uiElapsedMs, const ResetBounds& bounds)
{
    if (hrCoop == D3DERR_DEVICELOST)
    {
        if (OverBounds(0, uiElapsedMs, bounds))
        {
            return kResetFatal;
        }
        return kResetWait;
    }
    if (SUCCEEDED(hrCoop) || hrCoop == D3DERR_DEVICENOTRESET)
    {
        return kResetNow;
    }
    return kResetFatal;
}

ResetAction ResetStep(long hr, unsigned int uiFailures, unsigned int uiElapsedMs,
                      bool bRevertAvailable, const ResetBounds& bounds)
{
    if (SUCCEEDED(hr))
    {
        return kResetDone;
    }
    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET)
    {
        if (OverBounds(uiFailures, uiElapsedMs, bounds))
        {
            return RevertOrFatal(bRevertAvailable);
        }
        return kResetWait;
    }
    if (hr == D3DERR_INVALIDCALL || hr == D3DERR_NOTAVAILABLE || hr == E_OUTOFMEMORY ||
        hr == D3DERR_OUTOFVIDEOMEMORY)
    {
        return RevertOrFatal(bRevertAvailable);
    }
    return kResetFatal;
}

static const unsigned long kMaxClient = 16384;
static const unsigned long kBorderlessExStyle = 0;

DisplayMode ResolveMode(bool bBorderless, bool bCliWin, bool bProfileWindowed)
{
    if (bBorderless)
    {
        return kModeBorderless;
    }
    if (bCliWin || bProfileWindowed)
    {
        return kModeWindowed;
    }
    return kModeFullscreen;
}

bool D3DWindowed(DisplayMode m)
{
    return m != kModeFullscreen;
}

bool MinimizeOnDeactivate(DisplayMode m)
{
    return m == kModeFullscreen;
}

bool Topmost(DisplayMode m)
{
    return m == kModeFullscreen;
}

WinStyle WindowStyleFor(DisplayMode m, bool bVisible)
{
    WinStyle ws;
    if (m == kModeWindowed)
    {
        ws.style = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX |
                   (bVisible ? WS_VISIBLE : 0);
        ws.exStyle = 0;
        return ws;
    }
    ws.style = WS_POPUP | WS_VISIBLE;
    if (m == kModeBorderless)
    {
        ws.exStyle = kBorderlessExStyle;
    }
    else
    {
        ws.exStyle = WS_EX_TOPMOST;
    }
    if (!bVisible)
    {
        ws.style &= ~(unsigned long)WS_VISIBLE;
    }
    return ws;
}

Rect PlaceWindow(DisplayMode m, const Rect& monitor, const Rect& work, unsigned long clientW,
                 unsigned long clientH, const Frame& frame)
{
    if (m == kModeBorderless)
    {
        return monitor;
    }
    if (clientW > kMaxClient)
    {
        clientW = kMaxClient;
    }
    if (clientH > kMaxClient)
    {
        clientH = kMaxClient;
    }
    const long lWidth = (long)clientW + frame.r - frame.l;
    const long lHeight = (long)clientH + frame.b - frame.t;
    Rect out;
    if (m == kModeWindowed)
    {
        out.l = (work.l - lWidth + work.r) / 2;
        out.t = (work.t - lHeight + work.b) / 2;
        if (out.l < work.l)
        {
            out.l = work.l;
        }
        if (out.t < work.t)
        {
            out.t = work.t;
        }
    }
    else
    {
        out.l = (monitor.l - lWidth + monitor.r) / 2;
        out.t = (monitor.t - lHeight + monitor.b) / 2;
    }
    out.r = out.l + lWidth;
    out.b = out.t + lHeight;
    return out;
}

Rect DpiChangedRect(const Rect& suggested, unsigned long clientW, unsigned long clientH,
                    const Frame& frame)
{
    Rect out;
    out.l = suggested.l;
    out.t = suggested.t;
    out.r = out.l + (long)clientW + frame.r - frame.l;
    out.b = out.t + (long)clientH + frame.b - frame.t;
    return out;
}

static bool DpiSettled(DpiStepFn fn, void* pCtx)
{
    const int iResult = fn(pCtx);
    if (iResult == kDpiAlready)
    {
        return true;
    }
    return iResult == kDpiOk;
}

DpiLevel DpiCascade(const DpiSteps& steps, bool bNoDpi)
{
    if (bNoDpi)
    {
        return kDpiLevelOff;
    }
    if (steps.pmv2)
    {
        if (DpiSettled(steps.pmv2, steps.pCtx))
        {
            return kDpiLevelPmV2;
        }
    }
    if (steps.pm)
    {
        if (DpiSettled(steps.pm, steps.pCtx))
        {
            return kDpiLevelPm;
        }
    }
    if (steps.system)
    {
        if (DpiSettled(steps.system, steps.pCtx))
        {
            return kDpiLevelSystem;
        }
    }
    return kDpiLevelNone;
}

const char* DpiLevelName(DpiLevel l)
{
    switch (l)
    {
    case kDpiLevelPmV2:
        return "pmv2";
    case kDpiLevelPm:
        return "pm";
    case kDpiLevelSystem:
        return "system";
    case kDpiLevelOff:
        return "off";
    default:
        return "none";
    }
}

DpiLevel PresetDpiLevel(int iAwareness, bool bPmV2, DpiLevel fallback)
{
    switch (iAwareness)
    {
    case 0:
        return kDpiLevelNone;
    case 1:
        return kDpiLevelSystem;
    case 2:
        return bPmV2 ? kDpiLevelPmV2 : kDpiLevelPm;
    default:
        return fallback;
    }
}

static int MapAxis(int v, unsigned int uiSrc, unsigned int uiDst)
{
    if (uiSrc == 0 || uiDst == 0)
    {
        return v < 0 ? 0 : v;
    }
    long long iResult = ((long long)v * (long long)uiDst + (long long)(uiSrc / 2)) / (long long)uiSrc;
    if (iResult > (long long)uiDst - 1)
    {
        iResult = (long long)uiDst - 1;
    }
    if (iResult < 0)
    {
        iResult = 0;
    }
    return (int)iResult;
}

void MapBackBufferToClient(int x, int y, unsigned int bbW, unsigned int bbH, unsigned int cW,
                           unsigned int cH, int& ox, int& oy)
{
    ox = MapAxis(x, bbW, cW);
    oy = MapAxis(y, bbH, cH);
}

void MapClientToBackBuffer(int x, int y, unsigned int cW, unsigned int cH, unsigned int bbW,
                           unsigned int bbH, int& ox, int& oy)
{
    ox = MapAxis(x, cW, bbW);
    oy = MapAxis(y, cH, bbH);
}

ShellSize ShellSizePolicy(DisplayMode m, bool bIsShell, unsigned int uiCliW, unsigned int uiCliH,
                          unsigned int uiProfW, unsigned int uiProfH, unsigned int uiDeskW,
                          unsigned int uiDeskH)
{
    const bool bCli = uiCliW > 0 && uiCliH > 0;
    const bool bProf = uiProfW > 0 && uiProfH > 0;
    unsigned int uiReqW = 0;
    unsigned int uiReqH = 0;
    if (bCli)
    {
        uiReqW = uiCliW;
        uiReqH = uiCliH;
    }
    else if (bIsShell)
    {
        uiReqW = 0;
        uiReqH = 0;
    }
    else if (bProf)
    {
        uiReqW = uiProfW;
        uiReqH = uiProfH;
    }

    ShellSize out;
    out.bWarnIgnoredSize = false;
    out.bWarnNoMonitor = false;
    out.uiBackW = uiReqW;
    out.uiBackH = uiReqH;
    out.uiClientW = uiReqW;
    out.uiClientH = uiReqH;

    if (m == kModeWindowed && !bCli && bProf)
    {
        out.uiClientW = uiProfW;
        out.uiClientH = uiProfH;
        if (bIsShell)
        {
            ShellBackSize(uiProfW, uiProfH, out.uiBackW, out.uiBackH);
        }
    }
    else if (m == kModeBorderless && (uiDeskW == 0 || uiDeskH == 0))
    {
        out.bWarnNoMonitor = !bIsShell;
    }
    else if (m == kModeBorderless)
    {
        out.uiClientW = uiDeskW;
        out.uiClientH = uiDeskH;
        if (!bIsShell)
        {
            out.uiBackW = uiDeskW;
            out.uiBackH = uiDeskH;
            out.bWarnIgnoredSize = uiReqW > 0 && uiDeskW > 0 && (uiReqW != uiDeskW || uiReqH != uiDeskH);
        }
        else if (!bCli)
        {
            ShellBackSize(uiDeskW, uiDeskH, out.uiBackW, out.uiBackH);
        }
    }
    // the shell never multisamples (StretchRect to the window)
    out.bClearMsaa = out.uiBackW == 0 || out.uiBackH == 0 || (bIsShell && !bCli);
    return out;
}

void ShellBackSize(unsigned int uiClientW, unsigned int uiClientH, unsigned int& uiBackW,
                   unsigned int& uiBackH)
{
    uiBackW = 0;
    uiBackH = 0;
    if (uiClientW == 0 || uiClientH <= 600 || uiClientW > kMaxDim || uiClientH > kMaxDim)
    {
        return;
    }
    unsigned int uiW = (uiClientH * 4 + 1) / 3;
    unsigned int uiH = uiClientH;
    if (uiW > uiClientW)
    {
        // narrower than 4:3 (5:4): as wide as the window
        uiW = uiClientW;
        uiH = (uiClientW * 3 + 2) / 4;
    }
    if (uiH <= 600)
    {
        return;
    }
    uiBackW = uiW;
    uiBackH = uiH;
}

float ShellUiScale(unsigned int uiScreenH)
{
    if (uiScreenH <= 600 || uiScreenH > kMaxDim)
    {
        return 1.0f;
    }
    return (float)uiScreenH / 600.0f;
}

ShellStretch ShellStretchPlan(DisplayMode m, bool bIsShell, unsigned int uiBackW,
                              unsigned int uiBackH, unsigned int uiClientW, unsigned int uiClientH,
                              bool bLinearCaps)
{
    const bool bSizes = uiBackW >= 1 && uiBackW <= kMaxDim && uiBackH >= 1 && uiBackH <= kMaxDim &&
                        uiClientW >= 1 && uiClientW <= kMaxDim && uiClientH >= 1 &&
                        uiClientH <= kMaxDim;
    ShellStretch out;
    out.bActive = bIsShell && m != kModeFullscreen && bSizes &&
                  (uiBackW != uiClientW || uiBackH != uiClientH);
    out.bLinear = out.bActive && bLinearCaps;
    return out;
}

bool SameDisplayParams(const DisplayRec& a, const DisplayRec& b)
{
    return a.uiW == b.uiW && a.uiH == b.uiH && a.bWindowed == b.bWindowed &&
           a.uiMsType == b.uiMsType && a.uiMsQuality == b.uiMsQuality &&
           a.uiInterval == b.uiInterval && a.uiRefresh == b.uiRefresh && a.eMode == b.eMode &&
           a.uiClientW == b.uiClientW && a.uiClientH == b.uiClientH;
}

LegacyFlags LegacyFlagsFor(DisplayMode eMode, unsigned int uiVsync)
{
    LegacyFlags out;
    out.bWindowed = eMode == kModeWindowed;
    out.bVsync = uiVsync != 0;
    return out;
}

static const unsigned int sFrameCaps[] = {0, 30, 60, 75, 120, 144, 165, 240};

static void EmitCap(unsigned int uiValue, unsigned int* pOut, unsigned int uiOutCap,
                    unsigned int& uiTotal)
{
    if (pOut && uiTotal < uiOutCap)
    {
        pOut[uiTotal] = uiValue;
    }
    uiTotal++;
}

unsigned int FrameCapList(unsigned int uiCurrent, unsigned int* pOut, unsigned int uiOutCap)
{
    const unsigned int uiBase = sizeof(sFrameCaps) / sizeof(sFrameCaps[0]);
    bool bListed = false;
    for (unsigned int i = 0; i < uiBase; i++)
    {
        if (sFrameCaps[i] == uiCurrent)
        {
            bListed = true;
        }
    }
    bool bPending = !bListed;
    unsigned int uiTotal = 0;
    for (unsigned int i = 0; i < uiBase; i++)
    {
        if (bPending && uiCurrent < sFrameCaps[i])
        {
            EmitCap(uiCurrent, pOut, uiOutCap, uiTotal);
            bPending = false;
        }
        EmitCap(sFrameCaps[i], pOut, uiOutCap, uiTotal);
    }
    if (bPending)
    {
        EmitCap(uiCurrent, pOut, uiOutCap, uiTotal);
    }
    return uiTotal;
}

unsigned int RefreshRates(const DisplayModeRec* pList, unsigned int uiCount, unsigned int uiW,
                          unsigned int uiH, unsigned int* pOut, unsigned int uiOutCap)
{
    unsigned int auiRates[64];
    unsigned int uiTotal = 0;
    for (unsigned int i = 0; i < uiCount; i++)
    {
        const unsigned int uiRate = pList[i].refresh;
        if (pList[i].w != uiW || pList[i].h != uiH || uiRate == 0)
        {
            continue;
        }
        unsigned int uiPos = 0;
        while (uiPos < uiTotal && auiRates[uiPos] < uiRate)
        {
            uiPos++;
        }
        if ((uiPos < uiTotal && auiRates[uiPos] == uiRate) || uiTotal == 64)
        {
            continue;
        }
        for (unsigned int j = uiTotal; j > uiPos; j--)
        {
            auiRates[j] = auiRates[j - 1];
        }
        auiRates[uiPos] = uiRate;
        uiTotal++;
    }
    for (unsigned int i = 0; pOut && i < uiTotal && i < uiOutCap; i++)
    {
        pOut[i] = auiRates[i];
    }
    return uiTotal;
}

unsigned int DisplayNumberFromDeviceName(const char* pcDevice)
{
    if (!pcDevice)
    {
        return 0;
    }
    const size_t uiLen = strlen(pcDevice);
    size_t uiStart = uiLen;
    while (uiStart > 0 && pcDevice[uiStart - 1] >= '0' && pcDevice[uiStart - 1] <= '9')
    {
        uiStart--;
    }
    const size_t uiDigits = uiLen - uiStart;
    if (uiDigits < 1 || uiDigits > 3)
    {
        return 0;
    }
    unsigned int uiValue = 0;
    for (size_t i = uiStart; i < uiLen; i++)
    {
        uiValue = uiValue * 10 + (unsigned int)(pcDevice[i] - '0');
    }
    return uiValue;
}

static bool CopyLabel(const char* pcLabel, char* pcOut, unsigned int uiSize)
{
    if (!pcOut || uiSize == 0)
    {
        return false;
    }
    unsigned int uiLen = 0;
    while (pcLabel[uiLen] && uiLen + 1 < uiSize)
    {
        pcOut[uiLen] = pcLabel[uiLen];
        uiLen++;
    }
    pcOut[uiLen] = 0;
    return true;
}

bool MonitorLabel(unsigned int uiAdapter, unsigned int uiDisplay, unsigned int uiW,
                  unsigned int uiH, char* pcOut, unsigned int uiSize)
{
    if (!pcOut || uiSize == 0)
    {
        return false;
    }
    char acBuf[32];
    const bool bSize = uiW >= 1 && uiW <= kMaxDim && uiH >= 1 && uiH <= kMaxDim;
    const bool bDisplay = uiDisplay != 0;
    const unsigned int uiNumber = bDisplay ? uiDisplay : uiAdapter + 1;
    const char* pcName = bDisplay ? "Display" : "Monitor";
    if (bSize)
    {
        _snprintf(acBuf, sizeof(acBuf), "%s %u (%ux%u)", pcName, uiNumber, uiW, uiH);
    }
    else
    {
        _snprintf(acBuf, sizeof(acBuf), "%s %u", pcName, uiNumber);
    }
    acBuf[sizeof(acBuf) - 1] = 0;
    return CopyLabel(acBuf, pcOut, uiSize);
}

unsigned int MultisampleList(const unsigned int* auiLevels, MultisampleEntry* pOut,
                             unsigned int uiOutCap)
{
    if (!pOut || uiOutCap == 0)
    {
        return 0;
    }
    pOut[0].uiType = 0;
    pOut[0].uiQuality = 0;
    unsigned int uiTotal = 1;
    if (!auiLevels)
    {
        return uiTotal;
    }
    for (unsigned int uiType = 2; uiType < kMultisampleTypes; uiType++)
    {
        for (unsigned int uiQ = 0; uiQ < auiLevels[uiType]; uiQ++)
        {
            if (uiTotal >= uiOutCap)
            {
                return uiTotal;
            }
            pOut[uiTotal].uiType = uiType;
            pOut[uiTotal].uiQuality = uiQ;
            uiTotal++;
        }
    }
    return uiTotal;
}

bool MultisampleLabel(unsigned int uiType, unsigned int uiQuality, char* pcOut,
                      unsigned int uiSize)
{
    if (!pcOut || uiSize == 0)
    {
        return false;
    }
    char acBuf[32];
    if (uiType == 0)
    {
        _snprintf(acBuf, sizeof(acBuf), "Off");
    }
    else if (uiQuality == 0)
    {
        _snprintf(acBuf, sizeof(acBuf), "%ux", uiType);
    }
    else
    {
        _snprintf(acBuf, sizeof(acBuf), "%ux Q%u", uiType, uiQuality + 1);
    }
    acBuf[sizeof(acBuf) - 1] = 0;
    return CopyLabel(acBuf, pcOut, uiSize);
}

void FakeMultisampleLevels(unsigned int* auiLevels)
{
    if (!auiLevels)
    {
        return;
    }
    for (unsigned int i = 0; i < kMultisampleTypes; i++)
    {
        auiLevels[i] = 0;
    }
    auiLevels[2] = 1;
    auiLevels[4] = 1;
    auiLevels[8] = 1;
}

unsigned int EffectiveFpsCap(unsigned int uiFps, bool bPresentThrottled)
{
    return bPresentThrottled ? 0 : uiFps;
}

void ScopeBox(unsigned int uiW, unsigned int uiH, unsigned int& uiBoxW, unsigned int& uiBarW)
{
    // even, so the box splits into two equal halves about the centre
    unsigned int uiBox = (unsigned int)(((unsigned long long)uiH * 4 / 3) & ~1ULL);
    if (uiBox == 0 || uiBox >= uiW)
    {
        uiBoxW = uiW;
        uiBarW = 0;
        return;
    }
    uiBoxW = uiBox;
    uiBarW = (uiW - uiBox + 1) / 2;
}

void PresentWatchReset(PresentWatch& w)
{
    w.uiFrames = 0;
    w.dSum = 0.0;
    w.bOverridden = false;
    w.bThrottled = false;
}

void PresentWatchStep(PresentWatch& w, bool bVsyncInterval, bool bPresentOk, bool bVisible,
                      double dFrameSeconds, unsigned int uiRefreshHz)
{
    if (!bVsyncInterval || !bPresentOk || !bVisible)
    {
        w.uiFrames = 0;
        w.dSum = 0.0;
        w.bThrottled = false;
        return;
    }
    if (!w.bOverridden)
    {
        w.dSum += dFrameSeconds;
        w.uiFrames++;
        if (w.uiFrames >= kPresentWatchFrames)
        {
            const unsigned int uiHz = (uiRefreshHz >= 1 && uiRefreshHz <= 1000) ? uiRefreshHz : 60;
            if (w.dSum / kPresentWatchFrames < 0.75 / uiHz)
            {
                w.bOverridden = true;
            }
            w.uiFrames = 0;
            w.dSum = 0.0;
        }
    }
    w.bThrottled = !w.bOverridden;
}

bool PresentThrottledNow(const PresentWatch& w, bool bVisible, double dSecondsSincePresent)
{
    return w.bThrottled && bVisible && dSecondsSincePresent < 0.25;
}

bool ConfirmNeeded(const DisplayRec& kCur, const DisplayRec& kNext)
{
    return kCur.uiW != kNext.uiW || kCur.uiH != kNext.uiH || kCur.eMode != kNext.eMode ||
           kCur.uiRefresh != kNext.uiRefresh || kCur.uiClientW != kNext.uiClientW ||
           kCur.uiClientH != kNext.uiClientH;
}

bool MergeStageArm(bool bArmSoFar, bool bTagValid, bool bTagArm, bool bSet)
{
    return bSet && (bArmSoFar || (bTagValid && bTagArm));
}

bool StagedConfirm(bool bArm, bool bTagValid, bool bSet, bool bNeeded)
{
    return bArm && bTagValid && bSet && bNeeded;
}

ConfirmStep ConfirmPhaseStep(ConfirmPhase ePhase, ConfirmEvent eEvent, unsigned int uiElapsedMs)
{
    ConfirmStep out;
    out.ePhase = ePhase;
    out.eAction = kConfirmActNone;
    if (eEvent == kConfirmEvArm)
    {
        out.ePhase = kConfirmAwaitPresent;
    }
    else if (ePhase == kConfirmAwaitPresent && eEvent == kConfirmEvApplied)
    {
        out.ePhase = kConfirmCounting;
        out.eAction = kConfirmActStart;
    }
    else if (ePhase != kConfirmNone && eEvent == kConfirmEvApplyFailed)
    {
        out.ePhase = kConfirmNone;
        out.eAction = kConfirmActClear;
    }
    else if (ePhase == kConfirmAwaitPresent && eEvent == kConfirmEvAppliedPlain)
    {
        out.ePhase = kConfirmNone;
        out.eAction = kConfirmActClear;
    }
    else if (ePhase == kConfirmCounting && eEvent == kConfirmEvKeep)
    {
        out.ePhase = kConfirmNone;
        out.eAction = kConfirmActKeep;
    }
    else if (ePhase == kConfirmCounting && eEvent == kConfirmEvRevert)
    {
        out.ePhase = kConfirmNone;
        out.eAction = kConfirmActRevert;
    }
    else if (ePhase == kConfirmCounting && eEvent == kConfirmEvTick && uiElapsedMs >= kConfirmMs)
    {
        out.ePhase = kConfirmNone;
        out.eAction = kConfirmActRevert;
    }
    return out;
}

static unsigned int Pow2Floor(unsigned int uiValue, unsigned int uiCap)
{
    unsigned int uiLimit = uiValue;
    if (uiCap != 0 && uiCap < uiLimit)
    {
        uiLimit = uiCap;
    }
    unsigned int uiPow2 = 1;
    while (uiPow2 <= uiLimit / 2)
    {
        uiPow2 <<= 1;
    }
    return uiPow2;
}

BufferSet BufferSizes(unsigned int uiBackW, unsigned int uiBackH, unsigned int uiMaxTexW,
                      unsigned int uiMaxTexH, bool bNonPow2)
{
    const unsigned int uiPow2W = Pow2Floor(uiBackW, uiMaxTexW);
    const unsigned int uiPow2H = Pow2Floor(uiBackH, uiMaxTexH);
    BufferSet out;
    out.akBuf[0].uiW = uiPow2W > 0x200 ? 0x200 : uiPow2W;
    out.akBuf[0].uiH = uiPow2H > 0x200 ? 0x200 : uiPow2H;
    if (bNonPow2)
    {
        out.akBuf[1].uiW = (uiMaxTexW && uiBackW > uiMaxTexW) ? uiMaxTexW : (uiBackW ? uiBackW : 1);
        out.akBuf[1].uiH = (uiMaxTexH && uiBackH > uiMaxTexH) ? uiMaxTexH : (uiBackH ? uiBackH : 1);
    }
    else
    {
        out.akBuf[1].uiW = uiPow2W;
        out.akBuf[1].uiH = uiPow2H;
    }
    out.akBuf[2].uiW = uiPow2W > 0x100 ? 0x100 : uiPow2W;
    out.akBuf[2].uiH = uiPow2H > 0x100 ? 0x100 : uiPow2H;
    return out;
}

float FitDownSize(float fFactor, unsigned int uiViewW, unsigned int uiViewH, unsigned int uiBufW,
                  unsigned int uiBufH)
{
    float fFit = fFactor;
    if (uiViewW > 0 && (float)uiViewW * fFit > (float)uiBufW)
    {
        fFit = (float)uiBufW / (float)uiViewW;
    }
    if (uiViewH > 0 && (float)uiViewH * fFit > (float)uiBufH)
    {
        fFit = (float)uiBufH / (float)uiViewH;
    }
    return fFit;
}

bool ShadowQuadSharesSky(unsigned int uiBackW, unsigned int uiBackH, unsigned int uiMsType,
                         bool bDriverMs, unsigned int uiSkyW, unsigned int uiSkyH)
{
    return uiMsType == 0 && !bDriverMs && uiSkyW == uiBackW && uiSkyH == uiBackH;
}

ApplyStep ApplyPhaseStep(ApplyPhase ePhase, ApplyEvent eEvent)
{
    ApplyStep out;
    out.ePhase = ePhase;
    out.eAction = kActNone;
    if (ePhase == kApplyResetting && eEvent == kEvResetOk)
    {
        out.ePhase = kApplyAwaitPresent;
    }
    else if (ePhase == kApplyAwaitPresent && eEvent == kEvPresentOk)
    {
        out.ePhase = kApplyNone;
        out.eAction = kActPromote;
    }
    else if (ePhase == kApplyAwaitPresent && eEvent == kEvPresentFail)
    {
        out.ePhase = kApplyNone;
        out.eAction = kActRevert;
    }
    return out;
}

ApplyEvent PresentEvent(long hr)
{
    if (SUCCEEDED(hr))
    {
        return kEvPresentOk;
    }
    if (hr == D3DERR_DEVICELOST)
    {
        return kEvPresentLost;
    }
    return kEvPresentFail;
}

AdapterChoice AdapterDecision(unsigned int uiRequested, unsigned int uiRunning,
                              unsigned int uiCount)
{
    AdapterChoice out;
    out.bValid = uiRequested < uiCount;
    out.bRestart = out.bValid && uiRequested != uiRunning;
    return out;
}

bool ShouldRestoreProfile(bool bSnapshotValid, unsigned int uiOwner, unsigned int uiCurrent,
                          bool bRequestPending)
{
    return bSnapshotValid && uiOwner == uiCurrent && !bRequestPending;
}

bool SizeInBounds(unsigned int uiW, unsigned int uiH)
{
    return uiW >= kMinW && uiH >= kMinH && uiW <= kMaxDim && uiH <= kMaxDim;
}

bool SizeFitsCaps(unsigned int uiW, unsigned int uiH, unsigned int uiMaxTexW,
                  unsigned int uiMaxTexH)
{
    return (uiMaxTexW == 0 || uiW <= uiMaxTexW) && (uiMaxTexH == 0 || uiH <= uiMaxTexH);
}

void RampToLut(const unsigned short awRamp[256], unsigned char abLut[256])
{
    for (int i = 0; i < 256; ++i)
    {
        abLut[i] = (unsigned char)(awRamp[i] >> 8);
    }
}

bool LutIsIdentity(const unsigned char abLut[256])
{
    for (int i = 0; i < 256; ++i)
    {
        if (abLut[i] != (unsigned char)i)
        {
            return false;
        }
    }
    return true;
}

GammaReason GammaPathFor(bool bNoRender, bool bTestKind, bool bProg2)
{
    if (bNoRender && !bTestKind)
    {
        return kGammaReasonNoRender;
    }
    if (!bProg2)
    {
        return kGammaReasonNoProg2;
    }
    return kGammaReasonNone;
}

bool GammaPassRuns(GammaPath ePath, bool bIdentity, bool bResourcesReleased, bool bHaveTarget)
{
    return ePath == kGammaPass && !bIdentity && !bResourcesReleased && bHaveTarget;
}

CursorPlan CursorPlanFor(bool bShown, DisplayMode eMode, bool bActive, bool bSoftware, bool bNoRender)
{
    CursorPlan p = {false, true, false, false};
    if (bSoftware)
    {
        return p;
    }
    p.bHideDrawn = true;
    if (!bShown)
    {
        return p;
    }
    p.bExclusive = false;
    p.bHcursor = !bNoRender;
    p.bClip = !bNoRender && bActive && eMode == kModeFullscreen;
    return p;
}

unsigned int CursorButtonLatch(unsigned int uiRawDown, unsigned int uiPrevRawDown, bool bBlockNew,
                               unsigned int& uiLatched)
{
    const unsigned int uiNew = uiRawDown & ~uiPrevRawDown;
    if (bBlockNew)
    {
        uiLatched |= uiNew;
    }
    uiLatched &= uiRawDown;
    return uiRawDown & ~uiLatched;
}

static unsigned int CursorAxis(unsigned int uiTex, unsigned int uiBack, unsigned int uiClient)
{
    const unsigned long long ullV =
        ((unsigned long long)uiTex * uiClient + uiBack / 2) / uiBack;
    if (ullV < 1)
    {
        return 1;
    }
    if (ullV > kCursorMaxDim)
    {
        return kCursorMaxDim;
    }
    return (unsigned int)ullV;
}

void CursorSize(unsigned int uiTexW, unsigned int uiTexH, unsigned int uiBackW, unsigned int uiBackH,
                unsigned int uiClientW, unsigned int uiClientH, unsigned int& uiW, unsigned int& uiH)
{
    if (uiTexW == 0 || uiTexH == 0 || uiBackW == 0 || uiBackH == 0 || uiClientW == 0 ||
        uiClientH == 0)
    {
        uiW = CursorAxis(uiTexW, 1, 1);
        uiH = CursorAxis(uiTexH, 1, 1);
        return;
    }
    uiW = CursorAxis(uiTexW, uiBackW, uiClientW);
    uiH = CursorAxis(uiTexH, uiBackH, uiClientH);
}

static void CursorFilterAxis(unsigned int uiD, unsigned int uiSrc, unsigned int uiDst,
                             unsigned int& uiI0, unsigned int& uiI1, unsigned int& uiF)
{
    long long llPos =
        (long long)(((2ULL * uiD + 1) * uiSrc * 65536ULL) / (2ULL * uiDst)) - 32768;
    const long long llMax = (long long)(uiSrc - 1) * 65536;
    if (llPos < 0)
    {
        llPos = 0;
    }
    if (llPos > llMax)
    {
        llPos = llMax;
    }
    uiI0 = (unsigned int)(llPos >> 16);
    uiF = (unsigned int)(llPos & 0xFFFF);
    uiI1 = (uiI0 + 1 < uiSrc) ? uiI0 + 1 : uiSrc - 1;
}

static void CursorTap(unsigned int uiPx, unsigned long long ullW, unsigned long long* pullAcc)
{
    const unsigned long long ullA = ullW * (uiPx >> 24);
    pullAcc[0] += ullA;
    pullAcc[1] += ullA * ((uiPx >> 16) & 0xFF);
    pullAcc[2] += ullA * ((uiPx >> 8) & 0xFF);
    pullAcc[3] += ullA * (uiPx & 0xFF);
}

static unsigned int CursorStraight(unsigned long long ullP, unsigned long long ullA)
{
    const unsigned long long ullC = (ullP + ullA / 2) / ullA;
    return (ullC > 255) ? 255u : (unsigned int)ullC;
}

void CursorScaleBgra(const unsigned int* puiSrc, unsigned int uiSrcW, unsigned int uiSrcH,
                     unsigned int* puiDst, unsigned int uiDstW, unsigned int uiDstH)
{
    if (puiSrc == 0 || puiDst == 0 || uiSrcW == 0 || uiSrcH == 0 || uiDstW == 0 || uiDstH == 0)
    {
        return;
    }
    for (unsigned int y = 0; y < uiDstH; ++y)
    {
        unsigned int uiY0, uiY1, uiFy;
        CursorFilterAxis(y, uiSrcH, uiDstH, uiY0, uiY1, uiFy);
        const unsigned int* puiRow0 = puiSrc + uiY0 * uiSrcW;
        const unsigned int* puiRow1 = puiSrc + uiY1 * uiSrcW;
        const unsigned long long ullWy0 = 65536 - uiFy;
        const unsigned long long ullWy1 = uiFy;
        for (unsigned int x = 0; x < uiDstW; ++x)
        {
            unsigned int uiX0, uiX1, uiFx;
            CursorFilterAxis(x, uiSrcW, uiDstW, uiX0, uiX1, uiFx);
            const unsigned long long ullWx0 = 65536 - uiFx;
            const unsigned long long ullWx1 = uiFx;
            unsigned long long aullAcc[4] = {0, 0, 0, 0};
            CursorTap(puiRow0[uiX0], ullWy0 * ullWx0, aullAcc);
            CursorTap(puiRow0[uiX1], ullWy0 * ullWx1, aullAcc);
            CursorTap(puiRow1[uiX0], ullWy1 * ullWx0, aullAcc);
            CursorTap(puiRow1[uiX1], ullWy1 * ullWx1, aullAcc);
            const unsigned int uiA = (unsigned int)((aullAcc[0] + 0x80000000ULL) >> 32);
            if (uiA == 0)
            {
                puiDst[y * uiDstW + x] = 0;
                continue;
            }
            puiDst[y * uiDstW + x] = (uiA << 24) | (CursorStraight(aullAcc[1], aullAcc[0]) << 16) |
                                     (CursorStraight(aullAcc[2], aullAcc[0]) << 8) |
                                     CursorStraight(aullAcc[3], aullAcc[0]);
        }
    }
}

unsigned int CursorMaskStride(unsigned int uiW)
{
    return ((uiW + 31) / 32) * 4;
}

void CursorMaskFromBgra(const unsigned int* puiBgra, unsigned int uiW, unsigned int uiH,
                        unsigned char* pucMask)
{
    if (puiBgra == 0 || pucMask == 0)
    {
        return;
    }
    const unsigned int uiStride = CursorMaskStride(uiW);
    memset(pucMask, 0, uiStride * uiH);
    for (unsigned int y = 0; y < uiH; ++y)
    {
        for (unsigned int x = 0; x < uiW; ++x)
        {
            if ((puiBgra[y * uiW + x] >> 24) == 0)
            {
                pucMask[y * uiStride + x / 8] |= (unsigned char)(0x80 >> (x & 7));
            }
        }
    }
}

float UiScale(unsigned int uiScreenH)
{
    if (uiScreenH <= 1080 || uiScreenH > kMaxDim)
    {
        return 1.0f;
    }
    return (float)uiScreenH / 1080.0f;
}

void UiSize(unsigned int uiScreenW, unsigned int uiScreenH, unsigned int& uiW, unsigned int& uiH)
{
    const float fScale = UiScale(uiScreenH);
    uiW = (unsigned int)((float)uiScreenW / fScale + 0.5f);
    uiH = (unsigned int)((float)uiScreenH / fScale + 0.5f);
}

float HudAnchor(float fPos, float fExtent)
{
    if (fPos < fExtent / 3.0f)
    {
        return 0.0f;
    }
    if (fPos > fExtent * 2.0f / 3.0f)
    {
        return fExtent;
    }
    return fExtent * 0.5f;
}

float HudScalePos(float fPos, float fAnchor, float fScale)
{
    return fAnchor + (fPos - fAnchor) * fScale;
}

bool HudIsCentral(float fX, float fY, float fW, float fH)
{
    return HudAnchor(fX, fW) == fW * 0.5f && HudAnchor(fY, fH) == fH * 0.5f;
}

float FovTanScale(unsigned int uiDegrees)
{
    if (uiDegrees == kFovDefault)
    {
        return 1.0f;
    }
    if (uiDegrees < kFovMin)
    {
        uiDegrees = kFovMin;
    }
    else if (uiDegrees > kFovMax)
    {
        uiDegrees = kFovMax;
    }
    const double kHalfDegToRad = 3.14159265358979323846 / 360.0;
    return (float)(tan((double)uiDegrees * kHalfDegToRad) / tan((double)kFovDefault * kHalfDegToRad));
}

float ScaleFov(float fFov, float fTanScale)
{
    return (float)atan((double)fTanScale * tan((double)fFov * 0.5)) * 2.0f;
}

}
