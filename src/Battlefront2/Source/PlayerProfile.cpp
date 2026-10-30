#include <string.h>
#include "RedEngineFL/Application/pc/pcRedParticle.h"

#include "PlayerProfile.h"
#include "AITuning.h"

#include "CommandLine.h"
#include "FLInputManager.h"
#include "GameSoundEngine.h"
#include "GameState.h"
#include "PauseMenu.h"
#include "PebbleFL/Common/PblHashTable.h"
#include "PebbleFL/Common/PblUnicode.h"
#include "RedEngineFL/Graphics/PC/pcRedAutoSettings.h"
#include "RedEngineFL/Graphics/PC/pcRedRenderer.h"
#include "RedEngineFL/Graphics/PC/pcDisplayLogic.h"
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"
#include "RedEngineFL/Util/RedLog.h"
#include "RedEngineFL/Util/RedLocalizeDB.h"
#include "ColorControl.h"
#include "Damageable.h"
#include "HDREffect.h"
#include "LockOnManager.h"
#include "NetComm.h"
#include "NetGame.h"
#include "PropCluster.h"
#include "VoiceChatNode.h"
#include "RedEngineFL/movie/RedMovie.h"
#include "SoundFL/source/win/sndengine.h"

#include <math.h>
#include "GameVoiceChatNode.h"
#include "NetMatch.h"
#include "RedEngineFL/input/RedJoystick.h"
#include "NetWire.h"

BOOL CALLBACK EnumObjectsCallback(DIDEVICEOBJECTINSTANCEA const* pObject, void* pvRef);

// GLOBAL: SWBF2 0x01fa9264
unsigned int mLastDirtyCRC[1];

namespace PblBuildType { extern unsigned char gCreateMemoryProfile; }

extern bool pcWindowed;
extern bool g_bNoRender;
extern int pcWidth;
extern int pcHeight;

namespace ParticleSystem { void SetFadeBias(float fBias); }
using ParticleSystem::SetFadeBias;

namespace GameVoiceChat { void* FindNodeByDeviceID(); }

// GLOBAL: SWBF2 0x01ef90d0
// SYNTHETIC: SWBF2 0x00405f40
// `dynamic initializer for 'mProfiles''
PlayerProfile::tProfile mProfiles[1];

// GLOBAL: SWBF2 0x01ef7fd0
// SYNTHETIC: SWBF2 0x00405f50
// `dynamic initializer for 'mDefaultProfile''
PlayerProfile::tProfile mDefaultProfile;

// FUNCTION: SWBF2 0x0061c870
bool PlayerProfile::tProfile::GetUnlockableState(int iIndex)
{
    return (mGameOptions.mUnlockableState & (1u << iIndex)) != 0;
}

// FUNCTION: SWBF2 0x0061c890
void PlayerProfile::tProfile::SetUnlockableState(int iIndex, bool)
{
    unsigned int bit = 1u << iIndex;
    if (!(mGameOptions.mUnlockableState & bit))
    {
        mGameOptions.mUnlockableState |= bit;
        UpdateProfCRC();
    }
}

// FUNCTION: SWBF2 0x0061d6e0
int PlayerProfile::FindByName(const unsigned short* pusName)
{
    for (unsigned int i = 0; i < sizeof(mProfiles) / sizeof(mProfiles[0]); ++i)
    {
        if (!PblUnicode::ustrcmp(pusName, mProfiles[i].mName))
        {
            return (int)i;
        }
    }
    return -1;
}

// FUNCTION: SWBF2 0x0061d7a0
void PlayerProfile::tProfile::SetTrueProfileName(const unsigned short* pusName)
{
    PblUnicode::ustrlcpy(mName, pusName, 16);
}

// FUNCTION: SWBF2 0x0061d7c0
void PlayerProfile::BackupDefaultProfile()
{
    if (!mProfiles[PlayerProfile::mCurProfile].mbFakeProfile)
    {
        memcpy(&mDefaultProfile, &mProfiles[PlayerProfile::mCurProfile], sizeof(mDefaultProfile));
    }
}

// FUNCTION: SWBF2 0x0061d7f0
void PlayerProfile::DuplicateDefaultProfile(unsigned int uiIndex, const unsigned short* pusName)
{
    if (uiIndex < sizeof(mProfiles) / sizeof(mProfiles[0]) && mDefaultProfile.mName[0])
    {
        memcpy(&mProfiles[uiIndex], &mDefaultProfile, sizeof(mDefaultProfile));
        if (pusName)
        {
            mProfiles[uiIndex].SetTrueProfileName(pusName);
        }
    }
}

// FUNCTION: SWBF2 0x0061d840
const unsigned short* PlayerProfile::GetDefaultProfileName()
{
    return mDefaultProfile.mName;
}

// GLOBAL: SWBF2 0x01ef7fa0
// SYNTHETIC: SWBF2 0x00405f30
// `dynamic initializer for 'PlayerPrefs::mAllUserPrefs''
namespace PlayerPrefs { OnePrefs mAllUserPrefs[1]; }

// GLOBAL: SWBF2 0x01eaedf4
namespace PlayerProfile { int mCurProfile; }

// GLOBAL: SWBF2 0x01eaedf8
bool mSafeMode;

// GLOBAL: SWBF2 0x01fa9268
int mNumMemoryProfiles;

// GLOBAL: SWBF2 0x009661bc
bool RedParticleRenderer::s_blurEnable;

// FUNCTION: SWBF2 0x0061c410
void PlayerPrefs::OnePrefs::Clear()
{
    mPlayerId = -1;
    mViewport = 0;
    mJoystick = 0;
    mTeamNum = PlayerPrefs::gDefaultTeamNum;
    mDifficulty = (eGAME_DIFFICULTY)PlayerPrefs::gDefaultDifficulty;
    pProfile = 0;
    hasChallenge = false;
}

// FUNCTION: SWBF2 0x0061c440
void PlayerPrefs::InitAll()
{
    PlayerProfile::ResetAllProfiles();
    PlayerPrefs::mAllUserPrefs[0].Clear();

    PlayerPrefs::mAllUserPrefs[0].mPlayerId = 0;
    PlayerPrefs::mAllUserPrefs[0].mViewport = 0;
    PlayerPrefs::mAllUserPrefs[0].mJoystick = 0;

    PlayerPrefs::mAllUserPrefs[0].pProfile = PlayerProfile::GetProfile(0);
    *(unsigned int*)((char*)&mDefaultProfile + 0x10) = (unsigned int)(uintptr_t)PlayerProfile::GetProfile(1);
}

// FUNCTION: SWBF2 0x0061c4d0
double ConvertMouseScaleToRawScale(float fScale, int iMode)
{
    double dScale = pow(5.0, (fScale - 5.0) / 5.0);
    if (iMode == 0)
    {
        dScale *= 1.3;
    }
    return dScale;
}

// FUNCTION: SWBF2 0x0061c510
void PlayerProfile::tGameOpt::ResetToDefault()
{
    mbCrossHair = true;
    mbRumble = true;
    mbTargetAutoTracking = false;
    mbSubtitle = false;

    mbWidescreen = false;
    mbHerosEnabled = true;
    mCockpitCam = false;
    mCockpitCamFlyer = false;

    mbMetaAllMaps = false;
    mbObjectiveDetails = true;

    mCurDiffLevel = (eGAME_DIFFICULTY)PlayerPrefs::gDefaultDifficulty;

    mu8MovieSubtitles = 0;
    mbSoldierHints = false;

    eGAMELANGUAGE eLang = RedLocalizeDB::GetLanguageID();
    if (eLang == eLANG_SPANISH || eLang == eLANG_ITALIAN)
    {
        mbSubtitle = true;
        mu8MovieSubtitles = 1;
    }

    mCockpitCam = true;
    mReticuleAlpha = 0.5f;
    mFriendlyFire = 100;
    mUnlockableState = 0;
    mbAimAssist = false;
    mbStickyReticule = false;
    mbFlyerRolling = true;
    mbWidescreen = false;

    memset(mPadding, 0, sizeof(mPadding));
}

// FUNCTION: SWBF2 0x0061c5a0
bool PlayerProfile::tGameOpt::ResetBindings(int iPort)
{
    mFlip[iPort] = (iPort == 2);
    mControllerScale[0][iPort] = 4;
    mControllerScale[1][iPort] = 4;
    mMouseSensitivity[iPort] = 50.0f;
    mJoystickSensitivityX[iPort] = 4;
    mJoystickSensitivityY[iPort] = 4;
    mDeadZone = 5;
    mMouseAssist[iPort] = true;

    ActionKey aDefaults[43];
    memcpy(aDefaults, RawControllerInputs::sDefaultKeyboardBindings[iPort], sizeof(aDefaults));
    memcpy(mKeyboardBindings[iPort], aDefaults, sizeof(aDefaults));

    RawControllerInputs* pJoystick =
        FLInputManager::s_instance.GetPlayerJoystick(PauseMenu::GetPausingViewport());
    memcpy(pJoystick->mCurKeyboardBindings[iPort], aDefaults,
           sizeof(aDefaults));

    return true;
}

// FUNCTION: SWBF2 0x0061c660
bool PlayerProfile::tGameOpt::NoteBindingChange()
{
    bool bChanged = false;
    for (int iMode = 0; iMode < 5; ++iMode)
    {
        RawControllerInputs* pJoystick =
            FLInputManager::s_instance.GetPlayerJoystick(PauseMenu::GetPausingViewport());
        bool bFlip = pJoystick->GetFlipForMode(iMode);
        if (mFlip[iMode] != bFlip)
        {
            mFlip[iMode] = bFlip;
            bChanged = true;
        }
        bool bAssist = pJoystick->mMouseAssist[iMode];
        if (mMouseAssist[iMode] != bAssist)
        {
            mMouseAssist[iMode] = bAssist;
            bChanged = true;
        }
    }
    return bChanged;
}

// FUNCTION: SWBF2 0x0061c6b0
void PlayerProfile::tGameOpt::ApplySettings(int iPort, int  )
{
    if (!NetGame::IsEnabled())
    {
        Damageable::SetTeamDamageScale(mFriendlyFire / 100.0f);
    }

    // OpenSWBF2: written with members; retail wrote Win32 byte offsets from the start of the
    // object (0x20bc, 0x000c, 0x2610, 0x2638, 0x263d, 0x2642), which move on x64.
    RawControllerInputs* pJoystick = FLInputManager::s_instance.GetPlayerJoystick(iPort);

    for (int iMode = 0; iMode < 5; ++iMode)
    {
        memcpy(pJoystick->mCurKeyboardBindings[iMode], mKeyboardBindings[iMode], sizeof(mKeyboardBindings[0]));
    }

    for (int iMode = 0; iMode < 5; ++iMode)
    {
        if (!mControllerScale[0][iMode])
        {
            mControllerScale[0][iMode] = 4;
        }
        if (!mControllerScale[1][iMode])
        {
            mControllerScale[1][iMode] = 4;
        }

        pJoystick->mJoystickSensitivityX[iMode] = mControllerScale[0][iMode];
        pJoystick->mJoystickSensitivityY[iMode] = mControllerScale[1][iMode];
        *(unsigned char*)&pJoystick->mFlip[iMode] = (unsigned char)mFlip[iMode];
        *(double*)pJoystick->mMouseSensitivity[iMode] = ConvertMouseScaleToRawScale(mMouseSensitivity[iMode] / 10.0f, iMode);
        pJoystick->mJoystickSensitivityX[iMode] = mJoystickSensitivityX[iMode];
        pJoystick->mJoystickSensitivityY[iMode] = mJoystickSensitivityY[iMode];
        *(unsigned char*)&pJoystick->mMouseAssist[iMode] = (unsigned char)mMouseAssist[iMode];

        pJoystick->ApplyDeadZone(mDeadZone);
    }

    FLInputManager::s_instance.EnableRumble(iPort, mbRumble);

    LockOnManager* pLockOn = iPort < 0 ? 0 : LockOnManager::GetByPlayerIndex(iPort);
    if (pLockOn)
    {
        pLockOn->SetDisplayIconsOnEverything(mbSoldierHints);
    }

    RedMovie::SubtitlesEnable(mu8MovieSubtitles != 0);
}

// FUNCTION: SWBF2 0x0061c8c0
void PlayerProfile::tSoundOpt::ResetToDefault()
{
    memset(mPadding, 0, sizeof(mPadding));

    mu8MusicVol = 7;
    mu8SFXVol = 7;
    mu8SpeechVol = 9;
    mPlaybackMode = GameSoundEngine::gDefaultSpeakerConfig
                        ? (unsigned char)GameSoundEngine::gDefaultSpeakerConfig
                        : 3;
    mMixerMode = 1;
    mEffectsEnabled = 1;
    mu8MasterVol = 8;
    mBassManagement = 0;
    mu8BattleSpeechVol = 10;
}

// FUNCTION: SWBF2 0x0061c900
void PlayerProfile::tSoundOpt::ApplySettings()
{
    if (mMixerMode)
    {
        unsigned int uiMixConfig = mMixerMode;
        if (uiMixConfig != (unsigned int)Snd::Engine::GetMixConfig() ||
            (mEffectsEnabled == 1) != (Snd::Engine::smSoftwareEffectsEnabled ||
                                       Snd::Engine::smHardwareEffectsAvailable))
        {
            GameSoundEngine::ChangeMixConfig(uiMixConfig, mEffectsEnabled != 0);
        }
    }
    mMixerMode = (unsigned char)Snd::Engine::GetMixConfig();

    if (mPlaybackMode && mPlaybackMode != Snd::EngineBase::smSpeakerConfig)
    {
        Snd::Engine::SetSpeakerConfig((Snd::EngineBase::SpeakerConfig)mPlaybackMode);
    }
    mPlaybackMode = (unsigned char)Snd::EngineBase::smSpeakerConfig;

    GameSoundEngine::UpdateBusGains(mu8SFXVol * 0.1f, mu8MusicVol * 0.1f, mu8SpeechVol * 0.1f,
                                    mu8BattleSpeechVol * 0.1f, mu8MasterVol * 0.1f);
    Snd::Engine::SetBassManagement(mBassManagement == 1);
}

// FUNCTION: SWBF2 0x0061ca00
void PlayerProfile::tSoundOpt::SanityCheck()
{
    if (mu8MusicVol > 10)
    {
        mu8MusicVol = 7;
    }
    if (mu8SFXVol > 10)
    {
        mu8SFXVol = 7;
    }
    if (mu8SpeechVol > 10)
    {
        mu8SpeechVol = 9;
    }
    if (mu8BattleSpeechVol > 10)
    {
        mu8BattleSpeechVol = 10;
    }
    if (mu8MasterVol > 10)
    {
        mu8MasterVol = 8;
    }
    for (unsigned int i = 0; i < sizeof(mPadding); ++i)
    {
        if (mPadding[i])
        {
            ResetToDefault();
        }
    }
}

// FUNCTION: SWBF2 0x0061ca50
void PlayerProfile::tSoundOpt::EnsureIsSafe()
{
    mMixerMode = 1;
    mEffectsEnabled = 0;
    mPlaybackMode = GameSoundEngine::gDefaultSpeakerConfig
                        ? (unsigned char)GameSoundEngine::gDefaultSpeakerConfig
                        : 3;
}

// FUNCTION: SWBF2 0x0061ca70
void PlayerProfile::tPCOpt::ResetToDefault()
{
    memset(mPadding, 0, sizeof(mPadding));

    mJoyEnabled = true;
    mWindowed = false;
    mVSync = false;
    // OpenSWBF2: 4x antialiasing by default (off where the device can't do it,
    // see ComputeTargetMode). 8x stays a choice: at 4K it costs ~0.5 GB in a
    // 32-bit game.
    mMultiSample = D3DMULTISAMPLE_4_SAMPLES;
    mMultiSampleQuality = 0;

    renderOptions = RedAutoSettings::GetRenderOptions();

    mResolutionX = (unsigned short)renderOptions.defaultWidth;
    mResolutionY = (unsigned short)renderOptions.defaultHeight;

    mMustBeZero = 0;
    strlcpy(mJoinIp, "127.0.0.1", sizeof(mJoinIp));
}

// FUNCTION: SWBF2 0x0061cb00
void PlayerProfile::tPCOpt::ApplySettings()
{
    ApplyPostLoadSettings();
}

// FUNCTION: SWBF2 0x0061cb10
void PlayerProfile::tPCOpt::SanityCheck()
{
    if (mMustBeZero)
    {
        ResetToDefault();
    }

    if (renderOptions.mGammaBrightness < 0.2f)
    {
        renderOptions.mGammaBrightness = 0.2f;
    }
    else if (renderOptions.mGammaBrightness > 0.8f)
    {
        renderOptions.mGammaBrightness = 0.8f;
    }
    if (renderOptions.mGammaContrast < 0.2f)
    {
        renderOptions.mGammaContrast = 0.2f;
    }
    else if (renderOptions.mGammaContrast > 0.8f)
    {
        renderOptions.mGammaContrast = 0.8f;
    }
    if (renderOptions.mFarSceneRangeFactor < 0.0f)
    {
        renderOptions.mFarSceneRangeFactor = 0.0f;
    }
    else if (renderOptions.mFarSceneRangeFactor > 1.0f)
    {
        renderOptions.mFarSceneRangeFactor = 1.0f;
    }
    if (renderOptions.mNearSceneFadeFactor < 0.0f)
    {
        renderOptions.mNearSceneFadeFactor = 0.0f;
    }
    else if (renderOptions.mNearSceneFadeFactor > 1.0f)
    {
        renderOptions.mNearSceneFadeFactor = 1.0f;
    }

    for (unsigned int i = 0; i < sizeof(mPadding); ++i)
    {
        if (mPadding[i])
        {
            ResetToDefault();
        }
    }
}

// FUNCTION: SWBF2 0x0061cbd0
void PlayerProfile::tPCOpt::EnsureIsSafe()
{
    ResetToDefault();
    mResolutionX = 800;
    mResolutionY = 600;
    renderOptions.textureDetail = QUALITY_LOW;
    renderOptions.terrainQuality = QUALITY_LOW;
    renderOptions.waterQuality = QUALITY_LOW;
    renderOptions.particleQuality = QUALITY_LOW;
    renderOptions.shadowQuality = QUALITY2_OFF;
    renderOptions.specularQuality = QUALITY2_LOW;
    renderOptions.bumpMapping = false;
    renderOptions.motionBlur = false;
    renderOptions.distortion = false;
    renderOptions.mGraphicsTier = 0;
    renderOptions.mEnableHDR = false;
}

struct TargetMode
{
    unsigned int uiWidth;
    unsigned int uiHeight;
    DisplayLogic::DisplayMode eMode;
    unsigned int uiClientW;
    unsigned int uiClientH;
    bool bVSync;
    D3DMULTISAMPLE_TYPE msType;
    unsigned long ulMSQuality;
};

static DisplayRequestTag sAppliedPC = { false, 0, 0, 0, false, false, (D3DMULTISAMPLE_TYPE)0, 0,
                                        false, 0, 0, false, false };

static bool sArmMenuConfirm;
static DisplayLogic::ConfirmPhase sConfirmPhase = DisplayLogic::kConfirmNone;
static DWORD sConfirmStart;
static unsigned int sConfirmSkewMs;
static DisplayRequestTag sConfirmRevert = { false, 0, 0, 0, false, false, (D3DMULTISAMPLE_TYPE)0,
                                            0, false, 0, 0, false, false };
static bool sConfirmHaveTarget;
static unsigned int sConfirmResult;
static bool sConfirmCliCleared;
static bool sConfirmCliPrev;
static unsigned int sConfirmPrevFps;
static bool sConfirmPrevNoWait;
static DisplayCfg::DisplaySettings sMarkDisplay;
static DisplayCfg::DisplaySettings sDeferBase;
static bool sSaveDeferred;
static unsigned int sKeepSaveResult;
static bool sMarkCliWindowed;
static bool sMarked;

static void StageRequestTag(const PlayerProfile::tPCOpt& rOpt)
{
    DisplayRequestTag kTag;
    kTag.bValid = true;
    kTag.uiOwner = PlayerProfile::GetCurProfileIndex();
    kTag.resX = rOpt.mResolutionX;
    kTag.resY = rOpt.mResolutionY;
    kTag.windowed = rOpt.mWindowed;
    kTag.vsync = rOpt.mVSync;
    kTag.ms = rOpt.mMultiSample;
    kTag.msq = rOpt.mMultiSampleQuality;
    kTag.bBorderless = DisplayCfg::gDisplay.bBorderless;
    kTag.uiRefresh = DisplayCfg::gDisplay.uiRefresh;
    kTag.uiVsync = DisplayCfg::gDisplay.uiVsync;
    kTag.bVsyncSet = (DisplayCfg::gDisplay.uiSet & DisplayCfg::kVsync) != 0;
    kTag.bConfirm = sArmMenuConfirm;
    RedRenderer::pcStageRequestTag(kTag);
}

static bool ProfileSizeUsable(unsigned int uiW, unsigned int uiH)
{
    if (uiW == 0 && uiH == 0)
    {
        return true;
    }
    unsigned int uiMaxW = 0;
    unsigned int uiMaxH = 0;
    RedRenderer::pcGetMaxTextureSize(uiMaxW, uiMaxH);
    return DisplayLogic::SizeInBounds(uiW, uiH) &&
           DisplayLogic::SizeFitsCaps(uiW, uiH, uiMaxW, uiMaxH);
}

static void ComputeTargetMode(const PlayerProfile::tPCOpt& rOpt, bool bQuiet, TargetMode& rOut)
{
    const bool bIsShell = (GameState::GetState() == 0x11E1FC01);
    RedRenderer::pcSetShellActive(bIsShell);

    unsigned int uiWidth = 0;
    unsigned int uiHeight = 0;

    D3DMULTISAMPLE_TYPE msType = rOpt.mMultiSample;
    unsigned long ulMSQuality = rOpt.mMultiSampleQuality;

    const DisplayLogic::DisplayMode eMode =
        DisplayLogic::ResolveMode(DisplayCfg::gDisplay.bBorderless, pcWindowed, rOpt.mWindowed);

    unsigned int uiDeskW = 0;
    unsigned int uiDeskH = 0;
    if (eMode == DisplayLogic::kModeBorderless && !g_bNoRender)
    {
        RedRenderer::pcGetAdapterDesktop(gAdapter, uiDeskW, uiDeskH);
    }

    unsigned int uiResX = rOpt.mResolutionX;
    unsigned int uiResY = rOpt.mResolutionY;
    if (!ProfileSizeUsable(uiResX, uiResY))
    {
        if (!bQuiet)
        {
            LOG(LogRender, Warning, "OPENSWBF2 DISPLAY profile size %ux%u out of bounds, using default",
                uiResX, uiResY);
        }
        uiResX = 0;
        uiResY = 0;
    }

    const bool bCli = pcWidth > 0 && pcHeight > 0;
    const DisplayLogic::ShellSize kSize = DisplayLogic::ShellSizePolicy(
        eMode, bIsShell, bCli ? (unsigned int)pcWidth : 0, bCli ? (unsigned int)pcHeight : 0,
        uiResX, uiResY, uiDeskW, uiDeskH);

    uiWidth = kSize.uiBackW;
    uiHeight = kSize.uiBackH;
    // OpenSWBF2: a sharp shell (DisplayLogic::ShellBackSize) the device can't
    // take falls back to the stock 800x600.
    if (bIsShell && !bCli && uiWidth && !ProfileSizeUsable(uiWidth, uiHeight))
    {
        uiWidth = 0;
        uiHeight = 0;
    }
    if (kSize.bClearMsaa)
    {
        msType = (D3DMULTISAMPLE_TYPE)0;
        ulMSQuality = 0;
    }

    if (kSize.bWarnIgnoredSize && !bQuiet)
    {
        LOG(LogRender, Warning, "OPENSWBF2 DISPLAY borderless ignores requested %ux%u, using monitor %ux%u",
            bCli ? (unsigned int)pcWidth : rOpt.mResolutionX,
            bCli ? (unsigned int)pcHeight : rOpt.mResolutionY, uiDeskW, uiDeskH);
    }

    if (!RedRenderer::pcMultiSampleSupported(gAdapter, msType, ulMSQuality,
                                             DisplayLogic::D3DWindowed(eMode)))
    {
        if (!bQuiet)
        {
            LOG(LogRender, Warning, "OPENSWBF2 DISPLAY multisample ms=%d q=%u unsupported, off",
                (int)msType, (unsigned int)ulMSQuality);
        }
        msType = (D3DMULTISAMPLE_TYPE)0;
        ulMSQuality = 0;
    }

    if (!DisplayLogic::D3DWindowed(eMode))
    {
        if (!RedVideoConfig::VidMode_Usable((int)uiWidth, (int)uiHeight, msType,
                                            ulMSQuality))
        {

            msType = (D3DMULTISAMPLE_TYPE)0;
            ulMSQuality = 0;

            if (!RedVideoConfig::VidMode_Usable((int)uiWidth, (int)uiHeight,
                                                (D3DMULTISAMPLE_TYPE)0, 0))
            {

                uiWidth = 0;
                uiHeight = 0;
            }
        }
    }

    if (!uiWidth || !uiHeight)
    {
        const unsigned int fallback[2][2] = {
            { 800, 600 },
            { 640, 480 },
        };

        const unsigned int uiModes =
            RedRenderer::pcGetValidDisplayModes(gAdapter, 0, 0, 640, 480);
        DisplayLogic::DisplayModeRec* modes =
            uiModes ? new DisplayLogic::DisplayModeRec[uiModes] : 0;
        if (modes)
        {
            RedRenderer::pcGetValidDisplayModes(gAdapter, modes, uiModes, 640, 480);
        }

        int iFound = -1;
        for (unsigned int j = 0; j < 2; ++j)
        {
            for (unsigned int i = 0; i < uiModes; ++i)
            {
                if (modes[i].w == fallback[j][0] && modes[i].h == fallback[j][1])
                {
                    iFound = (int)i;
                    break;
                }
            }
            if (iFound >= 0)
            {
                break;
            }
        }

        if (uiModes == 0)
        {
            uiWidth = DisplayLogic::kFallbackWidth;
            uiHeight = DisplayLogic::kFallbackHeight;
        }
        else
        {
            if (iFound < 0)
            {
                iFound = 0;
            }

            uiWidth = modes[iFound].w;
            uiHeight = modes[iFound].h;
        }

        delete[] modes;
    }

    if (kSize.bWarnNoMonitor && !g_bNoRender && !bQuiet)
    {
        LOG(LogRender, Warning, "OPENSWBF2 DISPLAY borderless monitor size unknown, using %ux%u",
            uiWidth, uiHeight);
    }

    rOut.uiWidth = uiWidth;
    rOut.uiHeight = uiHeight;
    rOut.eMode = eMode;
    rOut.uiClientW = kSize.uiClientW ? kSize.uiClientW : uiWidth;
    rOut.uiClientH = kSize.uiClientH ? kSize.uiClientH : uiHeight;
    rOut.bVSync = rOpt.mVSync;
    rOut.msType = msType;
    rOut.ulMSQuality = ulMSQuality;
}

// FUNCTION: SWBF2 0x0061cc20
void PlayerProfile::tPCOpt::ApplyPreLoadSettings()
{
    RedRenderer::pcSetRenderOptions(renderOptions);

    TargetMode kMode;
    ComputeTargetMode(*this, false, kMode);

    const unsigned int uiWidth = kMode.uiWidth;
    const unsigned int uiHeight = kMode.uiHeight;

    StageRequestTag(*this);
    RedRenderer::pcSetDisplayMode(uiWidth, uiHeight, kMode.eMode, kMode.uiClientW,
                                  kMode.uiClientH, kMode.bVSync, kMode.msType,
                                  kMode.ulMSQuality);
}

void PlayerProfile::ApplyLiveDisplay()
{
    tProfile* pProfile = GetCurProfile();
    if (!pProfile)
    {
        return;
    }

    tPCOpt& rPC = pProfile->mPCOptions;
    if (!ProfileSizeUsable(rPC.mResolutionX, rPC.mResolutionY))
    {
        LOG(LogRender, Warning, "OPENSWBF2 DISPLAY live size %ux%u out of bounds, ignored",
            (unsigned int)rPC.mResolutionX, (unsigned int)rPC.mResolutionY);
        return;
    }

    TargetMode kMode;
    ComputeTargetMode(rPC, true, kMode);

    StageRequestTag(rPC);
    if (RedRenderer::pcApplyDisplayMode(kMode.uiWidth, kMode.uiHeight, kMode.eMode,
                                        kMode.uiClientW, kMode.uiClientH, kMode.bVSync,
                                        kMode.msType, kMode.ulMSQuality))
    {
        TargetMode kLogged;
        ComputeTargetMode(rPC, false, kLogged);
    }

    if (RedRenderer::pcPendingConfirm())
    {
        sConfirmPhase =
            DisplayLogic::ConfirmPhaseStep(sConfirmPhase, DisplayLogic::kConfirmEvArm, 0).ePhase;
        sConfirmResult = 0;
        sKeepSaveResult = 0;
    }
}

static DWORD ConfirmClock()
{
    return GetTickCount() + sConfirmSkewMs;
}

static unsigned int ConfirmElapsed()
{
    return (unsigned int)(DWORD)(ConfirmClock() - sConfirmStart);
}

static const char* ConfirmModeName(const DisplayRequestTag& kTag)
{
    const DisplayLogic::DisplayMode eMode =
        DisplayLogic::ResolveMode(kTag.bBorderless, pcWindowed, kTag.windowed);
    if (eMode == DisplayLogic::kModeWindowed)
    {
        return "windowed";
    }
    if (eMode == DisplayLogic::kModeBorderless)
    {
        return "borderless";
    }
    return "fullscreen";
}

static void ClearConfirmTarget()
{
    sConfirmHaveTarget = false;
    sConfirmCliCleared = false;
}

static void WriteDisplayFields(bool bBorderless, unsigned int uiRefresh, unsigned int uiVsync,
                               bool bVsyncSet, unsigned int uiFps, bool bNoWaitQuery)
{
    DisplayCfg::DisplaySettings& rDisplay = DisplayCfg::gDisplay;
    rDisplay.bBorderless = bBorderless;
    rDisplay.uiRefresh = uiRefresh;
    rDisplay.uiVsync = uiVsync;
    if (bVsyncSet)
    {
        rDisplay.uiSet |= DisplayCfg::kVsync;
    }
    else
    {
        rDisplay.uiSet &= ~DisplayCfg::kVsync;
    }
    rDisplay.uiFps = uiFps;
    rDisplay.bNoWaitQuery = bNoWaitQuery;
    RedRenderer::pcSetWaitForFrameQuery(!bNoWaitQuery);
}

static unsigned int WriteMenuIni()
{
    char acPath[MAX_PATH];
    if (!DisplayCfg::DisplayIniPath(acPath, sizeof(acPath)))
    {
        LOG(LogRender, Warning, "OPENSWBF2 DISPLAY menu save failed path");
        return DisplayCfg::kSaveFailed;
    }

    const DisplayCfg::DisplaySettings& rDisplay = DisplayCfg::gDisplay;
    const DisplayCfg::DisplaySettings& rBase = sSaveDeferred ? sDeferBase : sMarkDisplay;
    unsigned int uiWritten = 0;
    const DisplayCfg::SaveResult eResult = DisplayCfg::SaveMenu(
        rBase, rDisplay, acPath, sConfirmPhase == DisplayLogic::kConfirmNone, uiWritten);

    const struct
    {
        unsigned int uiKey;
        const char* pcKey;
        const char* pcSwitch;
        unsigned int uiValue;
    } akKeys[] = {
        { DisplayCfg::kBorderless, "Borderless", "borderless", rDisplay.bBorderless ? 1u : 0u },
        { DisplayCfg::kRefresh, "Refresh", "refresh", rDisplay.uiRefresh },
        { DisplayCfg::kVsync, "Vsync", "vsync", rDisplay.uiVsync },
        { DisplayCfg::kFps, "Fps", "fps", rDisplay.uiFps },
        { DisplayCfg::kNoWaitQuery, "NoWaitQuery", "nowaitquery", rDisplay.bNoWaitQuery ? 1u : 0u },
    };
    for (unsigned int i = 0; i < sizeof(akKeys) / sizeof(akKeys[0]); ++i)
    {
        if ((uiWritten & akKeys[i].uiKey) && (rDisplay.uiCliMask & akKeys[i].uiKey))
        {
            LOG(LogRender, Inform,
                "OPENSWBF2 DISPLAY %s=%u saved from menu, -%s switch still wins next start",
                akKeys[i].pcKey, akKeys[i].uiValue, akKeys[i].pcSwitch);
        }
    }
    if (uiWritten != 0)
    {
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menu saved keys=0x%x", uiWritten);
    }

    if (eResult == DisplayCfg::kSaveOk)
    {
        sSaveDeferred = false;
        sMarkDisplay = rDisplay;
    }
    else if (eResult == DisplayCfg::kSaveDeferred)
    {
        if (!sSaveDeferred)
        {
            sDeferBase = sMarkDisplay;
            sSaveDeferred = true;
        }
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menu save deferred until the confirm ends");
    }
    else
    {
        LOG(LogRender, Warning, "OPENSWBF2 DISPLAY menu save failed");
    }
    return (unsigned int)eResult;
}

static void FinishDeferredSave()
{
    if (!sSaveDeferred || sConfirmPhase != DisplayLogic::kConfirmNone)
    {
        return;
    }
    sKeepSaveResult = WriteMenuIni();
}

static unsigned int SavePendingAdapter()
{
    const DisplayCfg::PendingAdapter& rPending = DisplayCfg::GetPendingAdapter();
    if (!rPending.bSet)
    {
        return DisplayCfg::kSaveOk;
    }
    const unsigned int uiIndex = rPending.uiIndex;
    if (!DisplayCfg::SaveAdapter(uiIndex, rPending.acName))
    {
        LOG(LogRender, Warning, "OPENSWBF2 DISPLAY adapter save failed");
        return DisplayCfg::kSaveFailed;
    }

    if (uiIndex != RedRenderer::pcGetRunningAdapter())
    {
        if (DisplayCfg::gDisplay.uiCliMask & DisplayCfg::kAdapter)
        {
            LOG(LogRender, Inform,
                "OPENSWBF2 DISPLAY adapter=%u saved, restart required, -adapter switch still wins next start",
                uiIndex);
        }
        else
        {
            LOG(LogRender, Inform, "OPENSWBF2 DISPLAY adapter=%u saved, restart required", uiIndex);
        }
    }
    else
    {
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY adapter=%u saved, running adapter unchanged",
            uiIndex);
    }
    DisplayCfg::ClearPendingAdapter();
    return DisplayCfg::kSaveOk;
}

static void RevertConfirm(const char* pcWhy)
{
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetCurProfile();
    if (!sConfirmHaveTarget || !pProfile)
    {
        ClearConfirmTarget();
        FinishDeferredSave();
        return;
    }

    if (sConfirmCliCleared)
    {
        pcWindowed = sConfirmCliPrev;
    }

    PlayerProfile::tPCOpt& rPC = pProfile->mPCOptions;
    rPC.mResolutionX = sConfirmRevert.resX;
    rPC.mResolutionY = sConfirmRevert.resY;
    rPC.mWindowed = sConfirmRevert.windowed;
    rPC.mVSync = sConfirmRevert.vsync;
    rPC.mMultiSample = sConfirmRevert.ms;
    rPC.mMultiSampleQuality = sConfirmRevert.msq;
    WriteDisplayFields(sConfirmRevert.bBorderless, sConfirmRevert.uiRefresh, sConfirmRevert.uiVsync,
                       sConfirmRevert.bVsyncSet, sConfirmPrevFps, sConfirmPrevNoWait);
    pProfile->UpdateProfCRC();
    LOG(LogRender, Warning,
        "OPENSWBF2 DISPLAY confirm %s, reverting to %ux%u mode=%s fps=%u waitquery=%d", pcWhy,
        (unsigned int)sConfirmRevert.resX, (unsigned int)sConfirmRevert.resY,
        ConfirmModeName(sConfirmRevert), sConfirmPrevFps, sConfirmPrevNoWait ? 0 : 1);
    ClearConfirmTarget();
    RedRenderer::pcClearStageArm();
    PlayerProfile::ApplyLiveDisplay();
    sConfirmResult = 2;
    FinishDeferredSave();
}

static void RunConfirmAction(DisplayLogic::ConfirmAction eAct, const char* pcWhy)
{
    switch (eAct)
    {
    case DisplayLogic::kConfirmActStart:
        sConfirmStart = ConfirmClock();
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY confirm pending 15s");
        break;
    case DisplayLogic::kConfirmActKeep:
        sConfirmResult = 1;
        ClearConfirmTarget();
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY confirm kept");
        FinishDeferredSave();
        break;
    case DisplayLogic::kConfirmActClear:
        if (sConfirmHaveTarget)
        {
            sConfirmPhase = DisplayLogic::kConfirmCounting;
        }
        else
        {
            ClearConfirmTarget();
            FinishDeferredSave();
        }
        break;
    case DisplayLogic::kConfirmActRevert:
        RevertConfirm(pcWhy);
        break;
    default:
        break;
    }
}

static void StepConfirm(DisplayLogic::ConfirmEvent eEvent, const char* pcWhy)
{
    const DisplayLogic::ConfirmStep kStep =
        DisplayLogic::ConfirmPhaseStep(sConfirmPhase, eEvent, ConfirmElapsed());
    sConfirmPhase = kStep.ePhase;
    RunConfirmAction(kStep.eAction, pcWhy);
}

static void ConfirmOnApplied(const DisplayRequestTag& kTag)
{
    if (!kTag.bValid || sConfirmPhase != DisplayLogic::kConfirmAwaitPresent)
    {
        return;
    }

    DisplayLogic::ConfirmStep kStep = DisplayLogic::ConfirmPhaseStep(
        sConfirmPhase,
        kTag.bConfirm ? DisplayLogic::kConfirmEvApplied : DisplayLogic::kConfirmEvAppliedPlain, 0);
    if (kStep.eAction == DisplayLogic::kConfirmActStart && !sConfirmHaveTarget)
    {
        sConfirmRevert = sAppliedPC;
        sConfirmHaveTarget = sAppliedPC.bValid;
        if (!sConfirmHaveTarget)
        {
            LOG(LogRender, Warning, "OPENSWBF2 DISPLAY confirm has no previous mode, kept");
            kStep.ePhase = DisplayLogic::kConfirmNone;
            kStep.eAction = DisplayLogic::kConfirmActClear;
        }
    }
    sConfirmPhase = kStep.ePhase;
    RunConfirmAction(kStep.eAction, "timeout");
}

void PlayerProfile::PollApplyNotice()
{
    unsigned int uiKind = 0;
    DisplayRequestTag kTag;
    while (RedRenderer::pcTakeApplyNotice(uiKind, kTag))
    {
        if (uiKind == 1)
        {
            ConfirmOnApplied(kTag);
            if (kTag.bValid)
            {
                sAppliedPC = kTag;
            }
            continue;
        }

        if (uiKind != 2)
        {
            continue;
        }

        const bool bArmedFail =
            sConfirmPhase == DisplayLogic::kConfirmAwaitPresent && !sConfirmHaveTarget;
        tProfile* pProfile = GetCurProfile();
        if (pProfile &&
            DisplayLogic::ShouldRestoreProfile(sAppliedPC.bValid, sAppliedPC.uiOwner,
                                               GetCurProfileIndex(),
                                               RedRenderer::pcApplyPending()))
        {
            tPCOpt& rPC = pProfile->mPCOptions;
            rPC.mResolutionX = sAppliedPC.resX;
            rPC.mResolutionY = sAppliedPC.resY;
            rPC.mWindowed = sAppliedPC.windowed;
            rPC.mVSync = sAppliedPC.vsync;
            rPC.mMultiSample = sAppliedPC.ms;
            rPC.mMultiSampleQuality = sAppliedPC.msq;
            WriteDisplayFields(sAppliedPC.bBorderless, sAppliedPC.uiRefresh, sAppliedPC.uiVsync,
                               sAppliedPC.bVsyncSet,
                               bArmedFail ? sConfirmPrevFps : DisplayCfg::gDisplay.uiFps,
                               bArmedFail ? sConfirmPrevNoWait : DisplayCfg::gDisplay.bNoWaitQuery);
            pProfile->UpdateProfCRC();
            LOG(LogRender, Warning,
                "OPENSWBF2 DISPLAY profile restored %ux%u windowed=%d vsync=%d fps=%u waitquery=%d",
                (unsigned int)sAppliedPC.resX, (unsigned int)sAppliedPC.resY,
                (int)sAppliedPC.windowed, (int)sAppliedPC.vsync, DisplayCfg::gDisplay.uiFps,
                DisplayCfg::gDisplay.bNoWaitQuery ? 0 : 1);
        }

        StepConfirm(DisplayLogic::kConfirmEvApplyFailed, "timeout");
    }

    if (sConfirmPhase == DisplayLogic::kConfirmCounting)
    {
        StepConfirm(DisplayLogic::kConfirmEvTick, "timeout");
    }
}

void PlayerProfile::PollDisplayConfirm(unsigned int& uiState, unsigned int& uiSecondsLeft,
                                       unsigned int& uiResult, unsigned int& uiKeepSave)
{
    uiState = (unsigned int)sConfirmPhase;
    uiSecondsLeft = 0;
    if (sConfirmPhase == DisplayLogic::kConfirmCounting)
    {
        const unsigned int uiElapsed = ConfirmElapsed();
        if (uiElapsed < DisplayLogic::kConfirmMs)
        {
            uiSecondsLeft = (DisplayLogic::kConfirmMs - uiElapsed + 999) / 1000;
        }
    }
    uiResult = sConfirmResult;
    uiKeepSave = sKeepSaveResult;
}

void PlayerProfile::EndDisplayConfirm(bool bKeep)
{
    StepConfirm(bKeep ? DisplayLogic::kConfirmEvKeep : DisplayLogic::kConfirmEvRevert,
                "reverted by player");
}

void PlayerProfile::SkewDisplayConfirmClock(unsigned int uiMs)
{
    sConfirmSkewMs += uiMs;
}

bool PlayerProfile::ApplyMenuDisplay(const DisplayCfg::MenuFields& kFields)
{
    if (!DisplayCfg::ValidateMenu(kFields))
    {
        LOG(LogRender, Warning,
            "OPENSWBF2 DISPLAY menu options rejected mode=%u refresh=%u vsync=%u fps=%u",
            kFields.uiMode, kFields.uiRefresh, kFields.uiVsync, kFields.uiFps);
        return false;
    }

    tProfile* pProfile = GetCurProfile();
    if (!pProfile)
    {
        return false;
    }

    tPCOpt& rPC = pProfile->mPCOptions;
    DisplayCfg::DisplaySettings& rDisplay = DisplayCfg::gDisplay;
    const DisplayLogic::LegacyFlags kFlags =
        DisplayLogic::LegacyFlagsFor((DisplayLogic::DisplayMode)kFields.uiMode, kFields.uiVsync);

    if (sConfirmPhase == DisplayLogic::kConfirmNone)
    {
        sConfirmPrevFps = rDisplay.uiFps;
        sConfirmPrevNoWait = rDisplay.bNoWaitQuery;
    }

    rDisplay.bBorderless = kFields.uiMode == DisplayLogic::kModeBorderless;
    rDisplay.uiRefresh = kFields.uiRefresh;
    rDisplay.uiVsync = kFields.uiVsync;
    rDisplay.uiSet |= DisplayCfg::kVsync;
    if (rPC.mWindowed != kFlags.bWindowed || rPC.mVSync != kFlags.bVsync)
    {
        rPC.mWindowed = kFlags.bWindowed;
        rPC.mVSync = kFlags.bVsync;
        pProfile->UpdateProfCRC();
    }

    if (kFields.uiMode != DisplayLogic::kModeWindowed && pcWindowed)
    {
        sConfirmCliPrev = true;
        sConfirmCliCleared = true;
        pcWindowed = false;
        LOG(LogRender, Inform, "OPENSWBF2 DISPLAY -win switch overridden for this session");
    }

    if (kFields.uiFps != rDisplay.uiFps)
    {
        rDisplay.uiFps = kFields.uiFps;
        if (kFields.uiFps == 0)
        {
            LOG(LogCore, Warning, "pacing: fps cap is uncapped, frame-count effects are not fixed yet");
        }
        else if (kFields.uiFps > 80)
        {
            LOG(LogCore, Warning, "pacing: fps cap %u is above 80, frame-count effects are not fixed yet",
                kFields.uiFps);
        }
    }

    RedRenderer::pcSetWaitForFrameQuery(kFields.bWaitQuery);

    sArmMenuConfirm = true;
    ApplyLiveDisplay();
    sArmMenuConfirm = false;
    return true;
}

void PlayerProfile::MarkDisplay()
{
    sMarkDisplay = DisplayCfg::gDisplay;
    sMarkCliWindowed = pcWindowed;
    sMarked = true;
}

void PlayerProfile::RestoreDisplay()
{
    sSaveDeferred = false;
    DisplayCfg::ClearPendingAdapter();
    if (!sMarked)
    {
        return;
    }

    WriteDisplayFields(sMarkDisplay.bBorderless, sMarkDisplay.uiRefresh, sMarkDisplay.uiVsync,
                       (sMarkDisplay.uiSet & DisplayCfg::kVsync) != 0, sMarkDisplay.uiFps,
                       sMarkDisplay.bNoWaitQuery);
    pcWindowed = sMarkCliWindowed;

    sConfirmPhase = DisplayLogic::kConfirmNone;
    sConfirmHaveTarget = false;
    sConfirmCliCleared = false;

    RedRenderer::pcClearStageArm();
    ApplyLiveDisplay();
    LOG(LogRender, Inform, "OPENSWBF2 DISPLAY menu cancel restored");
}

unsigned int PlayerProfile::SaveDisplay()
{
    if (!sMarked)
    {
        MarkDisplay();
    }
    const unsigned int uiAdapterResult = SavePendingAdapter();
    const unsigned int uiMenuResult = WriteMenuIni();
    if (uiAdapterResult == DisplayCfg::kSaveFailed || uiMenuResult == DisplayCfg::kSaveFailed)
    {
        return DisplayCfg::kSaveFailed;
    }
    return uiMenuResult;
}

// FUNCTION: SWBF2 0x0061cdc0
void PlayerProfile::tPCOpt::ApplyPostLoadSettings()
{
    RedRenderer::pcSetRenderOptions(renderOptions);

    RedParticleRenderer::s_blurEnable = (RedRenderer::pcGetDeviceCaps().DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES) &&
                  RedRenderer::pcGetRenderOptions().distortion;

    SetPropDistanceScale(RedRenderer::pcGetRenderOptions().mNearSceneFadeFactor * 1.7f);

    float fParticleBias;
    switch (RedRenderer::pcGetRenderOptions().particleQuality)
    {
    case QUALITY_MEDIUM:
        fParticleBias = 40.0f;
        break;
    case QUALITY_HIGH:
        fParticleBias = 80.0f;
        break;
    default:
        fParticleBias = 0.0f;
        break;
    }
    ParticleSystem::SetFadeBias(fParticleBias);

    if (ColorControl::s_pInstance)
    {
        ColorControl::s_pInstance->SetLevel(COLORCONTROL_GAMMA_CONTRAST,
                                            RedRenderer::pcGetRenderOptions().mGammaContrast);
        ColorControl::s_pInstance->SetLevel(COLORCONTROL_GAMMA_BRIGHTNESS,
                                            RedRenderer::pcGetRenderOptions().mGammaBrightness);
        ColorControl::s_pInstance->Update(0.0f);
    }

    if (HDREffect::s_pInstance)
    {
        HDREffect::s_pInstance->UpdateHDREnabled();
    }
}

// FUNCTION: SWBF2 0x0061cea0
void PlayerProfile::tOnlineOpt::ResetToDefault()
{
    memset(mPadding, 0, sizeof(mPadding));

    mu8AppearOffline = 0;
    mu8VoiceMask = 0;
    mu8VoiceToTVVol = 0;
    mu8DisplayGamertags = 0xff;

    memset(mGamespyNickname, 0, sizeof(mGamespyNickname));
    mMustBeZero1 = 0;
    memset(mGamespyEmail, 0, sizeof(mGamespyEmail));
    mMustBeZero2 = 0;
    memset(mGamespyPassword, 0, sizeof(mGamespyPassword));
    mMustBeZero3 = 0;

    mu8_SaveInfo = 0x11;
    mHasSeenEULA = false;
    mu8VoiceRecordVol = 10;
    mu8VoicePlayVol = 10;
    mu8PlayersSupported = NetWire::kHostBandwidthDefault; // retail 0 (128K)
    mu8TurnsPerSecond = NetWire::kDefaultTurnRate; // retail 20
    mOnlineIcon = 1;
    mVoiceEnable = 0;
    mShowAllRegions = 0;
    mu8GSID_Autologin = 0;
    mMinPlayers = 0;
    mMaxPing = 0;
    mMapIdx = 0;
    mEraIdx = 0;
    mServerIdx = 0;

    memset(mGameNameFilter, 0, sizeof(mGameNameFilter));
    mMustBeZero4 = 0;

    mVoiceRecordDevice = 0xff;
    mVoiceRecordInput = 0xff;
    mVoicePlaybackDevice = 0xff;
}

// FUNCTION: SWBF2 0x0061cf70
void PlayerProfile::tOnlineOpt::SanityCheck()
{
    if (mMustBeZero1 || mMustBeZero2 || mMustBeZero3 || mMustBeZero4)
    {
        ResetToDefault();
    }
    if (mu8VoiceToTVVol > 10)
    {
        mu8VoiceToTVVol = 0;
    }
    if (mu8VoiceRecordVol > 10)
    {
        mu8VoiceRecordVol = 0;
    }
    if (mu8VoicePlayVol > 10)
    {
        mu8VoicePlayVol = 0;
    }
    for (unsigned int i = 0; i < sizeof(mPadding); ++i)
    {
        if (mPadding[i])
        {
            ResetToDefault();
        }
    }
}

// FUNCTION: SWBF2 0x0061cff0
void PlayerProfile::tOnlineOpt::ApplySettings()
{
    int iIndex = 0;
    for (int i = 0; i < (int)(sizeof(mProfiles) / sizeof(mProfiles[0])); ++i)
    {
        if (this == &mProfiles[i].mOnlineOptions)
        {
            iIndex = i;
            break;
        }
    }

    unsigned int uiState = GameState::GetState();
    bool bShellOrNet = uiState == 0x11E1FC01 || NetGame::IsEnabled();

    VoiceChat::Node* pNode = (VoiceChat::Node*)GameVoiceChat::FindNodeByDeviceID();
    int iPlayer = FLInputManager::s_instance.GetPlayerFromHWPort(iIndex);
    if (uiState != 0x11E1FC01)
    {
        iIndex = iPlayer < 0 ? -1 : NetGame::GetLocalIndex(iPlayer);
    }

    if (pNode && iIndex >= 0)
    {
        void* pDecoder = &pNode->GetDecoder();
        pNode->GetEncoder().SetGain(mu8VoiceRecordVol / 10.0f);
        ((VoiceChat::Decoder*)pDecoder)->SetGain(mu8VoicePlayVol / 10.0f, 0);

        if (((VoiceChat::Node*)pNode)->IsOpen())
        {
            ((GameVoiceChat::Node*)pNode)->SetID((bShellOrNet && mVoiceEnable) ? (unsigned char)iIndex : 0xFF);
        }

        if (mVoiceEnable && !((VoiceChat::Encoder*)(&pNode->GetEncoder()))->IsEnabled())
        {
            mu8VoiceToTVVol = 10;
        }

        if (((VoiceChat::Decoder*)pDecoder)->GetNumOutputs() > 1)
        {
            ((VoiceChat::Decoder*)pDecoder)->SetGain(mu8VoiceToTVVol / 10.0f, 1);
        }

        if (!mu8VoiceToTVVol && !((VoiceChat::Encoder*)(&pNode->GetEncoder()))->IsEnabled())
        {
            ((GameVoiceChat::Node*)pNode)->SetID(0xFF);
        }

        if (pNode->GetID() != 0xFF)
        {
            return;
        }
    }

    if (bShellOrNet)
    {
        mu8VoiceToTVVol = 0;
        mVoiceEnable = 0;
    }
}

// FUNCTION: SWBF2 0x0061d190
void PlayerProfile::tToolTipOpt::ResetToDefault()
{
    memset(mToolTipCount, 0, sizeof(mToolTipCount));
    mOn = false;
    mOff = false;
    memset(mPadding, 0, sizeof(mPadding));
}

// FUNCTION: SWBF2 0x0061d1b0
void PlayerProfile::tToolTipOpt::SanityCheck()
{
    for (unsigned int i = 0; i < sizeof(mPadding); ++i)
    {
        if (mPadding[i])
        {
            ResetToDefault();
        }
    }
}

// FUNCTION: SWBF2 0x0061d1d0
void PlayerProfile::tAccumPoints::ResetToDefault()
{
    mTotalPoints = 0;
    mTotalKills = 0;
    mTotalDeaths = 0;
}

// FUNCTION: SWBF2 0x0061d1f0
PlayerProfile::tProfile::tProfile()
{
    mInitialized = false;
    mVersion = 1;
    mbFakeProfile = true;
    memset(mPadding, 0, sizeof(mPadding));
}

// FUNCTION: SWBF2 0x0061d2f0
void PlayerProfile::tProfile::ResetProfile()
{
    memset(this, 0, sizeof(*this));

    mInitialized = false;

    mGameOptions.ResetToDefault();
    memset(mGameOptions.mMedalsGathered, 0, sizeof(mGameOptions.mMedalsGathered));
    memset(mGameOptions.mCampaignProgress, 0, sizeof(mGameOptions.mCampaignProgress));

    ResetBindings();

    mSoundOptions.ResetToDefault();
    mOnlineOptions.ResetToDefault();
    mToolTipOptions.ResetToDefault();
    mAccumPoints.ResetToDefault();
    mPCOptions.ResetToDefault();

    memset(mName, 0, sizeof(mName));
}

// FUNCTION: SWBF2 0x0061d380
void PlayerProfile::tProfile::ResetToDefault()
{
    ResetProfile();
    PblUnicode::ustrlcpy(mName, RedLocalizeDB::Find(0x62a0afc0, false), 16);
    mName[15] = 0;
}

// FUNCTION: SWBF2 0x0061d480
void PlayerProfile::tProfile::NoteBindingChange()
{
    mIsDirty |= mGameOptions.NoteBindingChange();
}

// FUNCTION: SWBF2 0x0061d4a0
void PlayerProfile::tProfile::ResetBindings()
{
    for (int i = 0; i < 5; ++i)
    {
        mIsDirty |= mGameOptions.ResetBindings(i);
    }
}

// FUNCTION: SWBF2 0x0061d4c0
void PlayerProfile::tProfile::SanityCheck()
{
    if (mMustBeZero1)
    {
        ResetToDefault();
    }

    if (!PblUnicode::ustrlen(mName))
    {
        PblUnicode::ustrlcpy(mName, RedLocalizeDB::Find(0x62a0afc0, false), 16);
        mName[15] = 0;
    }
    if (!PblUnicode::ustrlen(mName))
    {
        mName[0] = 'A';
        mName[1] = 0;
    }
    mName[15] = 0;

    mSoundOptions.SanityCheck();
    mOnlineOptions.SanityCheck();
    mToolTipOptions.SanityCheck();
    mPCOptions.SanityCheck();
    // OpenSWBF2: the AI tables are indexed by this, and retail never range-checked it.
    mGameOptions.mCurDiffLevel =
        (eGAME_DIFFICULTY)AITuning::ValidLevelOr(mGameOptions.mCurDiffLevel, eDIFFICULTY_MEDIUM);
    if (mSafeMode)
    {
        mSoundOptions.EnsureIsSafe();
        mPCOptions.EnsureIsSafe();
    }

    for (unsigned int i = 0; i < sizeof(mPadding); ++i)
    {
        if (mPadding[i])
        {
            ResetToDefault();
        }
    }

    UpdateProfCRC();
}

// FUNCTION: SWBF2 0x0061d230
unsigned int PlayerProfile::tProfile::CalcProfCRC()
{
    return CalcCRC(this, offsetof(tProfile, mCRC), 0);
}

// FUNCTION: SWBF2 0x0061d250
void PlayerProfile::tProfile::UpdateProfCRC()
{
    bool bDirty = mIsDirty;
    unsigned int uiOldCRC = mCRC;
    mIsDirty = false;
    mCRC = CalcProfCRC();
    if (mCRC != uiOldCRC)
    {
        bDirty = true;
    }
    mIsDirty = bDirty;
    if (mbFakeProfile)
    {
        mIsDirty = false;
    }

    if (IsDefaultProfile() && !mbFakeProfile)
    {
        memcpy(&mDefaultProfile, this, sizeof(tProfile));
    }
}

// FUNCTION: SWBF2 0x0061d2c0
bool PlayerProfile::tProfile::IsDefaultProfile()
{
    return mName[0] && !PblUnicode::ustrcmp(mName, mDefaultProfile.mName);
}

// GLOBAL: SWBF2 0x01fac0e0

unsigned short sProfileViewportName[40];

// FUNCTION: SWBF2 0x0061d730
const unsigned short* PlayerProfile::tProfile::GetName()
{
    if (!NetComm::IsLAN())
    {
        int iViewport;
        for (iViewport = 0; iViewport < 1; ++iViewport)
        {
            if (this == &mProfiles[iViewport])
            {
                break;
            }
        }

        const char* pcName = NetMatch::GetLoginOverrideName(iViewport);
        if (pcName)
        {
            memset(sProfileViewportName, 0, sizeof(sProfileViewportName));
            PblUnicode::ustrncnv(sProfileViewportName, pcName, 32);
            return sProfileViewportName;
        }
    }
    return mName;
}

// FUNCTION: SWBF2 0x0061d590
void PlayerProfile::tProfile::ApplySettings()
{
    if (this == &mProfiles[0])
    {
        int iPort = -1;
        const char* pRecord = (const char*)&PlayerPrefs::mAllUserPrefs[0].mJoystick;
        for (int i = 0; i < 1; ++i, pRecord += 0x2c)
        {
            if (*(const int*)pRecord == 0)
            {
                iPort = i;
            }
        }
        mGameOptions.ApplySettings(iPort, 0);
    }

    mSoundOptions.ApplySettings();
    mOnlineOptions.ApplySettings();
    mPCOptions.ApplySettings();
}

// FUNCTION: SWBF2 0x0061d670
PlayerProfile::tProfile* PlayerProfile::GetProfile(unsigned int uiIndex)
{
    if (uiIndex >= sizeof(mProfiles) / sizeof(mProfiles[0]))
    {
        return 0;
    }

    return &mProfiles[uiIndex];
}

// FUNCTION: SWBF2 0x0061d5f0
PlayerProfile::tProfile* PlayerProfile::GetCurProfile()
{
    if ((unsigned int)PlayerProfile::mCurProfile >= sizeof(mProfiles) / sizeof(mProfiles[0]))
    {
        return 0;
    }

    return &mProfiles[PlayerProfile::mCurProfile];
}

// FUNCTION: SWBF2 0x0061d610
unsigned int PlayerProfile::GetCurProfileIndex()
{
    return PlayerProfile::mCurProfile;
}

// FUNCTION: SWBF2 0x0061d620
bool PlayerProfile::SetCurProfile(unsigned int uiIndex)
{
    PlayerProfile::mCurProfile = uiIndex;

    tProfile* pProfile = GetCurProfile();
    if (!pProfile)
    {
        return true;
    }

    pProfile->ApplySettings();

    bool bDirty = pProfile->mIsDirty;
    pProfile->mIsDirty = false;
    unsigned int uiCRC = pProfile->CalcProfCRC();
    pProfile->mIsDirty = bDirty;

    return uiCRC == pProfile->mCRC;
}

// FUNCTION: SWBF2 0x0061d6c0
int PlayerProfile::GetNumMemoryProfiles()
{
    return mNumMemoryProfiles;
}

// FUNCTION: SWBF2 0x0061d6d0
void PlayerProfile::SetNumMemoryProfiles(int iNum)
{
    mNumMemoryProfiles = iNum;
}

// FUNCTION: SWBF2 0x0061d690
void PlayerProfile::ResetAllProfiles()
{
    PlayerProfile::tProfile* pProfile = PlayerProfile::GetProfile(0);

    pProfile->ResetProfile();
    pProfile->mIsDirty = false;

    PlayerProfile::mCurProfile = 0;
    mNumMemoryProfiles = 0;
}

// FUNCTION: SWBF2 0x0061d3b0
void PlayerProfile::tProfile::InitProfile(unsigned short const* pusName)
{
    mInitialized = true;
    if (pusName)
    {
        PblUnicode::ustrlcpy(mName, pusName, 16);
        mName[15] = 0;
        mbFakeProfile = false;
    }
    else
    {
        memset(mName, 0, 16);
        mbFakeProfile = true;
    }

    memset(mGameOptions.mMedalsGathered, 0, sizeof(mGameOptions.mMedalsGathered));
    memset(mGameOptions.mCampaignProgress, 0, sizeof(mGameOptions.mCampaignProgress));
    mGameOptions.ResetToDefault();
    ResetBindings();
    mSoundOptions.ResetToDefault();
    mOnlineOptions.ResetToDefault();
    mToolTipOptions.ResetToDefault();
    mAccumPoints.ResetToDefault();
    mPCOptions.ResetToDefault();

    mMustBeZero1 = 0;
    mIsDirty = false;
    mCRC = CalcProfCRC();
    mIsDirty = mName[0] != 0;
}

namespace PlayerProfile {

// FUNCTION: SWBF2 0x0061d850
int GetDefaultProfileIdx()
{
    if (!mDefaultProfile.mName[0])
    {
        return -1;
    }
    for (int i = 0; i < 1; ++i)
    {
        if (mProfiles[i].IsDefaultProfile())
        {
            return i;
        }
    }
    return -1;
}
}

namespace PlayerProfile {

// FUNCTION: SWBF2 0x0061d8a0
bool tProfile::IsDirty(bool bArg)
{
    for (int i = 0; i < 1; ++i)
    {
        if (this != &mProfiles[i])
        {
            continue;
        }
        if (PblBuildType::gCreateMemoryProfile)
        {
            return false;
        }
        if (bArg)
        {
            return mIsDirty;
        }
        if (!mIsDirty || mLastDirtyCRC[i] == mCRC)
        {
            return false;
        }
        mLastDirtyCRC[i] = mCRC;
        return true;
    }
    return false;
}
}

// FUNCTION: SWBF2 0x006c5b00
void RedJoystick::SetDeadZoneScale(float f)
{
    mDeadZoneScale = f;
    // XInput ports have no DirectInput device; they read mDeadZoneScale directly.
    if (pc_device)
    {
        pc_device->EnumObjects(EnumObjectsCallback, this, 0);
    }
}

// GLOBAL: SWBF2 0x00788140

static const float sDeadZoneScaleByLevel[11] =
{
    0.0f, 0.002f, 0.004f, 0.006f, 0.008f, 0.01f, 0.05f, 0.1f, 0.15f, 0.2f, 0.3f
};

// FUNCTION: SWBF2 0x00415c90
void RawControllerInputs::ApplyDeadZone(int iLevel)
{
    if (iLevel < 0)
    {
        iLevel = 0;
    }
    else if (iLevel > 10)
    {
        iLevel = 10;
    }

    float fValue = sDeadZoneScaleByLevel[iLevel];
    RedJoystick** ppObjects = _redJoystick;
    for (int i = 0; i < 10; ++i)
    {
        if (ppObjects[i])
        {
            ppObjects[i]->SetDeadZoneScale(fValue);
        }
    }
}
