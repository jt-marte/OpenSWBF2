#include "third_party/doctest/doctest.h"

#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "testutil.h"
#include "PebbleFL/Common/PblHashTable.h"
#include "RedEngineFL/Graphics/PC/pcDisplayLogic.h"
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"

// OpenSWBF2: the Vulkan (DXVK) / D3D9 renderer choice and its fallback.

namespace
{

const char* RendererIni()
{
    static char acPath[MAX_PATH];
    GetFullPathNameA("renderer_choice_test.ini", MAX_PATH, acPath, 0);
    return acPath;
}

void WriteIni(const char* pcText)
{
    FILE* pFile = fopen(RendererIni(), "wb");
    if (pFile)
    {
        fwrite(pcText, 1, strlen(pcText), pFile);
        fclose(pFile);
    }
}

std::string ReadIni()
{
    std::string sText;
    FILE* pFile = fopen(RendererIni(), "rb");
    if (pFile)
    {
        char acBuf[512];
        size_t uiRead;
        while ((uiRead = fread(acBuf, 1, sizeof(acBuf), pFile)) > 0)
        {
            sText.append(acBuf, uiRead);
        }
        fclose(pFile);
    }
    return sText;
}

void CleanupIni()
{
    SetFileAttributesA(RendererIni(), FILE_ATTRIBUTE_NORMAL);
    DeleteFileA(RendererIni());
    char acTemp[MAX_PATH + 8];
    sprintf(acTemp, "%s.tmp", RendererIni());
    DeleteFileA(acTemp);
}

void ResetGlobals()
{
    DisplayCfg::SetDefaults(DisplayCfg::gCliDisplay);
    DisplayCfg::SetDefaults(DisplayCfg::gIniDisplay);
    DisplayCfg::SetDefaults(DisplayCfg::gDisplay);
}

bool ReadSource(const char* pcPath, std::string& sOut)
{
    FILE* pFile = fopen(pcPath, "rb");
    if (!pFile)
    {
        return false;
    }
    char acBuf[4096];
    size_t uiRead;
    while ((uiRead = fread(acBuf, 1, sizeof(acBuf), pFile)) > 0)
    {
        sOut.append(acBuf, uiRead);
    }
    fclose(pFile);
    std::string sNoCr;
    sNoCr.reserve(sOut.size());
    for (size_t i = 0; i < sOut.size(); ++i)
    {
        if (sOut[i] != '\r')
        {
            sNoCr += sOut[i];
        }
    }
    sOut.swap(sNoCr);
    return true;
}

bool Has(const std::string& sText, const char* pcNeedle)
{
    return sText.find(pcNeedle) != std::string::npos;
}

std::string Between(const std::string& sText, const char* pcBegin, const char* pcEnd)
{
    const size_t uiBegin = sText.find(pcBegin);
    if (uiBegin == std::string::npos)
    {
        return std::string();
    }
    const size_t uiEnd = sText.find(pcEnd, uiBegin + 1);
    if (uiEnd == std::string::npos)
    {
        return std::string();
    }
    return sText.substr(uiBegin, uiEnd - uiBegin);
}

std::string LogText(DisplayLogic::Renderer eChosen, bool bFromCli, DisplayLogic::VulkanFail eFail,
                    unsigned long ulCode)
{
    char acText[128];
    DisplayLogic::RendererLogText(eChosen, bFromCli, eFail, ulCode, acText, sizeof(acText));
    return acText;
}

}

TEST_CASE("renderer defaults to vulkan" * doctest::test_suite("unit"))
{
    DisplayCfg::DisplaySettings kSettings;
    DisplayCfg::SetDefaults(kSettings);
    CHECK(kSettings.uiRenderer == DisplayLogic::kRendererVulkan);
    CHECK((kSettings.uiSet & DisplayCfg::kRenderer) == 0);
    CHECK(DisplayLogic::ChooseRenderer(kSettings.uiRenderer, false) == DisplayLogic::kRendererVulkan);
}

TEST_CASE("renderer setting reads vulkan and d3d9" * doctest::test_suite("unit"))
{
    DisplayCfg::DisplaySettings kSettings;
    DisplayCfg::SetDefaults(kSettings);
    CHECK(DisplayCfg::SetFromText(kSettings, DisplayCfg::kRenderer, "d3d9", "test"));
    CHECK(kSettings.uiRenderer == DisplayLogic::kRendererD3D9);
    CHECK((kSettings.uiSet & DisplayCfg::kRenderer) != 0);
    CHECK(DisplayCfg::SetFromText(kSettings, DisplayCfg::kRenderer, "Vulkan", "test"));
    CHECK(kSettings.uiRenderer == DisplayLogic::kRendererVulkan);
    CHECK(DisplayCfg::SetFromText(kSettings, DisplayCfg::kRenderer, "1", "test"));
    CHECK(kSettings.uiRenderer == DisplayLogic::kRendererD3D9);
    CHECK(DisplayCfg::SetFromText(kSettings, DisplayCfg::kRenderer, "0", "test"));
    CHECK(kSettings.uiRenderer == DisplayLogic::kRendererVulkan);
    CHECK(DisplayCfg::SetFromText(kSettings, DisplayCfg::kRenderer, "D3D9", "test"));
    {
        LogCapture capture;
        CHECK_FALSE(DisplayCfg::SetFromText(kSettings, DisplayCfg::kRenderer, "opengl", "test"));
        CHECK_FALSE(DisplayCfg::SetFromText(kSettings, DisplayCfg::kRenderer, "2", "test"));
        CHECK_FALSE(DisplayCfg::SetFromText(kSettings, DisplayCfg::kRenderer, "", "test"));
        CHECK_FALSE(DisplayCfg::SetFromText(kSettings, DisplayCfg::kRenderer, 0, "test"));
        CHECK(capture.Contains("display: bad Renderer 'opengl'"));
    }
    CHECK(kSettings.uiRenderer == DisplayLogic::kRendererD3D9);
}

TEST_CASE("renderer absent or bad in display.ini means vulkan" * doctest::test_suite("unit"))
{
    CleanupIni();
    WriteIni("[Display]\r\nBorderless=1\r\n");
    DisplayCfg::DisplaySettings kLoaded;
    DisplayCfg::SetDefaults(kLoaded);
    DisplayCfg::LoadIni(kLoaded, RendererIni());
    CHECK(DisplayLogic::ChooseRenderer(kLoaded.uiRenderer, false) == DisplayLogic::kRendererVulkan);

    WriteIni("[Display]\r\nRenderer=directx\r\n");
    DisplayCfg::SetDefaults(kLoaded);
    {
        LogCapture capture;
        DisplayCfg::LoadIni(kLoaded, RendererIni());
        CHECK(capture.Contains("display: bad Renderer 'directx' from display.ini"));
    }
    CHECK(DisplayLogic::ChooseRenderer(kLoaded.uiRenderer, false) == DisplayLogic::kRendererVulkan);

    WriteIni("[Display]\r\nRenderer=d3d9\r\n");
    DisplayCfg::SetDefaults(kLoaded);
    DisplayCfg::LoadIni(kLoaded, RendererIni());
    CleanupIni();
    CHECK(DisplayLogic::ChooseRenderer(kLoaded.uiRenderer, false) == DisplayLogic::kRendererD3D9);
}

TEST_CASE("renderer switches override display.ini for one run" * doctest::test_suite("unit"))
{
    // -vulkan over Renderer=d3d9, and the switch is never written back
    CleanupIni();
    WriteIni("[Display]\r\nRenderer=d3d9\r\nFps=100\r\n");
    ResetGlobals();
    DisplayCfg::SetFromText(DisplayCfg::gCliDisplay, DisplayCfg::kDisplayIni, RendererIni(), "command line");
    DisplayCfg::SetFromText(DisplayCfg::gCliDisplay, DisplayCfg::kRenderer, "vulkan", "command line");
    DisplayCfg::Init();
    CHECK(DisplayCfg::gIniDisplay.uiRenderer == DisplayLogic::kRendererD3D9);
    CHECK(DisplayCfg::gDisplay.uiRenderer == DisplayLogic::kRendererVulkan);
    CHECK((DisplayCfg::gDisplay.uiCliMask & DisplayCfg::kRenderer) != 0);
    CHECK(DisplayLogic::ChooseRenderer(DisplayCfg::gDisplay.uiRenderer, false) == DisplayLogic::kRendererVulkan);
    CHECK(DisplayCfg::SaveIni(DisplayCfg::gDisplay, RendererIni(), true));
    CHECK(Has(ReadIni(), "Renderer=d3d9"));

    // -d3d9 over the default
    CleanupIni();
    ResetGlobals();
    DisplayCfg::SetFromText(DisplayCfg::gCliDisplay, DisplayCfg::kDisplayIni, RendererIni(), "command line");
    DisplayCfg::SetFromText(DisplayCfg::gCliDisplay, DisplayCfg::kRenderer, "d3d9", "command line");
    DisplayCfg::Init();
    CHECK(DisplayLogic::ChooseRenderer(DisplayCfg::gDisplay.uiRenderer, false) == DisplayLogic::kRendererD3D9);

    // both switches: the last one wins, as for the other display switches
    DisplayCfg::DisplaySettings kCli;
    DisplayCfg::SetDefaults(kCli);
    DisplayCfg::SetFromText(kCli, DisplayCfg::kRenderer, "d3d9", "command line");
    DisplayCfg::SetFromText(kCli, DisplayCfg::kRenderer, "vulkan", "command line");
    DisplayCfg::DisplaySettings kFinal;
    DisplayCfg::SetDefaults(kFinal);
    DisplayCfg::SetFromText(kFinal, DisplayCfg::kRenderer, "d3d9", "display.ini");
    DisplayCfg::Merge(kFinal, kCli);
    CHECK(kFinal.uiRenderer == DisplayLogic::kRendererVulkan);
    CleanupIni();
    ResetGlobals();
}

TEST_CASE("renderer headless runs never load dxvk" * doctest::test_suite("unit"))
{
    CHECK(DisplayLogic::ChooseRenderer(DisplayLogic::kRendererVulkan, true) == DisplayLogic::kRendererNone);
    CHECK(DisplayLogic::ChooseRenderer(DisplayLogic::kRendererD3D9, true) == DisplayLogic::kRendererNone);
    CHECK(DisplayLogic::ChooseRenderer(7, true) == DisplayLogic::kRendererNone);
    CHECK(DisplayLogic::ChooseRenderer(7, false) == DisplayLogic::kRendererVulkan);
}

TEST_CASE("renderer save writes only its key and keeps the running choice" * doctest::test_suite("unit"))
{
    CleanupIni();
    WriteIni("[Display]\r\nBorderless=1\r\nHudScale=80\r\n");
    ResetGlobals();
    DisplayCfg::SetFromText(DisplayCfg::gCliDisplay, DisplayCfg::kDisplayIni, RendererIni(), "command line");
    DisplayCfg::Init();
    CHECK(DisplayCfg::gDisplay.uiRenderer == DisplayLogic::kRendererVulkan);
    CHECK(DisplayCfg::SaveRenderer(DisplayLogic::kRendererD3D9));
    CHECK(DisplayCfg::gIniDisplay.uiRenderer == DisplayLogic::kRendererD3D9);
    CHECK((DisplayCfg::gIniDisplay.uiSet & DisplayCfg::kRenderer) != 0);
    // the running renderer was picked at startup and stays
    CHECK(DisplayCfg::gDisplay.uiRenderer == DisplayLogic::kRendererVulkan);
    const std::string sText = ReadIni();
    CHECK(Has(sText, "Renderer=d3d9"));
    CHECK(Has(sText, "Borderless=1"));
    CHECK(Has(sText, "HudScale=80"));

    CHECK(DisplayCfg::SaveRenderer(DisplayLogic::kRendererVulkan));
    DisplayCfg::DisplaySettings kLoaded;
    DisplayCfg::SetDefaults(kLoaded);
    DisplayCfg::LoadIni(kLoaded, RendererIni());
    CHECK(Has(ReadIni(), "Renderer=vulkan"));
    CHECK(kLoaded.uiRenderer == DisplayLogic::kRendererVulkan);
    CHECK((kLoaded.uiSet & DisplayCfg::kRenderer) != 0);
    CHECK(kLoaded.uiHudScale == 80);
    CleanupIni();
    ResetGlobals();
}

TEST_CASE("vulkan fallback steps in order" * doctest::test_suite("unit"))
{
    using namespace DisplayLogic;
    CHECK(VulkanCheck(true, true, true, true, 1, S_OK) == kVulkanOk);
    CHECK(VulkanCheck(true, true, true, true, 2, S_OK) == kVulkanOk);
    CHECK(VulkanCheck(false, true, true, true, 1, S_OK) == kVulkanNoDll);
    CHECK(VulkanCheck(false, false, false, false, 0, E_FAIL) == kVulkanNoDll);
    CHECK(VulkanCheck(true, false, false, false, 0, S_OK) == kVulkanLoadFailed);
    CHECK(VulkanCheck(true, true, false, false, 0, S_OK) == kVulkanNoEntry);
    CHECK(VulkanCheck(true, true, true, false, 0, S_OK) == kVulkanNoInterface);
    CHECK(VulkanCheck(true, true, true, true, 0, S_OK) == kVulkanNoAdapter);
    CHECK(VulkanCheck(true, true, true, true, 1, D3DERR_NOTAVAILABLE) == kVulkanNoDevice);
    CHECK(VulkanCheck(true, true, true, true, 1, D3DERR_INVALIDCALL) == kVulkanNoDevice);
    CHECK(VulkanCheck(true, true, true, true, 0, D3DERR_NOTAVAILABLE) == kVulkanNoAdapter);
}

TEST_CASE("renderer log lines" * doctest::test_suite("unit"))
{
    using namespace DisplayLogic;
    CHECK(LogText(kRendererVulkan, false, kVulkanOk, 0) == "vulkan (DXVK 3.1.1)");
    CHECK(LogText(kRendererVulkan, true, kVulkanOk, 0) == "vulkan (DXVK 3.1.1)");
    CHECK(LogText(kRendererD3D9, false, kVulkanOk, 0) == "d3d9 (setting)");
    CHECK(LogText(kRendererD3D9, true, kVulkanOk, 0) == "d3d9 (-d3d9)");
    CHECK(LogText(kRendererNone, false, kVulkanOk, 0) == "none (headless)");
    CHECK(LogText(kRendererVulkan, false, kVulkanNoDll, 0) == "d3d9 (fallback: no dxvk\\d3d9.dll)");
    CHECK(LogText(kRendererVulkan, false, kVulkanLoadFailed, 193) ==
          "d3d9 (fallback: dxvk\\d3d9.dll did not load, error 193)");
    CHECK(LogText(kRendererVulkan, false, kVulkanNoEntry, 0) ==
          "d3d9 (fallback: dxvk\\d3d9.dll has no Direct3DCreate9)");
    CHECK(LogText(kRendererVulkan, false, kVulkanNoInterface, 0) == "d3d9 (fallback: DXVK found no Vulkan driver)");
    CHECK(LogText(kRendererVulkan, false, kVulkanNoAdapter, 0) == "d3d9 (fallback: no adapter DXVK can use)");
    CHECK(LogText(kRendererVulkan, true, kVulkanNoDevice, (unsigned long)D3DERR_NOTAVAILABLE) ==
          "d3d9 (fallback: Vulkan device failed hr=0x8876086A)");

    // a short buffer is cut, never overrun
    char acShort[8];
    memset(acShort, 'x', sizeof(acShort));
    RendererLogText(kRendererVulkan, false, kVulkanOk, 0, acShort, sizeof(acShort));
    CHECK(strcmp(acShort, "vulkan ") == 0);
    RendererLogText(kRendererVulkan, false, kVulkanOk, 0, 0, 16);
}

TEST_CASE("dxvk dll sits in a dxvk folder beside the exe" * doctest::test_suite("unit"))
{
    char acPath[MAX_PATH];
    REQUIRE(DisplayLogic::DxvkDllPath("C:\\Games\\SWBF2\\BattlefrontII.exe", acPath, sizeof(acPath)));
    CHECK(strcmp(acPath, "C:\\Games\\SWBF2\\dxvk\\d3d9.dll") == 0);
    CHECK_FALSE(DisplayLogic::DxvkDllPath("BattlefrontII.exe", acPath, sizeof(acPath)));
    CHECK_FALSE(DisplayLogic::DxvkDllPath(0, acPath, sizeof(acPath)));

    // "C:\a\" plus "dxvk\d3d9.dll" and its terminator is 19 characters
    char acFit[19];
    CHECK(DisplayLogic::DxvkDllPath("C:\\a\\x.exe", acFit, sizeof(acFit)));
    CHECK(strcmp(acFit, "C:\\a\\dxvk\\d3d9.dll") == 0);
    char acTight[18];
    CHECK_FALSE(DisplayLogic::DxvkDllPath("C:\\a\\x.exe", acTight, sizeof(acTight)));
}

TEST_CASE("dxvk log goes beside the game log or into the dxvk folder" * doctest::test_suite("unit"))
{
    char acDir[MAX_PATH];
    const char* pcExe = "C:\\Games\\SWBF2\\BattlefrontII.exe";
    REQUIRE(DisplayLogic::DxvkLogDir("C:\\runs\\case1\\run.log", pcExe, acDir, sizeof(acDir)));
    CHECK(strcmp(acDir, "C:\\runs\\case1") == 0);
    REQUIRE(DisplayLogic::DxvkLogDir("C:/runs/case2/run.log", pcExe, acDir, sizeof(acDir)));
    CHECK(strcmp(acDir, "C:/runs/case2") == 0);
    REQUIRE(DisplayLogic::DxvkLogDir(0, pcExe, acDir, sizeof(acDir)));
    CHECK(strcmp(acDir, "C:\\Games\\SWBF2\\dxvk") == 0);
    REQUIRE(DisplayLogic::DxvkLogDir("", pcExe, acDir, sizeof(acDir)));
    CHECK(strcmp(acDir, "C:\\Games\\SWBF2\\dxvk") == 0);
    REQUIRE(DisplayLogic::DxvkLogDir("run.log", pcExe, acDir, sizeof(acDir)));
    CHECK(strcmp(acDir, "C:\\Games\\SWBF2\\dxvk") == 0);
    char acSmall[4];
    CHECK_FALSE(DisplayLogic::DxvkLogDir("C:\\runs\\case1\\run.log", pcExe, acSmall, sizeof(acSmall)));
}

TEST_CASE("hash pins: renderer switches and menu text" * doctest::test_suite("unit"))
{
    CHECK((unsigned int)PblHash("vulkan") == 0xFC99B1F2);
    CHECK((unsigned int)PblHash("vulkan") == DisplayCfg::kHashVulkan);
    CHECK((unsigned int)PblHash("d3d9") == 0x3E21D70F);
    CHECK((unsigned int)PblHash("d3d9") == DisplayCfg::kHashD3d9);
    // RedLocalizeDB::Find answers these with OpenSWBF2's own text
    CHECK((unsigned int)PblHash("ifs.videoopt.renderer") == 0xF22A2BE4);
    CHECK((unsigned int)PblHash("ifs.videoopt.vulkan") == 0xC068200C);
    CHECK((unsigned int)PblHash("ifs.videoopt.directx9") == 0xF97DF20D);
    CHECK((unsigned int)PblHash("ifs.videoopt.rendererrestart") == 0x5357AFDF);
}

TEST_CASE("renderer source pins" * doctest::test_suite("unit"))
{
    std::string sRenderer;
    std::string sDxvk;
    std::string sCommand;
    std::string sMain;
    std::string sProject;
    std::string sSetup;
    std::string sVideo;
    std::string sLocalize;
    std::string sLua;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRenderer) ||
        !ReadSource("../src/RedEngineFL/Graphics/PC/pcDxvk.cpp", sDxvk) ||
        !ReadSource("../src/Battlefront2/Source/CommandLine.cpp", sCommand) ||
        !ReadSource("../src/Battlefront2/Source/pcMain.cpp", sMain) ||
        !ReadSource("../BattlefrontII.vcxproj", sProject) ||
        !ReadSource("../tools/setup.ps1", sSetup) ||
        !ReadSource("../data/scripts/common/ifs_opt_pcvideo.lua", sVideo) ||
        !ReadSource("../src/RedEngineFL/Util/RedLocalizeDB.cpp", sLocalize) ||
        !ReadSource("../src/Battlefront2/Source/LuaCallbacks_PC.cpp", sLua))
    {
        SkipCase("source files not found");
    }

    // headless runs keep the fake device and never reach DXVK
    const std::string sInit = Between(sRenderer, "void RedRenderer::InitStatic()", "void RedRenderer::OpenDynamic()");
    REQUIRE(!sInit.empty());
    CHECK(Has(sInit, "DisplayLogic::ChooseRenderer(DisplayCfg::gDisplay.uiRenderer, g_bNoRender);"));
    CHECK(Has(sInit, "if (eChosen == DisplayLogic::kRendererVulkan)\n    {\n        m_pD3D = Dxvk::Create(eFail, ulFailCode);"));
    CHECK(Has(sInit, "    else\n    {\n        m_pD3D = new FakeD3D;\n    }"));
    CHECK(Has(sInit, "m_pd3dDevice = new FakeD3DDevice;"));
    CHECK(Has(sMain, "-norender -nosound -nowait -nofocuspause -nolegal %s"));

    // the Vulkan device is tried before VidMode_Init saves modes for it, and a
    // failed real device starts over on D3D9 with vidmode.ini read afresh
    const size_t uiProbe = sInit.find("Dxvk::ProbeDevice(m_pD3D, gAdapter, hGlobalWnd)");
    const size_t uiVidMode = sInit.find("RedVideoConfig::VidMode_Init(m_pD3D);");
    REQUIRE(uiProbe != std::string::npos);
    REQUIRE(uiVidMode != std::string::npos);
    CHECK(uiProbe < uiVidMode);
    const std::string sRetry = Between(sInit, "if (FAILED(hr) && bVulkan)", "HRESULT hrIdentifier");
    REQUIRE(!sRetry.empty());
    CHECK(Has(sRetry, "FallBackToD3D9();\n            VidConfig::ClearConfigs();\n            RedVideoConfig::VidMode_Init(m_pD3D);"));
    CHECK(Has(sRetry, "hr = CreateRenderDevice();"));
    // every fallback is logged before D3D9 is tried, the rest once at the end
    CHECK(Has(sRenderer, "LOG(LogRender, Warning, \"OPENSWBF2 RENDERER %s\", acRenderer);"));
    CHECK(Has(sRenderer, "LOG(LogRender, Inform, \"OPENSWBF2 RENDERER %s\", acRenderer);"));
    CHECK(Has(sInit, "m_pD3D = Dxvk::Create(eFail, ulFailCode);\n        if (!m_pD3D)\n        {\n            LogRenderer(eChosen, eFail, ulFailCode);"));
    CHECK(Has(sInit, "LogRenderer(eChosen, eFail, ulFailCode);\n            FallBackToD3D9();"));
    CHECK(Has(sRetry, "LogRenderer(eChosen, eFail, ulFailCode);\n            FallBackToD3D9();"));
    CHECK(Has(sInit, "if (eFail == DisplayLogic::kVulkanOk)\n    {\n        LogRenderer(eChosen, eFail, 0);"));
    const std::string sFallBack = Between(sRenderer, "static void FallBackToD3D9()", "static HRESULT CreateRenderDevice()");
    REQUIRE(!sFallBack.empty());
    CHECK(Has(sFallBack, "RedRenderer::m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);"));
    CHECK(Has(sFallBack, "EnumerateAdapters();"));
    CHECK(Has(sRenderer, "pcRedDeviceEnumerator::GetDeviceList().Clear();\n    pcRedDeviceEnumerator::Init(RedRenderer::m_pD3D);"));

    // DXVK is loaded by full path from the dxvk folder, never by name
    CHECK(Has(sDxvk, "DisplayLogic::DxvkDllPath(acExe, acDll, sizeof(acDll))"));
    CHECK(Has(sDxvk, "LoadLibraryExA(acDll, 0, LOAD_WITH_ALTERED_SEARCH_PATH)"));
    CHECK(Has(sDxvk, "GetProcAddress(sModule, \"Direct3DCreate9\")"));
    CHECK(Has(sDxvk, "IsDxvkModule(sModule)"));
    CHECK(!Has(sDxvk, "LoadLibraryA("));
    CHECK(!Has(sDxvk, "FreeLibrary("));
    CHECK(Has(sDxvk, "\"DXVK_LOG_PATH\""));
    CHECK(Has(sDxvk, "\"d3d9.dpiAware = False\""));

    // the switches
    CHECK(Has(sCommand, "case DisplayCfg::kHashVulkan:"));
    CHECK(Has(sCommand, "DisplayCfg::kRenderer, \"vulkan\", \"command line\""));
    CHECK(Has(sCommand, "case DisplayCfg::kHashD3d9:"));
    CHECK(Has(sCommand, "DisplayCfg::kRenderer, \"d3d9\", \"command line\""));

    // the build copies DXVK into dxvk\ (not beside the exe) and still links the system d3d9
    CHECK(Has(sProject, "<ClCompile Include=\"src\\RedEngineFL\\Graphics\\PC\\pcDxvk.cpp\" />"));
    CHECK(Has(sProject, "<DxvkDll>$(ProjectDir)third_party\\dxvk\\$(DxvkArch)\\d3d9.dll</DxvkDll>"));
    CHECK(Has(sProject, "DestinationFolder=\"$(OutDir)dxvk\""));
    CHECK(Has(sProject, "<Warning Condition=\"!Exists('$(DxvkDll)')\""));
    CHECK(Has(sProject, "d3d9.lib;"));
    CHECK(Has(sProject, "lib\\dxvk\\LICENSE.txt"));

    // setup.ps1 fetches the same pinned DXVK the log names
    const std::string sVersion = std::string("$dxvkVersion = '") + DisplayLogic::kDxvkVersion + "'";
    CHECK(Has(sSetup, sVersion.c_str()));
    CHECK(Has(sSetup, "$dxvkSha = '40565B4A724AADC4433FA4E010B4B23916D9B1F1BAEEE64E17186DB94F54E608'"));
    CHECK(Has(sSetup, "https://github.com/doitsujin/dxvk/releases/download/v$dxvkVersion/dxvk-$dxvkVersion.tar.gz"));
    CHECK(Has(sSetup, "Get-Download $dxvkUrl $dxvkSha"));

    // Options -> Video: the row, its text, the restart note, shown in a match too
    // (it only saves the next start's renderer)
    CHECK(Has(sVideo, "{ tag = \"renderer\", title = \"ifs.videoopt.renderer\""));
    CHECK(Has(sVideo, "values = {\"ifs.videoopt.vulkan\", \"ifs.videoopt.directx9\"}"));
    CHECK(!Has(sVideo, "generalForm.elements[\"renderer\"].hidden = 1"));
    CHECK(Has(sVideo, "ScriptCB_SetRenderer(element.selValue - 1)"));
    CHECK(Has(sVideo, "gPopup_fnSetTitleStr(Popup_Ok, \"ifs.videoopt.rendererrestart\")"));
    CHECK(Has(sVideo, "generalForm.elements[\"renderer\"].selValue = ScriptCB_GetRenderer() + 1"));
    CHECK(Has(sLua, "{ \"ScriptCB_GetRenderer\", ScriptCB_GetRenderer },"));
    CHECK(Has(sLua, "{ \"ScriptCB_SetRenderer\", ScriptCB_SetRenderer },"));
    CHECK(Has(sLocalize, "{ 0xf22a2be4, L\"Renderer\" },"));
    CHECK(Has(sLocalize, "{ 0xc068200c, L\"Vulkan\" },"));
    CHECK(Has(sLocalize, "{ 0xf97df20d, L\"DirectX 9\" },"));
    CHECK(Has(sLocalize, "{ 0x5357afdf, L\"Restart the game to use the new renderer.\" },"));
}
