#pragma once

namespace DisplayCfg
{

const unsigned int kBorderless = 1;
const unsigned int kAdapter = 2;
const unsigned int kRefresh = 4;
const unsigned int kVsync = 8;
const unsigned int kFps = 16;
const unsigned int kNoWaitQuery = 32;
const unsigned int kNoDpi = 64;
const unsigned int kDisplayIni = 128;
const unsigned int kAdapterName = 256;
const unsigned int kSoftwareCursor = 512;
const unsigned int kPadLayout = 1024; // OpenSWBF2: controller layout, 0 modern / 1 legacy
const unsigned int kPadLookSpeed = 2048; // OpenSWBF2: controller sensitivity level, 1..10 (default 4)
const unsigned int kHudScale = 4096; // OpenSWBF2: HUD scale, percent 50..150 (default 100)
const unsigned int kFov = 8192; // OpenSWBF2: field of view, degrees at 16:9 55..100 (default 61)
const unsigned int kCrispFonts = 16384; // OpenSWBF2: redraw the font atlases sharp above 1080p (default on)
const unsigned int kRenderer = 32768; // OpenSWBF2: renderer, vulkan (default) or d3d9, read at startup

const unsigned int kAdapterMax = 15;
const unsigned int kRefreshMax = 1000;
const unsigned int kFpsMax = 1000;
const unsigned int kVsyncMax = 2;
const unsigned int kModeMax = 2;

const unsigned int kMenuKeys = kBorderless | kRefresh | kVsync | kFps | kNoWaitQuery;

enum SaveResult
{
    kSaveOk = 0,
    kSaveDeferred = 1,
    kSaveFailed = 2
};

const unsigned int kHashBorderless = 0x49DA825A;
const unsigned int kHashAdapter = 0xE7CAE794;
const unsigned int kHashRefresh = 0xD4F26634;
const unsigned int kHashVsync = 0x06F8F066;
const unsigned int kHashFps = 0xBDD34AA8;
const unsigned int kHashNoWaitQuery = 0xFF222165;
const unsigned int kHashNoDpi = 0x5B1931B5;
const unsigned int kHashDisplayIni = 0x8CDD8845;
const unsigned int kHashSwCursor = 0x21567C6D;
const unsigned int kHashVulkan = 0xFC99B1F2;
const unsigned int kHashD3d9 = 0x3E21D70F;

struct DisplaySettings
{
    bool bBorderless;
    unsigned int uiAdapter;
    char acAdapterName[64];
    unsigned int uiRefresh;
    unsigned int uiVsync;
    unsigned int uiFps;
    bool bNoWaitQuery;
    bool bNoDpi;
    bool bSoftwareCursor;
    unsigned int uiPadLayout;
    unsigned int uiPadLookSpeed;
    unsigned int uiHudScale;
    unsigned int uiFov;
    bool bCrispFonts;
    unsigned int uiRenderer;
    char acDisplayIni[260];
    unsigned int uiSet;
    unsigned int uiCliMask;
};

struct MenuFields
{
    unsigned int uiMode;
    unsigned int uiRefresh;
    unsigned int uiVsync;
    unsigned int uiFps;
    bool bWaitQuery;
};

struct PendingAdapter
{
    bool bSet;
    unsigned int uiIndex;
    char acName[64];
};

void SetDefaults(DisplaySettings& kSettings);
bool ParseUInt(const char* pcText, unsigned int uiLo, unsigned int uiHi, unsigned int& uiOut);
bool ParseBool(const char* pcText, bool& bOut);
bool ParseVsync(const char* pcText, unsigned int& uiOut);
bool SetFromText(DisplaySettings& kSettings, unsigned int uiKey, const char* pcValue, const char* pcSource);
void SetFlag(DisplaySettings& kSettings, unsigned int uiKey);
void Merge(DisplaySettings& dst, const DisplaySettings& cli);
void LoadIni(DisplaySettings& kSettings, const char* pcPath);
bool SaveIni(const DisplaySettings& kSettings, const char* pcPath, bool bGoodFrame);
bool DisplayIniPath(char* pcPath, unsigned int uiSize);
bool SaveAdapter(unsigned int uiIndex, const char* pcName);
bool SavePadLayout(unsigned int uiLayout);
bool SavePadLookSpeed(unsigned int uiLevel);
bool SaveHudScale(unsigned int uiPercent);
bool SaveFov(unsigned int uiDegrees);
bool SaveRenderer(unsigned int uiRenderer);
void SetPendingAdapter(unsigned int uiIndex, const char* pcName);
void ClearPendingAdapter();
const PendingAdapter& GetPendingAdapter();
bool ValidateMenu(const MenuFields& kFields);
SaveResult SaveMenu(const DisplaySettings& kMark, const DisplaySettings& kNow, const char* pcPath, bool bGoodFrame, unsigned int& uiWritten);
void Init();

extern DisplaySettings gCliDisplay;
extern DisplaySettings gIniDisplay;
extern DisplaySettings gDisplay;

}
