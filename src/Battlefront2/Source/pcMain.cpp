#include <windows.h>
#include <math.h>

#include <d3d9.h>
#include <process.h>
#include <stdlib.h>
#include <string.h>

#include "PebbleFL/Common/PblHashTable.h"
#include "RedEngineFL/Application/pc/pcFrameLimiter.h"
#include "RedEngineFL/Application/pc/pcMessageDrain.h"
#include "RedEngineFL/Application/pc/pcWindow.h"
#include "RedEngineFL/Graphics/PC/pcDisplayLogic.h"
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"
#include "RedEngineFL/input/pc/pcXInputPad.h"
#include "RedEngineFL/Graphics/PC/pcRedAutoSettings.h"
#include "RedEngineFL/Graphics/PC/pcRedRenderer.h"
#include "RedEngineFL/Graphics/PC/pcRedTexture.h"
#include "RedEngineFL/Memory/RedMemory.h"
#include "RedEngineFL/Util/RedLog.h"
#include "RedEngineFL/Util/RedWarning.h"
#include "SoundFL/source/sndbus.h"
#include "SoundFL/source/win/sndengine.h"
#include "SoundFL/source/win/snddsbuffer.h"
#include "SoundFL/source/sndfile.h"
#include "SoundFL/source/sndglobals.h"
#include "SoundFL/source/sndsource3d.h"
#include "Steam/gfSteamAuth.h"
#include "Steam/galaxy_api.h"
#include "PebbleFL/Common/PblUnicode.h"
#include "NetWire.h"
#include "CommandLine.h"
#include "pcCrash.h"
#include "GameLoop.h"
#include "GameMemory.h"
#include "GameSoundEngine.h"
#include "GameState.h"
#include "LoadDisplay.h"
#include "LoadUtil.h"
#include "PauseMenu.h"
#include "RedEngineFL/input/pc/pcRedInputManager.h"
#include "NetSetup.h"
#include "PlayerProfile.h"

void __fastcall ParseAutoNetCommandLine(char* pCmdLine);
void InitializeMainCameraState();

void* DebugSystemAlloc(unsigned int uiSize);
void DebugSystemFree(void* pBlock);
long __stdcall LowLevelKeyboardProc(int nCode, unsigned int wParam, long lParam);

void CloseJournal();

extern bool sPauseMenuSoundEnabled;

char appName[260] = "Star Wars Battlefront II";

bool gAppActive = true;

void* s_memoryArena;
void* g_hMutex;

// GLOBAL: SWBF2 0x01eaedd4
bool gAltMeleeHitEffects;

bool pcForceFixedFunction;
bool pcWindowed;

static const UINT kWmDpiChanged = 0x02E0;
HWND hGlobalWnd;
bool pcPostInstall;
int gExitCode = 0;

// GLOBAL: SWBF2 0x01eaede5
bool gIsQuitting;
bool pcEnableLogging;
bool gbMutexValid;
HINSTANCE hGlobalInstance;
int PCDebugHeap;

int pcHeight;
int pcWidth;

// FUNCTION: SWBF2 0x006170f0
bool ToggleAltMeleeHitEffectsCheat()
{
    gAltMeleeHitEffects = !gAltMeleeHitEffects;
    return gAltMeleeHitEffects;
}

// FUNCTION: SWBF2 0x00617100
bool ToggleSoundEnabled()
{
    sPauseMenuSoundEnabled = !sPauseMenuSoundEnabled;
    return !sPauseMenuSoundEnabled;
}

// FUNCTION: SWBF2 0x00617120
bool ToggleSoundPitch()
{

    if (Snd::Source3D::sPitchMultiplier == 1.0f)
    {
        Snd::Source3D::sPitchMultiplier = 0.3f;
        return true;
    }
    Snd::Source3D::sPitchMultiplier = 1.0f;
    return false;
}

// FUNCTION: SWBF2 0x00617150
static bool AcquireMutex()
{
    g_hMutex = CreateMutexA(0, FALSE, "PlayBF");
    if (g_hMutex != 0 && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        gbMutexValid = true;
        return true;
    }

    CloseHandle(g_hMutex);
    return false;
}

// FUNCTION: SWBF2 0x00617190
static void ReleaseMutex()
{
    if (gbMutexValid)
    {
        CloseHandle(g_hMutex);
    }
    gbMutexValid = false;
}

// FUNCTION: SWBF2 0x006171b0
void Startup_InitApplicationMemory()
{
    pcCrash::ReserveLowAddressSpace();
    s_memoryArena = malloc(GameMemory::kArenaSize);
    if (!s_memoryArena || pcCrash::SimulateAllocFailure("arenaoom"))
    {
        pcCrash::FatalInit("arena", GameMemory::kArenaSize);
    }

    GameMemory::InitApplicationMemory((char*)s_memoryArena,
                                      (char*)s_memoryArena + GameMemory::kArenaSize);
}

// FUNCTION: SWBF2 0x006171d0
void Startup_DebugHeap()
{
    void* p = malloc(0x300000);
    if (!p || pcCrash::SimulateAllocFailure("debugheapoom"))
    {
        pcCrash::FatalInit("debug heap", 0x300000);
    }
    PCDebugHeap = RedCreateHeap(p, (char*)p + 0x300000, 0x10, "Debug");
    RedSetDebugHeap(PCDebugHeap);
}

// FUNCTION: SWBF2 0x00617200

extern "C" void Startup_RedInitHeap()
{
    Startup_InitApplicationMemory();
    Startup_DebugHeap();
}

// FUNCTION: SWBF2 0x00617280
static void SoundEnable(bool bEnable)
{

    if (!Snd::EngineBase::smOpened)
    {
        return;
    }

    if (bEnable)
    {
        PlayerProfile::tProfile* pProfile = PlayerProfile::GetProfile(PauseMenu::GetPausingJoystick());
        if (pProfile)
        {

            unsigned char ucGain = pProfile->mSoundOptions.mu8MasterVol;
            Snd::Bus::smRootBus.SetGain((float)ucGain / 10.0f, true);
        }
        else
        {
            Snd::Bus::smRootBus.SetGain(1.0f, true);
        }

        Snd::DSBuffer::ResetBuffers();
    }
    else
    {
        Snd::Bus::smRootBus.SetGain(0.0f, true);
    }
}

static bool sCloseDeferred;
extern char autoNetSession[64];
extern int hostshort;
namespace GameLoop { extern bool sQuitRequested; }
extern int gAutoNetPlayers;
extern int netFPS;

// True when pcLine holds pcWord as its own -word or /word token.
static bool HasSwitchWord(const char* pcLine, const char* pcWord)
{
    const size_t uLen = strlen(pcWord);
    for (const char* pc = pcLine; (pc = strpbrk(pc, "-/")) != 0; ++pc)
    {
        const bool bStart = pc == pcLine || pc[-1] == ' ' || pc[-1] == '\t';
        if (bStart && !_strnicmp(pc + 1, pcWord, uLen) && (pc[1 + uLen] == 0 || pc[1 + uLen] == ' ' || pc[1 + uLen] == '\t'))
        {
            return true;
        }
    }
    return false;
}

// Dedicated servers: Ctrl+C, Ctrl+Break and closing the console shut down the way the
// window's close button does.
static BOOL WINAPI DedicatedConsoleHandler(DWORD dwType)
{
    if (dwType != CTRL_C_EVENT && dwType != CTRL_BREAK_EVENT && dwType != CTRL_CLOSE_EVENT &&
        dwType != CTRL_SHUTDOWN_EVENT)
    {
        return FALSE;
    }
    LOG(LogCore, Inform, "OPENSWBF2 SERVER stop requested (console event %u)", (unsigned int)dwType);
    if (hGlobalWnd)
    {
        PostMessageA(hGlobalWnd, WM_CLOSE, 0, 0);
    }
    else
    {
        GameLoop::sQuitRequested = true;
    }
    return TRUE;
}

// FUNCTION: SWBF2 0x006172a0
LRESULT __stdcall WindowProc(HWND__* hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_CLOSE:
        if (gLoading)
        {
            if (!sCloseDeferred)
            {
                sCloseDeferred = true;
                LOG(LogCore, Inform, "OPENSWBF2 CLOSE deferred until loading finishes");
            }
            gIsQuitting = false;
            return 0;
        }
        gIsQuitting = true;

        if (GameSoundEngine::gDefaultSpeakerConfig)
        {
            Snd::Engine::SetSpeakerConfig(GameSoundEngine::gDefaultSpeakerConfig);
        }
        CloseJournal();
        GameLoop::Cleanup();

        GameState::PostStateCleanup(GameState::GetState() != 0x11E1FC01);
        GameState::OneTimeCleanup();
        {

            HMENU hMenu = GetMenu(hGlobalWnd);
            if (hMenu)
            {
                DestroyMenu(hMenu);
            }
        }
        DestroyWindow(hGlobalWnd);
        PostQuitMessage(0);
        hGlobalWnd = 0;
        RedLog::LogEnsureSummary();
        HwCursor::ReleaseClip();
        pcCrash::Exit(gExitCode);

    case WM_ACTIVATE:
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_ACTIVATEAPP:
        gAppActive = wParam != 0;
        return 0;

    case kWmDpiChanged:
        PcWindow::OnDpiChanged(hWnd, wParam, lParam, RedRenderer::GetDisplayMode());
        return 0;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            PcWindow::ApplyPending();
        }
        return DefWindowProcA(hWnd, Msg, wParam, lParam);

    case WM_SETCURSOR:
        if (LOWORD(lParam) != HTCLIENT)
        {
            return DefWindowProcA(hWnd, Msg, wParam, lParam);
        }
        if (!HwCursor::Software())
        {
            SetCursor(HwCursor::Current());
            return 1;
        }
        SetCursor(0);

        if (!g_bNoRender)
        {
            RedRenderer::SetCursorImage(
                RedTexture::s_textureHashTable.Find(0xA2BB0616), 0, 0);
        }
        return 1;

    case WM_KEYDOWN:
        return 0;

    case WM_SYSCOMMAND:
        switch (wParam)
        {
        case SC_MINIMIZE:
            HwCursor::ReleaseClip();
            if (RedInputManager::GetMouse(0))
            {
                if (RedInputManager::GetMouse(0)->Exists())
                {
                    RedInputManager::GetMouse(0)->Remove();
                }
            }
            if (!PauseMenu::GetPaused(0))
            {
                SoundEnable(false);
            }
            return DefWindowProcA(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, lParam);

        case SC_MAXIMIZE:
        case SC_RESTORE:
            if (!PauseMenu::GetPaused(0))
            {
                SoundEnable(true);
            }
            break;

        case SC_CLOSE:
            gIsQuitting = true;
            return DefWindowProcA(hWnd, WM_SYSCOMMAND, SC_CLOSE, lParam);

        case SC_TASKLIST:
        case SC_SCREENSAVE:
        case SC_MONITORPOWER:
            return 0;
        }
        return DefWindowProcA(hWnd, WM_SYSCOMMAND, wParam, lParam);
    }

    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

// FUNCTION: SWBF2 0x006174e0
void ActivateFrontline(bool bActivate)
{
    const bool bMinimize = DisplayLogic::MinimizeOnDeactivate(RedRenderer::GetDisplayMode());
    if (bActivate)
    {
        if (!PauseMenu::GetPaused(0))
        {
            SoundEnable(true);
        }
        SetCursor(HwCursor::Current());
        if (bMinimize && !g_bNoRender)
        {
            ShowWindow(hGlobalWnd, SW_RESTORE);
        }
        return;
    }

    if (gIsQuitting)
    {
        return;
    }

    if (GameState::GetState() != 0x11E1FC01 && PauseMenu::IsPauseAllowed())
    {
        LoadDisplay* pLoadDisplay = LoadDisplay::sInstance;

        if (pLoadDisplay &&
            pLoadDisplay->m_loadState ==
                LoadDisplay::LOAD_STATE_FINISHED)
        {
            PauseMenu::SetPaused(0, true);
            GameLoop::Pause();
        }
    }

    HwCursor::ReleaseClip();
    if (RedInputManager::GetMouse(0))
    {
        if (RedInputManager::GetMouse(0)->Exists())
        {
            RedInputManager::GetMouse(0)->Remove();
        }
    }

    SetCursor(LoadCursorA(hGlobalInstance, IDC_ARROW));
    if (bMinimize && !g_bNoRender)
    {
        ShowWindow(hGlobalWnd, SW_MINIMIZE);
    }
}

static bool DrainPeek(void*, MSG* pMsg)
{
    return PeekMessageA(pMsg, 0, 0, 0, PM_REMOVE) != 0;
}

static void DrainDispatch(void*, const MSG* pMsg)
{
    TranslateMessage(pMsg);
    DispatchMessageA(pMsg);
}

static __int64 DrainNow(void*)
{
    LARGE_INTEGER iNow;
    QueryPerformanceCounter(&iNow);
    return iNow.QuadPart;
}

// FUNCTION: SWBF2 0x006175d0
int __stdcall GameWinMain(HINSTANCE hInstance, HINSTANCE, char* lpCmdLine, int)
{
#ifdef _WIN64
    // The x64 CRT's math library takes FMA3 paths on CPUs that have them, which changes
    // sin/pow/etc. in the last bit; keep one result on every machine (tests/corpus/mathfn/x64).
    _set_FMA3_enable(0);
#endif

    char szKey[64];
    strncpy(szKey, "952F41D039DB4507A84B40488E76932F 0123456789ABCDECAT0123456IPA", 0x3D);

    bool bRestarting = false;
    if (!gf::_sap()->Init(0x17AC, bRestarting) && bRestarting)
    {
        return 0;
    }
    pcCrash::Reinstall();

    atexit(ReleaseMutex);

    // OpenSWBF2: "-dedicated" is a headless Direct server (LAN transport, any address), short
    // for the autonet switches below; maps and other switches follow as usual. Unlike plain
    // /autonet it loads settings\ServerSettings.cfg and ServerRotation.cfg even with a game
    // name given, skips the 20 s startup wait and logs to stdout.
    static char sacExpandedCmdLine[4096];
    if (HasSwitchWord(lpCmdLine, "dedicated") && !strstr(lpCmdLine, "/autonet"))
    {
        _snprintf(sacExpandedCmdLine, sizeof(sacExpandedCmdLine) - 1,
                  "/autonet dedicated -lan -nopregame -playercount 0 -norender -nosound -nowait -nofocuspause -nolegal %s",
                  lpCmdLine);
        lpCmdLine = sacExpandedCmdLine;
        gDedicatedShorthand = true;
    }
    // OpenSWBF2: "-listmissions <file>" runs headless (and so next to a running game) just long
    // enough to load the shell and the mods, writes the mission list, and quits
    const bool bListMissions = HasSwitchWord(lpCmdLine, "listmissions") && !strstr(lpCmdLine, "/autonet");
    if (bListMissions)
    {
        _snprintf(sacExpandedCmdLine, sizeof(sacExpandedCmdLine) - 1,
                  "-norender -nosound -nowait -nofocuspause -nolegal %s", lpCmdLine);
        lpCmdLine = sacExpandedCmdLine;
    }

    bool bDedicated = strstr(lpCmdLine, "/autonet") != 0 &&
                      strstr(lpCmdLine, "dedicated") != 0;
    RedWarning::Init();
    GameLoop::SetNextMission(0);

    g_bNoRender = false;
    g_bNoWait = false;
    g_bNoSound = false;
    g_fFixedRate = 0.0f;
    ParseAutoNetCommandLine(lpCmdLine);

    ParseCommandLine(0, lpCmdLine);
    if (gDedicatedShorthand)
    {
        if (!strcmp(autoNetSession, "LY")) // retail's default name
        {
            strlcpy(autoNetSession, "OpenSWBF2 Server", sizeof(autoNetSession));
        }
        if (!gLogStdout)
        {
            gLogStdout = true;
        }
        if (!gAutoNetPlayers) // no /netplayers: a server's upload cap defaults to the 100M option
        {
            gAutoNetPlayers = NetWire::HostBandwidthSlots(12);
        }
    }
    if (bListMissions)
    {
        gLogStdout = true;
    }
    RedLog::SetStdout(gLogConsole ? RedLog::StdoutConsole
                                  : (gLogStdout ? (gDedicatedShorthand ? RedLog::StdoutAttach : RedLog::StdoutRedirectOnly)
                                                : RedLog::StdoutOff));
    if (bDedicated)
    {
        SetConsoleCtrlHandler(DedicatedConsoleHandler, TRUE);
        LOG(LogCore, Inform, "OPENSWBF2 SERVER dedicated name=\"%s\" port=%d", autoNetSession, hostshort);
    }
    DisplayCfg::Init(pcWindowed);
    if (g_bNoRender && g_fFixedRate <= 0.0f)
    {
        // A headless frame sleeps 1 ms (GameState::UpdateTime), which at Windows' default 15.6 ms
        // timer resolution held headless runs to 64 frames, and so 64 updates, a second.
        timeBeginPeriod(1);
    }
    if (gDedicatedShorthand)
    {
        // Frame cap of twice the turn rate, so updates follow 60 and 120+ turns.
        const unsigned int uiWantFps = netFPS * 2 > 80 ? (unsigned int)netFPS * 2 : 80u;
        if (DisplayCfg::gDisplay.uiFps != 0 && DisplayCfg::gDisplay.uiFps < uiWantFps)
        {
            DisplayCfg::gDisplay.uiFps = uiWantFps;
        }
    }
    XInputPad::gPadLayout =
        DisplayCfg::gDisplay.uiPadLayout ? XInputPad::kLayoutLegacy : XInputPad::kLayoutModern;
    XInputPad::gPadLookLevel = (int)DisplayCfg::gDisplay.uiPadLookSpeed;
    PcWindow::InitDpi(DisplayCfg::gDisplay.bNoDpi);

    if (!bDedicated && !g_bNoRender)
    {
        if (!AcquireMutex())
        {
            return 0;
        }
    }

    if (g_bNoRender || bDedicated)
    {
        SetErrorMode(SEM_FAILCRITICALERRORS);
        RedWarning::SetSuppressDialogs(true);
        pcCrash::StartWatchdog();
    }
    if (gSeedSet)
    {
        srand(gSeed);
    }

    if (!g_bNoRender)
    {
        HWND hRunning = FindWindowA(appName, 0);
        if (hRunning)
        {
            ShowWindow(hRunning, SW_RESTORE);
            return 0;
        }
    }

    RedWarning::SetSeverityToDestination(
        RED_LOG_DESTINATION_CONSOLE | RED_LOG_DESTINATION_TRACE, RedWarning::RED_SEVERITY_NONE);
    if (pcEnableLogging)
    {
        RedWarning::SetLogFileOverwrite(false);
        RedWarning::SetSeverityToDestination(RED_LOG_DESTINATION_FILE,
                                             (RedWarning::RED_ERROR_SEVERITY)RedLog::MinVerbosity());
        RedLog::Apply(pcCrash::GetBuildId());
        LOG(LogCore, Inform,
            "OPENSWBF2 ADDRSPACE arena=0x%p arena_end=0x%p laa=%d steam=%d steam_api=0x%p "
            "Galaxy=0x%p GalaxyPeer=0x%p binkw32=0x%p", // %p is %08X on Win32
            s_memoryArena, (char*)s_memoryArena + GameMemory::kArenaSize,
            pcCrash::IsLargeAddressAware() ? 1 : 0, gf::_sap()->IsInitialized() ? 1 : 0,
            (void*)GetModuleHandleA("steam_api.dll"), (void*)GetModuleHandleA("Galaxy.dll"),
            (void*)GetModuleHandleA("GalaxyPeer.dll"), (void*)GetModuleHandleA("binkw32.dll"));
    }

    const unsigned int uiPaceFps = DisplayCfg::gDisplay.uiFps;
    if (uiPaceFps == 0 || uiPaceFps > 80)
    {
        if (uiPaceFps == 0)
        {
            LOG(LogCore, Warning, "pacing: fps cap is uncapped, frame-count effects are not fixed yet");
        }
        else
        {
            LOG(LogCore, Warning, "pacing: fps cap %u is above 80, frame-count effects are not fixed yet",
                uiPaceFps);
        }
    }

    GameState::SetState(GameLoop::mMissionScript[0] ? 0x8FF60339 : 0x11E1FC01);

    if (g_bNoRender || bDedicated)
    {
        strcat(appName, " Dedicated");
    }

    WNDCLASSA wc;
    wc.style = CS_BYTEALIGNCLIENT | CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(101));
    wc.hCursor = 0;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = appName;
    if (!RegisterClassA(&wc))
    {
        return 0;
    }

    const DisplayLogic::DisplayMode eMode =
        DisplayLogic::ResolveMode(DisplayCfg::gDisplay.bBorderless, pcWindowed, false);
    DisplayLogic::WinStyle ws =
        DisplayLogic::WindowStyleFor(eMode, eMode != DisplayLogic::kModeWindowed && !g_bNoRender);
    if (g_bNoRender)
    {
        ws.style &= ~WS_VISIBLE;
        ws.exStyle = 0;
    }

    const unsigned long ulClientW = pcWidth ? pcWidth : 800;
    const unsigned long ulClientH = pcHeight ? pcHeight : 600;

    DisplayLogic::Rect monitor = {0, 0, 0, 0};
    DisplayLogic::Rect work = {0, 0, 0, 0};
    DisplayLogic::Rect rc;
    if (!g_bNoRender && PcWindow::PrimaryMonitorRects(monitor, work))
    {
        DisplayLogic::Frame frame = PcWindow::MeasureFrame(ws.style, ws.exStyle, 0);
        rc = DisplayLogic::PlaceWindow(eMode, monitor, work, ulClientW, ulClientH, frame);
    }
    else
    {
        RECT rcAdj;
        SetRect(&rcAdj, 0, 0, ulClientW, ulClientH);
        AdjustWindowRect(&rcAdj, ws.style, FALSE);
        rc.l = 0;
        rc.t = 0;
        rc.r = rcAdj.right - rcAdj.left;
        rc.b = rcAdj.bottom - rcAdj.top;
    }

    char szTitle[264];
    strcpy(szTitle, "Star Wars Battlefront II");
    if (strcmp(autoNetSession, "") != 0)
    {
        strcat(szTitle, " : ");
        strcat(szTitle, autoNetSession);
    }

    hGlobalInstance = hInstance;
    hGlobalWnd = CreateWindowExA(ws.exStyle, appName, "Star Wars Battlefront II", ws.style,
                                 rc.l, rc.t, rc.r - rc.l, rc.b - rc.t,
                                 g_bNoRender ? HWND_MESSAGE : 0, 0, hInstance, 0);
    RedRenderer::pcSeedDisplayMode(eMode);

    if (pcEnableLogging)
    {
        LOG(LogCore, Inform, "OPENSWBF2 DPI awareness=%s dpi=%u",
            DisplayLogic::DpiLevelName(PcWindow::GetDpiLevel()), PcWindow::QueryDpi(hGlobalWnd));
    }

    if (pcPostInstall)
    {
        Snd::File::SystemOpen(0x7FFFFFFF);
        Snd::Globals::smAlloc = malloc;
        Snd::Globals::smFree = free;
        IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
        RedVideoConfig::VidMode_Init(pD3D);
        pD3D->Release();
        Snd::File::SystemClose();
        return 0;
    }

    FrameLimiter::Init(g_bNoRender);

    if (!g_bNoRender)
    {
        SetICMMode(GetDC(hGlobalWnd), ICM_ON);
    }

    MSG msg;
    bool bActive = true;
    msg.message = 0;
    PeekMessageA(&msg, 0, 0, 0, PM_NOREMOVE);
    if (msg.message != WM_QUIT)
    {
        MessageDrain::DrainOps ops;
        ops.Peek = DrainPeek;
        ops.Dispatch = DrainDispatch;
        ops.Now = DrainNow;
        ops.pCtx = 0;
        LARGE_INTEGER iFreq;
        QueryPerformanceFrequency(&iFreq);
        const __int64 iDrainTicks = iFreq.QuadPart * MessageDrain::kMaxMsPerPass / 1000;

        for (;;)
        {
            MessageDrain::DrainResult r =
                MessageDrain::DrainMessages(ops, MessageDrain::kMaxMsgsPerPass, iDrainTicks);
            if (r.bQuit)
            {
                break;
            }

            if (bActive != gAppActive && !gNoFocusPause)
            {
                bActive = gAppActive;

                ActivateFrontline(bActive);
            }

            if (!bActive)
            {
                Sleep(10);
            }
            if (!GameState::Update())
            {
                SendMessageA(hGlobalWnd, WM_CLOSE, 0, 0);
            }
            if (sCloseDeferred && !gLoading)
            {
                sCloseDeferred = false;
                PostMessageA(hGlobalWnd, WM_CLOSE, 0, 0);
            }
        }
    }

    InitializeMainCameraState();
    FrameLimiter::Shutdown();
    return 0;
}

// FUNCTION: SWBF2 0x00617ac0
int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE, char* lpCmdLine, int)
{
    pcCrash::Install();
    __try
    {
        return GameWinMain(hInstance, 0, lpCmdLine, 0);
    }
    __except (pcCrash::Filter(GetExceptionInformation()))
    {
        pcCrash::Exit(3);
    }
}

// FUNCTION: SWBF2 0x00617b50
void PumpWaitingMessages()
{
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

// FUNCTION: SWBF2 0x00617bb0
void* GetGalaxyNetworking()
{
    if (!galaxy::api::GalaxyFactory::GetInstance())
    {
        return 0;
    }
    galaxy::api::IGalaxy* pGalaxy = galaxy::api::GalaxyFactory::GetInstance();
    return pGalaxy->Networking();
}

// FUNCTION: SWBF2 0x00617bd0
void* GetGalaxyHostNetworking()
{
    if (!galaxy::api::GalaxyFactory::GetInstance())
    {
        return 0;
    }
    galaxy::api::IGalaxy* pGalaxy = galaxy::api::GalaxyFactory::GetInstance();
    return pGalaxy->ServerNetworking();
}

// FUNCTION: SWBF2 0x006ccc90
void InitializeMainCameraState()
{
}

// GLOBAL: SWBF2 0x01faa8f8

void* sHeapScratchBuffer;

// GLOBAL: SWBF2 0x01faa8fc

bool sHeapScratchAllocated;

// FUNCTION: SWBF2 0x00766ed0
void __cdecl ReleaseHeapScratchBuffer()
{
    if (sHeapScratchAllocated)
    {
        ::operator delete[](sHeapScratchBuffer);
    }
}
