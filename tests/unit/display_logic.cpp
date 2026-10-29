#include "third_party/doctest/doctest.h"

#include <d3d9.h>
#include <string.h>

#include <string>
#include <vector>

#include "testutil.h"
#include "RedEngineFL/Graphics/PC/pcDisplayLogic.h"
#include "Battlefront2/Platform/Pc/pcColorRamp.h"

using namespace DisplayLogic;

namespace
{

DisplayModeRec M(unsigned int w, unsigned int h, unsigned int r, unsigned int a = 0)
{
    DisplayModeRec k = {w, h, r, a};
    return k;
}

}

TEST_CASE("display logic mode list keeps one adapter" * doctest::test_suite("unit"))
{
    DisplayModeRec src[] = {M(1920, 1080, 60, 0), M(1280, 720, 60, 1), M(1920, 1080, 60, 1),
                            M(1920, 1080, 144, 1), M(1920, 1080, 144, 1)};
    DisplayModeRec out[8];
    CHECK(BuildModeList(src, 5, 1, out, 8) == 3);
    CHECK(out[0].w == 1280);
    CHECK(out[1].refresh == 60);
    CHECK(out[2].refresh == 144);
    CHECK(BuildModeList(src, 5, 0, out, 8) == 1);
}

TEST_CASE("display logic mode list collapses duplicates and has no cap" * doctest::test_suite("unit"))
{
    DisplayModeRec src[300];
    for (unsigned int i = 0; i < 200; i++)
    {
        src[i] = M(640 + i, 480 + i, 60);
    }
    DisplayModeRec out[300];
    CHECK(BuildModeList(src, 200, 0, out, 300) == 200);
    CHECK(BuildModeList(src, 200, 0, 0, 0) == 200);
    DisplayModeRec dup[] = {M(800, 600, 60), M(800, 600, 60), M(800, 600, 75)};
    CHECK(BuildModeList(dup, 3, 0, out, 300) == 2);
}

TEST_CASE("display logic mode list respects the output cap" * doctest::test_suite("unit"))
{
    DisplayModeRec src[] = {M(640, 480, 60), M(800, 600, 60), M(1024, 768, 60)};
    DisplayModeRec out[4] = {M(7, 7, 7), M(7, 7, 7), M(7, 7, 7), M(7, 7, 7)};
    CHECK(BuildModeList(src, 3, 0, out, 2) == 3);
    CHECK(out[1].w == 800);
    CHECK(out[2].w == 7);
}

TEST_CASE("display logic unique resolutions" * doctest::test_suite("unit"))
{
    DisplayModeRec src[] = {M(640, 480, 60), M(800, 600, 60), M(800, 600, 75), M(1920, 1080, 60)};
    DisplayModeRec out[4];
    CHECK(UniqueResolutions(src, 4, 800, 600, out, 4) == 2);
    CHECK(out[0].w == 800);
    CHECK(out[1].w == 1920);
}

TEST_CASE("display logic desktop default exact and nearest" * doctest::test_suite("unit"))
{
    unsigned int w = 0, h = 0;
    DisplayModeRec a[] = {M(1280, 1024, 60), M(2560, 1440, 60), M(1920, 1080, 60)};
    DesktopDefault(a, 3, 2560, 1440, w, h);
    CHECK(w == 2560);
    CHECK(h == 1440);
    DisplayModeRec b[] = {M(1280, 1024, 60), M(1920, 1080, 60)};
    DesktopDefault(b, 2, 2560, 1440, w, h);
    CHECK(w == 1920);
    CHECK(h == 1080);
    DisplayModeRec c[] = {M(640, 480, 60)};
    DesktopDefault(c, 1, 3840, 2160, w, h);
    CHECK(w == 640);
    CHECK(h == 480);
}

TEST_CASE("display logic desktop default empty list" * doctest::test_suite("unit"))
{
    unsigned int w = 1, h = 1;
    DesktopDefault(0, 0, 1920, 1080, w, h);
    CHECK(w == 800);
    CHECK(h == 600);
}

TEST_CASE("display logic desktop default zero desktop" * doctest::test_suite("unit"))
{
    unsigned int w = 1, h = 1;
    DisplayModeRec a[] = {M(1920, 1080, 60)};
    DesktopDefault(a, 1, 0, 0, w, h);
    CHECK(w == 800);
    CHECK(h == 600);
}

TEST_CASE("display logic desktop default nothing fits" * doctest::test_suite("unit"))
{
    unsigned int w = 1, h = 1;
    DisplayModeRec a[] = {M(1920, 1080, 60)};
    DesktopDefault(a, 1, 1024, 768, w, h);
    CHECK(w == 800);
    CHECK(h == 600);
}

TEST_CASE("display logic refresh windowed or unrequested" * doctest::test_suite("unit"))
{
    DisplayModeRec a[] = {M(1920, 1080, 60), M(1920, 1080, 144)};
    CHECK(PickRefresh(a, 2, 1920, 1080, 144, true, 0) == 0);
    CHECK(PickRefresh(a, 2, 1920, 1080, 0, false, 0) == 144);
    CHECK(PickRefresh(a, 2, 1920, 1080, 144, false, 0) == 144);
    CHECK(PickRefresh(a, 2, 1280, 720, 60, false, 0) == 0);
}

TEST_CASE("display logic refresh fallback" * doctest::test_suite("unit"))
{
    DisplayModeRec a[] = {M(1920, 1080, 60), M(1920, 1080, 144), M(1280, 720, 240)};
    LogCapture capture;
    CHECK(PickRefresh(a, 3, 1920, 1080, 100, false, 0) == 144);
    CHECK(capture.Contains("display: refresh 100Hz not listed"));
}

TEST_CASE("display logic refresh desktop default" * doctest::test_suite("unit"))
{
    DisplayModeRec a[] = {M(1920, 1080, 60), M(1920, 1080, 144), M(1280, 720, 240)};
    {
        LogCapture capture;
        CHECK(PickRefresh(a, 3, 1920, 1080, 0, true, 144) == 0);
        CHECK(PickRefresh(a, 3, 1920, 1080, 144, true, 0) == 0);
        CHECK(PickRefresh(a, 3, 1920, 1080, 0, false, 60) == 60);
        CHECK(PickRefresh(a, 3, 1920, 1080, 0, false, 144) == 144);
        CHECK(PickRefresh(a, 3, 1920, 1080, 0, false, 75) == 144);
        CHECK(PickRefresh(a, 3, 1920, 1080, 0, false, 0) == 144);
        CHECK(PickRefresh(a, 3, 2560, 1440, 0, false, 60) == 0);
        CHECK(!capture.Contains("not listed"));
    }
    {
        LogCapture capture;
        CHECK(PickRefresh(a, 3, 1920, 1080, 144, false, 60) == 144);
        CHECK(PickRefresh(a, 3, 1920, 1080, 144, false, 0) == 144);
        CHECK(!capture.Contains("not listed"));
    }
    {
        LogCapture capture;
        CHECK(PickRefresh(a, 3, 1920, 1080, 100, false, 60) == 144);
        CHECK(capture.Contains("display: refresh 100Hz not listed"));
    }
}

TEST_CASE("display logic adapter name beats ordinal" * doctest::test_suite("unit"))
{
    AdapterRec ads[] = {{0, "\\\\.\\DISPLAY1"}, {1, "\\\\.\\DISPLAY2"}};
    CHECK(ResolveAdapter(0, false, "\\\\.\\display2", ads, 2, false) == 1);
}

TEST_CASE("display logic adapter fallback" * doctest::test_suite("unit"))
{
    AdapterRec ads[] = {{0, "\\\\.\\DISPLAY1"}, {1, "\\\\.\\DISPLAY2"}};
    {
        LogCapture capture;
        CHECK(ResolveAdapter(1, false, "\\\\.\\DISPLAY9", ads, 2, false) == 1);
        CHECK(capture.Contains("display: adapter name"));
    }
    {
        LogCapture capture;
        CHECK(ResolveAdapter(5, false, "\\\\.\\DISPLAY9", ads, 2, false) == 0);
        CHECK(capture.Contains("not found, using adapter 0"));
    }
    {
        LogCapture capture;
        CHECK(ResolveAdapter(1, true, "\\\\.\\DISPLAY1", ads, 2, false) == 1);
        CHECK(!capture.Contains("display:"));
    }
    {
        LogCapture capture;
        CHECK(ResolveAdapter(5, true, "", ads, 2, false) == 0);
        CHECK(capture.Contains("display: adapter 5"));
    }
}

TEST_CASE("display logic adapter norender is silent" * doctest::test_suite("unit"))
{
    AdapterRec ads[] = {{0, "a"}};
    LogCapture capture;
    CHECK(ResolveAdapter(5, true, "x", ads, 1, true) == 0);
    CHECK(!capture.Contains("display:"));
}

TEST_CASE("display logic usable uncached" * doctest::test_suite("unit"))
{
    CHECK(UsableUncached(true, false, true, 0, 0));
    CHECK(UsableUncached(false, true, true, 0, 0));
    CHECK(UsableUncached(true, false, false, 4, 0));
    CHECK(!UsableUncached(true, false, false, 0, 0));
    CHECK(!UsableUncached(false, false, true, 4, 0));
}

TEST_CASE("display logic effective vsync precedence" * doctest::test_suite("unit"))
{
    CHECK(EffectiveVsync(true, 2, false) == 2);
    CHECK(EffectiveVsync(true, 0, true) == 0);
    CHECK(EffectiveVsync(false, 1, false) == 0);
    CHECK(EffectiveVsync(false, 0, true) == 1);
}

TEST_CASE("display logic vsync off and on" * doctest::test_suite("unit"))
{
    const unsigned long caps[] = {0, 0x1, 0x3, 0x80000001ul, 0x80000003ul};
    for (int w = 0; w < 2; ++w)
    {
        for (unsigned long c : caps)
        {
            bool fb = true;
            bool lacks = c != 0 && !(c & kIntervalImmediate);
            CHECK(ResolveVsyncInterval(0, c, w != 0, &fb) == (lacks ? kIntervalDefault : kIntervalImmediate));
            CHECK(fb == lacks);
            fb = true;
            CHECK(ResolveVsyncInterval(1, c, w != 0, &fb) == kIntervalDefault);
            CHECK(!fb);
        }
    }
}

TEST_CASE("display logic vsync half needs fullscreen and caps" * doctest::test_suite("unit"))
{
    const unsigned long caps[] = {0, 0x1, 0x3, 0x80000001ul, 0x80000003ul};
    for (int w = 0; w < 2; ++w)
    {
        for (unsigned long c : caps)
        {
            bool fb = false;
            bool ok = !w && (c & 2);
            CHECK(ResolveVsyncInterval(2, c, w != 0, &fb) == (ok ? kIntervalTwo : kIntervalDefault));
            CHECK(fb == !ok);
        }
    }
}

TEST_CASE("display logic vsync unknown request and null flag" * doctest::test_suite("unit"))
{
    bool fb = true;
    CHECK(ResolveVsyncInterval(7, 0x80000003ul, false, &fb) == kIntervalDefault);
    CHECK(!fb);
    CHECK(ResolveVsyncInterval(7, 0, true, nullptr) == kIntervalDefault);
    CHECK(ResolveVsyncInterval(0, 0x1, true, nullptr) == kIntervalDefault);
    CHECK(ResolveVsyncInterval(2, 0, true, nullptr) == kIntervalDefault);
    CHECK(ResolveVsyncInterval(2, 3, false, nullptr) == kIntervalTwo);
}

TEST_CASE("reset step maps every code" * doctest::test_suite("unit"))
{
    CHECK(ResetStep(S_OK, 0, 0xFFFFFFFFu, true, kRuntimeBounds) == kResetDone);
    CHECK(ResetStep(S_OK, 99, 0xFFFFFFFFu, false, kStartupBounds) == kResetDone);
    CHECK(ResetStep(0x0876017C, 1, 0, false, kRuntimeBounds) == kResetDone);
    CHECK(ResetStep(S_FALSE, 1, 0, false, kStartupBounds) == kResetDone);
    CHECK(ResetStep(D3DERR_DEVICENOTRESET, 1, 0, true, kRuntimeBounds) == kResetWait);
    CHECK(ResetStep(D3DERR_DEVICENOTRESET, 3, 100, false, kStartupBounds) == kResetWait);
    CHECK(ResetStep(D3DERR_DRIVERINTERNALERROR, 1, 0, true, kRuntimeBounds) == kResetFatal);
    CHECK(ResetStep(E_FAIL, 1, 0, true, kRuntimeBounds) == kResetFatal);
    CHECK(ResetStep(D3DERR_DEVICEREMOVED, 1, 0, true, kStartupBounds) == kResetFatal);
    CHECK(ResetStep(D3DERR_INVALIDCALL, 1, 0, true, kRuntimeBounds) == kResetRevert);
}

TEST_CASE("reset step lost device waits without limit" * doctest::test_suite("unit"))
{
    CHECK(kRuntimeBounds.uiMaxMs == 0);
    CHECK(ResetStep(D3DERR_DEVICELOST, 1, 0xFFFFFFFFu, false, kRuntimeBounds) == kResetWait);
    CHECK(ResetStep(D3DERR_DEVICELOST, 19, 0xFFFFFFFFu, true, kRuntimeBounds) == kResetWait);
    CHECK(CoopStep(D3DERR_DEVICELOST, 0, kRuntimeBounds) == kResetWait);
    CHECK(CoopStep(D3DERR_DEVICELOST, 30000, kRuntimeBounds) == kResetWait);
    CHECK(CoopStep(D3DERR_DEVICELOST, 0xFFFFFFFFu, kRuntimeBounds) == kResetWait);
}

TEST_CASE("coop step maps every code" * doctest::test_suite("unit"))
{
    CHECK(CoopStep(S_OK, 0, kRuntimeBounds) == kResetNow);
    CHECK(CoopStep(S_OK, 0xFFFFFFFFu, kStartupBounds) == kResetNow);
    CHECK(CoopStep(0x08760868, 0, kRuntimeBounds) == kResetNow);
    CHECK(CoopStep(D3DERR_DEVICENOTRESET, 0, kRuntimeBounds) == kResetNow);
    CHECK(CoopStep(D3DERR_DEVICENOTRESET, 0xFFFFFFFFu, kStartupBounds) == kResetNow);
    CHECK(CoopStep(D3DERR_DRIVERINTERNALERROR, 0, kRuntimeBounds) == kResetFatal);
    CHECK(CoopStep(D3DERR_DEVICEREMOVED, 0, kStartupBounds) == kResetFatal);
    CHECK(CoopStep(E_FAIL, 0, kRuntimeBounds) == kResetFatal);
}

TEST_CASE("coop step startup lost device times out without revert" * doctest::test_suite("unit"))
{
    CHECK(CoopStep(D3DERR_DEVICELOST, 29999, kStartupBounds) == kResetWait);
    CHECK(CoopStep(D3DERR_DEVICELOST, 30000, kStartupBounds) == kResetFatal);
    CHECK(CoopStep(D3DERR_DEVICELOST, 0xFFFFFFFFu, kStartupBounds) == kResetFatal);
}

TEST_CASE("reset step startup timeout is fatal" * doctest::test_suite("unit"))
{
    CHECK(kStartupBounds.uiMaxMs == 30000);
    CHECK(ResetStep(D3DERR_DEVICELOST, 5, 29999, false, kStartupBounds) == kResetWait);
    CHECK(ResetStep(D3DERR_DEVICELOST, 5, 30000, false, kStartupBounds) == kResetFatal);
    CHECK(ResetStep(D3DERR_DEVICELOST, 5, 30000, true, kStartupBounds) == kResetRevert);
    CHECK(ResetStep(D3DERR_DEVICELOST, 500, 29999, false, kStartupBounds) == kResetWait);
}

TEST_CASE("reset step runtime failure bound" * doctest::test_suite("unit"))
{
    CHECK(kRuntimeBounds.uiMaxFailures == 20);
    CHECK(ResetStep(D3DERR_DEVICELOST, 19, 0xFFFFFFFFu, false, kRuntimeBounds) == kResetWait);
    CHECK(ResetStep(D3DERR_DEVICELOST, 20, 0, false, kRuntimeBounds) == kResetFatal);
    CHECK(ResetStep(D3DERR_DEVICELOST, 20, 0, true, kRuntimeBounds) == kResetRevert);
    CHECK(ResetStep(D3DERR_DEVICENOTRESET, 19, 0, false, kRuntimeBounds) == kResetWait);
    CHECK(ResetStep(D3DERR_DEVICENOTRESET, 20, 0, false, kRuntimeBounds) == kResetFatal);
    CHECK(ResetStep(D3DERR_DEVICENOTRESET, 20, 0, true, kRuntimeBounds) == kResetRevert);
    CHECK(ResetStep(D3DERR_DEVICENOTRESET, 1, 30000, false, kStartupBounds) == kResetFatal);
}

TEST_CASE("reset step reverts once then fatal" * doctest::test_suite("unit"))
{
    const long codes[] = {D3DERR_INVALIDCALL, D3DERR_NOTAVAILABLE, E_OUTOFMEMORY,
                          D3DERR_OUTOFVIDEOMEMORY};
    for (long hr : codes)
    {
        CHECK(ResetStep(hr, 1, 0, true, kRuntimeBounds) == kResetRevert);
        CHECK(ResetStep(hr, 1, 0, false, kRuntimeBounds) == kResetFatal);
    }
    const long unknown = (long)0x88760999;
    CHECK(ResetStep(unknown, 1, 0, true, kRuntimeBounds) == kResetFatal);
    CHECK(ResetStep(unknown, 1, 0, false, kRuntimeBounds) == kResetFatal);
}

TEST_CASE("display logic mode resolution" * doctest::test_suite("unit"))
{
    CHECK(ResolveMode(true, false, false) == kModeBorderless);
    CHECK(ResolveMode(true, true, true) == kModeBorderless);
    CHECK(ResolveMode(false, true, false) == kModeWindowed);
    CHECK(ResolveMode(false, false, true) == kModeWindowed);
    CHECK(ResolveMode(false, false, false) == kModeFullscreen);
}

TEST_CASE("display logic mode predicates" * doctest::test_suite("unit"))
{
    CHECK_FALSE(D3DWindowed(kModeFullscreen));
    CHECK(D3DWindowed(kModeWindowed));
    CHECK(D3DWindowed(kModeBorderless));
    CHECK(MinimizeOnDeactivate(kModeFullscreen));
    CHECK_FALSE(MinimizeOnDeactivate(kModeWindowed));
    CHECK_FALSE(MinimizeOnDeactivate(kModeBorderless));
    CHECK(Topmost(kModeFullscreen));
    CHECK_FALSE(Topmost(kModeWindowed));
    CHECK_FALSE(Topmost(kModeBorderless));
}

TEST_CASE("display logic window style" * doctest::test_suite("unit"))
{
    WinStyle f = WindowStyleFor(kModeFullscreen, true);
    CHECK(f.style == (WS_POPUP | WS_VISIBLE));
    CHECK(f.exStyle == WS_EX_TOPMOST);
    CHECK((WindowStyleFor(kModeFullscreen, false).style & WS_VISIBLE) == 0);

    WinStyle w = WindowStyleFor(kModeWindowed, false);
    CHECK(w.style == (WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX));
    CHECK((w.style & WS_MAXIMIZEBOX) == 0);
    CHECK((w.style & WS_VISIBLE) == 0);
    CHECK(w.exStyle == 0);
    CHECK((WindowStyleFor(kModeWindowed, true).style & WS_VISIBLE) != 0);

    WinStyle b = WindowStyleFor(kModeBorderless, true);
    CHECK(b.style == (WS_POPUP | WS_VISIBLE));
    CHECK((b.style & (WS_CAPTION | WS_THICKFRAME)) == 0);
    CHECK((b.exStyle & WS_EX_TOPMOST) == 0);
}

TEST_CASE("display logic window placement" * doctest::test_suite("unit"))
{
    const Frame none = {0, 0, 0, 0};
    const Rect monitors[] = {{0, 0, 1920, 1080}, {-1920, 0, 0, 1080}, {1920, -200, 3840, 880}};
    for (const Rect& mon : monitors)
    {
        const Rect shrunk = {mon.l + 10, mon.t + 20, mon.r - 30, mon.b - 40};
        Rect r = PlaceWindow(kModeBorderless, mon, shrunk, 800, 600, none);
        CHECK(r.l == mon.l);
        CHECK(r.t == mon.t);
        CHECK(r.r == mon.r);
        CHECK(r.b == mon.b);
    }

    const Frame frame = {-8, -31, 8, 8};
    const Rect work = {0, 0, 1920, 1040};
    Rect w = PlaceWindow(kModeWindowed, monitors[0], work, 800, 600, frame);
    CHECK(w.r - w.l == 816);
    CHECK(w.b - w.t == 639);
    CHECK(w.l == (0 - 816 + 1920) / 2);
    CHECK(w.t == (0 - 639 + 1040) / 2);

    const Rect tiny = {100, 50, 500, 400};
    Rect c = PlaceWindow(kModeWindowed, tiny, tiny, 800, 600, frame);
    CHECK(c.l == tiny.l);
    CHECK(c.t == tiny.t);

    Rect h = PlaceWindow(kModeWindowed, monitors[0], work, 99999, 99999, frame);
    CHECK(h.r - h.l == 16384 + 16);
    CHECK(h.b - h.t == 16384 + 39);

    Rect fs = PlaceWindow(kModeFullscreen, monitors[2], monitors[2], 800, 600, frame);
    CHECK(fs.r - fs.l == 816);
    CHECK(fs.b - fs.t == 639);
    CHECK(fs.l == (1920 - 816 + 3840) / 2);
    CHECK(fs.t == (-200 - 639 + 880) / 2);
}

TEST_CASE("display logic dpi changed rect" * doctest::test_suite("unit"))
{
    const Rect suggested = {100, 50, 900, 700};
    const Frame frame = {-12, -45, 12, 12};
    Rect r = DpiChangedRect(suggested, 800, 600, frame);
    CHECK(r.l == 100);
    CHECK(r.t == 50);
    CHECK(r.r == 100 + 824);
    CHECK(r.b == 50 + 657);
}

namespace
{

char sOrder[8];
int sOrderLen = 0;
int sPmv2Result = kDpiFail;
int sPmResult = kDpiFail;
int sSystemResult = kDpiFail;

int StepPmv2(void*)
{
    sOrder[sOrderLen++] = 'a';
    return sPmv2Result;
}

int StepPm(void*)
{
    sOrder[sOrderLen++] = 'b';
    return sPmResult;
}

int StepSystem(void*)
{
    sOrder[sOrderLen++] = 'c';
    return sSystemResult;
}

DpiLevel RunCascade(int iPmv2, int iPm, int iSystem, bool bNoDpi, bool bNull)
{
    sPmv2Result = iPmv2;
    sPmResult = iPm;
    sSystemResult = iSystem;
    sOrderLen = 0;
    memset(sOrder, 0, sizeof(sOrder));
    DpiSteps steps = {bNull ? 0 : StepPmv2, bNull ? 0 : StepPm, bNull ? 0 : StepSystem, 0};
    return DpiCascade(steps, bNoDpi);
}

}

TEST_CASE("display logic dpi cascade" * doctest::test_suite("unit"))
{
    CHECK(RunCascade(kDpiOk, kDpiOk, kDpiOk, false, false) == kDpiLevelPmV2);
    CHECK(strcmp(sOrder, "a") == 0);
    CHECK(RunCascade(kDpiFail, kDpiOk, kDpiOk, false, false) == kDpiLevelPm);
    CHECK(strcmp(sOrder, "ab") == 0);
    CHECK(RunCascade(kDpiFail, kDpiFail, kDpiOk, false, false) == kDpiLevelSystem);
    CHECK(strcmp(sOrder, "abc") == 0);
    CHECK(RunCascade(kDpiFail, kDpiFail, kDpiFail, false, false) == kDpiLevelNone);
    CHECK(RunCascade(kDpiOk, kDpiOk, kDpiOk, false, true) == kDpiLevelNone);
    CHECK(RunCascade(kDpiAlready, kDpiOk, kDpiOk, false, false) == kDpiLevelPmV2);
    CHECK(strcmp(sOrder, "a") == 0);
    CHECK(RunCascade(kDpiFail, kDpiAlready, kDpiOk, false, false) == kDpiLevelPm);
    CHECK(strcmp(sOrder, "ab") == 0);
    CHECK(RunCascade(kDpiOk, kDpiOk, kDpiOk, true, false) == kDpiLevelOff);
    CHECK(sOrderLen == 0);
    CHECK(strcmp(DpiLevelName(kDpiLevelNone), "none") == 0);
    CHECK(strcmp(DpiLevelName(kDpiLevelPmV2), "pmv2") == 0);
    CHECK(strcmp(DpiLevelName(kDpiLevelPm), "pm") == 0);
    CHECK(strcmp(DpiLevelName(kDpiLevelSystem), "system") == 0);
    CHECK(strcmp(DpiLevelName(kDpiLevelOff), "off") == 0);
}

TEST_CASE("display logic dpi preset" * doctest::test_suite("unit"))
{
    CHECK(PresetDpiLevel(2, true, kDpiLevelPm) == kDpiLevelPmV2);
    CHECK(PresetDpiLevel(2, false, kDpiLevelPmV2) == kDpiLevelPm);
    CHECK(PresetDpiLevel(1, false, kDpiLevelPmV2) == kDpiLevelSystem);
    CHECK(PresetDpiLevel(1, true, kDpiLevelPmV2) == kDpiLevelSystem);
    CHECK(PresetDpiLevel(0, false, kDpiLevelPmV2) == kDpiLevelNone);
    CHECK(PresetDpiLevel(-1, false, kDpiLevelPmV2) == kDpiLevelPmV2);
    CHECK(PresetDpiLevel(-1, false, kDpiLevelPm) == kDpiLevelPm);
}

TEST_CASE("display logic coordinate mapping" * doctest::test_suite("unit"))
{
    int ox = -1;
    int oy = -1;
    MapBackBufferToClient(5, 7, 800, 600, 800, 600, ox, oy);
    CHECK(ox == 5);
    CHECK(oy == 7);
    MapBackBufferToClient(800, 600, 800, 600, 800, 600, ox, oy);
    CHECK(ox == 799);
    CHECK(oy == 599);

    MapBackBufferToClient(0, 0, 800, 600, 1920, 1080, ox, oy);
    CHECK(ox == 0);
    CHECK(oy == 0);
    MapBackBufferToClient(400, 300, 800, 600, 1920, 1080, ox, oy);
    CHECK(ox == 960);
    CHECK(oy == 540);
    MapBackBufferToClient(800, 600, 800, 600, 1920, 1080, ox, oy);
    CHECK(ox == 1919);
    CHECK(oy == 1079);
    MapBackBufferToClient(-5, -9, 800, 600, 1920, 1080, ox, oy);
    CHECK(ox == 0);
    CHECK(oy == 0);

    MapBackBufferToClient(401, 301, 801, 601, 1280, 720, ox, oy);
    CHECK(ox == (401 * 1280 + 400) / 801);
    CHECK(oy == (301 * 720 + 300) / 601);

    MapBackBufferToClient(33, 44, 0, 600, 1920, 0, ox, oy);
    CHECK(ox == 33);
    CHECK(oy == 44);
    MapBackBufferToClient(-3, 44, 800, 600, 0, 0, ox, oy);
    CHECK(ox == 0);
    CHECK(oy == 44);
    MapBackBufferToClient(2000000000, 2000000000, 800, 600, 1920, 1080, ox, oy);
    CHECK(ox == 1919);
    CHECK(oy == 1079);

    for (int x = 0; x < 800; x += 7)
    {
        int cx = 0;
        int cy = 0;
        int bx = 0;
        int by = 0;
        MapBackBufferToClient(x, x % 600, 800, 600, 1920, 1080, cx, cy);
        MapClientToBackBuffer(cx, cy, 1920, 1080, 800, 600, bx, by);
        CHECK(abs(bx - x) <= 1);
        CHECK(abs(by - (x % 600)) <= 1);
    }
}

TEST_CASE("display logic shell size policy" * doctest::test_suite("unit"))
{
    ShellSize s = ShellSizePolicy(kModeFullscreen, false, 0, 0, 1280, 720, 1920, 1080);
    CHECK(s.uiBackW == 1280);
    CHECK(s.uiBackH == 720);
    CHECK(s.uiClientW == 1280);
    CHECK(s.uiClientH == 720);
    CHECK_FALSE(s.bClearMsaa);

    s = ShellSizePolicy(kModeFullscreen, true, 0, 0, 1280, 720, 1920, 1080);
    CHECK(s.uiBackW == 0);
    CHECK(s.uiBackH == 0);
    CHECK(s.uiClientW == 0);
    CHECK(s.uiClientH == 0);
    CHECK(s.bClearMsaa);

    s = ShellSizePolicy(kModeFullscreen, true, 1024, 768, 1280, 720, 1920, 1080);
    CHECK(s.uiBackW == 1024);
    CHECK(s.uiBackH == 768);
    CHECK(s.uiClientW == 1024);
    CHECK(s.uiClientH == 768);

    s = ShellSizePolicy(kModeWindowed, true, 0, 0, 1280, 720, 1920, 1080);
    CHECK(s.uiBackW == 0);
    CHECK(s.uiBackH == 0);
    CHECK(s.uiClientW == 1280);
    CHECK(s.uiClientH == 720);
    CHECK(s.bClearMsaa);

    s = ShellSizePolicy(kModeWindowed, true, 1024, 768, 1280, 720, 1920, 1080);
    CHECK(s.uiBackW == 1024);
    CHECK(s.uiBackH == 768);
    CHECK(s.uiClientW == 1024);
    CHECK(s.uiClientH == 768);

    s = ShellSizePolicy(kModeWindowed, true, 0, 0, 0, 0, 1920, 1080);
    CHECK(s.uiBackW == 0);
    CHECK(s.uiClientW == 0);
    CHECK(s.uiClientH == 0);

    s = ShellSizePolicy(kModeWindowed, false, 0, 0, 1280, 720, 1920, 1080);
    CHECK(s.uiBackW == 1280);
    CHECK(s.uiBackH == 720);
    CHECK(s.uiClientW == 1280);
    CHECK(s.uiClientH == 720);
    CHECK_FALSE(s.bClearMsaa);

    s = ShellSizePolicy(kModeBorderless, false, 0, 0, 1280, 720, 2560, 1440);
    CHECK(s.uiBackW == 2560);
    CHECK(s.uiBackH == 1440);
    CHECK(s.uiClientW == 2560);
    CHECK(s.uiClientH == 1440);
    CHECK(s.bWarnIgnoredSize);

    s = ShellSizePolicy(kModeBorderless, false, 0, 0, 2560, 1440, 2560, 1440);
    CHECK_FALSE(s.bWarnIgnoredSize);
    s = ShellSizePolicy(kModeBorderless, false, 0, 0, 0, 0, 2560, 1440);
    CHECK_FALSE(s.bWarnIgnoredSize);

    CHECK_FALSE(s.bWarnNoMonitor);

    s = ShellSizePolicy(kModeBorderless, false, 0, 0, 1280, 720, 0, 0);
    CHECK(s.uiBackW == 1280);
    CHECK(s.uiBackH == 720);
    CHECK(s.uiClientW == 1280);
    CHECK(s.uiClientH == 720);
    CHECK_FALSE(s.bWarnIgnoredSize);
    CHECK(s.bWarnNoMonitor);
    CHECK_FALSE(s.bClearMsaa);

    s = ShellSizePolicy(kModeBorderless, false, 1024, 768, 1280, 720, 0, 0);
    CHECK(s.uiBackW == 1024);
    CHECK(s.uiClientH == 768);
    CHECK(s.bWarnNoMonitor);

    s = ShellSizePolicy(kModeBorderless, false, 0, 0, 0, 0, 2560, 0);
    CHECK(s.uiBackW == 0);
    CHECK(s.uiClientW == 0);
    CHECK(s.bWarnNoMonitor);

    s = ShellSizePolicy(kModeBorderless, true, 0, 0, 1280, 720, 0, 0);
    CHECK(s.uiBackW == 0);
    CHECK_FALSE(s.bWarnNoMonitor);

    s = ShellSizePolicy(kModeBorderless, true, 0, 0, 1280, 720, 2560, 1440);
    CHECK(s.uiBackW == 0);
    CHECK(s.uiBackH == 0);
    CHECK(s.uiClientW == 2560);
    CHECK(s.uiClientH == 1440);
    CHECK_FALSE(s.bWarnIgnoredSize);

    s = ShellSizePolicy(kModeBorderless, true, 1024, 768, 1280, 720, 2560, 1440);
    CHECK(s.uiBackW == 1024);
    CHECK(s.uiBackH == 768);
    CHECK(s.uiClientW == 2560);
    CHECK(s.uiClientH == 1440);
}

TEST_CASE("display logic shell stretch plan" * doctest::test_suite("unit"))
{
    ShellStretch s = ShellStretchPlan(kModeWindowed, true, 800, 600, 1920, 1080, true);
    CHECK(s.bActive);
    CHECK(s.bLinear);

    s = ShellStretchPlan(kModeBorderless, true, 800, 600, 2560, 1440, false);
    CHECK(s.bActive);
    CHECK_FALSE(s.bLinear);

    s = ShellStretchPlan(kModeWindowed, true, 640, 480, 800, 480, true);
    CHECK(s.bActive);

    s = ShellStretchPlan(kModeFullscreen, true, 800, 600, 1920, 1080, true);
    CHECK_FALSE(s.bActive);
    CHECK_FALSE(s.bLinear);

    s = ShellStretchPlan(kModeWindowed, false, 800, 600, 1920, 1080, true);
    CHECK_FALSE(s.bActive);
    CHECK_FALSE(s.bLinear);

    s = ShellStretchPlan(kModeBorderless, false, 800, 600, 2560, 1440, true);
    CHECK_FALSE(s.bActive);

    s = ShellStretchPlan(kModeWindowed, true, 800, 600, 800, 600, true);
    CHECK_FALSE(s.bActive);
    CHECK_FALSE(s.bLinear);

    s = ShellStretchPlan(kModeWindowed, true, 0, 600, 1920, 1080, true);
    CHECK_FALSE(s.bActive);
    s = ShellStretchPlan(kModeWindowed, true, 800, 0, 1920, 1080, true);
    CHECK_FALSE(s.bActive);
    s = ShellStretchPlan(kModeWindowed, true, 800, 600, 0, 1080, true);
    CHECK_FALSE(s.bActive);
    s = ShellStretchPlan(kModeBorderless, true, 800, 600, 2560, 0, true);
    CHECK_FALSE(s.bActive);

    s = ShellStretchPlan(kModeWindowed, true, 800, 600, kMaxDim + 1, 1080, true);
    CHECK_FALSE(s.bActive);
    s = ShellStretchPlan(kModeWindowed, true, 800, 600, 1920, kMaxDim + 1, true);
    CHECK_FALSE(s.bActive);
    s = ShellStretchPlan(kModeWindowed, true, kMaxDim + 1, 600, 1920, 1080, true);
    CHECK_FALSE(s.bActive);
    s = ShellStretchPlan(kModeWindowed, true, 800, 600, kMaxDim, kMaxDim, true);
    CHECK(s.bActive);
}

TEST_CASE("display logic apply same params" * doctest::test_suite("unit"))
{
    DisplayRec a = {800, 600, true, 0, 0, 1, 60, kModeWindowed, 800, 600};
    DisplayRec b = a;
    CHECK(SameDisplayParams(a, b));
    b = a; b.uiW = 801; CHECK_FALSE(SameDisplayParams(a, b));
    b = a; b.uiH = 601; CHECK_FALSE(SameDisplayParams(a, b));
    b = a; b.bWindowed = false; CHECK_FALSE(SameDisplayParams(a, b));
    b = a; b.uiMsType = 2; CHECK_FALSE(SameDisplayParams(a, b));
    b = a; b.uiMsQuality = 1; CHECK_FALSE(SameDisplayParams(a, b));
    b = a; b.uiInterval = 2; CHECK_FALSE(SameDisplayParams(a, b));
    b = a; b.uiRefresh = 75; CHECK_FALSE(SameDisplayParams(a, b));
    b = a; b.eMode = kModeBorderless; CHECK_FALSE(SameDisplayParams(a, b));
    b = a; b.uiClientW = 1; CHECK_FALSE(SameDisplayParams(a, b));
    b = a; b.uiClientH = 1; CHECK_FALSE(SameDisplayParams(a, b));
}

TEST_CASE("display logic apply buffer sizes" * doctest::test_suite("unit"))
{
    BufferSet s = BufferSizes(800, 600, 4096, 4096, false);
    CHECK(s.akBuf[1].uiW == 512);
    CHECK(s.akBuf[1].uiH == 512);
    CHECK(s.akBuf[0].uiW == 512);
    CHECK(s.akBuf[0].uiH == 512);
    CHECK(s.akBuf[2].uiW == 256);
    CHECK(s.akBuf[2].uiH == 256);

    s = BufferSizes(1920, 1080, 4096, 4096, true);
    CHECK(s.akBuf[1].uiW == 1920);
    CHECK(s.akBuf[1].uiH == 1080);
    CHECK(s.akBuf[0].uiW == 512);
    CHECK(s.akBuf[0].uiH == 512);
    CHECK(s.akBuf[2].uiW == 256);
    CHECK(s.akBuf[2].uiH == 256);

    s = BufferSizes(200, 150, 4096, 4096, false);
    CHECK(s.akBuf[1].uiW == 128);
    CHECK(s.akBuf[1].uiH == 128);
    CHECK(s.akBuf[0].uiW == 128);
    CHECK(s.akBuf[0].uiH == 128);
    CHECK(s.akBuf[2].uiW == 128);
    CHECK(s.akBuf[2].uiH == 128);

    s = BufferSizes(1920, 1080, 1024, 512, false);
    CHECK(s.akBuf[1].uiW == 1024);
    CHECK(s.akBuf[1].uiH == 512);

    s = BufferSizes(4000, 3000, 2048, 2048, true);
    CHECK(s.akBuf[1].uiW == 2048);
    CHECK(s.akBuf[1].uiH == 2048);
    s = BufferSizes(1920, 3000, 2048, 2048, true);
    CHECK(s.akBuf[1].uiW == 1920);
    CHECK(s.akBuf[1].uiH == 2048);
    s = BufferSizes(16384, 16384, 8192, 4096, true);
    CHECK(s.akBuf[1].uiW == 8192);
    CHECK(s.akBuf[1].uiH == 4096);
    CHECK(s.akBuf[0].uiW == 512);
    CHECK(s.akBuf[2].uiH == 256);
}

TEST_CASE("display logic blur copy fits the temp buffer" * doctest::test_suite("unit"))
{
    // stock: 0.25 of 1080p (480x270) fits the 512x512 TEMP buffer untouched
    CHECK(FitDownSize(0.25f, 1920, 1080, 512, 512) == 0.25f);
    CHECK(FitDownSize(0.25f, 800, 600, 512, 512) == 0.25f);
    CHECK(FitDownSize(0.5f, 1024, 768, 512, 512) == 0.5f);
    // 1440p and 4K overflowed it (640 and 960 wide), so the factor drops
    CHECK(FitDownSize(0.25f, 2560, 1440, 512, 512) == 0.2f);
    CHECK(FitDownSize(0.25f, 3840, 2160, 512, 512) == doctest::Approx(512.0f / 3840.0f));
    CHECK(3840.0f * FitDownSize(0.25f, 3840, 2160, 512, 512) <= 512.0f);
    // a tall view is limited by its height
    CHECK(FitDownSize(0.25f, 1080, 3840, 512, 512) == doctest::Approx(512.0f / 3840.0f));
    // nothing to fit
    CHECK(FitDownSize(0.25f, 0, 0, 512, 512) == 0.25f);
}

TEST_CASE("display logic live size fits caps" * doctest::test_suite("unit"))
{
    CHECK(SizeFitsCaps(1920, 1080, 2048, 2048));
    CHECK(SizeFitsCaps(2048, 2048, 2048, 2048));
    CHECK_FALSE(SizeFitsCaps(2049, 1080, 2048, 2048));
    CHECK_FALSE(SizeFitsCaps(1920, 2049, 2048, 2048));
    CHECK(SizeFitsCaps(16384, 16384, 0, 0));
    CHECK_FALSE(SizeFitsCaps(8192, 1080, 4096, 0));
}

TEST_CASE("display logic apply buffer sizes zero caps" * doctest::test_suite("unit"))
{
    BufferSet s = BufferSizes(800, 600, 0, 0, false);
    CHECK(s.akBuf[1].uiW == 512);
    CHECK(s.akBuf[1].uiH == 512);
    CHECK(s.akBuf[2].uiW == 256);

    s = BufferSizes(0, 0, 0, 0, false);
    for (int i = 0; i < 3; i++)
    {
        CHECK(s.akBuf[i].uiW == 1);
        CHECK(s.akBuf[i].uiH == 1);
    }

    s = BufferSizes(0, 0, 0, 0, true);
    CHECK(s.akBuf[1].uiW == 1);
    CHECK(s.akBuf[1].uiH == 1);
}

TEST_CASE("display logic shadow quad shares sky" * doctest::test_suite("unit"))
{
    CHECK(ShadowQuadSharesSky(1920, 1080, 0, false, 1920, 1080));
    CHECK_FALSE(ShadowQuadSharesSky(1920, 1080, 4, false, 1920, 1080));
    CHECK_FALSE(ShadowQuadSharesSky(1920, 1080, 0, true, 1920, 1080));
    CHECK_FALSE(ShadowQuadSharesSky(1920, 1080, 0, false, 1024, 1024));
    CHECK_FALSE(ShadowQuadSharesSky(1280, 720, 0, false, 1920, 720));
    CHECK_FALSE(ShadowQuadSharesSky(1280, 720, 0, false, 1280, 1080));

    BufferSet s = BufferSizes(1280, 720, 4096, 4096, false);
    CHECK_FALSE(ShadowQuadSharesSky(1280, 720, 0, false, s.akBuf[1].uiW, s.akBuf[1].uiH));
    s = BufferSizes(1280, 720, 4096, 4096, true);
    CHECK(ShadowQuadSharesSky(1280, 720, 0, false, s.akBuf[1].uiW, s.akBuf[1].uiH));
}

TEST_CASE("display logic revert restores profile" * doctest::test_suite("unit"))
{
    CHECK(ShouldRestoreProfile(true, 2, 2, false));
    CHECK_FALSE(ShouldRestoreProfile(false, 2, 2, false));
    CHECK_FALSE(ShouldRestoreProfile(true, 1, 2, false));
    CHECK_FALSE(ShouldRestoreProfile(true, 2, 2, true));
    CHECK_FALSE(ShouldRestoreProfile(true, 0, 3, true));
}

TEST_CASE("display logic apply phase" * doctest::test_suite("unit"))
{
    ApplyStep s = ApplyPhaseStep(kApplyResetting, kEvResetOk);
    CHECK(s.ePhase == kApplyAwaitPresent);
    CHECK(s.eAction == kActNone);

    s = ApplyPhaseStep(kApplyAwaitPresent, kEvResetOk);
    CHECK(s.ePhase == kApplyAwaitPresent);
    CHECK(s.eAction == kActNone);

    s = ApplyPhaseStep(kApplyAwaitPresent, kEvPresentOk);
    CHECK(s.ePhase == kApplyNone);
    CHECK(s.eAction == kActPromote);

    s = ApplyPhaseStep(kApplyAwaitPresent, kEvPresentFail);
    CHECK(s.ePhase == kApplyNone);
    CHECK(s.eAction == kActRevert);

    s = ApplyPhaseStep(kApplyAwaitPresent, kEvPresentLost);
    CHECK(s.ePhase == kApplyAwaitPresent);
    CHECK(s.eAction == kActNone);

    for (int e = kEvResetOk; e <= kEvPresentFail; e++)
    {
        s = ApplyPhaseStep(kApplyNone, (ApplyEvent)e);
        CHECK(s.ePhase == kApplyNone);
        CHECK(s.eAction == kActNone);
    }

    s = ApplyPhaseStep(kApplyResetting, kEvPresentOk);
    CHECK(s.ePhase == kApplyResetting);
    CHECK(s.eAction == kActNone);
}

TEST_CASE("display logic apply present event" * doctest::test_suite("unit"))
{
    CHECK(PresentEvent(S_OK) == kEvPresentOk);
    CHECK(PresentEvent(D3DERR_DEVICELOST) == kEvPresentLost);
    CHECK(PresentEvent(D3DERR_INVALIDCALL) == kEvPresentFail);
    CHECK(PresentEvent(D3DERR_OUTOFVIDEOMEMORY) == kEvPresentFail);
    CHECK(PresentEvent(E_FAIL) == kEvPresentFail);
}

TEST_CASE("display logic adapter decision" * doctest::test_suite("unit"))
{
    AdapterChoice c = AdapterDecision(1, 0, 1);
    CHECK_FALSE(c.bValid);
    CHECK_FALSE(c.bRestart);

    c = AdapterDecision(0, 0, 1);
    CHECK(c.bValid);
    CHECK_FALSE(c.bRestart);

    c = AdapterDecision(1, 0, 2);
    CHECK(c.bValid);
    CHECK(c.bRestart);

    c = AdapterDecision(0, 1, 2);
    CHECK(c.bValid);
    CHECK(c.bRestart);

    c = AdapterDecision(0, 0, 0);
    CHECK_FALSE(c.bValid);
    CHECK_FALSE(c.bRestart);
}

TEST_CASE("display logic apply size bounds" * doctest::test_suite("unit"))
{
    CHECK(SizeInBounds(320, 240));
    CHECK_FALSE(SizeInBounds(319, 240));
    CHECK_FALSE(SizeInBounds(320, 239));
    CHECK(SizeInBounds(16384, 16384));
    CHECK_FALSE(SizeInBounds(16385, 600));
    CHECK_FALSE(SizeInBounds(0, 0));
}

TEST_CASE("display logic gamma lut" * doctest::test_suite("unit"))
{
    unsigned short aw[256];
    unsigned char ab[256];
    for (int i = 0; i < 256; ++i)
    {
        aw[i] = (unsigned short)(i * 0x101);
    }
    RampToLut(aw, ab);
    for (int i = 0; i < 256; ++i)
    {
        CHECK(ab[i] == i);
    }

    for (int i = 0; i < 256; ++i)
    {
        aw[i] = 0xFFFF;
    }
    RampToLut(aw, ab);
    for (int i = 0; i < 256; ++i)
    {
        CHECK(ab[i] == 0xFF);
    }

    for (int i = 0; i < 256; ++i)
    {
        aw[i] = 0x00FF;
    }
    RampToLut(aw, ab);
    for (int i = 0; i < 256; ++i)
    {
        CHECK(ab[i] == 0);
    }

    const float afLevel[3] = {0.8f, 0.8f, 0.5f};
    ColorRamp::Build(afLevel, aw);
    RampToLut(aw, ab);
    for (int i = 0; i < 256; ++i)
    {
        CHECK(ab[i] == (aw[i] >> 8));
    }
}

TEST_CASE("display logic gamma identity" * doctest::test_suite("unit"))
{
    unsigned short aw[256];
    unsigned char ab[256];
    for (int i = 0; i < 256; ++i)
    {
        aw[i] = (unsigned short)(i * 0x101);
    }
    RampToLut(aw, ab);
    CHECK(LutIsIdentity(ab));

    ab[255] = 254;
    CHECK_FALSE(LutIsIdentity(ab));
    ab[255] = 255;
    ab[0] = 1;
    CHECK_FALSE(LutIsIdentity(ab));

    const float afDefault[3] = {0.5f, 0.5f, 0.5f};
    ColorRamp::Build(afDefault, aw);
    RampToLut(aw, ab);
    CHECK(LutIsIdentity(ab));

    const float afBright[3] = {0.8f, 0.8f, 0.5f};
    ColorRamp::Build(afBright, aw);
    RampToLut(aw, ab);
    CHECK_FALSE(LutIsIdentity(ab));

    const float afDark[3] = {0.2f, 0.2f, 0.5f};
    ColorRamp::Build(afDark, aw);
    RampToLut(aw, ab);
    CHECK_FALSE(LutIsIdentity(ab));
}

TEST_CASE("display logic gamma path" * doctest::test_suite("unit"))
{
    CHECK(GammaPathFor(false, false, true) == kGammaReasonNone);
    CHECK(GammaPathFor(false, false, false) == kGammaReasonNoProg2);
    CHECK(GammaPathFor(true, false, true) == kGammaReasonNoRender);
    CHECK(GammaPathFor(true, true, true) == kGammaReasonNone);
    CHECK(GammaPathFor(true, false, false) == kGammaReasonNoRender);
    CHECK(GammaPathFor(true, true, false) == kGammaReasonNoProg2);
}

TEST_CASE("display logic gamma pass runs" * doctest::test_suite("unit"))
{
    CHECK(GammaPassRuns(kGammaPass, false, false, true));
    CHECK_FALSE(GammaPassRuns(kGammaUndecided, false, false, true));
    CHECK_FALSE(GammaPassRuns(kGammaRamp, false, false, true));
    CHECK_FALSE(GammaPassRuns(kGammaPass, true, false, true));
    CHECK_FALSE(GammaPassRuns(kGammaPass, false, true, true));
    CHECK_FALSE(GammaPassRuns(kGammaPass, false, false, false));
}

TEST_CASE("display logic legacy flags" * doctest::test_suite("unit"))
{
    LegacyFlags f = LegacyFlagsFor(kModeFullscreen, 0);
    CHECK_FALSE(f.bWindowed);
    CHECK_FALSE(f.bVsync);
    f = LegacyFlagsFor(kModeWindowed, 1);
    CHECK(f.bWindowed);
    CHECK(f.bVsync);
    f = LegacyFlagsFor(kModeBorderless, 2);
    CHECK_FALSE(f.bWindowed);
    CHECK(f.bVsync);
    f = LegacyFlagsFor(kModeBorderless, 0);
    CHECK_FALSE(f.bWindowed);
    CHECK_FALSE(f.bVsync);
}

TEST_CASE("display logic frame cap list" * doctest::test_suite("unit"))
{
    unsigned int out[12];
    const unsigned int base[] = {0, 30, 60, 75, 120, 144, 165, 240};
    REQUIRE(FrameCapList(0, out, 12) == 8);
    for (unsigned int i = 0; i < 8; i++)
    {
        CHECK(out[i] == base[i]);
    }
    const unsigned int with80[] = {0, 30, 60, 75, 80, 120, 144, 165, 240};
    REQUIRE(FrameCapList(80, out, 12) == 9);
    for (unsigned int i = 0; i < 9; i++)
    {
        CHECK(out[i] == with80[i]);
    }
    CHECK(FrameCapList(60, out, 12) == 8);
    CHECK(FrameCapList(100, out, 12) == 9);
    CHECK(out[4] == 100);
    CHECK(out[5] == 120);
    REQUIRE(FrameCapList(1000, out, 12) == 9);
    CHECK(out[7] == 240);
    CHECK(out[8] == 1000);
    unsigned int capped[4] = {7, 7, 7, 7};
    CHECK(FrameCapList(80, capped, 3) == 9);
    CHECK(capped[0] == 0);
    CHECK(capped[2] == 60);
    CHECK(capped[3] == 7);
    CHECK(FrameCapList(80, 0, 0) == 9);
    CHECK(FrameCapList(0, 0, 0) == 8);
}

TEST_CASE("display logic monitor label" * doctest::test_suite("unit"))
{
    CHECK(DisplayNumberFromDeviceName("\\\\.\\DISPLAY2") == 2);
    CHECK(DisplayNumberFromDeviceName("\\\\.\\DISPLAY12") == 12);
    CHECK(DisplayNumberFromDeviceName(0) == 0);
    CHECK(DisplayNumberFromDeviceName("") == 0);
    CHECK(DisplayNumberFromDeviceName("\\\\.\\DISPLAY") == 0);
    CHECK(DisplayNumberFromDeviceName("\\\\.\\DISPLAY0") == 0);
    CHECK(DisplayNumberFromDeviceName("\\\\.\\DISPLAY1234") == 0);
    char buf[64];
    REQUIRE(MonitorLabel(1, 2, 2560, 1440, buf, 64));
    CHECK(std::string(buf) == "Display 2 (2560x1440)");
    REQUIRE(MonitorLabel(0, 2, 0, 0, buf, 64));
    CHECK(std::string(buf) == "Display 2");
    REQUIRE(MonitorLabel(0, 0, 1920, 1080, buf, 64));
    CHECK(std::string(buf) == "Monitor 1 (1920x1080)");
    REQUIRE(MonitorLabel(3, 0, 0, 0, buf, 64));
    CHECK(std::string(buf) == "Monitor 4");
    REQUIRE(MonitorLabel(0, 1, kMaxDim + 1, 1080, buf, 64));
    CHECK(std::string(buf) == "Display 1");
    REQUIRE(MonitorLabel(0, 1, 1920, kMaxDim + 1, buf, 64));
    CHECK(std::string(buf) == "Display 1");
    REQUIRE(MonitorLabel(0, 999, 0xFFFFFFFFu, 0xFFFFFFFFu, buf, 64));
    CHECK(strlen(buf) <= 31);
    char acSmall[9];
    memset(acSmall, 'z', sizeof(acSmall));
    REQUIRE(MonitorLabel(1, 2, 2560, 1440, acSmall, 8));
    CHECK(strlen(acSmall) == 7);
    CHECK(std::string(acSmall) == "Display");
    CHECK(acSmall[8] == 'z');
    CHECK_FALSE(MonitorLabel(0, 1, 1920, 1080, 0, 64));
    CHECK_FALSE(MonitorLabel(0, 1, 1920, 1080, buf, 0));
}

TEST_CASE("display logic multisample list" * doctest::test_suite("unit"))
{
    MultisampleEntry out[kMultisampleTypes + 1];
    REQUIRE(MultisampleList(0, out, kMultisampleTypes) == 1);
    CHECK(out[0].uiType == 0);
    CHECK(out[0].uiQuality == 0);

    unsigned int levels[kMultisampleTypes];
    memset(levels, 0, sizeof(levels));
    levels[2] = 1;
    levels[4] = 2;
    levels[1] = 5;
    REQUIRE(MultisampleList(levels, out, kMultisampleTypes) == 4);
    CHECK(out[0].uiType == 0);
    CHECK(out[0].uiQuality == 0);
    CHECK(out[1].uiType == 2);
    CHECK(out[1].uiQuality == 0);
    CHECK(out[2].uiType == 4);
    CHECK(out[2].uiQuality == 0);
    CHECK(out[3].uiType == 4);
    CHECK(out[3].uiQuality == 1);

    for (unsigned int i = 0; i < kMultisampleTypes; i++)
    {
        levels[i] = (i >= 2) ? 3 : 0;
    }
    out[kMultisampleTypes].uiType = 0xDEADBEEFu;
    out[kMultisampleTypes].uiQuality = 0xDEADBEEFu;
    REQUIRE(MultisampleList(levels, out, kMultisampleTypes) == kMultisampleTypes);
    CHECK(out[0].uiType == 0);
    CHECK(out[0].uiQuality == 0);
    CHECK(out[1].uiType == 2);
    CHECK(out[kMultisampleTypes].uiType == 0xDEADBEEFu);
    CHECK(out[kMultisampleTypes].uiQuality == 0xDEADBEEFu);
    for (unsigned int i = 1; i < kMultisampleTypes; i++)
    {
        CHECK(out[i].uiType != 1);
    }
    CHECK(MultisampleList(levels, out, 0) == 0);
    CHECK(MultisampleList(levels, 0, kMultisampleTypes) == 0);

    FakeMultisampleLevels(levels);
    REQUIRE(MultisampleList(levels, out, kMultisampleTypes) == 4);
    const unsigned int types[] = {0, 2, 4, 8};
    for (unsigned int i = 0; i < 4; i++)
    {
        CHECK(out[i].uiType == types[i]);
        CHECK(out[i].uiQuality == 0);
    }

    char buf[32];
    REQUIRE(MultisampleLabel(4, 0, buf, sizeof(buf)));
    CHECK(std::string(buf) == "4x");
    REQUIRE(MultisampleLabel(8, 1, buf, sizeof(buf)));
    CHECK(std::string(buf) == "8x Q2");
    REQUIRE(MultisampleLabel(0, 0, buf, sizeof(buf)));
    CHECK(std::string(buf) == "Off");
    char acSmall[3];
    REQUIRE(MultisampleLabel(16, 3, acSmall, sizeof(acSmall)));
    CHECK(std::string(acSmall) == "16");
    CHECK_FALSE(MultisampleLabel(4, 0, 0, 8));
    CHECK_FALSE(MultisampleLabel(4, 0, buf, 0));
}

TEST_CASE("display logic effective fps cap" * doctest::test_suite("unit"))
{
    CHECK(EffectiveFpsCap(80, false) == 80);
    CHECK(EffectiveFpsCap(80, true) == 0);
    CHECK(EffectiveFpsCap(0, false) == 0);
}

TEST_CASE("display logic present watch" * doctest::test_suite("unit"))
{
    PresentWatch w;
    memset(&w, 0xFF, sizeof(w));
    PresentWatchReset(w);
    CHECK_FALSE(w.bThrottled);
    CHECK_FALSE(w.bOverridden);
    CHECK(w.uiFrames == 0);

    PresentWatchStep(w, true, true, true, 1.0 / 60, 60);
    CHECK(w.bThrottled);

    PresentWatchReset(w);
    for (unsigned int i = 0; i < 120; i++)
    {
        PresentWatchStep(w, true, true, true, 1.0 / 60, 60);
        CHECK(w.bThrottled);
    }
    CHECK_FALSE(w.bOverridden);

    PresentWatchReset(w);
    for (unsigned int i = 0; i < 60; i++)
    {
        PresentWatchStep(w, true, true, true, 1.0 / 300, 60);
    }
    CHECK(w.bOverridden);
    CHECK_FALSE(w.bThrottled);
    for (unsigned int i = 0; i < 120; i++)
    {
        PresentWatchStep(w, true, true, true, 1.0 / 60, 60);
    }
    CHECK_FALSE(w.bThrottled);
    CHECK(w.bOverridden);
    PresentWatchReset(w);
    CHECK_FALSE(w.bOverridden);
    PresentWatchStep(w, true, true, true, 1.0 / 60, 60);
    CHECK(w.bThrottled);

    PresentWatchReset(w);
    for (unsigned int i = 0; i < 60; i++)
    {
        PresentWatchStep(w, true, true, true, 1.0 / 300, 144);
    }
    CHECK(w.bOverridden);
    PresentWatchReset(w);
    for (unsigned int i = 0; i < 60; i++)
    {
        PresentWatchStep(w, true, true, true, 1.0 / 144, 144);
    }
    CHECK_FALSE(w.bOverridden);
    CHECK(w.bThrottled);

    PresentWatchReset(w);
    for (unsigned int i = 0; i < 60; i++)
    {
        PresentWatchStep(w, true, true, true, 1.0 / 144, 0);
    }
    CHECK(w.bOverridden);
    PresentWatchReset(w);
    for (unsigned int i = 0; i < 60; i++)
    {
        PresentWatchStep(w, true, true, true, 1.0 / 144, 5000);
    }
    CHECK(w.bOverridden);

    for (unsigned int k = 0; k < 3; k++)
    {
        PresentWatchReset(w);
        for (unsigned int i = 0; i < 59; i++)
        {
            PresentWatchStep(w, true, true, true, 1.0 / 300, 60);
        }
        CHECK(w.uiFrames == 59);
        PresentWatchStep(w, k != 0, k != 1, k != 2, 1.0 / 300, 60);
        CHECK_FALSE(w.bThrottled);
        CHECK_FALSE(w.bOverridden);
        CHECK(w.uiFrames == 0);
        CHECK(w.dSum == 0.0);
        PresentWatchStep(w, true, true, true, 1.0 / 300, 60);
        CHECK(w.bThrottled);
        CHECK_FALSE(w.bOverridden);
    }

    PresentWatchReset(w);
    PresentWatchStep(w, true, true, true, 1.0 / 60, 60);
    REQUIRE(w.bThrottled);
    CHECK(PresentThrottledNow(w, true, 0.01));
    CHECK_FALSE(PresentThrottledNow(w, false, 0.01));
    CHECK_FALSE(PresentThrottledNow(w, true, 0.5));
    PresentWatchReset(w);
    CHECK_FALSE(PresentThrottledNow(w, true, 0.01));
    CHECK_FALSE(PresentThrottledNow(w, false, 0.01));
    CHECK_FALSE(PresentThrottledNow(w, true, 0.5));
}

TEST_CASE("display logic refresh rates" * doctest::test_suite("unit"))
{
    DisplayModeRec src[] = {M(1920, 1080, 144), M(1280, 720, 60), M(1920, 1080, 60),
                            M(1920, 1080, 144), M(1920, 1080, 120), M(1920, 1080, 0)};
    unsigned int out[8] = {7, 7, 7, 7, 7, 7, 7, 7};
    REQUIRE(RefreshRates(src, 6, 1920, 1080, out, 8) == 3);
    CHECK(out[0] == 60);
    CHECK(out[1] == 120);
    CHECK(out[2] == 144);
    CHECK(out[3] == 7);
    CHECK(RefreshRates(src, 6, 1280, 720, out, 8) == 1);
    CHECK(out[0] == 60);
    CHECK(RefreshRates(src, 6, 2560, 1440, out, 8) == 0);
    unsigned int capped[3] = {7, 7, 7};
    CHECK(RefreshRates(src, 6, 1920, 1080, capped, 2) == 3);
    CHECK(capped[0] == 60);
    CHECK(capped[1] == 120);
    CHECK(capped[2] == 7);
    CHECK(RefreshRates(src, 6, 1920, 1080, 0, 0) == 3);
}

TEST_CASE("display logic confirm needed" * doctest::test_suite("unit"))
{
    DisplayRec a = {1920, 1080, false, 0, 0, 1, 60, kModeFullscreen, 1920, 1080};
    DisplayRec b = a;
    CHECK_FALSE(ConfirmNeeded(a, b));
    b.uiInterval = 0x80000000u;
    CHECK_FALSE(ConfirmNeeded(a, b));
    b = a;
    b.uiMsType = 4;
    b.uiMsQuality = 1;
    CHECK_FALSE(ConfirmNeeded(a, b));
    b = a;
    b.uiW = 1280;
    CHECK(ConfirmNeeded(a, b));
    b = a;
    b.uiH = 720;
    CHECK(ConfirmNeeded(a, b));
    b = a;
    b.eMode = kModeBorderless;
    CHECK(ConfirmNeeded(a, b));
    b = a;
    b.uiRefresh = 144;
    CHECK(ConfirmNeeded(a, b));
    b = a;
    b.uiClientW = 1280;
    CHECK(ConfirmNeeded(a, b));
    b = a;
    b.uiClientH = 720;
    CHECK(ConfirmNeeded(a, b));
}

TEST_CASE("display logic stage arm merge" * doctest::test_suite("unit"))
{
    bool bArm = MergeStageArm(false, true, true, true);
    bArm = MergeStageArm(bArm, true, false, true);
    CHECK(bArm);
    CHECK(StagedConfirm(bArm, true, true, true));

    bArm = MergeStageArm(false, true, false, true);
    bArm = MergeStageArm(bArm, true, true, true);
    CHECK(bArm);
    CHECK(StagedConfirm(bArm, true, true, true));

    bArm = MergeStageArm(false, true, true, true);
    CHECK(StagedConfirm(bArm, true, true, true));

    bArm = MergeStageArm(false, true, false, true);
    CHECK_FALSE(bArm);
    CHECK_FALSE(StagedConfirm(bArm, true, true, true));

    bArm = MergeStageArm(false, true, true, false);
    CHECK_FALSE(bArm);
    CHECK_FALSE(StagedConfirm(bArm, true, false, false));
    bArm = MergeStageArm(bArm, true, false, true);
    CHECK_FALSE(bArm);
    CHECK_FALSE(StagedConfirm(bArm, true, true, true));

    bArm = MergeStageArm(false, true, true, true);
    bArm = MergeStageArm(bArm, true, false, false);
    CHECK_FALSE(bArm);
    bArm = MergeStageArm(bArm, true, false, true);
    CHECK_FALSE(StagedConfirm(bArm, true, true, true));

    CHECK_FALSE(MergeStageArm(false, false, true, true));
    CHECK(MergeStageArm(true, false, false, true));
    CHECK_FALSE(MergeStageArm(true, true, true, false));
    CHECK_FALSE(StagedConfirm(true, false, true, true));
    CHECK_FALSE(StagedConfirm(true, true, false, true));
    CHECK_FALSE(StagedConfirm(true, true, true, false));
    CHECK_FALSE(StagedConfirm(false, true, true, true));
}

TEST_CASE("display logic confirm phase" * doctest::test_suite("unit"))
{
    CHECK(kConfirmMs == 15000);

    ConfirmStep s = ConfirmPhaseStep(kConfirmNone, kConfirmEvArm, 0);
    CHECK(s.ePhase == kConfirmAwaitPresent);
    CHECK(s.eAction == kConfirmActNone);

    s = ConfirmPhaseStep(kConfirmAwaitPresent, kConfirmEvArm, 500);
    CHECK(s.ePhase == kConfirmAwaitPresent);
    CHECK(s.eAction == kConfirmActNone);

    s = ConfirmPhaseStep(kConfirmCounting, kConfirmEvArm, 500);
    CHECK(s.ePhase == kConfirmAwaitPresent);
    CHECK(s.eAction == kConfirmActNone);

    s = ConfirmPhaseStep(kConfirmAwaitPresent, kConfirmEvApplied, 0);
    CHECK(s.ePhase == kConfirmCounting);
    CHECK(s.eAction == kConfirmActStart);

    s = ConfirmPhaseStep(kConfirmAwaitPresent, kConfirmEvAppliedPlain, 0);
    CHECK(s.ePhase == kConfirmNone);
    CHECK(s.eAction == kConfirmActClear);

    s = ConfirmPhaseStep(kConfirmAwaitPresent, kConfirmEvApplyFailed, 0);
    CHECK(s.ePhase == kConfirmNone);
    CHECK(s.eAction == kConfirmActClear);

    s = ConfirmPhaseStep(kConfirmCounting, kConfirmEvApplyFailed, 0);
    CHECK(s.ePhase == kConfirmNone);
    CHECK(s.eAction == kConfirmActClear);

    s = ConfirmPhaseStep(kConfirmAwaitPresent, kConfirmEvTick, 999999);
    CHECK(s.ePhase == kConfirmAwaitPresent);
    CHECK(s.eAction == kConfirmActNone);

    s = ConfirmPhaseStep(kConfirmCounting, kConfirmEvTick, 14999);
    CHECK(s.ePhase == kConfirmCounting);
    CHECK(s.eAction == kConfirmActNone);

    s = ConfirmPhaseStep(kConfirmCounting, kConfirmEvTick, 15000);
    CHECK(s.ePhase == kConfirmNone);
    CHECK(s.eAction == kConfirmActRevert);

    s = ConfirmPhaseStep(kConfirmCounting, kConfirmEvTick, 0xFFFFFFFFu);
    CHECK(s.ePhase == kConfirmNone);
    CHECK(s.eAction == kConfirmActRevert);

    s = ConfirmPhaseStep(kConfirmCounting, kConfirmEvKeep, 14999);
    CHECK(s.ePhase == kConfirmNone);
    CHECK(s.eAction == kConfirmActKeep);

    s = ConfirmPhaseStep(kConfirmCounting, kConfirmEvRevert, 0);
    CHECK(s.ePhase == kConfirmNone);
    CHECK(s.eAction == kConfirmActRevert);

    s = ConfirmPhaseStep(kConfirmAwaitPresent, kConfirmEvKeep, 0);
    CHECK(s.ePhase == kConfirmAwaitPresent);
    CHECK(s.eAction == kConfirmActNone);

    s = ConfirmPhaseStep(kConfirmAwaitPresent, kConfirmEvRevert, 0);
    CHECK(s.ePhase == kConfirmAwaitPresent);
    CHECK(s.eAction == kConfirmActNone);

    for (int e = kConfirmEvArm; e <= kConfirmEvTick; e++)
    {
        if (e == kConfirmEvArm)
        {
            continue;
        }
        s = ConfirmPhaseStep(kConfirmNone, (ConfirmEvent)e, 0xFFFFFFFFu);
        CHECK(s.ePhase == kConfirmNone);
        CHECK(s.eAction == kConfirmActNone);
    }
}

TEST_CASE("display logic cursor plan" * doctest::test_suite("unit"))
{
    const DisplayMode aModes[3] = {kModeFullscreen, kModeWindowed, kModeBorderless};
    int iRows = 0;
    for (int m = 0; m < 3; ++m)
    {
        for (int i = 0; i < 16; ++i)
        {
            const DisplayMode eMode = aModes[m];
            const bool bShown = (i & 1) != 0;
            const bool bActive = (i & 2) != 0;
            const bool bSoftware = (i & 4) != 0;
            const bool bNoRender = (i & 8) != 0;
            CAPTURE(eMode);
            CAPTURE(bShown);
            CAPTURE(bActive);
            CAPTURE(bSoftware);
            CAPTURE(bNoRender);
            bool bHcursor = false;
            bool bExclusive = true;
            bool bClip = false;
            bool bHideDrawn = false;
            if (!bSoftware)
            {
                bHideDrawn = true;
                if (bShown)
                {
                    bExclusive = false;
                    bHcursor = !bNoRender;
                    bClip = !bNoRender && bActive && eMode == kModeFullscreen;
                }
            }
            const CursorPlan p = CursorPlanFor(bShown, eMode, bActive, bSoftware, bNoRender);
            CHECK(p.bHcursor == bHcursor);
            CHECK(p.bExclusive == bExclusive);
            CHECK(p.bClip == bClip);
            CHECK(p.bHideDrawn == bHideDrawn);
            ++iRows;
        }
    }
    CHECK(iRows == 48);

    CursorPlan p = CursorPlanFor(true, kModeWindowed, true, false, false);
    CHECK_FALSE(p.bClip);
    CHECK(p.bHcursor);
    CHECK_FALSE(p.bExclusive);
    CHECK(p.bHideDrawn);

    p = CursorPlanFor(true, kModeBorderless, true, false, false);
    CHECK_FALSE(p.bClip);

    p = CursorPlanFor(true, kModeFullscreen, true, false, false);
    CHECK(p.bClip);
    CHECK(p.bHcursor);
    CHECK_FALSE(p.bExclusive);

    p = CursorPlanFor(true, kModeFullscreen, false, false, false);
    CHECK_FALSE(p.bClip);

    p = CursorPlanFor(true, kModeFullscreen, true, false, true);
    CHECK_FALSE(p.bHcursor);
    CHECK_FALSE(p.bClip);
    CHECK_FALSE(p.bExclusive);

    p = CursorPlanFor(true, kModeBorderless, true, true, false);
    CHECK_FALSE(p.bHcursor);
    CHECK(p.bExclusive);
    CHECK_FALSE(p.bClip);
    CHECK_FALSE(p.bHideDrawn);

    p = CursorPlanFor(false, kModeFullscreen, true, false, false);
    CHECK_FALSE(p.bHcursor);
    CHECK(p.bExclusive);
    CHECK_FALSE(p.bClip);
    CHECK(p.bHideDrawn);
}

TEST_CASE("display logic cursor button latch" * doctest::test_suite("unit"))
{
    unsigned int uiLatched = 0;
    CHECK(CursorButtonLatch(0x1, 0, true, uiLatched) == 0);
    CHECK(uiLatched == 0x1);
    CHECK(CursorButtonLatch(0x1, 0x1, false, uiLatched) == 0);
    CHECK(uiLatched == 0x1);
    CHECK(CursorButtonLatch(0, 0x1, false, uiLatched) == 0);
    CHECK(uiLatched == 0);
    CHECK(CursorButtonLatch(0x1, 0, false, uiLatched) == 0x1);
    CHECK(uiLatched == 0);

    uiLatched = 0;
    CHECK(CursorButtonLatch(0x2, 0x2, true, uiLatched) == 0x2);
    CHECK(uiLatched == 0);
    CHECK(CursorButtonLatch(0x3, 0x2, true, uiLatched) == 0x2);
    CHECK(uiLatched == 0x1);
}

TEST_CASE("display logic cursor size" * doctest::test_suite("unit"))
{
    unsigned int w = 0;
    unsigned int h = 0;
    CursorSize(16, 16, 800, 600, 800, 600, w, h);
    CHECK(w == 16);
    CHECK(h == 16);
    CursorSize(16, 16, 800, 600, 1920, 1080, w, h);
    CHECK(w == 38);
    CHECK(h == 29);
    CursorSize(16, 16, 3840, 2160, 3840, 2160, w, h);
    CHECK(w == 16);
    CHECK(h == 16);
    CursorSize(16, 16, 800, 600, 0, 0, w, h);
    CHECK(w == 16);
    CHECK(h == 16);
    CursorSize(16, 16, 0, 600, 1920, 1080, w, h);
    CHECK(w == 16);
    CHECK(h == 16);
    CursorSize(16, 16, 1, 1, 1000, 1000, w, h);
    CHECK(w == kCursorMaxDim);
    CHECK(h == kCursorMaxDim);
    CHECK(kCursorMaxDim == 256);
    CursorSize(0, 0, 800, 600, 800, 600, w, h);
    CHECK(w == 1);
    CHECK(h == 1);
    CursorSize(16, 16, 1024, 768, 512, 384, w, h);
    CHECK(w == 8);
    CHECK(h == 8);
    CursorSize(0xFFFFFFFFu, 0xFFFFFFFFu, 1, 1, 0xFFFFFFFFu, 0xFFFFFFFFu, w, h);
    CHECK(w == kCursorMaxDim);
    CHECK(h == kCursorMaxDim);
}

TEST_CASE("display logic cursor scale" * doctest::test_suite("unit"))
{
    const unsigned int aRow[2] = {0xFFFFFFFFu, 0x00FF0000u};
    unsigned int aRowOut[4] = {0, 0, 0, 0};
    CursorScaleBgra(aRow, 2, 1, aRowOut, 4, 1);
    CHECK(aRowOut[0] == 0xFFFFFFFFu);
    CHECK(aRowOut[1] == 0xBFFFFFFFu);
    CHECK(aRowOut[2] == 0x40FFFFFFu);
    CHECK(aRowOut[3] == 0x00000000u);

    const unsigned int aCol[2] = {0xFF0000FFu, 0x00000000u};
    unsigned int aColOut[4] = {1, 1, 1, 1};
    CursorScaleBgra(aCol, 1, 2, aColOut, 1, 4);
    CHECK(aColOut[0] == 0xFF0000FFu);
    CHECK(aColOut[1] == 0xBF0000FFu);
    CHECK(aColOut[2] == 0x400000FFu);
    CHECK(aColOut[3] == 0x00000000u);

    unsigned int aDown[16];
    memset(aDown, 0, sizeof(aDown));
    aDown[0] = 0xFFFFFFFFu;
    unsigned int aDownOut[4] = {1, 1, 1, 1};
    CursorScaleBgra(aDown, 4, 4, aDownOut, 2, 2);
    CHECK(aDownOut[0] == 0x40FFFFFFu);
    CHECK(aDownOut[1] == 0x00000000u);
    CHECK(aDownOut[2] == 0x00000000u);
    CHECK(aDownOut[3] == 0x00000000u);

    std::vector<unsigned int> vFlat(7 * 5, 0xFF336699u);
    std::vector<unsigned int> vFlatOut(38 * 29, 0);
    CursorScaleBgra(&vFlat[0], 7, 5, &vFlatOut[0], 38, 29);
    for (unsigned int i = 0; i < vFlatOut.size(); ++i)
    {
        CAPTURE(i);
        CHECK(vFlatOut[i] == 0xFF336699u);
    }

    const unsigned int aExact[4] = {0xFF112233u, 0x80445566u, 0x01ABCDEFu, 0x00778899u};
    unsigned int aExactOut[4] = {1, 1, 1, 1};
    CursorScaleBgra(aExact, 2, 2, aExactOut, 2, 2);
    CHECK(aExactOut[0] == 0xFF112233u);
    CHECK(aExactOut[1] == 0x80445566u);
    CHECK(aExactOut[2] == 0x01ABCDEFu);
    CHECK(aExactOut[3] == 0x00000000u);

    std::vector<unsigned int> vSrc(16 * 16);
    for (unsigned int i = 0; i < vSrc.size(); ++i)
    {
        vSrc[i] = 0x01000000u + i;
    }
    std::vector<unsigned int> vDst(38 * 29, 0xDEADBEEFu);
    CursorScaleBgra(&vSrc[0], 16, 16, &vDst[0], 38, 29);
    CHECK(vDst[0] == vSrc[0]);
    CHECK(vDst[28 * 38 + 37] == vSrc[15 * 16 + 15]);
    for (unsigned int i = 0; i < vDst.size(); ++i)
    {
        CHECK(vDst[i] != 0xDEADBEEFu);
    }

    std::vector<unsigned int> vCopy(16 * 16, 0);
    CursorScaleBgra(&vSrc[0], 16, 16, &vCopy[0], 16, 16);
    CHECK(vCopy == vSrc);

    unsigned int uiUntouched = 0x12345678u;
    CursorScaleBgra(&vSrc[0], 0, 16, &uiUntouched, 1, 1);
    CHECK(uiUntouched == 0x12345678u);
    CursorScaleBgra(0, 16, 16, &uiUntouched, 1, 1);
    CHECK(uiUntouched == 0x12345678u);
    CursorScaleBgra(&vSrc[0], 16, 16, &uiUntouched, 0, 1);
    CHECK(uiUntouched == 0x12345678u);
}

TEST_CASE("display logic cursor mask" * doctest::test_suite("unit"))
{
    CHECK(CursorMaskStride(16) == 4);
    CHECK(CursorMaskStride(38) == 8);
    CHECK(CursorMaskStride(1) == 4);
    CHECK(CursorMaskStride(32) == 4);
    CHECK(CursorMaskStride(33) == 8);

    std::vector<unsigned int> vImg(16 * 16, 0);
    vImg[1 * 16 + 3] = 0xFF000000u;
    vImg[15 * 16 + 13] = 0xFF000000u;
    unsigned char aMask[4 * 16];
    memset(aMask, 0xAA, sizeof(aMask));
    CursorMaskFromBgra(&vImg[0], 16, 16, aMask);
    for (int y = 0; y < 16; ++y)
    {
        CAPTURE(y);
        const unsigned char ucB0 = (y == 1) ? 0xEF : 0xFF;
        const unsigned char ucB1 = (y == 15) ? 0xFB : 0xFF;
        CHECK(aMask[y * 4 + 0] == ucB0);
        CHECK(aMask[y * 4 + 1] == ucB1);
        CHECK(aMask[y * 4 + 2] == 0);
        CHECK(aMask[y * 4 + 3] == 0);
    }

    const unsigned int aAlpha[2] = {0x00FFFFFFu, 0x01000000u};
    unsigned char aMask2[4];
    memset(aMask2, 0, sizeof(aMask2));
    CursorMaskFromBgra(aAlpha, 2, 1, aMask2);
    CHECK(aMask2[0] == 0x80);
    CHECK(aMask2[1] == 0);
}

TEST_CASE("display logic ui scale" * doctest::test_suite("unit"))
{
    // stock size up to 1080p, so nothing changes for existing setups
    CHECK(UiScale(0) == 1.0f);
    CHECK(UiScale(600) == 1.0f);
    CHECK(UiScale(1080) == 1.0f);
    // grows with the height above that
    CHECK(UiScale(1440) == doctest::Approx(1440.0f / 1080.0f));
    CHECK(UiScale(2160) == 2.0f);
    CHECK(UiScale(4320) == 4.0f);
    CHECK(UiScale(1200) > 1.0f);
    CHECK(UiScale(1440) < UiScale(2160));
    // nonsense heights fall back to stock
    CHECK(UiScale(kMaxDim + 1) == 1.0f);
}

TEST_CASE("display logic ui size" * doctest::test_suite("unit"))
{
    unsigned int w = 0;
    unsigned int h = 0;
    // 1080p and below are untouched (the shell's 800x600 too)
    UiSize(800, 600, w, h);
    CHECK(w == 800);
    CHECK(h == 600);
    UiSize(1920, 1080, w, h);
    CHECK(w == 1920);
    CHECK(h == 1080);
    // higher resolutions lay out as 1080p
    UiSize(3840, 2160, w, h);
    CHECK(w == 1920);
    CHECK(h == 1080);
    UiSize(2560, 1440, w, h);
    CHECK(w == 1920);
    CHECK(h == 1080);
    // ultrawide keeps its aspect
    UiSize(3440, 1440, w, h);
    CHECK(w == 2580);
    CHECK(h == 1080);
    UiSize(5120, 2160, w, h);
    CHECK(w == 2560);
    CHECK(h == 1080);
}
