#include "third_party/doctest/doctest.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "testutil.h"
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"

using namespace DisplayCfg;

namespace
{

const char* IniPath()
{
    static char acPath[MAX_PATH];
    GetFullPathNameA("display_settings_test.ini", MAX_PATH, acPath, 0);
    return acPath;
}

void WriteFile(const char* pcPath, const char* pcText, size_t uiLen)
{
    FILE* pFile = fopen(pcPath, "wb");
    if (pFile)
    {
        fwrite(pcText, 1, uiLen, pFile);
        fclose(pFile);
    }
}

void WriteText(const char* pcText)
{
    WriteFile(IniPath(), pcText, strlen(pcText));
}

void Cleanup()
{
    char acTemp[MAX_PATH + 8];
    SetFileAttributesA(IniPath(), FILE_ATTRIBUTE_NORMAL);
    DeleteFileA(IniPath());
    sprintf(acTemp, "%s.tmp", IniPath());
    SetFileAttributesA(acTemp, FILE_ATTRIBUTE_NORMAL);
    DeleteFileA(acTemp);
}

bool Exists(const char* pcPath)
{
    return GetFileAttributesA(pcPath) != INVALID_FILE_ATTRIBUTES;
}

size_t ReadText(char* pcOut, size_t uiSize)
{
    size_t uiLen = 0;
    FILE* pFile = fopen(IniPath(), "rb");
    if (pFile)
    {
        uiLen = fread(pcOut, 1, uiSize - 1, pFile);
        fclose(pFile);
    }
    pcOut[uiLen] = '\0';
    return uiLen;
}

}

TEST_CASE("display settings parse uint is strict" * doctest::test_suite("unit"))
{
    unsigned int uiOut = 77;
    CHECK(ParseUInt("12", 0, 15, uiOut));
    CHECK(uiOut == 12);
    uiOut = 77;
    CHECK(!ParseUInt(0, 0, 15, uiOut));
    CHECK(!ParseUInt("", 0, 15, uiOut));
    CHECK(!ParseUInt("abc", 0, 15, uiOut));
    CHECK(!ParseUInt("-1", 0, 15, uiOut));
    CHECK(!ParseUInt("+1", 0, 15, uiOut));
    CHECK(!ParseUInt("1x", 0, 15, uiOut));
    CHECK(!ParseUInt(" 1", 0, 15, uiOut));
    CHECK(!ParseUInt("99999999999", 0, 0xFFFFFFFFu, uiOut));
    CHECK(!ParseUInt("16", 0, 15, uiOut));
    CHECK(uiOut == 77);
}

TEST_CASE("display settings parse bool and vsync" * doctest::test_suite("unit"))
{
    bool bOut = false;
    CHECK(ParseBool("ON", bOut));
    CHECK(bOut);
    CHECK(ParseBool("False", bOut));
    CHECK(!bOut);
    CHECK(!ParseBool("yes", bOut));
    CHECK(!ParseBool("", bOut));
    unsigned int uiOut = 9;
    CHECK(ParseVsync("half", uiOut));
    CHECK(uiOut == 2);
    CHECK(ParseVsync("off", uiOut));
    CHECK(uiOut == 0);
    CHECK(!ParseVsync("3", uiOut));
}

TEST_CASE("display settings defaults" * doctest::test_suite("unit"))
{
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    CHECK(kSettings.uiVsync == 1);
    CHECK(kSettings.uiFps == 0);
    CHECK(kSettings.uiRefresh == 0);
    CHECK(kSettings.uiAdapter == 0);
    CHECK(!kSettings.bBorderless);
    CHECK_FALSE(kSettings.bSoftwareCursor);
    CHECK(kSettings.uiSet == 0);
}

TEST_CASE("display settings default lets frames queue" * doctest::test_suite("unit"))
{
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    CHECK(kSettings.bNoWaitQuery);
    CHECK((kSettings.uiSet & kNoWaitQuery) == 0);
    CHECK(SetFromText(kSettings, kNoWaitQuery, "0", "test"));
    CHECK(!kSettings.bNoWaitQuery);
    CHECK(SetFromText(kSettings, kNoWaitQuery, "1", "test"));
    CHECK(kSettings.bNoWaitQuery);
    SetDefaults(kSettings);
    kSettings.bNoWaitQuery = false;
    SetFlag(kSettings, kNoWaitQuery);
    CHECK(kSettings.bNoWaitQuery);

    Cleanup();
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
    WriteText("[Display]\r\nNoWaitQuery=0\r\n");
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    CHECK(!kLoaded.bNoWaitQuery);

    WriteText("[Display]\r\nFps=60\r\n");
    DisplaySettings kMark;
    SetDefaults(kMark);
    DisplaySettings kNow;
    SetDefaults(kNow);
    unsigned int uiWritten = 99;
    CHECK(SaveMenu(kMark, kNow, IniPath(), true, uiWritten) == kSaveOk);
    CHECK(uiWritten == 0);
    char acAfter[512];
    ReadText(acAfter, sizeof(acAfter));
    CHECK(strstr(acAfter, "NoWaitQuery=") == static_cast<char*>(0));

    kNow.bNoWaitQuery = false;
    CHECK(SaveMenu(kMark, kNow, IniPath(), true, uiWritten) == kSaveOk);
    CHECK(uiWritten == kNoWaitQuery);
    ReadText(acAfter, sizeof(acAfter));
    CHECK(strstr(acAfter, "NoWaitQuery=0") != static_cast<char*>(0));
    Cleanup();
}

TEST_CASE("display settings bad value keeps default and logs once" * doctest::test_suite("unit"))
{
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    LogCapture capture;
    CHECK(!SetFromText(kSettings, kRefresh, "abc", "command line"));
    CHECK(capture.Contains("display: bad Refresh 'abc'"));
    CHECK(kSettings.uiRefresh == 0);
    CHECK((kSettings.uiSet & kRefresh) == 0);
    CHECK(!SetFromText(kSettings, kAdapter, "16", "command line"));
    CHECK(kSettings.uiAdapter == 0);
    CHECK(SetFromText(kSettings, kFps, "120", "command line"));
    CHECK(kSettings.uiFps == 120);
    CHECK((kSettings.uiSet & kFps) != 0);
}

TEST_CASE("display settings missing ini is silent" * doctest::test_suite("unit"))
{
    Cleanup();
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    LogCapture capture;
    LoadIni(kSettings, IniPath());
    CHECK(!capture.Contains("display:"));
    CHECK(kSettings.uiSet == 0);
    CHECK(kSettings.uiFps == 0);
}

TEST_CASE("display settings ini loads values" * doctest::test_suite("unit"))
{
    WriteText("[Display]\r\nRefresh=60\r\nVsync=half\r\nAdapterName=\\\\.\\DISPLAY2\r\nBorderless=1\r\nSoftwareCursor=1\r\n");
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    LoadIni(kSettings, IniPath());
    Cleanup();
    CHECK(kSettings.uiRefresh == 60);
    CHECK(kSettings.uiVsync == 2);
    CHECK(strcmp(kSettings.acAdapterName, "\\\\.\\DISPLAY2") == 0);
    CHECK(kSettings.bBorderless);
    CHECK(kSettings.bSoftwareCursor);
    CHECK((kSettings.uiSet & kSoftwareCursor) != 0);
}

TEST_CASE("display settings garbage ini loads nothing" * doctest::test_suite("unit"))
{
    char acGarbage[5120];
    unsigned int uiSeed = 12345;
    for (size_t i = 0; i < sizeof(acGarbage); ++i)
    {
        uiSeed = uiSeed * 1664525u + 1013904223u;
        acGarbage[i] = (char)(1 + ((uiSeed >> 16) % 254));
        if (acGarbage[i] == '[' || acGarbage[i] == '=')
        {
            acGarbage[i] = 'x';
        }
    }
    WriteFile(IniPath(), acGarbage, sizeof(acGarbage));
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    LoadIni(kSettings, IniPath());
    Cleanup();
    CHECK(kSettings.uiSet == 0);
    CHECK(kSettings.uiFps == 0);
}

TEST_CASE("display settings bad ini keys warn and keep defaults" * doctest::test_suite("unit"))
{
    char acBig[6000];
    strcpy(acBig, "[Display]\r\nRefresh=");
    size_t uiLen = strlen(acBig);
    memset(acBig + uiLen, 'x', 5000);
    strcpy(acBig + uiLen + 5000, "\r\nFps=abc\r\nAdapter=99\r\n");
    WriteText(acBig);
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    LogCapture capture;
    LoadIni(kSettings, IniPath());
    Cleanup();
    CHECK(capture.Contains("display: bad Refresh"));
    CHECK(capture.Contains("display: bad Fps 'abc'"));
    CHECK(capture.Contains("display: bad Adapter '99'"));
    CHECK(kSettings.uiSet == 0);
    CHECK(kSettings.uiFps == 0);
}

TEST_CASE("display settings merge precedence" * doctest::test_suite("unit"))
{
    DisplaySettings kIni;
    SetDefaults(kIni);
    SetFromText(kIni, kRefresh, "60", "display.ini");
    SetFromText(kIni, kFps, "100", "display.ini");
    SetFromText(kIni, kSoftwareCursor, "0", "display.ini");
    DisplaySettings kCli;
    SetDefaults(kCli);
    SetFromText(kCli, kRefresh, "75", "command line");
    SetFlag(kCli, kSoftwareCursor);
    DisplaySettings kFinal = kIni;
    Merge(kFinal, kCli);
    CHECK(kFinal.uiRefresh == 75);
    CHECK(kFinal.uiFps == 100);
    CHECK(kFinal.uiVsync == 1);
    CHECK((kFinal.uiCliMask & kRefresh) != 0);
    CHECK((kFinal.uiCliMask & kFps) == 0);
    CHECK(kFinal.bSoftwareCursor);
    CHECK((kFinal.uiCliMask & kSoftwareCursor) != 0);
}

TEST_CASE("display settings NoSonyPad from ini and command line" * doctest::test_suite("unit"))
{
    DisplaySettings kDefaults;
    SetDefaults(kDefaults);
    CHECK_FALSE(kDefaults.bNoSonyPad);

    WriteText("[Display]\r\nNoSonyPad=1\r\n");
    DisplaySettings kIni;
    SetDefaults(kIni);
    LoadIni(kIni, IniPath());
    Cleanup();
    CHECK(kIni.bNoSonyPad);
    CHECK((kIni.uiSet & kNoSonyPad) != 0);

    DisplaySettings kOff;
    SetDefaults(kOff);
    SetFromText(kOff, kNoSonyPad, "off", "display.ini");
    DisplaySettings kCli;
    SetDefaults(kCli);
    SetFlag(kCli, kNoSonyPad);
    Merge(kOff, kCli);
    CHECK(kOff.bNoSonyPad);
    CHECK((kOff.uiCliMask & kNoSonyPad) != 0);
}

TEST_CASE("display settings save gate refuses without a good frame" * doctest::test_suite("unit"))
{
    Cleanup();
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    SetFromText(kSettings, kRefresh, "144", "test");
    CHECK(!SaveIni(kSettings, IniPath(), false));
    CHECK(!Exists(IniPath()));
}

TEST_CASE("display settings save round trips and leaves no temp" * doctest::test_suite("unit"))
{
    Cleanup();
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    SetFromText(kSettings, kRefresh, "144", "test");
    SetFromText(kSettings, kVsync, "half", "test");
    SetFromText(kSettings, kAdapterName, "\\\\.\\DISPLAY3", "test");
    SetFlag(kSettings, kBorderless);
    CHECK(SaveIni(kSettings, IniPath(), true));
    char acTemp[MAX_PATH + 8];
    sprintf(acTemp, "%s.tmp", IniPath());
    CHECK(!Exists(acTemp));
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    Cleanup();
    CHECK(kLoaded.uiRefresh == 144);
    CHECK(kLoaded.uiVsync == 2);
    CHECK(strcmp(kLoaded.acAdapterName, "\\\\.\\DISPLAY3") == 0);
    CHECK(kLoaded.bBorderless);
    CHECK(kLoaded.uiFps == 0);
}

TEST_CASE("display settings save skips command line values" * doctest::test_suite("unit"))
{
    WriteText("[Display]\r\nRefresh=60\r\nFps=100\r\n");
    DisplaySettings kIni;
    SetDefaults(kIni);
    LoadIni(kIni, IniPath());
    DisplaySettings kCli;
    SetDefaults(kCli);
    SetFromText(kCli, kRefresh, "75", "command line");
    SetFromText(kCli, kFps, "30", "command line");
    SetFlag(kCli, kSoftwareCursor);
    DisplaySettings kFinal = kIni;
    Merge(kFinal, kCli);
    SetFromText(kFinal, kVsync, "0", "test");
    CHECK(SaveIni(kFinal, IniPath(), true));
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    Cleanup();
    CHECK(kLoaded.uiRefresh == 60);
    CHECK(kLoaded.uiFps == 100);
    CHECK(kLoaded.uiVsync == 0);
    CHECK((kLoaded.uiSet & kSoftwareCursor) == 0);
    CHECK(!kLoaded.bSoftwareCursor);
}

TEST_CASE("display settings save never writes unset keys" * doctest::test_suite("unit"))
{
    Cleanup();
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    SetFromText(kSettings, kRefresh, "144", "test");
    CHECK(SaveIni(kSettings, IniPath(), true));
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    Cleanup();
    CHECK(kLoaded.uiRefresh == 144);
    CHECK((kLoaded.uiSet & kVsync) == 0);
    CHECK((kLoaded.uiSet & kFps) == 0);
    CHECK((kLoaded.uiSet & kAdapter) == 0);
    CHECK((kLoaded.uiSet & kSoftwareCursor) == 0);
}

TEST_CASE("display settings init merges defaults ini and command line" * doctest::test_suite("unit"))
{
    WriteText("[Display]\r\nRefresh=60\r\nFps=100\r\n");
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kRefresh, "75", "command line");
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    Init();
    CHECK(gDisplay.uiRefresh == 75);
    CHECK(gDisplay.uiFps == 100);
    CHECK(gDisplay.uiVsync == 1);
    // borderless unless the ini or command line chose, or -win asked for a window
    CHECK(gDisplay.bBorderless);
    Init(true);
    CHECK_FALSE(gDisplay.bBorderless);
    WriteText("[Display]\r\nBorderless=0\r\n");
    Init();
    CHECK_FALSE(gDisplay.bBorderless);
    WriteText("[Display]\r\nBorderless=1\r\n");
    Init(true);
    CHECK(gDisplay.bBorderless);
    Cleanup();
    SetDefaults(gCliDisplay);
}

TEST_CASE("display settings a normal start is borderless" * doctest::test_suite("regression"))
{
    // "its starting in windowed and not borderless": every normal start below opens
    // borderless, and the log line names who chose it
    Cleanup();
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");

    // no display.ini at all
    Init();
    CHECK(gDisplay.bBorderless);
    CHECK(strcmp(BorderlessSource(false), "default") == 0);

    // a display.ini from an older build has no Borderless key (the launcher's own
    // has just Vsync and Fps), and a broken value counts as none
    WriteText("[Display]\r\nVsync=0\r\nFps=0\r\n");
    Init();
    CHECK(gDisplay.bBorderless);
    CHECK(strcmp(BorderlessSource(false), "default") == 0);
    WriteText("[Display]\r\nBorderless=2\r\nRefresh=60\r\n");
    Init();
    CHECK(gDisplay.bBorderless);
    CHECK(strcmp(BorderlessSource(false), "default") == 0);

    // the launcher's -borderless -nointro -name "<name>" wins over an ini that chose a window
    SetFlag(gCliDisplay, kBorderless);
    WriteText("[Display]\r\nBorderless=0\r\n");
    Init();
    CHECK(gDisplay.bBorderless);
    CHECK(strcmp(BorderlessSource(false), "command line") == 0);
    Init(true);
    CHECK(gDisplay.bBorderless);

    // only a Fullscreen or Windowed pick in the options (Borderless=0) or -win leave it
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    Init();
    CHECK_FALSE(gDisplay.bBorderless);
    CHECK(strcmp(BorderlessSource(false), "display.ini") == 0);
    Cleanup();
    Init(true);
    CHECK_FALSE(gDisplay.bBorderless);
    CHECK(strcmp(BorderlessSource(true), "-win") == 0);
    SetDefaults(gCliDisplay);
}

TEST_CASE("display settings save adapter round trips" * doctest::test_suite("unit"))
{
    Cleanup();
    WriteText("[Display]\r\nRefresh=60\r\n");
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    SetDefaults(gIniDisplay);
    CHECK(SaveAdapter(1, "\\\\.\\DISPLAY2"));
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    CHECK(kLoaded.uiAdapter == 1);
    CHECK(strcmp(kLoaded.acAdapterName, "\\\\.\\DISPLAY2") == 0);
    CHECK(kLoaded.uiRefresh == 60);
    CHECK((kLoaded.uiSet & kVsync) == 0);

    char acLong[80];
    memset(acLong, 'x', 70);
    acLong[70] = '\0';
    LoadIni(gIniDisplay, IniPath());
    CHECK(SaveAdapter(0, acLong));
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    CHECK(kLoaded.uiAdapter == 0);
    CHECK(kLoaded.acAdapterName[0] == '\0');
    CHECK(kLoaded.uiRefresh == 60);

    CHECK(!SaveAdapter(16, 0));
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    CHECK(kLoaded.uiAdapter == 0);
    Cleanup();
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
}

TEST_CASE("display settings save adapter writes only the adapter keys" * doctest::test_suite("unit"))
{
    Cleanup();
    WriteText("[Display]\r\nRefresh=60\r\nVsync=1\r\nFps=80\r\n");
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    Init();
    CHECK(gIniDisplay.uiRefresh == 60);
    WriteText("[Display]\r\nRefresh=75\r\nVsync=0\r\nFps=144\r\nBorderless=1\r\n");
    CHECK(SaveAdapter(1, "\\\\.\\DISPLAY2"));
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    Cleanup();
    CHECK(kLoaded.uiAdapter == 1);
    CHECK(strcmp(kLoaded.acAdapterName, "\\\\.\\DISPLAY2") == 0);
    CHECK(kLoaded.uiRefresh == 75);
    CHECK(kLoaded.uiVsync == 0);
    CHECK(kLoaded.uiFps == 144);
    CHECK(kLoaded.bBorderless);
    CHECK(gIniDisplay.uiAdapter == 1);
    CHECK(gIniDisplay.uiRefresh == 60);
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
    SetDefaults(gDisplay);
}

TEST_CASE("display settings save adapter keeps memory when the save fails" * doctest::test_suite("unit"))
{
    Cleanup();
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, "Z:\\openswbf2-missing-dir\\none\\display.ini", "command line");
    SetDefaults(gIniDisplay);
    SetFromText(gIniDisplay, kAdapter, "0", "test");
    CHECK(!SaveAdapter(1, "\\\\.\\DISPLAY2"));
    CHECK(gIniDisplay.uiAdapter == 0);
    CHECK(gIniDisplay.acAdapterName[0] == '\0');
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
}

TEST_CASE("display settings save adapter skips an unchanged file" * doctest::test_suite("unit"))
{
    Cleanup();
    WriteText("[Display]\r\nAdapter=1\r\nAdapterName=\\\\.\\DISPLAY2\r\nRefresh=60\r\n");
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    SetDefaults(gIniDisplay);
    SetFileAttributesA(IniPath(), FILE_ATTRIBUTE_READONLY);
    CHECK(SaveAdapter(1, "\\\\.\\DISPLAY2"));
    CHECK(gIniDisplay.uiAdapter == 1);
    CHECK(!SaveAdapter(0, "\\\\.\\DISPLAY1"));
    CHECK(gIniDisplay.uiAdapter == 1);
    char acTemp[MAX_PATH + 8];
    sprintf(acTemp, "%s.tmp", IniPath());
    CHECK(!Exists(acTemp));
    SetFileAttributesA(IniPath(), FILE_ATTRIBUTE_NORMAL);
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    Cleanup();
    CHECK(kLoaded.uiAdapter == 1);
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
}

TEST_CASE("display settings save adapter ignores cli mask" * doctest::test_suite("unit"))
{
    Cleanup();
    WriteText("[Display]\r\nAdapter=0\r\nAdapterName=\\\\.\\DISPLAY1\r\n");
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    SetFromText(gCliDisplay, kAdapter, "0", "command line");
    Init();
    CHECK((gDisplay.uiCliMask & kAdapter) != 0);
    CHECK(SaveAdapter(1, 0));
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    Cleanup();
    CHECK(kLoaded.uiAdapter == 1);
    CHECK(kLoaded.acAdapterName[0] == '\0');
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
    SetDefaults(gDisplay);
}

TEST_CASE("display settings pad layout" * doctest::test_suite("unit"))
{
    // Modern unless the ini says otherwise
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    CHECK(kSettings.uiPadLayout == 0);
    CHECK(SetFromText(kSettings, kPadLayout, "Legacy", "test"));
    CHECK(kSettings.uiPadLayout == 1);
    CHECK(SetFromText(kSettings, kPadLayout, "modern", "test"));
    CHECK(kSettings.uiPadLayout == 0);
    CHECK(SetFromText(kSettings, kPadLayout, "1", "test"));
    CHECK(kSettings.uiPadLayout == 1);
    {
        LogCapture capture;
        CHECK_FALSE(SetFromText(kSettings, kPadLayout, "classic", "test"));
    }
    CHECK(kSettings.uiPadLayout == 1);

    // saved on its own, next to the other keys
    Cleanup();
    WriteText("[Display]\r\nBorderless=1\r\nFps=144\r\n");
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    Init();
    CHECK(gDisplay.uiPadLayout == 0);
    CHECK(SavePadLayout(1));
    CHECK(gDisplay.uiPadLayout == 1);
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    char acText[512];
    ReadText(acText, sizeof(acText));
    Cleanup();
    CHECK(kLoaded.uiPadLayout == 1);
    CHECK(kLoaded.bBorderless);
    CHECK(kLoaded.uiFps == 144);
    const bool bWritten = strstr(acText, "PadLayout=legacy") != 0;
    CHECK(bWritten);
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
    SetDefaults(gDisplay);
}

TEST_CASE("display settings pad look speed" * doctest::test_suite("unit"))
{
    // level 4 (the tuned feel) unless the ini says otherwise; 1..10 only
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    CHECK(kSettings.uiPadLookSpeed == 4);
    CHECK((kSettings.uiSet & kPadLookSpeed) == 0);
    CHECK(SetFromText(kSettings, kPadLookSpeed, "7", "test"));
    CHECK(kSettings.uiPadLookSpeed == 7);
    CHECK(SetFromText(kSettings, kPadLookSpeed, "1", "test"));
    CHECK(SetFromText(kSettings, kPadLookSpeed, "10", "test"));
    CHECK(kSettings.uiPadLookSpeed == 10);
    {
        LogCapture capture;
        CHECK_FALSE(SetFromText(kSettings, kPadLookSpeed, "0", "test"));
        CHECK_FALSE(SetFromText(kSettings, kPadLookSpeed, "11", "test"));
        CHECK_FALSE(SetFromText(kSettings, kPadLookSpeed, "fast", "test"));
    }
    CHECK(kSettings.uiPadLookSpeed == 10);

    // saved on its own, next to the other keys
    Cleanup();
    WriteText("[Display]\r\nBorderless=1\r\nPadLayout=legacy\r\n");
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    Init();
    CHECK(gDisplay.uiPadLookSpeed == 4);
    CHECK(SavePadLookSpeed(6));
    CHECK(gDisplay.uiPadLookSpeed == 6);
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    char acText[512];
    ReadText(acText, sizeof(acText));
    Cleanup();
    CHECK(kLoaded.uiPadLookSpeed == 6);
    CHECK(kLoaded.uiPadLayout == 1);
    CHECK(kLoaded.bBorderless);
    const bool bWritten = strstr(acText, "PadLookSpeed=6") != 0;
    CHECK(bWritten);
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
    SetDefaults(gDisplay);
}

TEST_CASE("display settings hud scale and field of view" * doctest::test_suite("unit"))
{
    // 100% and 61 degrees (today's HUD and camera) unless the ini says otherwise
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    CHECK(kSettings.uiHudScale == 100);
    CHECK(kSettings.uiFov == 61);
    CHECK(kSettings.bCrispFonts);
    CHECK(SetFromText(kSettings, kCrispFonts, "0", "test"));
    CHECK_FALSE(kSettings.bCrispFonts);
    CHECK(SetFromText(kSettings, kCrispFonts, "1", "test"));
    CHECK(kSettings.bCrispFonts);
    CHECK((kSettings.uiSet & (kHudScale | kFov)) == 0);
    CHECK(SetFromText(kSettings, kHudScale, "75", "test"));
    CHECK(kSettings.uiHudScale == 75);
    CHECK(SetFromText(kSettings, kFov, "90", "test"));
    CHECK(kSettings.uiFov == 80); // an earlier build saved up to 100; the top is 80 now
    CHECK(SetFromText(kSettings, kFov, "72", "test"));
    CHECK(kSettings.uiFov == 72);
    {
        LogCapture capture;
        CHECK_FALSE(SetFromText(kSettings, kHudScale, "49", "test"));
        CHECK_FALSE(SetFromText(kSettings, kHudScale, "151", "test"));
        CHECK_FALSE(SetFromText(kSettings, kFov, "54", "test"));
        CHECK_FALSE(SetFromText(kSettings, kFov, "101", "test"));
        CHECK_FALSE(SetFromText(kSettings, kFov, "wide", "test"));
    }
    CHECK(kSettings.uiHudScale == 75);
    CHECK(kSettings.uiFov == 72);

    // each saved on its own, next to the other keys
    Cleanup();
    WriteText("[Display]\r\nBorderless=1\r\nPadLookSpeed=6\r\n");
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    Init();
    CHECK(gDisplay.uiHudScale == 100);
    CHECK(gDisplay.uiFov == 61);
    CHECK(SaveHudScale(80));
    CHECK(SaveFov(75));
    CHECK(gDisplay.uiHudScale == 80);
    CHECK(gDisplay.uiFov == 75);
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    char acText[512];
    ReadText(acText, sizeof(acText));
    Cleanup();
    CHECK(kLoaded.uiHudScale == 80);
    CHECK(kLoaded.uiFov == 75);
    CHECK(kLoaded.uiPadLookSpeed == 6);
    CHECK(kLoaded.bBorderless);
    const bool bHud = strstr(acText, "HudScale=80") != 0;
    const bool bFov = strstr(acText, "Fov=75") != 0;
    CHECK(bHud);
    CHECK(bFov);
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
    SetDefaults(gDisplay);
}

TEST_CASE("display settings pending adapter" * doctest::test_suite("unit"))
{
    Cleanup();
    SetDefaults(gCliDisplay);
    SetFromText(gCliDisplay, kDisplayIni, IniPath(), "command line");
    Init();
    const unsigned int uiIniAdapter = gIniDisplay.uiAdapter;
    const unsigned int uiIniSet = gIniDisplay.uiSet;
    const unsigned int uiAdapter = gDisplay.uiAdapter;
    char acIniName[sizeof(gIniDisplay.acAdapterName)];
    char acName[sizeof(gDisplay.acAdapterName)];
    strcpy(acIniName, gIniDisplay.acAdapterName);
    strcpy(acName, gDisplay.acAdapterName);

    SetPendingAdapter(1, "\\\\.\\DISPLAY2");
    CHECK(GetPendingAdapter().bSet);
    CHECK(GetPendingAdapter().uiIndex == 1);
    CHECK(strcmp(GetPendingAdapter().acName, "\\\\.\\DISPLAY2") == 0);
    CHECK(!Exists(IniPath()));
    CHECK(gIniDisplay.uiAdapter == uiIniAdapter);
    CHECK(gIniDisplay.uiSet == uiIniSet);
    CHECK(strcmp(gIniDisplay.acAdapterName, acIniName) == 0);
    CHECK(gDisplay.uiAdapter == uiAdapter);
    CHECK(strcmp(gDisplay.acAdapterName, acName) == 0);

    char acLong[101];
    memset(acLong, 'y', 100);
    acLong[100] = '\0';
    SetPendingAdapter(2, acLong);
    CHECK(GetPendingAdapter().uiIndex == 2);
    CHECK(strlen(GetPendingAdapter().acName) == 63);
    CHECK(strncmp(GetPendingAdapter().acName, acLong, 63) == 0);

    SetPendingAdapter(3, 0);
    CHECK(GetPendingAdapter().bSet);
    CHECK(GetPendingAdapter().uiIndex == 3);
    CHECK(GetPendingAdapter().acName[0] == '\0');
    CHECK(!Exists(IniPath()));

    ClearPendingAdapter();
    CHECK(!GetPendingAdapter().bSet);
    CHECK(GetPendingAdapter().uiIndex == 0);
    CHECK(GetPendingAdapter().acName[0] == '\0');

    ClearPendingAdapter();
    Cleanup();
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
    SetDefaults(gDisplay);
}

TEST_CASE("display settings validate menu fields" * doctest::test_suite("unit"))
{
    MenuFields kLow = {0, 0, 0, 0, true};
    CHECK(ValidateMenu(kLow));
    MenuFields kHigh = {2, kRefreshMax, kVsyncMax, kFpsMax, false};
    CHECK(ValidateMenu(kHigh));
    CHECK(kRefreshMax == 1000);
    CHECK(kFpsMax == 1000);
    CHECK(kVsyncMax == 2);
    CHECK(kAdapterMax == 15);
    MenuFields kBad = kHigh;
    kBad.uiMode = 3;
    CHECK(!ValidateMenu(kBad));
    kBad = kHigh;
    kBad.uiRefresh = 1001;
    CHECK(!ValidateMenu(kBad));
    kBad = kHigh;
    kBad.uiVsync = 3;
    CHECK(!ValidateMenu(kBad));
    kBad = kHigh;
    kBad.uiFps = 1001;
    CHECK(!ValidateMenu(kBad));
    DisplaySettings kSettings;
    SetDefaults(kSettings);
    CHECK(!SetFromText(kSettings, kAdapter, "16", "test"));
    CHECK(SetFromText(kSettings, kAdapter, "15", "test"));
    CHECK(!SetFromText(kSettings, kRefresh, "1001", "test"));
    CHECK(SetFromText(kSettings, kRefresh, "1000", "test"));
    CHECK(!SetFromText(kSettings, kFps, "1001", "test"));
    CHECK(SetFromText(kSettings, kFps, "1000", "test"));
}

TEST_CASE("display settings save menu writes only changed keys" * doctest::test_suite("unit"))
{
    Cleanup();
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
    WriteText("[Display]\r\nFps=60\r\n");
    DisplaySettings kMark;
    SetDefaults(kMark);
    DisplaySettings kNow = kMark;
    kNow.bBorderless = true;
    kNow.uiSet |= kBorderless;
    kNow.uiVsync = 2;
    kNow.uiSet |= kVsync;

    char acBefore[512];
    char acAfter[512];
    ReadText(acBefore, sizeof(acBefore));
    unsigned int uiWritten = 99;
    CHECK(SaveMenu(kMark, kNow, IniPath(), false, uiWritten) == kSaveDeferred);
    CHECK(uiWritten == 0);
    ReadText(acAfter, sizeof(acAfter));
    CHECK(strcmp(acBefore, acAfter) == 0);
    CHECK((gIniDisplay.uiSet & kMenuKeys) == 0);

    uiWritten = 99;
    CHECK(SaveMenu(kMark, kMark, IniPath(), true, uiWritten) == kSaveOk);
    CHECK(uiWritten == 0);
    ReadText(acAfter, sizeof(acAfter));
    CHECK(strcmp(acBefore, acAfter) == 0);

    uiWritten = 99;
    CHECK(SaveMenu(kMark, kMark, IniPath(), false, uiWritten) == kSaveOk);
    CHECK(uiWritten == 0);
    ReadText(acAfter, sizeof(acAfter));
    CHECK(strcmp(acBefore, acAfter) == 0);

    uiWritten = 99;
    CHECK(SaveMenu(kMark, kNow, IniPath(), true, uiWritten) == kSaveOk);
    CHECK(uiWritten == (kBorderless | kVsync));
    DisplaySettings kLoaded;
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    CHECK(kLoaded.bBorderless);
    CHECK(kLoaded.uiVsync == 2);
    CHECK(kLoaded.uiFps == 60);
    CHECK((kLoaded.uiSet & kRefresh) == 0);
    CHECK((kLoaded.uiSet & kNoWaitQuery) == 0);
    ReadText(acAfter, sizeof(acAfter));
    CHECK(strstr(acAfter, "Refresh=") == static_cast<char*>(0));
    CHECK(strstr(acAfter, "NoWaitQuery=") == static_cast<char*>(0));
    CHECK(gIniDisplay.bBorderless);
    CHECK(gIniDisplay.uiVsync == 2);
    CHECK((gIniDisplay.uiSet & (kBorderless | kVsync)) == (kBorderless | kVsync));
    CHECK((gIniDisplay.uiSet & kFps) == 0);

    DisplaySettings kCliMark;
    SetDefaults(kCliMark);
    SetFromText(kCliMark, kFps, "30", "command line");
    kCliMark.uiCliMask |= kFps;
    DisplaySettings kCliNow = kCliMark;
    kCliNow.uiFps = 144;
    uiWritten = 99;
    CHECK(SaveMenu(kCliMark, kCliNow, IniPath(), true, uiWritten) == kSaveOk);
    CHECK(uiWritten == kFps);
    SetDefaults(kLoaded);
    LoadIni(kLoaded, IniPath());
    CHECK(kLoaded.uiFps == 144);
    CHECK(kLoaded.bBorderless);
    CHECK(gIniDisplay.uiFps == 144);
    CHECK((gIniDisplay.uiSet & kFps) != 0);

    DisplaySettings kFailNow = kMark;
    kFailNow.uiRefresh = 144;
    uiWritten = 99;
    CHECK(SaveMenu(kMark, kFailNow, "Z:\\openswbf2-missing-dir\\none\\display.ini", true, uiWritten) == kSaveFailed);
    CHECK(uiWritten == 0);
    CHECK(gIniDisplay.uiRefresh == 0);
    CHECK((gIniDisplay.uiSet & kRefresh) == 0);

    WriteText("[Display]\r\nFps=60\r\n");
    ReadText(acBefore, sizeof(acBefore));
    SetFileAttributesA(IniPath(), FILE_ATTRIBUTE_READONLY);
    uiWritten = 99;
    CHECK(SaveMenu(kMark, kFailNow, IniPath(), true, uiWritten) == kSaveFailed);
    CHECK(uiWritten == 0);
    ReadText(acAfter, sizeof(acAfter));
    CHECK(strcmp(acBefore, acAfter) == 0);
    CHECK((gIniDisplay.uiSet & kRefresh) == 0);
    SetFileAttributesA(IniPath(), FILE_ATTRIBUTE_NORMAL);

    Cleanup();
    SetDefaults(gCliDisplay);
    SetDefaults(gIniDisplay);
}
