#include "third_party/doctest/doctest.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "testutil.h"
#include "PebbleFL/Common/PblUnicode.h"
#include "LuaFL/include/lua.h"
#include "LuaFL/include/lauxlib.h"

namespace
{

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
    return true;
}

std::string StripCr(const std::string& sIn)
{
    std::string sOut;
    sOut.reserve(sIn.size());
    for (size_t i = 0; i < sIn.size(); ++i)
    {
        if (sIn[i] != '\r')
        {
            sOut += sIn[i];
        }
    }
    return sOut;
}

std::string FunctionBody(const std::string& sText, const char* pcSignature)
{
    size_t uiBegin = sText.find(pcSignature);
    if (uiBegin == std::string::npos)
    {
        return std::string();
    }
    size_t uiOpen = sText.find("\n{\n", uiBegin);
    if (uiOpen == std::string::npos || sText.find(';', uiBegin) < uiOpen)
    {
        return std::string();
    }
    size_t uiEnd = sText.find("\n}\n", uiOpen);
    if (uiEnd == std::string::npos)
    {
        return std::string();
    }
    return sText.substr(uiBegin, uiEnd + 2 - uiBegin);
}

std::string LuaFunctionBody(const std::string& sText, const char* pcSignature)
{
    size_t uiBegin = sText.find(pcSignature);
    if (uiBegin == std::string::npos)
    {
        return std::string();
    }
    size_t uiEnd = sText.find("\nfunction ", uiBegin + 1);
    if (uiEnd == std::string::npos)
    {
        return sText.substr(uiBegin);
    }
    return sText.substr(uiBegin, uiEnd + 1 - uiBegin);
}

std::string LuaTopFunction(const std::string& sText, const char* pcSignature)
{
    const size_t uiBegin = sText.find(pcSignature);
    if (uiBegin == std::string::npos)
    {
        return std::string();
    }
    const size_t uiEnd = sText.find("\nend\n", uiBegin);
    if (uiEnd == std::string::npos)
    {
        return std::string();
    }
    return sText.substr(uiBegin, uiEnd + 5 - uiBegin);
}

bool RunLua(lua_State* L, const std::string& sCode, std::string& sError)
{
    if (luaL_loadbuffer(L, sCode.c_str(), sCode.size(), "pcvideo") != 0 || lua_pcall(L, 0, 0, 0) != 0)
    {
        const char* pcError = lua_tostring(L, -1);
        sError = pcError ? pcError : "unknown Lua error";
        lua_settop(L, 0);
        return false;
    }
    return true;
}

std::string LuaGlobal(lua_State* L, const char* pcName)
{
    lua_getglobal(L, pcName);
    const char* pcValue = lua_isstring(L, -1) ? lua_tostring(L, -1) : 0;
    const std::string sValue = pcValue ? pcValue : "nil";
    lua_pop(L, 1);
    return sValue;
}

const char* const kPcVideoStubs =
    "calls = ''\n"
    "function note(s) calls = calls .. s .. ';' end\n"
    "poll = { 0, 0, 0, 0 }\n"
    "save = 0\n"
    "dirty = nil\n"
    "gPlatformStr = 'PC'\n"
    "function ScriptCB_PollDisplayConfirm() return poll[1], poll[2], poll[3], poll[4] end\n"
    "function ScriptCB_MarkCurrentProfile() note('markprofile') end\n"
    "function ScriptCB_MarkDisplayOptions() note('markdisplay') end\n"
    "function ScriptCB_SaveDisplayOptions() note('save') return save end\n"
    "function ScriptCB_IsCurProfileDirty() return dirty end\n"
    "function ScriptCB_SetIFScreen(s) note('setscreen ' .. s) end\n"
    "function ScriptCB_PopScreen() note('pop') end\n"
    "function ifs_opt_pcvideo_StartSaveProfile() note('saveprofile') end\n"
    "function ifs_opt_pcvideo_fnConfirmEnded(this) note('ended') end\n"
    "function ifs_opt_pcvideo_fnConfirmOpen(this) this.bConfirmOpen = 1 note('open') end\n"
    "function ifs_opt_pcvideo_fnConfirmClose() ifs_opt_pcvideo.bConfirmOpen = nil end\n"
    "function ScriptCB_tounicode(s) return s end\n"
    "function gPopup_fnSetTitleUStr(p, s) end\n"
    "function gPopup_fnSetTitleStr(p, s) note('title ' .. s) end\n"
    "string = { format = function(f, n) return f end }\n"
    "Popup_YesNo = { fnActivate = function(self, v) end }\n"
    "Popup_Ok = { fnActivate = function(self, v) note('popup') end }\n"
    "ifs_opt_pcvideo = { Exit = function(self, b) note('exit') end, Enter = function(self, b) note('enter') end }\n";

std::string LineWith(const std::string& sText, const char* pcNeedle)
{
    size_t uiAt = sText.find(pcNeedle);
    if (uiAt == std::string::npos)
    {
        return std::string();
    }
    size_t uiBegin = sText.rfind('\n', uiAt);
    uiBegin = uiBegin == std::string::npos ? 0 : uiBegin + 1;
    size_t uiEnd = sText.find('\n', uiAt);
    if (uiEnd == std::string::npos)
    {
        uiEnd = sText.size();
    }
    return sText.substr(uiBegin, uiEnd - uiBegin);
}

bool Has(const std::string& sText, const char* pcNeedle)
{
    return sText.find(pcNeedle) != std::string::npos;
}

bool TreeHas(const std::string& sDir, const char* pcNeedle)
{
    WIN32_FIND_DATAA kData;
    HANDLE hFind = FindFirstFileA((sDir + "/*").c_str(), &kData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    bool bFound = false;
    do
    {
        const std::string sName = kData.cFileName;
        if (sName == "." || sName == "..")
        {
            continue;
        }
        const std::string sPath = sDir + "/" + sName;
        if (kData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            bFound = TreeHas(sPath, pcNeedle);
        }
        else
        {
            std::string sText;
            bFound = ReadSource(sPath.c_str(), sText) && Has(sText, pcNeedle);
        }
    } while (!bFound && FindNextFileA(hFind, &kData));
    FindClose(hFind);
    return bFound;
}

size_t CountOf(const std::string& sText, const char* pcNeedle)
{
    size_t uiCount = 0;
    const size_t uiLen = strlen(pcNeedle);
    for (size_t uiAt = sText.find(pcNeedle); uiAt != std::string::npos;
         uiAt = sText.find(pcNeedle, uiAt + uiLen))
    {
        ++uiCount;
    }
    return uiCount;
}

}

TEST_CASE("window mode source pins" * doctest::test_suite("unit"))
{
    std::string sMain;
    std::string sMouse;
    std::string sProfile;
    std::string sWindow;
    if (!ReadSource("../src/Battlefront2/Source/pcMain.cpp", sMain) ||
        !ReadSource("../src/RedEngineFL/input/pc/pcRedMouse.cpp", sMouse) ||
        !ReadSource("../src/Battlefront2/Source/PlayerProfile.cpp", sProfile) ||
        !ReadSource("../src/RedEngineFL/Application/pc/pcWindow.cpp", sWindow))
    {
        SkipCase("source files not found");
    }

    CHECK(sWindow.find("if (IsIconic(hGlobalWnd))") != std::string::npos);
    CHECK(sWindow.find("GWL_STYLE) & kStateBits)") != std::string::npos);
    CHECK(sMain.find("PcWindow::ApplyPending();") != std::string::npos);
    CHECK(sWindow.find("pMouse->Exists() && pMouse->m_connected") != std::string::npos);
    CHECK(sWindow.find("if (sLevel == DisplayLogic::kDpiLevelPmV2)") != std::string::npos);
    CHECK(sWindow.find("if (sAlready && sLevel != DisplayLogic::kDpiLevelOff)") != std::string::npos);
    size_t uiDpiChanged = sWindow.find("void OnDpiChanged(");
    REQUIRE(uiDpiChanged != std::string::npos);
    CHECK(sWindow.find("FixClientSize(hWnd, 0, rc, sClientW, sClientH", uiDpiChanged) !=
          std::string::npos);

    size_t uiMs = sProfile.find("RedRenderer::pcMultiSampleSupported(gAdapter, msType");
    REQUIRE(uiMs != std::string::npos);
    CHECK(uiMs < sProfile.find("RedRenderer::pcSetDisplayMode(uiWidth"));
    CHECK(sMain.find("DisplayLogic::MinimizeOnDeactivate(") != std::string::npos);
    CHECK(sMain.find("HTCLIENT") != std::string::npos);
    CHECK(sMain.find("!pcWindowed && !g_bNoRender") == std::string::npos);
    CHECK(sMain.find("pcWindowed ?") == std::string::npos);
    size_t uiDpi = sMain.find("PcWindow::InitDpi(");
    size_t uiClass = sMain.find("RegisterClassA(");
    CHECK(uiDpi != std::string::npos);
    CHECK(uiClass != std::string::npos);
    CHECK(uiDpi < uiClass);

    CHECK(sMouse.find("gAppActive") != std::string::npos);
    size_t uiMap = sMouse.find("MapBackBufferToClient(m_X, m_Y,");
    size_t uiToScreen = sMouse.find("ClientToScreen(hGlobalWnd, &pt);");
    size_t uiWarp = sMouse.find("::SetCursorPos(pt.x, pt.y);");
    CHECK(uiMap != std::string::npos);
    CHECK(uiToScreen != std::string::npos);
    CHECK(uiWarp != std::string::npos);
    CHECK(uiMap < uiToScreen);
    CHECK(uiToScreen < uiWarp);
    CHECK(sMouse.find("::SetCursorPos(iX, iY)") == std::string::npos);
    CHECK(sMouse.find("::SetCursorPos(m_X, m_Y)") == std::string::npos);

    CHECK(sProfile.find("pcWindowed ? true : mWindowed") == std::string::npos);
    CHECK(sProfile.find("ResolveMode(") != std::string::npos);
}

TEST_CASE("runtime reset restyle source pins" * doctest::test_suite("unit"))
{
    std::string sRenderer;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRenderer))
    {
        SkipCase("source files not found");
    }

    size_t uiBegin = sRenderer.find("void RedRenderer::internalReset()");
    size_t uiEnd = sRenderer.find("void RedRenderer::internalResetState()");
    REQUIRE(uiBegin != std::string::npos);
    REQUIRE(uiEnd != std::string::npos);
    REQUIRE(uiBegin < uiEnd);
    std::string sBody = sRenderer.substr(uiBegin, uiEnd - uiBegin);
    CHECK(sBody.find("if (bRestyle)") != std::string::npos);
    size_t uiRevert = sBody.find("m_clientH = m_clientHLast;");
    REQUIRE(uiRevert != std::string::npos);
    CHECK(sBody.find("ApplyCurrentMode(", uiRevert) != std::string::npos);
}

TEST_CASE("blur effect copy fits source pins" * doctest::test_suite("unit"))
{
    std::string sRaw;
    if (!ReadSource("../src/Battlefront2/Platform/Pc/pcBlurEffect.cpp", sRaw))
    {
        SkipCase("source file not found");
    }
    const std::string sBlur = StripCr(sRaw);
    const std::string sRender = FunctionBody(sBlur, "void BlurEffect::Render(int value)");
    const std::string sCopy = FunctionBody(sBlur, "void BlurEffect::pcCopyBackBuffer() const");
    REQUIRE(!sRender.empty());
    REQUIRE(!sCopy.empty());

    // the copy into TEMP and the UVs that read it back use the same fitted
    // factor, so the 4K copy never overflows the 512 buffer
    CHECK(Has(sCopy, "BlurDownSize(m_downSizeFactor, fMaxX - fMinX, fMaxY - fMinY);"));
    CHECK(Has(sCopy, "dest.right = (int)((fMaxX - fMinX) * fDownSize);"));
    CHECK(Has(sRender, "BlurDownSize(m_downSizeFactor, right - left, bottom - top);"));
    CHECK(!Has(sRender, "* m_downSizeFactor"));
    CHECK(!Has(sCopy, "* m_downSizeFactor"));
}

TEST_CASE("shadow quad reset source pins" * doctest::test_suite("unit"))
{
    std::string sQuadRaw;
    std::string sTextureRaw;
    if (!ReadSource("../src/RedEngineFL/Application/pc/pcShadowQuadRenderable.cpp", sQuadRaw) ||
        !ReadSource("../src/RedEngineFL/Graphics/PC/pcRedTexture.cpp", sTextureRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sQuad = StripCr(sQuadRaw);
    const std::string sTexture = StripCr(sTextureRaw);

    const std::string sInit = FunctionBody(sQuad, "void pcShadowQuadRenderable::Init()");
    const std::string sCleanup = FunctionBody(sQuad, "void pcShadowQuadRenderable::Cleanup()");
    const std::string sReset = FunctionBody(sQuad, "static void __cdecl ShadowQuadReset(");
    REQUIRE(!sInit.empty());
    REQUIRE(!sCleanup.empty());
    REQUIRE(!sReset.empty());
    CHECK(Has(sInit, "DisplayLogic::ShadowQuadSharesSky("));
    CHECK(Has(sInit, "RedRenderer::pcRegisterResetFunction(&ShadowQuadReset);"));
    CHECK(Has(sCleanup, "RedRenderer::pcUnregisterResetFunction(&ShadowQuadReset);"));

    size_t uiFree = sReset.find("if (eFlags & RESETFLAGS_FREE_RESOURCES)");
    size_t uiDelete = sReset.find("delete pQuad->m_pShadowTex;");
    size_t uiCreate = sReset.find("if (eFlags & RESETFLAGS_CREATE_RESOURCES)");
    size_t uiRule = sReset.find("DisplayLogic::ShadowQuadSharesSky(pp.BackBufferWidth, pp.BackBufferHeight,");
    size_t uiNew = sReset.find("new RedTexture(pSky->m_uiWidth, pSky->m_uiHeight,");
    REQUIRE(uiFree != std::string::npos);
    REQUIRE(uiDelete != std::string::npos);
    REQUIRE(uiCreate != std::string::npos);
    REQUIRE(uiRule != std::string::npos);
    REQUIRE(uiNew != std::string::npos);
    CHECK(uiFree < uiDelete);
    CHECK(uiDelete < uiCreate);
    CHECK(uiCreate < uiRule);
    CHECK(uiRule < uiNew);
    CHECK(Has(sReset, "(unsigned int)pp.MultiSampleType,"));

    const std::string sTexReset = FunctionBody(sTexture, "void RedTexture::pcReset(");
    REQUIRE(!sTexReset.empty());
    size_t uiCreateLoop = sTexReset.find("if (eFlags & RESETFLAGS_CREATE_RESOURCES)");
    size_t uiSkip = sTexReset.find("if (it->m_pTexture)\n            {\n                continue;\n            }");
    REQUIRE(uiCreateLoop != std::string::npos);
    REQUIRE(uiSkip != std::string::npos);
    CHECK(uiCreateLoop < uiSkip);
}

TEST_CASE("interface camera near plane source pins" * doctest::test_suite("unit"))
{
    std::string sRaw;
    if (!ReadSource("../src/RedEngineFL/Interface/RedInterfaceScreen.cpp", sRaw))
    {
        SkipCase("source file not found");
    }
    const std::string sScreen = StripCr(sRaw);
    const std::string sRender = FunctionBody(sScreen, "void RedInterfaceScreen::Render() const");
    REQUIRE(!sRender.empty());

    // HUD models sit ~554 * (640 / width) * 0.01 from the camera, about 0.92
    // at 3840 wide; the near plane has to stay well under that (it was 1.0).
    CHECK(Has(sRender, "static const float kNear = 0.05f;"));
    CHECK(Has(sRender, "camera.SetFrustum(kNear, sfFar, sfWidth * kNear,"));
    CHECK(Has(sRender, "((float)m_uiWidthInPixels * sfHeight) * kNear);"));
    // positions and sizes still come from the original 1.0-near frustum
    CHECK(Has(sRender, "const float fDepth = (float)m_uiWidthInPixels / sfWidth;"));
    CHECK(Has(sScreen, "const float RedInterfaceScreen::sfNear = 1.0f;"));
}

TEST_CASE("ui scale source pins" * doctest::test_suite("unit"))
{
    std::string sScreenRaw;
    std::string sIFaceRaw;
    std::string sGuiRaw;
    std::string sMouseRaw;
    if (!ReadSource("../src/RedEngineFL/Interface/RedInterfaceScreen.cpp", sScreenRaw) ||
        !ReadSource("../src/Battlefront2/Source/LuaCallbacks_IFace.cpp", sIFaceRaw) ||
        !ReadSource("../src/Battlefront2/Source/FLGuiManager.cpp", sGuiRaw) ||
        !ReadSource("../src/RedEngineFL/input/pc/pcRedMouse.cpp", sMouseRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sScreen = StripCr(sScreenRaw);
    const std::string sIFace = StripCr(sIFaceRaw);
    const std::string sGui = StripCr(sGuiRaw);
    const std::string sMouse = StripCr(sMouseRaw);

    // every screen group Lua creates is laid out in UI pixels
    size_t uiCount = 0;
    for (size_t uiAt = sIFace.find("new RedScreenGroupElement(pcName);"); uiAt != std::string::npos;
         uiAt = sIFace.find("new RedScreenGroupElement(pcName);", uiAt + 1))
    {
        ++uiCount;
        // right after the creation and its SetUseSafezone
        CHECK(sIFace.find("pGroup->SetScaleUi(true);", uiAt) < uiAt + 160);
    }
    CHECK(uiCount == 3);

    // and only those groups get the scale; HUD (perspective) groups keep theirs
    const std::string sRender = FunctionBody(sScreen, "void RedInterfaceScreen::Render() const");
    REQUIRE(!sRender.empty());
    CHECK(Has(sRender, "pGroup->m_bScaleUi ? fUiScale : (pGroup->m_bHudScale ? fHudScale : 1.0f);"));
    CHECK(Has(sRender, "mScreen.m[2][2] = fGroupScale;"));

    // Lua sees the UI size, and mouse coordinates cross the boundary converted
    const std::string sInfo = FunctionBody(sIFace, "int ScriptCB_GetScreenInfo(lua_State* L)");
    const std::string sSafe = FunctionBody(sIFace, "int ScriptCB_GetSafeScreenInfo(lua_State* L)");
    const std::string sOver = FunctionBody(sIFace, "int ScriptCB_IFObj_MouseOver(lua_State* L)");
    const std::string sCursor = FunctionBody(sGui, "void GuiManager::UpdateCursor()");
    REQUIRE(!sInfo.empty());
    REQUIRE(!sSafe.empty());
    REQUIRE(!sOver.empty());
    REQUIRE(!sCursor.empty());
    CHECK(Has(sInfo, "RedInterfaceScreen::GetUiWidth()"));
    CHECK(Has(sSafe, "RedInterfaceScreen::GetUiHeight() * fSafeZone"));
    CHECK(Has(sOver, "luaL_checknumber(LuaHelper::mState, 2) * fUiScale;"));
    CHECK(Has(sCursor, "(float)mMouseX / fUiScale, (float)mMouseY / fUiScale"));
    CHECK(Has(sMouse, "DisplayLogic::CursorSize(suiSrcW, suiSrcH, RedInterfaceScreen::GetUiWidth(),"));
}

TEST_CASE("display apply target failure source pins" * doctest::test_suite("unit"))
{
    std::string sTextureRaw;
    std::string sProfileRaw;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedTexture.cpp", sTextureRaw) ||
        !ReadSource("../src/Battlefront2/Source/PlayerProfile.cpp", sProfileRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sTexture = StripCr(sTextureRaw);
    const std::string sProfile = StripCr(sProfileRaw);

    const std::string sInit = FunctionBody(sTexture, "void RedTexture::pcInit(unsigned int uiWidth,");
    REQUIRE(!sInit.empty());
    CHECK(Has(sInit, "IDirect3DTexture9* pTexture = 0;"));
    CHECK(Has(sInit, "s_defaultPoolTextures.Add(&m_listNode, this);\n        }\n        return;\n    }\n"
                     "    pcInit(pTexture, uiNameHash);"));

    const std::string sReset = FunctionBody(sTexture, "void RedTexture::pcReset(");
    REQUIRE(!sReset.empty());
    CHECK(Has(sReset, "IDirect3DTexture9* pTexture = 0;"));
    CHECK(Has(sReset, "{\n                    pTexture = 0;\n                }"));

    const std::string sCreated = FunctionBody(sTexture, "bool RedTexture::pcDefaultPoolCreated()");
    REQUIRE(!sCreated.empty());
    CHECK(Has(sCreated, "if (!it->m_pTexture)\n        {\n            return false;"));

    const std::string sUsable = FunctionBody(sProfile, "static bool ProfileSizeUsable(");
    REQUIRE(!sUsable.empty());
    CHECK(Has(sUsable, "RedRenderer::pcGetMaxTextureSize(uiMaxW, uiMaxH);"));
    CHECK(Has(sUsable, "DisplayLogic::SizeInBounds(uiW, uiH) &&\n"
                       "           DisplayLogic::SizeFitsCaps(uiW, uiH, uiMaxW, uiMaxH);"));
    const std::string sTarget = FunctionBody(sProfile, "static void ComputeTargetMode(");
    REQUIRE(!sTarget.empty());
    size_t uiCheck = sTarget.find("if (!ProfileSizeUsable(uiResX, uiResY))");
    size_t uiPolicy = sTarget.find("uiResX, uiResY, uiDeskW, uiDeskH);");
    REQUIRE(uiCheck != std::string::npos);
    REQUIRE(uiPolicy != std::string::npos);
    CHECK(uiCheck < uiPolicy);
    CHECK(!Has(sTarget, "rOpt.mResolutionX, rOpt.mResolutionY, uiDeskW"));
    const std::string sLive = FunctionBody(sProfile, "void PlayerProfile::ApplyLiveDisplay()");
    REQUIRE(!sLive.empty());
    size_t uiLiveCheck = sLive.find("if (!ProfileSizeUsable(rPC.mResolutionX, rPC.mResolutionY))");
    size_t uiApply = sLive.find("RedRenderer::pcApplyDisplayMode(");
    REQUIRE(uiLiveCheck != std::string::npos);
    REQUIRE(uiApply != std::string::npos);
    CHECK(uiLiveCheck < uiApply);
}

TEST_CASE("display apply snapshot source pins" * doctest::test_suite("unit"))
{
    std::string sRendererRaw;
    std::string sProfileRaw;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRendererRaw) ||
        !ReadSource("../src/Battlefront2/Source/PlayerProfile.cpp", sProfileRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sRenderer = StripCr(sRendererRaw);
    const std::string sProfile = StripCr(sProfileRaw);

    CHECK(!Has(sProfile, "CaptureAppliedPC"));
    const std::string sPre = FunctionBody(sProfile, "void PlayerProfile::tPCOpt::ApplyPreLoadSettings()");
    const std::string sLive = FunctionBody(sProfile, "void PlayerProfile::ApplyLiveDisplay()");
    const std::string sPoll = FunctionBody(sProfile, "void PlayerProfile::PollApplyNotice()");
    REQUIRE(!sPre.empty());
    REQUIRE(!sLive.empty());
    REQUIRE(!sPoll.empty());
    CHECK(!Has(sPre, "sAppliedPC"));
    CHECK(!Has(sLive, "sAppliedPC"));
    size_t uiStagePre = sPre.find("StageRequestTag(*this);");
    size_t uiSet = sPre.find("RedRenderer::pcSetDisplayMode(");
    REQUIRE(uiStagePre != std::string::npos);
    REQUIRE(uiSet != std::string::npos);
    CHECK(uiStagePre < uiSet);
    size_t uiStageLive = sLive.find("StageRequestTag(rPC);");
    size_t uiApply = sLive.find("RedRenderer::pcApplyDisplayMode(");
    REQUIRE(uiStageLive != std::string::npos);
    REQUIRE(uiApply != std::string::npos);
    CHECK(uiStageLive < uiApply);
    CHECK(Has(sPoll, "if (kTag.bValid)\n            {\n                sAppliedPC = kTag;\n            }"));
    CHECK(Has(sPoll, "DisplayLogic::ShouldRestoreProfile(sAppliedPC.bValid, sAppliedPC.uiOwner,"));
    CHECK(Has(sPoll, "RedRenderer::pcApplyPending()"));
    const std::string sStage = FunctionBody(sProfile, "static void StageRequestTag(");
    REQUIRE(!sStage.empty());
    CHECK(Has(sStage, "kTag.uiOwner = PlayerProfile::GetCurProfileIndex();"));

    const std::string sPromote = FunctionBody(sRenderer, "static void PromoteApplied()");
    const std::string sBegin = FunctionBody(sRenderer, "bool RedRenderer::BeginFrame()");
    const std::string sApplyMode = FunctionBody(sRenderer, "bool RedRenderer::pcApplyDisplayMode(");
    const std::string sInit = FunctionBody(sRenderer, "void RedRenderer::Init()");
    const std::string sNote = FunctionBody(sRenderer, "static void NoteInitApplied(bool bApplied)\n");
    REQUIRE(!sPromote.empty());
    REQUIRE(!sBegin.empty());
    REQUIRE(!sApplyMode.empty());
    REQUIRE(!sInit.empty());
    REQUIRE(!sNote.empty());
    CHECK(Has(sPromote, "sNoticeTag = sInFlightTag;"));
    CHECK(Has(sBegin, "sInFlightTag = sPendingApply.kTag;"));
    CHECK(Has(sApplyMode, "sPendingApply.kTag = sStagedTag;"));
    CHECK(Has(sInit, "NoteInitApplied(!bReverted);"));
    CHECK(Has(sInit, "bRevertUsed = true;\n            bReverted = true;"));
    CHECK(Has(sNote, "if (bApplied && sStagedTag.bValid)"));
}

TEST_CASE("display apply cleanup source pins" * doctest::test_suite("unit"))
{
    std::string sRendererRaw;
    std::string sStateRaw;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRendererRaw) ||
        !ReadSource("../src/Battlefront2/Source/GameState.cpp", sStateRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sRenderer = StripCr(sRendererRaw);
    const std::string sState = StripCr(sStateRaw);

    const std::string sCleanup = FunctionBody(sRenderer, "void RedRenderer::Cleanup()");
    REQUIRE(!sCleanup.empty());
    CHECK(Has(sCleanup, "sPendingApply.bSet = false;"));
    CHECK(Has(sCleanup, "sApplyNotice = 0;"));
    CHECK(Has(sCleanup, "sNoticeTag.bValid = false;"));

    const std::string sUpdate = FunctionBody(sState, "bool GameState::Update()");
    REQUIRE(!sUpdate.empty());
    size_t uiPoll = sUpdate.find("PlayerProfile::PollApplyNotice();");
    size_t uiChange = sUpdate.find("if (_bDoChangeState)");
    REQUIRE(uiPoll != std::string::npos);
    REQUIRE(uiChange != std::string::npos);
    CHECK(uiPoll < uiChange);
    CHECK(sUpdate.find("PlayerProfile::PollApplyNotice();", uiPoll + 1) == std::string::npos);
}

TEST_CASE("display apply source pins" * doctest::test_suite("unit"))
{
    std::string sRendererRaw;
    std::string sLuaRaw;
    std::string sArgsRaw;
    std::string sMemRaw;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRendererRaw) ||
        !ReadSource("../src/Battlefront2/Source/LuaCallbacks_PC.cpp", sLuaRaw) ||
        !ReadSource("../src/Battlefront2/Source/LuaVideoArgs.cpp", sArgsRaw) ||
        !ReadSource("../src/Battlefront2/Source/LuaMemory.cpp", sMemRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sRenderer = StripCr(sRendererRaw);
    const std::string sLua = StripCr(sLuaRaw);
    const std::string sArgs = StripCr(sArgsRaw);
    const std::string sMem = StripCr(sMemRaw);

    const std::string sApply = FunctionBody(sRenderer, "bool RedRenderer::pcApplyDisplayMode(");
    REQUIRE(!sApply.empty());
    CHECK(Has(sApply, "pcSetDisplayMode("));
    CHECK(!Has(sApply, "internalReset("));
    CHECK(!Has(sApply, "Reset("));

    const std::string sRequest = FunctionBody(sRenderer, "bool RedRenderer::pcRequestAdapter(");
    const std::string sAdapterCb =
        FunctionBody(sLua, "int Lua_Callbacks::ScriptCB_SetPCDisplayAdapter(");
    REQUIRE(!sRequest.empty());
    REQUIRE(!sAdapterCb.empty());
    const char* apcForbidden[] = {"Reset(", "pcApplyDisplayMode", "internalReset", "CreateDevice"};
    for (size_t i = 0; i < sizeof(apcForbidden) / sizeof(apcForbidden[0]); ++i)
    {
        CHECK(!Has(sRequest, apcForbidden[i]));
        CHECK(!Has(sAdapterCb, apcForbidden[i]));
    }
    CHECK(Has(sRequest, "AdapterDecision"));
    CHECK(!Has(sRequest, "SaveAdapter"));
    CHECK(Has(sRequest, "SetPendingAdapter("));
    CHECK(Has(sAdapterCb, "pcRequestAdapter"));

    CHECK(!Has(sLua, "Reset("));
    CHECK(!Has(sLua, "pcApplyDisplayMode"));
    // the registration loop counts the table itself, so a new callback can't be left out
    CHECK(Has(sLua, "static const CallbackEntryT sPCCallbacks[] ="));
    CHECK(Has(sLua, "i < (int)(sizeof(sPCCallbacks) / sizeof(sPCCallbacks[0]))"));
    const std::string sVideo = FunctionBody(sLua, "int ScriptCB_SetPCVideoOptions(");
    REQUIRE(!sVideo.empty());
    size_t uiChanged = sVideo.find("if (bChanged)\n    {\n        pProfile->UpdateProfCRC();\n"
                                   "        rPC.ApplyPostLoadSettings();\n    }\n");
    size_t uiLive = sVideo.find("PlayerProfile::ApplyLiveDisplay();");
    size_t uiReturn = sVideo.rfind("return 0;");
    REQUIRE(uiChanged != std::string::npos);
    REQUIRE(uiLive != std::string::npos);
    REQUIRE(uiReturn != std::string::npos);
    CHECK(uiChanged < uiLive);
    CHECK(uiLive < uiReturn);
    CHECK(sVideo.find("return 0;", uiLive) == uiReturn);
    size_t uiRead = sVideo.find("LuaVideoArgs::ReadLegacyVideoSet(LuaHelper::mState, afArg)");
    REQUIRE(uiRead != std::string::npos);
    CHECK(uiRead < sVideo.find("return 0;"));
    CHECK(!Has(sVideo, "luaL_checknumber"));
    const std::string sVideoGet = FunctionBody(sLua, "int ScriptCB_GetPCVideoOptions(");
    REQUIRE(!sVideoGet.empty());
    CHECK(Has(sVideoGet, "return LuaVideoArgs::PushLegacyVideoGet("));
    CHECK((Has(sArgs, "!= kLegacyVideoSetArgs") || Has(sArgs, "== kLegacyVideoSetArgs")));
    CHECK(Has(sArgs, "return kLegacyVideoGetValues;"));
    const std::string sWarn = FunctionBody(sMem, "void __cdecl LuaScript_CallWARNINGF(");
    REQUIRE(!sWarn.empty());
    CHECK(Has(sWarn, "OPENSWBF2 LUA error"));

    const std::string sBegin = FunctionBody(sRenderer, "bool RedRenderer::BeginFrame()");
    REQUIRE(!sBegin.empty());
    size_t uiPending = sBegin.find(
        "if ((sPendingApply.bSet || sForceReset) && !m_bResourcesReleased && hrCoop == D3D_OK)");
    size_t uiRecover = sBegin.find("if (m_bResourcesReleased || hrCoop == D3DERR_DEVICENOTRESET)");
    REQUIRE(uiPending != std::string::npos);
    REQUIRE(uiRecover != std::string::npos);
    CHECK(uiPending < uiRecover);

    const std::string sInit = FunctionBody(sRenderer, "void RedRenderer::Init()");
    const std::string sResize = FunctionBody(sRenderer, "static void ResizeBuffersToBackBuffer()");
    REQUIRE(!sInit.empty());
    REQUIRE(!sResize.empty());
    CHECK(Has(sInit, "DisplayLogic::BufferSizes"));
    CHECK(Has(sResize, "DisplayLogic::BufferSizes"));

    const std::string sReset = FunctionBody(sRenderer, "void RedRenderer::internalReset()");
    REQUIRE(!sReset.empty());
    size_t uiResize = sReset.find("ResizeBuffersToBackBuffer();");
    size_t uiFree = sReset.find("m_resetFunctions[i](RESETFLAGS_FREE_RESOURCES);");
    REQUIRE(uiResize != std::string::npos);
    REQUIRE(uiFree != std::string::npos);
    CHECK(uiResize < uiFree);
    CHECK(!Has(sReset, "delete m_buffers"));
    CHECK(!Has(sApply, "delete m_buffers"));
}

TEST_CASE("display menu lua source pins" * doctest::test_suite("unit"))
{
    std::string sFormRaw;
    std::string sTabRaw;
    std::string sUtilRaw;
    std::string sRoundRaw;
    std::string sVideoRaw;
    REQUIRE(ReadSource("../data/scripts/common/ifelem_form.lua", sFormRaw));
    REQUIRE(ReadSource("../data/scripts/common/ifelem_tabmanager.lua", sTabRaw));
    REQUIRE(ReadSource("../data/scripts/common/interface_util.lua", sUtilRaw));
    REQUIRE(ReadSource("../data/scripts/common/ifelem_roundbutton.lua", sRoundRaw));
    REQUIRE(ReadSource("../data/scripts/common/ifs_opt_pcvideo.lua", sVideoRaw));
    const std::string sForm = StripCr(sFormRaw);
    const std::string sTab = StripCr(sTabRaw);
    const std::string sUtil = StripCr(sUtilRaw);
    const std::string sRound = StripCr(sRoundRaw);
    const std::string sVideo = StripCr(sVideoRaw);

    const std::string sAccept = LuaFunctionBody(sForm, "function Form_InputAccept(this)");
    REQUIRE(!sAccept.empty());
    CHECK(Has(sAccept, "if(curElement.locked) then"));
    const std::string sUpdate = LuaFunctionBody(sForm, "function Form_UpdateElement(form, elementTag)");
    REQUIRE(!sUpdate.empty());
    CHECK(Has(sUpdate, "bDimmed = form.elements[elementTag].locked"));
    CHECK(CountOf(sForm, "textbreak = \"none\"") >= 2);

    CHECK(Has(sTab, "textbreak = \"none\""));
    CHECK(Has(sUtil, "obj.textbreak"));
    CHECK(Has(sUtil, "ScriptCB_IFText_SetTextBreak("));
    CHECK(Has(sRound, "Template.textbreak"));

    const std::string sVsyncRow = LineWith(
        sVideo,
        "tag = \"vsync\", title = \"ifs.VideoOpt.vsync\", fnChanged = ifs_opt_pcvideo.OnRadioChanged");
    REQUIRE(!sVsyncRow.empty());
    CHECK(Has(sVsyncRow, "control = \"radio\""));
    CHECK(Has(sVsyncRow, "values = {\"common.off\", \"common.on\"}"));
    CHECK(!Has(sVideo, "vsyncmode"));
    const size_t uRadio = sVideo.find("OnRadioChanged = function(buttongroup, btnNum)");
    const size_t uElement = sVideo.find("OnElementChanged = function(form, element)");
    REQUIRE(uRadio != std::string::npos);
    REQUIRE(uElement != std::string::npos);
    REQUIRE(uRadio < uElement);
    const std::string sRadio = sVideo.substr(uRadio, uElement - uRadio);
    CHECK(Has(sRadio, "buttongroup.tag == \"vsync\""));
    CHECK(Has(sRadio, "this.bVsyncHalf = nil"));
    CHECK(Has(sRadio, "ifs_opt_pcvideo_fnShowRows(this)"));
    CHECK(!Has(sVideo, "tag = \"refresh\""));
    CHECK(!Has(sVideo, "fnFillRefresh"));
    CHECK(Has(sVideo, "refresh = this.iRefresh or 0"));
    CHECK(Has(sVideo, "this.iRefresh = display.clirefresh"));
    CHECK(!Has(sVideo, "display.refresh"));
    CHECK(!Has(sVideo, "\"Half rate\""));
    CHECK(!Has(sVideo, "querywait"));
    CHECK(Has(sVideo, "futureframes = "));
    CHECK(Has(sVideo, "display.futureframes"));
    CHECK(Has(sVideo, "bVsyncHalf"));

    const std::string sRows = LuaFunctionBody(sVideo, "function ifs_opt_pcvideo_fnShowRows(this)");
    REQUIRE(!sRows.empty());
    CHECK(Has(sRows, "elements[\"vsync\"].selValue == 2"));
    CHECK(Has(sRows, "locked = 1"));
    CHECK(Has(sRows, "\"Vsync on\""));
    CHECK(Has(sRows, "locked = nil"));

    const std::string sMsaa =
        LuaFunctionBody(sVideo, "function ifs_opt_pcvideo_fnFillMultisample(this, bKeep)");
    REQUIRE(!sMsaa.empty());
    CHECK(Has(sMsaa, "ScriptCB_FillMultisampleTable("));
    CHECK(!Has(sVideo, "GetMaxMultisamplingModeForScreenSize"));

    const std::string sSave = LuaFunctionBody(sVideo, "function ifs_opt_pcvideo_fnSaveDisplay(fnNext)");
    REQUIRE(!sSave.empty());
    CHECK(Has(sSave, "if(ScriptCB_SaveDisplayOptions() == 2) then"));
    const std::string sDone = LuaFunctionBody(sVideo, "function ifs_opt_pcvideo_fnConfirmDone(bKeep)");
    REQUIRE(!sDone.empty());
    CHECK(Has(sDone, "keepsave == 2"));
    const std::string sFailed = LuaFunctionBody(sVideo, "function ifs_opt_pcvideo_fnSaveFailed(fnNext)");
    REQUIRE(!sFailed.empty());
    CHECK(Has(sFailed, "Display settings could not be saved."));

    const size_t uiAcceptAt = sVideo.find("\tInput_Accept = function(this)");
    const size_t uiQualityAt = sVideo.find("UpdateOverallQuality = function(this, value)");
    const size_t uiBackAt = sVideo.find("\tInput_Back = function(this)");
    const size_t uiStartAt = sVideo.find("\tInput_Start = function(this)");
    REQUIRE(uiAcceptAt != std::string::npos);
    REQUIRE(uiQualityAt != std::string::npos);
    REQUIRE(uiBackAt != std::string::npos);
    REQUIRE(uiStartAt != std::string::npos);
    REQUIRE(uiAcceptAt < uiQualityAt);
    REQUIRE(uiBackAt < uiStartAt);
    const std::string sInputAccept = sVideo.substr(uiAcceptAt, uiQualityAt - uiAcceptAt);
    const std::string sInputBack = sVideo.substr(uiBackAt, uiStartAt - uiBackAt);
    CHECK(Has(sInputAccept, "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnLeaveTab)"));
    CHECK(Has(sInputAccept, "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnAfterApply)"));
    CHECK(!Has(sInputAccept, "ScriptCB_SetIFScreen("));
    CHECK(!Has(sInputAccept, "ifs_opt_pcvideo_StartSaveProfile("));
    CHECK(Has(sInputBack, "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnAfterBack)"));
    CHECK(!Has(sInputBack, "ScriptCB_PopScreen("));
    CHECK(!Has(sInputBack, "this:Exit("));
    CHECK(CountOf(sVideo, "ifs_opt_pcvideo_fnSaveDisplay(") == 4);
}

TEST_CASE("multiplayer: Modern first and by default with a Direct Connect tab and no Join IP button" *
          doctest::test_suite("regression"))
{
    std::string sListRaw;
    std::string sCreateRaw;
    REQUIRE(ReadSource("../data/scripts/shell/ifs_mp_sessionlist.lua", sListRaw));
    REQUIRE(ReadSource("../data/scripts/shell/ifs_missionselect_pcmulti.lua", sCreateRaw));
    const std::string sList = StripCr(sListRaw);
    const std::string sCreate = StripCr(sCreateRaw);

    // the picker rows are the source values: Modern is row 1 and the starting choice
    const size_t uModern = sList.find("{ string = \"ifs.mp.join.modern\" }, -- MP_SOURCE_MODERN");
    const size_t uLegacy = sList.find("{ string = \"ifs.mp.join.legacy\" }, -- MP_SOURCE_LEGACY");
    REQUIRE(uModern != std::string::npos);
    REQUIRE(uLegacy != std::string::npos);
    CHECK(uModern < uLegacy);
    CHECK(Has(sList, "MP_SOURCE_MODERN = 1\nMP_SOURCE_LEGACY = 2\ngMPSourceValue = MP_SOURCE_MODERN\n"));
    CHECK(!Has(sList, "ifs.mp.join.internet\" }"));
    // Create used to start on Internet (row 1) whenever Galaxy was signed in
    CHECK(Has(sCreate, "if( ScriptCB_IsLoggedIn() ) then\n\t\t\tthis.source_value = gMPSourceValue\n"
                       "\t\telse\n\t\t\tthis.source_value = MP_SOURCE_MODERN\n"));
    CHECK(Has(sCreate, "this.source_value == MP_SOURCE_LEGACY"));

    const std::string sTab = LineWith(sList, "tag = \"_tab_direct\"");
    REQUIRE(!sTab.empty());
    CHECK(Has(sTab, "string = \"ifs.mp.tabs.direct\""));
    CHECK(Has(sTab, "screen = \"ifs_mp_direct\""));
    CHECK(Has(sList, "AddIFScreen(ifs_mp_direct, \"ifs_mp_direct\")"));
    CHECK(Has(sList, "ifs_mp_direct.bPadSnap = 1"));

    CHECK(!Has(sList, "_joinip"));
    CHECK(!Has(sList, "bJoinIPMode"));
    // the command-line /connect join still goes through the Join screen
    CHECK(Has(sList, "ifs_mp_sessionlist_fnBeginSpecialJoin(this, ifs_vkeyboard.CurString)"));
}

TEST_CASE("multiplayer opens on Direct Connect with no sign-in screen" * doctest::test_suite("regression"))
{
    // The Galaxy sign-in screen (ifs_mpgs_login) used to stand in front of Multiplayer and showed
    // an error whenever Galaxy was not there (always on x64). Now the Multiplayer tab and the
    // start-up route open Direct Connect and start the sign-in quietly in the background.
    std::string sTabsRaw;
    std::string sStartRaw;
    std::string sListRaw;
    REQUIRE(ReadSource("../data/scripts/common/pctabs_options.lua", sTabsRaw));
    REQUIRE(ReadSource("../data/scripts/shell/ifs_start.lua", sStartRaw));
    REQUIRE(ReadSource("../data/scripts/shell/ifs_mp_sessionlist.lua", sListRaw));
    const std::string sTabs = StripCr(sTabsRaw);
    const std::string sStart = StripCr(sStartRaw);
    const std::string sList = StripCr(sListRaw);

    const std::string sMulti = LuaFunctionBody(sTabs, "function fnPCMainTabs_Multi( this )");
    REQUIRE(!sMulti.empty());
    CHECK(Has(sMulti, "ScriptCB_SetIFScreen(\"ifs_mp_direct\")"));
    CHECK(Has(sMulti, "ScriptCB_StartLogin()"));
    CHECK(!Has(sMulti, "ifs_mpgs_login"));
    CHECK(Has(LineWith(sTabs, "tag = \"_tab_multi\""), "screen = \"ifs_mp_direct\""));

    const std::string sDone = LuaFunctionBody(sStart, "function ifs_start_OnLoginDone( )");
    REQUIRE(!sDone.empty());
    CHECK(Has(sDone, "ifs_movietrans_PushScreen(ifs_mp_direct)"));
    CHECK(!Has(sDone, "ifs_mpgs_login"));

    // Direct Connect is the first Multiplayer tab, and the row's start position moved with it
    const size_t uDirect = sList.find("tag = \"_tab_direct\"");
    const size_t uJoin = sList.find("tag = \"_tab_join\"");
    REQUIRE(uDirect != std::string::npos);
    REQUIRE(uJoin != std::string::npos);
    CHECK(uDirect < uJoin);
    CHECK(Has(LineWith(sList, "tag = \"_tab_direct\""), "xPos = 87"));
}

TEST_CASE("pad bumpers switch tabs while a text box has focus" * doctest::test_suite("regression"))
{
    // Direct Connect keeps its address box focused, and LB/RB stayed dead there: keyboard 3/4
    // send the same menu events, so the tab bar ignored both whenever a box had focus. The pad
    // now reports its own bumpers, and only keyboard 3/4 are ignored while typing.
    std::string sTabRaw;
    std::string sJoyRaw;
    std::string sCbRaw;
    REQUIRE(ReadSource("../data/scripts/common/ifelem_tabmanager.lua", sTabRaw));
    REQUIRE(ReadSource("../src/Battlefront2/Platform/Pc/pcFLJoystick.cpp", sJoyRaw));
    REQUIRE(ReadSource("../src/Battlefront2/Source/LuaCallbacks_PC.cpp", sCbRaw));
    const std::string sTab = StripCr(sTabRaw);
    const std::string sJoy = StripCr(sJoyRaw);
    const std::string sCb = StripCr(sCbRaw);

    const std::string sCycle = LuaFunctionBody(sTab, "function ifelem_tabmanager_PadCycle(this, iDir, Which)");
    REQUIRE(!sCycle.empty());
    CHECK(Has(LineWith(sCycle, "if( gCurEditbox and (Which == \"top\")"), "ScriptCB_IsPadBumperDown()"));

    CHECK(Has(sJoy, "(i == XInputPad::kBitLB || i == XInputPad::kBitRB) && fValue > 0.0f"));
    CHECK(Has(sJoy, "XInputPad::gBumperDown = bBumperDown;"));
    CHECK(Has(sCb, "{ \"ScriptCB_IsPadBumperDown\", ScriptCB_IsPadBumperDown },"));
}

TEST_CASE("pad snap lands on a dropdown box like a slider bar" * doctest::test_suite("regression"))
{
    // Instant Action's AI difficulty: the box's hotspot starts at its left end, so its centre
    // sat outside the up/down cone from the rows around it and the pad skipped it.
    std::string sTabRaw;
    REQUIRE(ReadSource("../data/scripts/common/ifelem_tabmanager.lua", sTabRaw));
    const std::string sTab = StripCr(sTabRaw);
    const std::string sCollect = LuaFunctionBody(sTab, "function PadSnap_Collect(this, List, Seen, iDepth)");
    REQUIRE(!sCollect.empty());
    CHECK(Has(sCollect, "if( this.bDropDownBox and this.btnw ) then"));
    CHECK(Has(sCollect, "fHalfW = this.btnw * 0.45"));
    CHECK(Has(sCollect, "PadSnap_Add(List, this.label, nil, fHalfW)"));
}

TEST_CASE("display menu lua save failure waits" * doctest::test_suite("unit"))
{
    std::string sRaw;
    REQUIRE(ReadSource("../data/scripts/common/ifs_opt_pcvideo.lua", sRaw));
    const std::string sVideo = StripCr(sRaw);
    const char* apcFunctions[] = { "function ifs_opt_pcvideo_fnSaveFailed(fnNext)",
        "function ifs_opt_pcvideo_fnSaveDisplay(fnNext)", "function ifs_opt_pcvideo_fnLeaveTab()",
        "function ifs_opt_pcvideo_fnAfterApply()", "function ifs_opt_pcvideo_fnAfterBack()" };
    std::string sSetup = kPcVideoStubs;
    for (size_t i = 0; i < sizeof(apcFunctions) / sizeof(apcFunctions[0]); ++i)
    {
        const std::string sBody = LuaTopFunction(sVideo, apcFunctions[i]);
        CAPTURE(apcFunctions[i]);
        REQUIRE(!sBody.empty());
        sSetup += sBody;
    }

    lua_State* L = lua_open();
    REQUIRE(L);
    std::string sError;
    CAPTURE(sError);
    REQUIRE(RunLua(L, sSetup, sError));

    REQUIRE(RunLua(L,
                   "local this = ifs_opt_pcvideo\n"
                   "this.NextScreen = 'tab'\n"
                   "save = 2\n"
                   "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnLeaveTab)\n"
                   "shown = calls\n"
                   "calls = ''\n"
                   "Popup_Ok.fnDone()\n"
                   "dismissed = calls\n"
                   "cleared = Popup_Ok.fnDone\n"
                   "calls = ''\n"
                   "this.NextScreen = 'tab'\n"
                   "save = 1\n"
                   "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnLeaveTab)\n"
                   "deferred = calls\n"
                   "calls = ''\n"
                   "save = 0\n"
                   "dirty = 1\n"
                   "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnLeaveTab)\n"
                   "tabdirty = calls\n",
                   sError));
    CHECK(LuaGlobal(L, "shown") == "save;popup;title Display settings could not be saved.;");
    CHECK(LuaGlobal(L, "dismissed") == "setscreen tab;");
    CHECK(LuaGlobal(L, "cleared") == "nil");
    CHECK(LuaGlobal(L, "deferred") == "save;setscreen tab;");
    CHECK(LuaGlobal(L, "tabdirty") == "save;saveprofile;");

    REQUIRE(RunLua(L,
                   "local this = ifs_opt_pcvideo\n"
                   "calls = ''\n"
                   "save = 2\n"
                   "dirty = nil\n"
                   "this.bShellMode = 1\n"
                   "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnAfterApply)\n"
                   "applyshown = calls\n"
                   "calls = ''\n"
                   "Popup_Ok.fnDone()\n"
                   "applyshell = calls\n"
                   "calls = ''\n"
                   "dirty = 1\n"
                   "this.NextScreen = nil\n"
                   "save = 0\n"
                   "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnAfterApply)\n"
                   "applydirty = calls\n"
                   "applynext = this.NextScreen\n"
                   "calls = ''\n"
                   "dirty = nil\n"
                   "this.bShellMode = nil\n"
                   "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnAfterApply)\n"
                   "applygame = calls\n"
                   "calls = ''\n"
                   "save = 2\n"
                   "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnAfterBack)\n"
                   "backshown = calls\n"
                   "calls = ''\n"
                   "Popup_Ok.fnDone()\n"
                   "backgame = calls\n"
                   "calls = ''\n"
                   "this.bShellMode = 1\n"
                   "save = 0\n"
                   "ifs_opt_pcvideo_fnSaveDisplay(ifs_opt_pcvideo_fnAfterBack)\n"
                   "backshell = calls\n"
                   "calls = ''\n"
                   "ifs_opt_pcvideo_fnSaveFailed()\n"
                   "Popup_Ok.fnDone()\n"
                   "alone = calls\n",
                   sError));
    CHECK(LuaGlobal(L, "applyshown") == "save;popup;title Display settings could not be saved.;");
    CHECK(LuaGlobal(L, "applyshell") == "exit;enter;");
    CHECK(LuaGlobal(L, "applydirty") == "save;saveprofile;");
    CHECK(LuaGlobal(L, "applynext") == "-1");
    CHECK(LuaGlobal(L, "applygame") == "save;pop;");
    CHECK(LuaGlobal(L, "backshown") == "save;popup;title Display settings could not be saved.;");
    CHECK(LuaGlobal(L, "backgame") == "pop;");
    CHECK(LuaGlobal(L, "backshell") == "save;exit;enter;");
    CHECK(LuaGlobal(L, "alone") == "popup;title Display settings could not be saved.;");
    lua_close(L);
}

TEST_CASE("display menu lua confirm mark" * doctest::test_suite("unit"))
{
    std::string sRaw;
    REQUIRE(ReadSource("../data/scripts/common/ifs_opt_pcvideo.lua", sRaw));
    const std::string sVideo = StripCr(sRaw);
    const std::string sMark = LuaTopFunction(sVideo, "function ifs_opt_pcvideo_fnMark(this)");
    const std::string sUpdate = LuaTopFunction(sVideo, "function ifs_opt_pcvideo_fnConfirmUpdate(this)");
    REQUIRE(!sMark.empty());
    REQUIRE(!sUpdate.empty());

    lua_State* L = lua_open();
    REQUIRE(L);
    std::string sError;
    const std::string sSetup = std::string(kPcVideoStubs) +
                               "function ifs_opt_pcvideo_fnSaveFailed() note('savefailed') end\n" +
                               sMark + sUpdate;
    CAPTURE(sError);
    REQUIRE(RunLua(L, sSetup, sError));

    REQUIRE(RunLua(L,
                   "local this = ifs_opt_pcvideo\n"
                   "poll = { 1, 0, 0, 0 }\n"
                   "ifs_opt_pcvideo_fnMark(this)\n"
                   "enter = calls\n"
                   "pending = this.bMarkPending\n"
                   "poll = { 2, 15, 0, 0 }\n"
                   "ifs_opt_pcvideo_fnConfirmUpdate(this)\n"
                   "poll = { 0, 0, 2, 0 }\n"
                   "ifs_opt_pcvideo_fnConfirmUpdate(this)\n"
                   "ended = calls\n"
                   "after = this.bMarkPending\n"
                   "calls = ''\n"
                   "ifs_opt_pcvideo_fnConfirmUpdate(this)\n"
                   "again = calls\n",
                   sError));
    CHECK(LuaGlobal(L, "enter") == "");
    CHECK(LuaGlobal(L, "pending") == "1");
    CHECK(LuaGlobal(L, "ended") == "open;ended;markprofile;markdisplay;");
    CHECK(LuaGlobal(L, "after") == "nil");
    CHECK(LuaGlobal(L, "again") == "");

    REQUIRE(RunLua(L,
                   "local this = ifs_opt_pcvideo\n"
                   "calls = ''\n"
                   "poll = { 0, 0, 0, 0 }\n"
                   "ifs_opt_pcvideo_fnMark(this)\n"
                   "idle = calls\n"
                   "calls = ''\n"
                   "this.iKeepSave = nil\n"
                   "this.iConfirmResult = nil\n"
                   "poll = { 0, 0, 2, 2 }\n"
                   "ifs_opt_pcvideo_fnConfirmUpdate(this)\n"
                   "stale = calls\n"
                   "poll = { 1, 0, 0, 0 }\n"
                   "ifs_opt_pcvideo_fnConfirmUpdate(this)\n"
                   "poll = { 0, 0, 2, 2 }\n"
                   "ifs_opt_pcvideo_fnConfirmUpdate(this)\n"
                   "failed = calls\n"
                   "calls = ''\n"
                   "ifs_opt_pcvideo_fnConfirmUpdate(this)\n"
                   "repeated = calls\n",
                   sError));
    CHECK(LuaGlobal(L, "idle") == "markprofile;markdisplay;");
    CHECK(LuaGlobal(L, "stale") == "");
    CHECK(LuaGlobal(L, "failed") == "ended;savefailed;");
    CHECK(LuaGlobal(L, "repeated") == "");
    lua_close(L);
}

TEST_CASE("display menu source pins" * doctest::test_suite("unit"))
{
    std::string sLuaRaw;
    std::string sProfileRaw;
    if (!ReadSource("../src/Battlefront2/Source/LuaCallbacks_PC.cpp", sLuaRaw) ||
        !ReadSource("../src/Battlefront2/Source/PlayerProfile.cpp", sProfileRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sLua = StripCr(sLuaRaw);
    const std::string sProfile = StripCr(sProfileRaw);

    const std::string sVideo = FunctionBody(sLua, "int ScriptCB_SetPCVideoOptions(");
    REQUIRE(!sVideo.empty());
    CHECK(!Has(sVideo, "Confirm"));
    CHECK(!Has(sVideo, "ApplyMenuDisplay"));
    CHECK(!Has(sVideo, "bConfirm"));

    const std::string sMenu = FunctionBody(sProfile, "bool PlayerProfile::ApplyMenuDisplay(");
    REQUIRE(!sMenu.empty());
    CHECK(!Has(sMenu, "SaveMenu"));
    CHECK(!Has(sMenu, "SaveIni"));
    CHECK(!Has(sMenu, "SaveDisplay"));
    size_t uiValidate = sMenu.find("ValidateMenu");
    size_t uiLive = sMenu.find("ApplyLiveDisplay();");
    REQUIRE(uiValidate != std::string::npos);
    REQUIRE(uiLive != std::string::npos);
    CHECK(uiValidate < uiLive);

    const char* apcNames[] = {"GetDisplayOptions", "SetDisplayOptions", "PollDisplayConfirm",
                              "EndDisplayConfirm", "MarkDisplayOptions",
                              "RestoreDisplayOptions", "SaveDisplayOptions"};
    const char* apcForbidden[] = {"Reset(", "pcApplyDisplayMode", "internalReset", "CreateDevice"};
    for (size_t i = 0; i < sizeof(apcNames) / sizeof(apcNames[0]); ++i)
    {
        const std::string sSignature =
            std::string("int Lua_Callbacks::ScriptCB_") + apcNames[i] + "(";
        const std::string sBody = FunctionBody(sLua, sSignature.c_str());
        CAPTURE(apcNames[i]);
        REQUIRE(!sBody.empty());
        for (size_t j = 0; j < sizeof(apcForbidden) / sizeof(apcForbidden[0]); ++j)
        {
            CHECK(!Has(sBody, apcForbidden[j]));
        }
    }

    const std::string sSet = FunctionBody(sLua, "int Lua_Callbacks::ScriptCB_SetDisplayOptions(");
    REQUIRE(!sSet.empty());
    size_t uiSetValidate = sSet.find("ValidateMenu");
    size_t uiSetApply = sSet.find("ApplyMenuDisplay");
    REQUIRE(uiSetValidate != std::string::npos);
    REQUIRE(uiSetApply != std::string::npos);
    CHECK(uiSetValidate < uiSetApply);
    CHECK(Has(sSet, "bWaitQuery = uiFutureFrames == 1"));
    CHECK(!Has(sSet, "\"waitquery\""));

    const std::string sGet = FunctionBody(sLua, "int Lua_Callbacks::ScriptCB_GetDisplayOptions(");
    REQUIRE(!sGet.empty());
    CHECK(Has(sGet, "\"futureframes\", rDisplay.bNoWaitQuery ? 2 : 1"));
    CHECK(Has(sGet,
              "\"clirefresh\", (rDisplay.uiCliMask & DisplayCfg::kRefresh) ? (int)rDisplay.uiRefresh : 0"));
    CHECK(!Has(sGet, "\"refresh\""));
    const std::string sFill = FunctionBody(sLua, "int ScriptCB_FillMultisampleTable(");
    REQUIRE(!sFill.empty());
    CHECK(Has(sFill, "pcGetMultiSampleLevels("));
    CHECK(Has(sFill, "MultisampleList("));
    CHECK(!Has(sFill, "VidMode_Usable("));

    size_t uiSaves = 0;
    for (size_t uiPos = sProfile.find("SaveMenu("); uiPos != std::string::npos;
         uiPos = sProfile.find("SaveMenu(", uiPos + 1))
    {
        ++uiSaves;
    }
    CHECK(uiSaves == 1);

    const std::string sWrite = FunctionBody(sProfile, "static void WriteDisplayFields(");
    REQUIRE(!sWrite.empty());
    CHECK(Has(sWrite, "pcSetWaitForFrameQuery("));
    CHECK(Has(sWrite, "uiFps = "));
    const std::string sRevert = FunctionBody(sProfile, "static void RevertConfirm(");
    REQUIRE(!sRevert.empty());
    CHECK(Has(sRevert, "WriteDisplayFields("));
    CHECK(Has(sRevert, "sConfirmPrevFps"));
    CHECK(!Has(sRevert, "gDisplay.uiRefresh ="));
    CHECK(Has(sRevert, "RedRenderer::pcClearStageArm();\n    PlayerProfile::ApplyLiveDisplay();"));
    const std::string sRestore = FunctionBody(sProfile, "void PlayerProfile::RestoreDisplay(");
    REQUIRE(!sRestore.empty());
    CHECK(Has(sRestore, "WriteDisplayFields("));
    CHECK(Has(sRestore, "RedRenderer::pcClearStageArm();\n    ApplyLiveDisplay();"));
    CHECK(!Has(sRestore, "rDisplay.uiFps ="));
    const size_t uiClearPending = sRestore.find("ClearPendingAdapter()");
    const size_t uiMarkedCheck = sRestore.find("if (!sMarked)");
    REQUIRE(uiClearPending != std::string::npos);
    REQUIRE(uiMarkedCheck != std::string::npos);
    CHECK(uiClearPending < uiMarkedCheck);
    const std::string sSaveDisplay =
        FunctionBody(sProfile, "unsigned int PlayerProfile::SaveDisplay(");
    REQUIRE(!sSaveDisplay.empty());
    CHECK(Has(sSaveDisplay, "SavePendingAdapter()"));
    CHECK(CountOf(sProfile, "pcSetWaitForFrameQuery(") == 2);
}

TEST_CASE("gamma pass source pins" * doctest::test_suite("unit"))
{
    std::string sRendererRaw;
    std::string sPassRaw;
    std::string sColorRaw;
    std::string sStateRaw;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRendererRaw) ||
        !ReadSource("../src/RedEngineFL/Graphics/PC/pcGammaPass.cpp", sPassRaw) ||
        !ReadSource("../src/Battlefront2/Platform/Pc/pcColorControl.cpp", sColorRaw) ||
        !ReadSource("../src/Battlefront2/Source/GameState.cpp", sStateRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sRenderer = StripCr(sRendererRaw);
    const std::string sPass = StripCr(sPassRaw);
    const std::string sColor = StripCr(sColorRaw);
    const std::string sState = StripCr(sStateRaw);

    const std::string sCleanup = FunctionBody(sRenderer, "void RedRenderer::Cleanup()");
    REQUIRE(!sCleanup.empty());
    CHECK(Has(sCleanup, "GammaPass::ReleaseDefault("));

    const std::string sCleanupStatic = FunctionBody(sRenderer, "void RedRenderer::CleanupStatic()");
    REQUIRE(!sCleanupStatic.empty());
    size_t uiShutdown = sCleanupStatic.find("GammaPass::Shutdown(");
    size_t uiDeviceRelease = sCleanupStatic.find("m_pd3dDevice->Release(");
    REQUIRE(uiShutdown != std::string::npos);
    REQUIRE(uiDeviceRelease != std::string::npos);
    CHECK(uiShutdown < uiDeviceRelease);

    const std::string sEnd = FunctionBody(sRenderer, "void RedRenderer::EndFrame()");
    REQUIRE(!sEnd.empty());
    size_t uiDraw = sEnd.find("GammaPass::Draw(");
    size_t uiPresent = sEnd.find("Present(");
    REQUIRE(uiDraw != std::string::npos);
    REQUIRE(uiPresent != std::string::npos);
    CHECK(uiDraw < uiPresent);

    const std::string sRegister =
        FunctionBody(sRenderer, "void RedRenderer::pcRegisterResetFunction(");
    REQUIRE(!sRegister.empty());
    CHECK(Has(sRegister, "LOG(LogRender, Error"));
    CHECK(!Has(sRenderer, "->SetGammaRamp("));

    CHECK(!Has(sColor, "SetGammaRamp(0, 0"));
    CHECK(CountOf(sColor, "pcSetGammaRamp(m_gammaRamp)") == 3);
    const std::string sEnable = FunctionBody(sColor, "void ColorControl::PlatformEnable(");
    REQUIRE(!sEnable.empty());
    CHECK(Has(sEnable, "pcSetGammaRamp(m_gammaRamp)"));

    const char* apcForbidden[] = {"pcSetRenderState", "pcSetTexture", "pcSetSamplerState",
                                  "pcLoadPixelShader", "StateManager"};
    for (size_t i = 0; i < sizeof(apcForbidden) / sizeof(apcForbidden[0]); ++i)
    {
        CHECK(!Has(sPass, apcForbidden[i]));
    }
    const std::string sReset =
        FunctionBody(sPass, "static void __cdecl GammaReset(ResetFlagsT eFlags)\n{");
    REQUIRE(!sReset.empty());
    CHECK(Has(sReset, "RESETFLAGS_CREATE_RESOURCES"));
    CHECK(Has(sReset, "Activate(true)"));
    const std::string sActivate = FunctionBody(sPass, "static void Activate(bool bInReset)");
    REQUIRE(!sActivate.empty());
    CHECK(Has(sActivate, "RedRenderer::m_bResourcesReleased && !bInReset"));
    const std::string sFallback =
        FunctionBody(sPass, "static void Fallback(HRESULT hr, const char* pcReason)");
    REQUIRE(!sFallback.empty());
    CHECK(Has(sFallback, "GammaPass::Shutdown("));
    const std::string sShutdown = FunctionBody(sPass, "void GammaPass::Shutdown()");
    REQUIRE(!sShutdown.empty());
    CHECK(Has(sShutdown, "ReleaseDefault("));
    CHECK(Has(sShutdown, "sLutTexture->Release("));
    CHECK(Has(sShutdown, "sPixelShader->Release("));

    const std::string sPre = FunctionBody(sState, "void GameState::PreStateInit(bool bMission)");
    REQUIRE(!sPre.empty());
    size_t uiEffect = sPre.find("FLEffect::InitAll(bMission);");
    size_t uiBright = sPre.find("mGammaBrightness");
    size_t uiLoad = sPre.find("LoadDisplay::Create();");
    REQUIRE(uiEffect != std::string::npos);
    REQUIRE(uiBright != std::string::npos);
    REQUIRE(uiLoad != std::string::npos);
    CHECK(uiEffect < uiBright);
    CHECK(uiBright < uiLoad);

    CHECK(!TreeHas("../src", "GetDeviceGammaRamp"));
}

TEST_CASE("shell stretch source pins" * doctest::test_suite("unit"))
{
    std::string sRendererRaw;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRendererRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sRenderer = StripCr(sRendererRaw);

    const std::string sEnd = FunctionBody(sRenderer, "void RedRenderer::EndFrame()");
    REQUIRE(!sEnd.empty());
    CHECK(Has(sEnd, "StretchRect(m_pBackBufferSurface, 0, sShellSurface, 0,"));
    CHECK(Has(sEnd, "D3DTEXF_LINEAR"));
    CHECK(Has(sEnd, "sShellChain->Present("));

    const std::string sReset = FunctionBody(sRenderer, "static void __cdecl ShellStretchReset(");
    REQUIRE(!sReset.empty());
    CHECK(Has(sReset, "RESETFLAGS_FREE_RESOURCES"));
    CHECK(Has(sReset, "ReleaseShellStretch()"));

    const std::string sCleanup = FunctionBody(sRenderer, "void RedRenderer::Cleanup()");
    REQUIRE(!sCleanup.empty());
    size_t uiGamma = sCleanup.find("GammaPass::ReleaseDefault();");
    size_t uiShell = sCleanup.find("ReleaseShellStretch();");
    REQUIRE(uiGamma != std::string::npos);
    REQUIRE(uiShell != std::string::npos);
    CHECK(uiGamma < uiShell);

    CHECK(Has(sRenderer, "pcRegisterResetFunction(&ShellStretchReset)"));
}

TEST_CASE("display menu label format pins" * doctest::test_suite("unit"))
{
    std::string sLuaRaw;
    if (!ReadSource("../src/Battlefront2/Source/LuaCallbacks_PC.cpp", sLuaRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sLua = StripCr(sLuaRaw);
    const char* pcCall = "PblUnicode::usnprintf(";
    size_t uiCalls = 0;
    for (size_t uiAt = sLua.find(pcCall); uiAt != std::string::npos;
         uiAt = sLua.find(pcCall, uiAt + 1))
    {
        ++uiCalls;
        size_t uiComma = sLua.find(',', uiAt);
        REQUIRE(uiComma != std::string::npos);
        uiComma = sLua.find(',', uiComma + 1);
        REQUIRE(uiComma != std::string::npos);
        const size_t uiOpen = sLua.find('"', uiComma);
        const size_t uiEnd = sLua.find('\n', uiComma);
        REQUIRE(uiOpen != std::string::npos);
        REQUIRE(uiOpen < uiEnd);
        const size_t uiClose = sLua.find('"', uiOpen + 1);
        REQUIRE(uiClose != std::string::npos);
        const std::string sFormat = sLua.substr(uiOpen + 1, uiClose - uiOpen - 1);
        CAPTURE(sFormat);
        for (size_t i = 0; i < sFormat.size(); ++i)
        {
            if (sFormat[i] != '%')
            {
                continue;
            }
            REQUIRE(i + 1 < sFormat.size());
            const char cSpec = sFormat[i + 1];
            CHECK((cSpec == 'c' || cSpec == 'd' || cSpec == 'f' || cSpec == 's' || cSpec == '%'));
            ++i;
        }
    }
    CHECK(uiCalls >= 4);
}

TEST_CASE("unicode usnprintf integer labels" * doctest::test_suite("unit"))
{
    unsigned short ausBuf[256];
    PblUnicode::usnprintf(ausBuf, 255, "%d Hz", 60);
    ausBuf[255] = 0;
    const unsigned short ausHz[] = {'6', '0', ' ', 'H', 'z', 0};
    CHECK(memcmp(ausBuf, ausHz, sizeof(ausHz)) == 0);

    PblUnicode::usnprintf(ausBuf, 255, "%d", 144);
    ausBuf[255] = 0;
    const unsigned short ausCap[] = {'1', '4', '4', 0};
    CHECK(memcmp(ausBuf, ausCap, sizeof(ausCap)) == 0);
}

TEST_CASE("display msaa single query pins" * doctest::test_suite("unit"))
{
    std::string sRendererRaw;
    std::string sLuaRaw;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRendererRaw) ||
        !ReadSource("../src/Battlefront2/Source/LuaCallbacks_PC.cpp", sLuaRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sRenderer = StripCr(sRendererRaw);
    const std::string sLua = StripCr(sLuaRaw);

    const std::string sSupported =
        FunctionBody(sRenderer, "bool RedRenderer::pcMultiSampleSupported(");
    REQUIRE(!sSupported.empty());
    CHECK(Has(sSupported, "pcGetMultiSampleLevels("));
    CHECK(!Has(sSupported, "CheckDeviceMultiSampleType"));

    const std::string sLevels =
        FunctionBody(sRenderer, "void RedRenderer::pcGetMultiSampleLevels(");
    REQUIRE(!sLevels.empty());
    CHECK(Has(sLevels, "D3DFMT_A8R8G8B8"));
    CHECK(Has(sLevels, "D3DFMT_D24S8"));
    const size_t uiNoRender = sLevels.find("g_bNoRender");
    const size_t uiFake = sLevels.find("FakeMultisampleLevels(");
    const size_t uiQuery = sLevels.find("CheckDeviceMultiSampleType(");
    REQUIRE(uiNoRender != std::string::npos);
    REQUIRE(uiFake != std::string::npos);
    REQUIRE(uiQuery != std::string::npos);
    CHECK(uiNoRender < uiFake);
    CHECK(uiFake < uiQuery);
    CHECK(Has(sRenderer, "m_d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;"));
    CHECK(Has(sRenderer, "m_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;"));
    CHECK(CountOf(sRenderer, "BackBufferFormat =") == 1);
    CHECK(CountOf(sRenderer, "AutoDepthStencilFormat =") == 1);

    const std::string sFill = FunctionBody(sLua, "int ScriptCB_FillMultisampleTable(");
    REQUIRE(!sFill.empty());
    CHECK(Has(sFill, "msaa list truncated wanted="));
}

TEST_CASE("present watch wiring pins" * doctest::test_suite("unit"))
{
    std::string sRendererRaw;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRendererRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sRenderer = StripCr(sRendererRaw);

    const std::string sEnd = FunctionBody(sRenderer, "void RedRenderer::EndFrame()");
    REQUIRE(!sEnd.empty());
    const size_t uiPresent = sEnd.find("->Present(");
    const size_t uiStep = sEnd.find("PresentWatchStep(");
    REQUIRE(uiPresent != std::string::npos);
    REQUIRE(uiStep != std::string::npos);
    CHECK(uiPresent < uiStep);
    CHECK(Has(sEnd, "IsIconic(hGlobalWnd) == 0"));
    CHECK(Has(sEnd, "hr == D3D_OK"));

    const std::string sThrottles = FunctionBody(sRenderer, "bool RedRenderer::pcPresentThrottles()");
    REQUIRE(!sThrottles.empty());
    CHECK(Has(sThrottles, "PresentThrottledNow("));
    CHECK(Has(sThrottles, "g_bNoRender"));
    CHECK(!Has(sThrottles, "PresentationInterval"));

    CHECK(CountOf(sRenderer, "ResetPresentWatch();") >= 3);
}

TEST_CASE("desktop mode source pins" * doctest::test_suite("unit"))
{
    std::string sRendererRaw;
    if (!ReadSource("../src/RedEngineFL/Graphics/PC/pcRedRenderer.cpp", sRendererRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sRenderer = StripCr(sRendererRaw);

    const std::string sSaved = FunctionBody(sRenderer, "static bool SavedDesktopMode(");
    REQUIRE(!sSaved.empty());
    CHECK(Has(sSaved, "GetAdapterIdentifier("));
    const size_t uiRegistry = sSaved.find("ENUM_REGISTRY_SETTINGS");
    const size_t uiCurrent = sSaved.find("ENUM_CURRENT_SETTINGS");
    REQUIRE(uiRegistry != std::string::npos);
    REQUIRE(uiCurrent != std::string::npos);
    CHECK(uiRegistry < uiCurrent);

    const std::string sDesktop = FunctionBody(sRenderer, "void RedRenderer::pcGetAdapterDesktop(");
    REQUIRE(!sDesktop.empty());
    const size_t uiSaved = sDesktop.find("SavedDesktopMode(");
    const size_t uiLive = sDesktop.find("GetAdapterDisplayMode(");
    REQUIRE(uiSaved != std::string::npos);
    REQUIRE(uiLive != std::string::npos);
    CHECK(uiSaved < uiLive);
    CHECK(Has(sDesktop,
              "const bool bOwnExclusive = uiAdapter == gAdapter && m_pd3dDevice != 0 && !m_d3dpp.Windowed;"));
    CHECK(Has(sDesktop, "if (bOwnExclusive && SavedDesktopMode(uiAdapter, dm))"));

    const std::string sWatch = FunctionBody(sRenderer, "static unsigned int WatchRefreshHz()");
    REQUIRE(!sWatch.empty());
    CHECK(Has(sWatch, "ENUM_CURRENT_SETTINGS"));
    CHECK(!Has(sWatch, "ENUM_REGISTRY_SETTINGS"));

    const std::string sReadback = FunctionBody(sRenderer, "static unsigned int ReadbackRefresh()");
    REQUIRE(!sReadback.empty());
    CHECK(Has(sReadback, "ENUM_CURRENT_SETTINGS"));
    CHECK(!Has(sReadback, "ENUM_REGISTRY_SETTINGS"));

    const std::string sSet = FunctionBody(sRenderer, "void RedRenderer::pcSetDisplayMode(");
    REQUIRE(!sSet.empty());
    CHECK(Has(sSet, "SavedDesktopMode("));
    CHECK(Has(sSet, "dmDisplayFrequency"));
    CHECK(Has(sSet, "uiDesktopHz)"));
}

TEST_CASE("frame cap vsync source pins" * doctest::test_suite("unit"))
{
    std::string sStateRaw;
    if (!ReadSource("../src/Battlefront2/Source/GameState.cpp", sStateRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sState = StripCr(sStateRaw);

    const std::string sTime = FunctionBody(sState, "void GameState::UpdateTime()");
    REQUIRE(!sTime.empty());
    CHECK(Has(sTime, "bool bThrottled = RedRenderer::pcPresentThrottles();"));
    CHECK(Has(sTime, "BindReal(ops, plan, DisplayLogic::EffectiveFpsCap(DisplayCfg::gDisplay.uiFps, bThrottled));"));
    CHECK(!Has(sTime, "BindReal(ops, plan, DisplayCfg::gDisplay.uiFps)"));
    CHECK(CountOf(sTime, "BindReal(") == 1);
    CHECK(Has(sTime, "OPENSWBF2 PACING vsync on, fps cap %u not applied"));
    CHECK(Has(sTime, "OPENSWBF2 PACING fps cap %u applied"));
}

TEST_CASE("hardware cursor source pins" * doctest::test_suite("unit"))
{
    std::string sMouseRaw;
    std::string sMainRaw;
    std::string sCrashRaw;
    std::string sRenderRaw;
    std::string sJoyRaw;
    if (!ReadSource("../src/RedEngineFL/input/pc/pcRedMouse.cpp", sMouseRaw) ||
        !ReadSource("../src/Battlefront2/Source/pcMain.cpp", sMainRaw) ||
        !ReadSource("../src/Battlefront2/Source/pcCrash.cpp", sCrashRaw) ||
        !ReadSource("../src/RedEngineFL/Application/pc/pcFLRender.cpp", sRenderRaw) ||
        !ReadSource("../src/RedEngineFL/input/pc/pcRedJoystick.cpp", sJoyRaw))
    {
        SkipCase("source files not found");
    }
    const std::string sMouse = StripCr(sMouseRaw);
    const std::string sMain = StripCr(sMainRaw);
    const std::string sCrash = StripCr(sCrashRaw);
    const std::string sRender = StripCr(sRenderRaw);
    const std::string sJoy = StripCr(sJoyRaw);

    const char* apcCoop[] = { "void RedMouse::Remove()", "void RedMouse::Insert()" };
    for (size_t i = 0; i < sizeof(apcCoop) / sizeof(apcCoop[0]); ++i)
    {
        const std::string sBody = FunctionBody(sMouse, apcCoop[i]);
        REQUIRE(!sBody.empty());
        CHECK(Has(sBody, "sdwCoopFlags"));
        CHECK(!Has(sBody, "DISCL_EXCLUSIVE | DISCL_FOREGROUND)"));
    }

    const std::string sReconcile =
        FunctionBody(sMouse, "static void ReconcileCursor(RedMouse& kMouse)");
    REQUIRE(!sReconcile.empty());
    CHECK(Has(sReconcile, "CursorPlanFor("));
    CHECK(Has(sReconcile, "DISCL_NONEXCLUSIVE"));
    CHECK(Has(sReconcile, "GetClipCursor("));
    CHECK(Has(sReconcile, "HwCursor::ReleaseClip()"));
    CHECK(Has(sReconcile, "mCursorImage.Enable(false)"));
    CHECK(Has(sReconcile, "kMouse.SetCursorPos(kMouse.m_X, kMouse.m_Y)"));
    CHECK(Has(sReconcile, "if (kPlan.bHcursor && !kPrev.bHcursor && kPrev.bExclusive)\n    {\n        kMouse.SetCursorPos(kMouse.m_X, kMouse.m_Y);\n    }"));
    CHECK(CountOf(sReconcile, "SetCursorPos(") == 1);

    const std::string sRelease = FunctionBody(sMouse, "void HwCursor::ReleaseClip()");
    REQUIRE(!sRelease.empty());
    CHECK(Has(sRelease, "ClipCursor(0);"));
    CHECK(!Has(sRelease, "EqualRect("));
    CHECK(!Has(sRelease, "GetClipCursor("));

    const std::string sTerminate = FunctionBody(sCrash, "static void TerminateNow(int iCode)");
    REQUIRE(!sTerminate.empty());
    CHECK(Has(sTerminate, "FrameLimiter::EndPeriod();"));
    CHECK(sTerminate.find("FrameLimiter::EndPeriod();") < sTerminate.find("TerminateProcess("));
    CHECK(sTerminate.find("HwCursor::ReleaseClip();") < sTerminate.find("FrameLimiter::EndPeriod();"));
    CHECK(!Has(sTerminate, "FrameLimiter::Shutdown();"));
    std::string sLimiterRaw;
    REQUIRE(ReadSource("../src/RedEngineFL/Application/pc/pcFrameLimiter.cpp", sLimiterRaw));
    const std::string sLimiter = StripCr(sLimiterRaw);
    const std::string sEndPeriod = FunctionBody(sLimiter, "void EndPeriod()");
    REQUIRE(!sEndPeriod.empty());
    CHECK(Has(sEndPeriod, "timeEndPeriod(1);"));
    CHECK(!Has(sEndPeriod, "CloseHandle("));
    const std::string sLimiterShutdown = FunctionBody(sLimiter, "void Shutdown()");
    REQUIRE(!sLimiterShutdown.empty());
    CHECK(Has(sLimiterShutdown, "EndPeriod();"));

    const std::string sUpdate = FunctionBody(sMouse, "void RedMouse::Update(");
    REQUIRE(!sUpdate.empty());
    CHECK(Has(sUpdate, "ReconcileCursor(*this);"));
    CHECK(sUpdate.find("ReconcileCursor(*this);") < sUpdate.find("if (!pc_device)"));
    CHECK(Has(sReconcile, "if (kMouse.Exists())\n        {\n            kMouse.Remove();"));
    CHECK(Has(sUpdate, "CursorButtonLatch("));
    CHECK(Has(sUpdate, "MapClientToBackBuffer("));
    CHECK(Has(sUpdate, "WindowFromPoint("));
    CHECK(Has(sUpdate, "!= DisplayLogic::kModeFullscreen && !bInside;"));
    CHECK(Has(sUpdate, "bBlockNew = RedRenderer::GetDisplayMode() != DisplayLogic::kModeFullscreen;"));
    CHECK(!Has(sUpdate, "kModeWindowed"));
    CHECK(Has(sUpdate, "if (GetCursorPos(&kScreen) && GetClientRect(hGlobalWnd, &kRc))"));
    CHECK(CountOf(sUpdate, "GetCursorPos(") == 1);
    CHECK(Has(sUpdate, "m_X += m_dX;"));

    const std::string sBuild = FunctionBody(sMouse, "void HwCursor::Build(");
    REQUIRE(!sBuild.empty());
    CHECK(Has(sBuild, "g_bNoRender"));
    CHECK(Has(sBuild, "OPENSWBF2 CURSOR mode=%s image=skipped"));
    CHECK(Has(sMouse, "D3DPOOL_SCRATCH"));
    CHECK(Has(sMouse, "kCursorSrcMax"));
    CHECK(Has(sMouse, "xHotspot = 0"));
    CHECK(CountOf(sMouse, "DestroyIcon((HICON)shCursor);") == 2);

    const std::string sMake = FunctionBody(sMouse, "static HCURSOR MakeCursorFromCache(");
    REQUIRE(!sMake.empty());
    CHECK(Has(sMake, "BITMAPV5HEADER"));
    CHECK(Has(sMake, "BI_BITFIELDS"));
    CHECK(Has(sMake, "bV5AlphaMask = 0xFF000000"));
    CHECK(Has(sMake, "CursorMaskFromBgra("));
    CHECK(sMake.find("CursorScaleBgra(") < sMake.find("CursorMaskFromBgra("));
    CHECK(!Has(sMouse, "DestroyCursor("));

    const size_t uiSet = sMain.find("case WM_SETCURSOR:");
    REQUIRE(uiSet != std::string::npos);
    const size_t uiKey = sMain.find("case WM_KEYDOWN:", uiSet);
    REQUIRE(uiKey != std::string::npos);
    const std::string sSet = sMain.substr(uiSet, uiKey - uiSet);
    CHECK(Has(sSet, "HTCLIENT"));
    CHECK(Has(sSet, "HwCursor::Software()"));
    CHECK(Has(sSet, "SetCursor(HwCursor::Current());"));
    CHECK(Has(sSet, "RedTexture::s_textureHashTable.Find(0xA2BB0616)"));

    CHECK(CountOf(sMain, "HwCursor::ReleaseClip();") == 3);
    const size_t uiClose = sMain.find("case WM_CLOSE:");
    REQUIRE(uiClose != std::string::npos);
    const size_t uiExit = sMain.find("pcCrash::Exit(0);", uiClose);
    REQUIRE(uiExit != std::string::npos);
    const std::string sClose = sMain.substr(uiClose, uiExit - uiClose);
    CHECK(Has(sClose, "HwCursor::ReleaseClip();"));
    CHECK(sClose.find("\n    case ", 1) == std::string::npos);

    const std::string sActivate = FunctionBody(sMain, "void ActivateFrontline(bool bActivate)");
    REQUIRE(!sActivate.empty());
    CHECK(Has(sActivate, "SetCursor(HwCursor::Current());"));
    CHECK(Has(sActivate, "IDC_ARROW"));

    const std::string sFilter = FunctionBody(sCrash, "long pcCrash::Filter(");
    REQUIRE(!sFilter.empty());
    CHECK(Has(sFilter, "HwCursor::ReleaseClip();"));
    const char* apcExits[] = { "static void TerminateNow(int iCode)", "void __cdecl pcCrash::Exit(int iCode)",
        "static DWORD WINAPI WatchdogThread(LPVOID)", "void pcCrash::FatalReset(" };
    for (size_t i = 0; i < sizeof(apcExits) / sizeof(apcExits[0]); ++i)
    {
        const std::string sBody = FunctionBody(sCrash, apcExits[i]);
        REQUIRE(!sBody.empty());
        CHECK(Has(sBody, "HwCursor::ReleaseClip();"));
    }
    const std::string sWatchdog = FunctionBody(sCrash, "static DWORD WINAPI WatchdogThread(LPVOID)");
    CHECK(sWatchdog.find("HwCursor::ReleaseClip();") < sWatchdog.find("SetEvent(ghFaultEvent);"));

    const std::string sInit = FunctionBody(sRender, "void InitMouseCursor()");
    REQUIRE(!sInit.empty());
    CHECK(Has(sInit, "HwCursor::Build(PblHash(\"mousecursor\"));"));
    const std::string sCleanup = FunctionBody(sRender, "void FLRenderer::Cleanup()");
    REQUIRE(!sCleanup.empty());
    CHECK(Has(sCleanup, "HwCursor::Destroy();"));
    const std::string sEnable = FunctionBody(sRender, "void FLRenderer::EnableCursor(bool bEnable)");
    REQUIRE(!sEnable.empty());
    CHECK(!Has(sEnable, "HwCursor"));

    CHECK(Has(sJoy, "if (HwCursor::Shown())"));
    CHECK(Has(sJoy, "pMouse->SetCursorPos(iX, iY);"));
    CHECK(Has(sJoy, "FLRenderer::SetCursorPos((float)iX, (float)iY);"));
}
