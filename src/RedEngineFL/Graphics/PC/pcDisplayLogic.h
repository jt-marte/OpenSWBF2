#pragma once

namespace DisplayLogic
{

struct DisplayModeRec
{
    unsigned int w;
    unsigned int h;
    unsigned int refresh;
    unsigned int adapter;
};

struct AdapterRec
{
    unsigned int ordinal;
    const char* name;
};

const unsigned int kFallbackWidth = 800;
const unsigned int kFallbackHeight = 600;
const unsigned int kPrimaryAdapter = 0;
const unsigned int kIntervalDefault = 0;
const unsigned int kIntervalTwo = 2;
const unsigned int kIntervalImmediate = 0x80000000u;

unsigned int BuildModeList(const DisplayModeRec* pSrc, unsigned int uiSrc, unsigned int uiAdapter,
                           DisplayModeRec* pOut, unsigned int uiOutCap);

unsigned int UniqueResolutions(const DisplayModeRec* pList, unsigned int uiCount, unsigned int uiMinW,
                               unsigned int uiMinH, DisplayModeRec* pOut, unsigned int uiOutCap);

void DesktopDefault(const DisplayModeRec* pList, unsigned int uiCount, unsigned int uiDeskW,
                    unsigned int uiDeskH, unsigned int& uiW, unsigned int& uiH);

unsigned int PickRefresh(const DisplayModeRec* pList, unsigned int uiCount, unsigned int uiW,
                         unsigned int uiH, unsigned int uiRequested, bool bWindowed,
                         unsigned int uiDesktopHz);

unsigned int ResolveAdapter(unsigned int uiReqOrdinal, bool bOrdinalFromCli, const char* pcReqName,
                            const AdapterRec* pAdapters, unsigned int uiCount, bool bNoRender);

bool UsableUncached(bool bInList, bool bIsDesktop, bool bMsNone, unsigned int uiMsLevels,
                    unsigned int uiQuality);

unsigned int EffectiveVsync(bool bKeySet, unsigned int uiKeyValue, bool bProfile);

unsigned int ResolveVsyncInterval(unsigned int uiRequested, unsigned long ulCaps, bool bWindowed,
                                  bool* pbFellBack);

enum ResetAction
{
    kResetDone = 0,
    kResetWait = 1,
    kResetNow = 2,
    kResetRevert = 3,
    kResetFatal = 4
};

struct ResetBounds
{
    unsigned int uiMaxFailures;
    unsigned int uiMaxMs;
};

extern const ResetBounds kStartupBounds;
extern const ResetBounds kRuntimeBounds;

ResetAction CoopStep(long hrCoop, unsigned int uiElapsedMs, const ResetBounds& bounds);

ResetAction ResetStep(long hr, unsigned int uiFailures, unsigned int uiElapsedMs,
                      bool bRevertAvailable, const ResetBounds& bounds);

enum DisplayMode
{
    kModeFullscreen = 0,
    kModeWindowed = 1,
    kModeBorderless = 2
};

struct Rect
{
    long l, t, r, b;
};

struct Frame
{
    long l, t, r, b;
};

struct WinStyle
{
    unsigned long style;
    unsigned long exStyle;
};

DisplayMode ResolveMode(bool bBorderless, bool bCliWin, bool bProfileWindowed);

bool D3DWindowed(DisplayMode m);
bool MinimizeOnDeactivate(DisplayMode m);
bool Topmost(DisplayMode m);

WinStyle WindowStyleFor(DisplayMode m, bool bVisible);

Rect PlaceWindow(DisplayMode m, const Rect& monitor, const Rect& work, unsigned long clientW,
                 unsigned long clientH, const Frame& frame);

Rect DpiChangedRect(const Rect& suggested, unsigned long clientW, unsigned long clientH,
                    const Frame& frame);

enum DpiResult
{
    kDpiFail = 0,
    kDpiOk = 1,
    kDpiAlready = 2
};

enum DpiLevel
{
    kDpiLevelNone = 0,
    kDpiLevelPmV2 = 1,
    kDpiLevelPm = 2,
    kDpiLevelSystem = 3,
    kDpiLevelOff = 4
};

typedef int (*DpiStepFn)(void* pCtx);

struct DpiSteps
{
    DpiStepFn pmv2;
    DpiStepFn pm;
    DpiStepFn system;
    void* pCtx;
};

DpiLevel DpiCascade(const DpiSteps& steps, bool bNoDpi);

const char* DpiLevelName(DpiLevel l);

DpiLevel PresetDpiLevel(int iAwareness, bool bPmV2, DpiLevel fallback);

void MapBackBufferToClient(int x, int y, unsigned int bbW, unsigned int bbH, unsigned int cW,
                           unsigned int cH, int& ox, int& oy);

void MapClientToBackBuffer(int x, int y, unsigned int cW, unsigned int cH, unsigned int bbW,
                           unsigned int bbH, int& ox, int& oy);

struct ShellSize
{
    unsigned int uiBackW, uiBackH, uiClientW, uiClientH;
    bool bClearMsaa;
    bool bWarnIgnoredSize;
    bool bWarnNoMonitor;
};

ShellSize ShellSizePolicy(DisplayMode m, bool bIsShell, unsigned int uiCliW, unsigned int uiCliH,
                          unsigned int uiProfW, unsigned int uiProfH, unsigned int uiDeskW,
                          unsigned int uiDeskH);

// OpenSWBF2: the shell's back buffer in a window or borderless. Stock used
// 800x600 and stretched it to the window (blurry at 4K); now the biggest 4:3
// that fits the window (2880x2160 in a 3840x2160 one), so the stretch is the
// same 4:3-to-window look but drawn sharp. 0x0 (the stock 800x600 mode) for
// windows 600 lines or shorter.
void ShellBackSize(unsigned int uiClientW, unsigned int uiClientH, unsigned int& uiBackW,
                   unsigned int& uiBackH);

// The shell's menus are laid out in 800x600 pixels, so in the shell the UI
// scale (see UiScale) is the screen height over 600 and Lua keeps seeing
// 800x600 on a 4:3 back buffer of any size. 1.0 at 600 lines and below.
float ShellUiScale(unsigned int uiScreenH);

struct ShellStretch
{
    bool bActive;
    bool bLinear;
};

ShellStretch ShellStretchPlan(DisplayMode m, bool bIsShell, unsigned int uiBackW,
                              unsigned int uiBackH, unsigned int uiClientW, unsigned int uiClientH,
                              bool bLinearCaps);

struct DisplayRec
{
    unsigned int uiW, uiH;
    bool bWindowed;
    unsigned int uiMsType, uiMsQuality;
    unsigned int uiInterval;
    unsigned int uiRefresh;
    DisplayMode eMode;
    unsigned int uiClientW, uiClientH;
};

bool SameDisplayParams(const DisplayRec& a, const DisplayRec& b);

struct LegacyFlags
{
    bool bWindowed;
    bool bVsync;
};

LegacyFlags LegacyFlagsFor(DisplayMode eMode, unsigned int uiVsync);

unsigned int FrameCapList(unsigned int uiCurrent, unsigned int* pOut, unsigned int uiOutCap);

unsigned int RefreshRates(const DisplayModeRec* pList, unsigned int uiCount, unsigned int uiW,
                          unsigned int uiH, unsigned int* pOut, unsigned int uiOutCap);

unsigned int DisplayNumberFromDeviceName(const char* pcDevice);

bool MonitorLabel(unsigned int uiAdapter, unsigned int uiDisplay, unsigned int uiW,
                  unsigned int uiH, char* pcOut, unsigned int uiSize);

struct MultisampleEntry
{
    unsigned int uiType;
    unsigned int uiQuality;
};

const unsigned int kMultisampleTypes = 17;

unsigned int MultisampleList(const unsigned int* auiLevels, MultisampleEntry* pOut,
                             unsigned int uiOutCap);

bool MultisampleLabel(unsigned int uiType, unsigned int uiQuality, char* pcOut,
                      unsigned int uiSize);

void FakeMultisampleLevels(unsigned int* auiLevels);

unsigned int EffectiveFpsCap(unsigned int uiFps, bool bPresentThrottled);

struct PresentWatch
{
    unsigned int uiFrames;
    double dSum;
    bool bOverridden;
    bool bThrottled;
};

const unsigned int kPresentWatchFrames = 60;

void PresentWatchReset(PresentWatch& w);

void PresentWatchStep(PresentWatch& w, bool bVsyncInterval, bool bPresentOk, bool bVisible,
                      double dFrameSeconds, unsigned int uiRefreshHz);

bool PresentThrottledNow(const PresentWatch& w, bool bVisible, double dSecondsSincePresent);

bool ConfirmNeeded(const DisplayRec& kCur, const DisplayRec& kNext);

bool MergeStageArm(bool bArmSoFar, bool bTagValid, bool bTagArm, bool bSet);

bool StagedConfirm(bool bArm, bool bTagValid, bool bSet, bool bNeeded);

struct BufferSize
{
    unsigned int uiW, uiH;
};

struct BufferSet
{
    BufferSize akBuf[3];
};

BufferSet BufferSizes(unsigned int uiBackW, unsigned int uiBackH, unsigned int uiMaxTexW,
                      unsigned int uiMaxTexH, bool bNonPow2);

bool ShadowQuadSharesSky(unsigned int uiBackW, unsigned int uiBackH, unsigned int uiMsType,
                         bool bDriverMs, unsigned int uiSkyW, unsigned int uiSkyH);

enum ApplyPhase
{
    kApplyNone = 0,
    kApplyResetting = 1,
    kApplyAwaitPresent = 2
};

enum ApplyEvent
{
    kEvResetOk = 0,
    kEvPresentOk = 1,
    kEvPresentLost = 2,
    kEvPresentFail = 3
};

enum ApplyAction
{
    kActNone = 0,
    kActPromote = 1,
    kActRevert = 2
};

struct ApplyStep
{
    ApplyPhase ePhase;
    ApplyAction eAction;
};

ApplyStep ApplyPhaseStep(ApplyPhase ePhase, ApplyEvent eEvent);

ApplyEvent PresentEvent(long hr);

struct AdapterChoice
{
    bool bValid;
    bool bRestart;
};

AdapterChoice AdapterDecision(unsigned int uiRequested, unsigned int uiRunning,
                              unsigned int uiCount);

bool ShouldRestoreProfile(bool bSnapshotValid, unsigned int uiOwner, unsigned int uiCurrent,
                          bool bRequestPending);

const unsigned int kMinW = 320;
const unsigned int kMinH = 240;
const unsigned int kMaxDim = 16384;
const unsigned int kConfirmMs = 15000;

enum ConfirmPhase
{
    kConfirmNone = 0,
    kConfirmAwaitPresent = 1,
    kConfirmCounting = 2
};

enum ConfirmEvent
{
    kConfirmEvArm = 0,
    kConfirmEvApplied = 1,
    kConfirmEvAppliedPlain = 2,
    kConfirmEvApplyFailed = 3,
    kConfirmEvKeep = 4,
    kConfirmEvRevert = 5,
    kConfirmEvTick = 6
};

enum ConfirmAction
{
    kConfirmActNone = 0,
    kConfirmActStart = 1,
    kConfirmActKeep = 2,
    kConfirmActRevert = 3,
    kConfirmActClear = 4
};

struct ConfirmStep
{
    ConfirmPhase ePhase;
    ConfirmAction eAction;
};

ConfirmStep ConfirmPhaseStep(ConfirmPhase ePhase, ConfirmEvent eEvent, unsigned int uiElapsedMs);

bool SizeInBounds(unsigned int uiW, unsigned int uiH);

bool SizeFitsCaps(unsigned int uiW, unsigned int uiH, unsigned int uiMaxTexW,
                  unsigned int uiMaxTexH);

enum GammaPath
{
    kGammaUndecided = 0,
    kGammaPass = 1,
    kGammaRamp = 2
};

enum GammaReason
{
    kGammaReasonNone = 0,
    kGammaReasonNoProg2 = 1,
    kGammaReasonNoRender = 2
};

void RampToLut(const unsigned short awRamp[256], unsigned char abLut[256]);

bool LutIsIdentity(const unsigned char abLut[256]);

GammaReason GammaPathFor(bool bNoRender, bool bTestKind, bool bProg2);

bool GammaPassRuns(GammaPath ePath, bool bIdentity, bool bResourcesReleased, bool bHaveTarget);

struct CursorPlan
{
    bool bHcursor;
    bool bExclusive;
    bool bClip;
    bool bHideDrawn;
};

CursorPlan CursorPlanFor(bool bShown, DisplayMode eMode, bool bActive, bool bSoftware, bool bNoRender);

unsigned int CursorButtonLatch(unsigned int uiRawDown, unsigned int uiPrevRawDown, bool bBlockNew,
                               unsigned int& uiLatched);

const unsigned int kCursorMaxDim = 256;

void CursorSize(unsigned int uiTexW, unsigned int uiTexH, unsigned int uiBackW, unsigned int uiBackH,
                unsigned int uiClientW, unsigned int uiClientH, unsigned int& uiW, unsigned int& uiH);

void CursorScaleBgra(const unsigned int* puiSrc, unsigned int uiSrcW, unsigned int uiSrcH,
                     unsigned int* puiDst, unsigned int uiDstW, unsigned int uiDstH);

unsigned int CursorMaskStride(unsigned int uiW);

void CursorMaskFromBgra(const unsigned int* puiBgra, unsigned int uiW, unsigned int uiH,
                        unsigned char* pucMask);

// Pixel-sized interface (bitmap-font HUD text, Lua menus) grows with the screen
// height past 1080 lines (2x at 2160) so it reads as it does at 1080p.
// 1.0 at 1080 and below.
float UiScale(unsigned int uiScreenH);

// The screen size Lua menus lay out in: the real size divided by UiScale,
// rounded (1920x1080 at 3840x2160 and at 2560x1440).
void UiSize(unsigned int uiScreenW, unsigned int uiScreenH, unsigned int& uiW, unsigned int& uiH);

}
