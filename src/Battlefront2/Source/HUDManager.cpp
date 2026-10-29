#include <new>
#include <string.h>

#include "HUDManager.h"

#include "RedEngineFL/Application/pc/pcFLRender.h"
#include "RedEngineFL/Graphics/PC/pcDisplayLogic.h"
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"
#include "RedEngineFL/Interface/RedInterfaceScreen.h"
#include "RedEngineFL/Memory/RedMemory.h"
#include "CameraManager.h"
#include "GameMemory.h"
#include "HUDBorderedBox.h"
#include "HUDEditor.h"
#include "HUDElementBarBitmap.h"
#include "HUDElementBarSegmented.h"
#include "HUDElementBitmap.h"
#include "HUDElementGroup.h"
#include "HUDElementMap.h"
#include "HUDElementModel3D.h"
#include "HUDElementMultilineText.h"
#include "HUDElementTarget.h"
#include "HUDElementText.h"
#include "HUDElementVehicleSeating.h"
#include "HUDGameEvents.h"
#include "HUDObjectiveList.h"
#include "HUDProceduralBarBitmap.h"
#include "HUDSound.h"
#include "HUDTransformNameMesh.h"
#include "HUDTransformNumberColor.h"
#include "HUDTransformNumberColorBlend.h"
#include "HUDTransformNumberVector3.h"
#include "HUDViewPort.h"

void AdjustScreenGroupAspect(RedScreenGroupElement* pGroup);

#pragma warning(disable : 4316)

HUD::Manager::ConfigFile::Factory::Factory()
    : HUD::Item::Factory(0xD4E0C797u)
{
    mName = "FileInfo";
}

// FUNCTION: SWBF2 0x00564990
HUD::Item* HUD::TransformNumberColor::Factory::CreateItem(const char* const pName, ItemCallback pCallback,
                                                          void* const pUserData)
{
    HUD::Item* pItem = new TransformNumberColor(pName, pCallback, pUserData);
    if (pItem)
    {
        return pItem->SetFactory(this);
    }
    return 0;
}

// FUNCTION: SWBF2 0x00564a40
HUD::Item* HUD::TransformNumberColorBlend::Factory::CreateItem(const char* const pName, ItemCallback pCallback,
                                                               void* const pUserData)
{
    HUD::Item* pItem = new TransformNumberColorBlend(pName, pCallback, pUserData);
    if (pItem)
    {
        return pItem->SetFactory(this);
    }
    return 0;
}

// FUNCTION: SWBF2 0x00564af0
HUD::Item* HUD::TransformNameMesh::Factory::CreateItem(const char* const pName, ItemCallback pCallback,
                                                       void* const pUserData)
{
    HUD::Item* pItem = new TransformNameMesh(pName, pCallback, pUserData);
    if (pItem)
    {
        return pItem->SetFactory(this);
    }
    return 0;
}

// FUNCTION: SWBF2 0x00564ba0
HUD::Item* HUD::TransformNumberVector3::Factory::CreateItem(const char* const pName, ItemCallback pCallback,
                                                            void* const pUserData)
{
    HUD::Item* pItem = new TransformNumberVector3(pName, pCallback, pUserData);
    if (pItem)
    {
        return pItem->SetFactory(this);
    }
    return 0;
}

// FUNCTION: SWBF2 0x00564c50
HUD::Item* HUD::Sound::Factory::CreateItem(const char* const pName, ItemCallback pCallback,
                                           void* const pUserData)
{
    HUD::Item* pItem = new Sound(pName, pCallback, pUserData);
    if (pItem)
    {
        return pItem->SetFactory(this);
    }
    return 0;
}

// FUNCTION: SWBF2 0x00564cf0
HUD::Item* HUD::Manager::ConfigFile::Factory::CreateItem(const char* const pName, ItemCallback pCallback,
                                                         void* const pUserData)
{
    HUD::Item* pItem = new ConfigFile(pName, pCallback, pUserData);
    if (pItem)
    {
        return pItem->SetFactory(this);
    }
    return 0;
}

// FUNCTION: SWBF2 0x00564d80
bool HUD::Manager::ConfigFile::SetProperty(const unsigned int uHashID, const void* pValue)
{
    return false;
}

// FUNCTION: SWBF2 0x00564d90
bool HUD::Manager::ConfigFile::GetProperty(const unsigned int uHashID, void* pValue)
{
    return false;
}

// FUNCTION: SWBF2 0x00564da0
const char* HUD::Manager::ConfigFile::PropertyTranslateEnum(const unsigned int uHashID, const unsigned int uValue)
{
    return 0;
}

// GLOBAL: SWBF2 0x007e6988
char* gSplitModes[2] = { "Horizontal", "Vertical" };

// FUNCTION: SWBF2 0x00564db0
bool HUD::Manager::ConfigFile::ReadData(PblConfig& config, PblConfig::Data& data)
{
    switch (data.GetId())
    {
    case 0x70e37eec:
    {
        int iNumArgs = data.GetNumArgs();
        if (iNumArgs < 0)
        {
            iNumArgs = 0;
        }
        else if (iNumArgs > 2)
        {
            iNumArgs = 2;
        }
        mNumSupportedSplitModes = 0;
        for (int i = 0; i < iNumArgs; ++i)
        {

            const char* pName = (const char*)data.m_auiArgs + data.m_auiArgs[i];
            unsigned int uMode = TranslateString(gSplitModes, 2, pName);
            if (uMode < 2)
            {
                mSupportedSplitModes[mNumSupportedSplitModes++] = uMode;
            }
        }
        return true;
    }
    case 0xaa56e310:
    {
        int iNumArgs = data.GetNumArgs();
        if (iNumArgs < 0)
        {
            iNumArgs = 0;
        }
        else if (iNumArgs > 4)
        {
            iNumArgs = 4;
        }
        mNumSupportedViewPorts = 0;
        for (int i = 0; i < iNumArgs; ++i)
        {
            mSupportedViewPorts[mNumSupportedViewPorts++] = (unsigned int)data.GetFloatArg(i);
        }
        return true;
    }
    case 0xf5dc47f6:
        mWidescreen = data.GetFloatArg(0) != 0.0f;
        mWidescreenSet = true;
        return true;
    }
    return false;
}

// FUNCTION: SWBF2 0x00564ed0
void HUD::Manager::ConfigFile::PostReadSetup()
{
}

// FUNCTION: SWBF2 0x00564ee0
void HUD::Manager::ConfigFile::WriteData(HUD::File& file, unsigned int uTabs)
{
    if (mNumSupportedSplitModes)
    {
        file.WriteTabs(uTabs);
        file.WriteString("SplitMode(");
        for (unsigned int i = 0; i < mNumSupportedSplitModes; ++i)
        {
            file.WriteStringf("\"%s\"", gSplitModes[mSupportedSplitModes[i]]);
            if (i < mNumSupportedViewPorts - 1)
            {
                file.WriteString(", ");
            }
        }
        file.WriteString(")\n\n");
    }
    if (mNumSupportedViewPorts)
    {
        file.WriteTabs(uTabs);
        file.WriteString("Viewports(");
        for (unsigned int i = 0; i < mNumSupportedViewPorts; ++i)
        {
            file.WriteStringf("%d", mSupportedViewPorts[i]);
            if (i < mNumSupportedViewPorts - 1)
            {
                file.WriteString(", ");
            }
        }
        file.WriteString(")\n\n");
    }
    if (mWidescreenSet)
    {
        file.WriteTabs(uTabs);
        file.WriteStringf("Widescreen(%d)\n\n", mWidescreen);
    }
}

namespace
{

// OpenSWBF2: the HUD's root screen groups, drawn at the HUD scale option
// (Options -> Video, display.ini HudScale). Each top-level HUD element is
// scaled about the screen edge, corner or centre line it sits nearest
// (DisplayLogic::HudAnchor), so the HUD keeps to its corners; elements in the
// middle of the screen (the reticle) and world markers are left alone. Only
// the drawing changes: the matrices are restored afterwards, so HUD logic and
// events never see it. No data members, as gScreenGroup is indexed as
// RedScreenGroupElement.
class HudScreenGroup : public RedScreenGroupElement
{
protected:
    virtual void RenderUsingContext(const PblMatrix& mContext, const RedColor& color);
};

struct HudScaled
{
    RedInterfaceElement* pElement;
    PblMatrix mSaved;
};

const int kMaxHudScaled = 256;
HudScaled sHudScaled[kMaxHudScaled];

void HudScreenGroup::RenderUsingContext(const PblMatrix& mContext, const RedColor& color)
{
    const unsigned int uiPercent = DisplayCfg::gDisplay.uiHudScale;
    const PblMatrix& mRoot = GetMatrix();
    if (uiPercent == DisplayLogic::kHudScaleDefault || uiPercent == 0 || mRoot.m[0][0] == 0.0f ||
        mRoot.m[1][1] == 0.0f)
    {
        RedScreenGroupElement::RenderUsingContext(mContext, color);
        return;
    }

    const float fScale = (float)uiPercent / 100.0f;
    const float fW = HUD::Element::GetViewportWidth();
    const float fH = HUD::Element::GetViewportHeight();
    int iCount = 0;
    for (HUD::Element* pElement = HUD::Element::GetFirst(); pElement && iCount < kMaxHudScaled;
         pElement = HUD::Element::GetNext(pElement))
    {
        RedInterfaceElement* pRed = &pElement->GetRedInterfaceElement();
        if (pRed->GetGroup() != this || dynamic_cast<HUD::ElementTarget*>(pElement))
        {
            continue;
        }
        // where it sits on screen (the root group only offsets/scales its axes)
        const PblMatrix& mLocal = pRed->GetMatrix();
        const float fScreenX = mLocal.m[3][0] * mRoot.m[0][0] + mRoot.m[3][0];
        const float fScreenY = mLocal.m[3][1] * mRoot.m[1][1] + mRoot.m[3][1];
        if (DisplayLogic::HudIsCentral(fScreenX, fScreenY, fW, fH))
        {
            continue;
        }
        const float fAnchorX = (DisplayLogic::HudAnchor(fScreenX, fW) - mRoot.m[3][0]) / mRoot.m[0][0];
        const float fAnchorY = (DisplayLogic::HudAnchor(fScreenY, fH) - mRoot.m[3][1]) / mRoot.m[1][1];

        sHudScaled[iCount].pElement = pRed;
        sHudScaled[iCount].mSaved = mLocal;
        PblMatrix mScaled = mLocal;
        for (int i = 0; i < 3; ++i)
        {
            mScaled.m[i][0] *= fScale;
            mScaled.m[i][1] *= fScale;
            mScaled.m[i][2] *= fScale;
        }
        mScaled.m[3][0] = DisplayLogic::HudScalePos(mLocal.m[3][0], fAnchorX, fScale);
        mScaled.m[3][1] = DisplayLogic::HudScalePos(mLocal.m[3][1], fAnchorY, fScale);
        pRed->SetMatrix(mScaled);
        ++iCount;
    }

    RedScreenGroupElement::RenderUsingContext(mContext, color);

    while (iCount > 0)
    {
        --iCount;
        sHudScaled[iCount].pElement->SetMatrix(sHudScaled[iCount].mSaved);
    }
}

}

namespace HUD {
// GLOBAL: SWBF2 0x007e69c8
bool gScreenGroupEditEnabled = true;
int gEditHeap = -1;
void* gEditHeapMem;
RedScreenGroupElement* gScreenGroup;
RedScreenGroupElement* gScreenGroupEdit;
}

// GLOBAL: SWBF2 0x01e573c0
HUD::Editor* gEditor;
// GLOBAL: SWBF2 0x01e573c4
namespace HUD { int gNumConfigFiles; }

// FUNCTION: SWBF2 0x00565140
void HUD::Manager::Open()
{
    int iOldHeap = RedSetCurrentHeap(GameMemory::RunTimeHeap);

    if (__RedDebugHeap != -1)
    {
        HUD::gEditHeapMem = RedAllocFromHeap(__RedDebugHeap, 0x100000, 0);
        if (HUD::gEditHeapMem)
        {
            HUD::gEditHeap = RedCreateHeap(HUD::gEditHeapMem,
                                           (char*)HUD::gEditHeapMem + 0xFFFFF,
                                           0x10, "HUDEditHeap");
        }
    }

    new HUD::ElementGroup::Factory;
    new HUD::ElementText::Factory;
    new HUD::ElementBitmap::Factory;
    new HUD::ElementBitmapMasked::Factory;
    new HUD::ElementModel3D::Factory;
    new HUD::ElementMap::Factory;
    new HUD::ElementTarget::Factory;
    new HUD::ElementTarget::Target::Factory;
    new HUD::TransformNumberColor::Factory;
    new HUD::TransformNumberColorBlend::Factory;
    new HUD::TransformNameMesh::Factory;
    new HUD::TransformNumberVector3::Factory;
    new HUD::ObjectiveList::Factory;
    new HUD::BorderedBox::Factory;
    new HUD::ElementMultilineText::Factory;
    new HUD::ElementBarBitmap::Factory;
    new HUD::ProceduralBarBitmap::Factory;
    new HUD::ElementBarSegmented::Factory;
    new HUD::ElementVehicleSeating::Factory;
    new HUD::ViewPort::Factory;
    new HUD::Sound::Factory;
    new HUD::Manager::ConfigFile::Factory;

    gEditor = new HUD::Editor;

    HUD::gScreenGroup = new HudScreenGroup[5]; // OpenSWBF2: HUD scale
    HUD::gScreenGroupEdit = new RedScreenGroupElement[5];

    HUD::gScreenGroup->SetZOrder(50);
    HUD::gScreenGroup->m_fScreenRelativeX = 0.0f;
    HUD::gScreenGroup->m_fScreenRelativeY = 0.0f;
    HUD::gScreenGroup->Enable(true);
    HUD::gScreenGroup->m_bUseSafeZone = false;
    HUD::gScreenGroup->m_bUsePerspectiveHack = true;
    HUD::gScreenGroup->AddToRedScreen();

    HUD::gScreenGroupEdit->SetZOrder(50);
    HUD::gScreenGroupEdit->m_fScreenRelativeX = 0.0f;
    HUD::gScreenGroupEdit->m_fScreenRelativeY = 0.0f;
    HUD::gScreenGroupEdit->Enable(HUD::gScreenGroupEditEnabled);
    HUD::gScreenGroupEdit->m_bUseSafeZone = false;
    HUD::gScreenGroupEdit->m_bUsePerspectiveHack = true;
    HUD::gScreenGroupEdit->AddToRedScreen();

    HUD::gNumConfigFiles = 0;

    HUD::GameEvents::Open();

    RedSetCurrentHeap(iOldHeap);
}

// FUNCTION: SWBF2 0x00565740
void HUD::Manager::Close()
{
    int iOldHeap = RedSetCurrentHeap(GameMemory::RunTimeHeap);

    HUD::GameEvents::Close();
    HUD::Item::Factory::DestroyAll();
    HUD::Element::DestroyAll();
    HUD::Transform::DestroyAll();
    HUD::Sound::DestroyAll();
    HUD::EventClass::DestroyAll();
    HUD::Item::sList.Clear();

    HUD::gScreenGroup->Enable(false);
    HUD::gScreenGroup->RemoveFromRedScreen();
    HUD::gScreenGroupEdit->Enable(false);
    HUD::gScreenGroupEdit->RemoveFromRedScreen();

    unsigned int uNumViews = CameraManager::Instance()->m_iNumCam;
    for (unsigned int i = 0; i < uNumViews; ++i)
    {
        HUD::gScreenGroup[i + 1].Enable(false);
        FLRenderer::RemoveInterfaceElement(i, &HUD::gScreenGroup[i + 1]);
        HUD::gScreenGroupEdit[i + 1].Enable(false);
        FLRenderer::RemoveInterfaceElement(i, &HUD::gScreenGroupEdit[i + 1]);
    }

    delete[] static_cast<HudScreenGroup*>(HUD::gScreenGroup);
    delete[] HUD::gScreenGroupEdit;
    HUD::gScreenGroup = 0;
    HUD::gScreenGroupEdit = 0;

    delete gEditor;
    gEditor = 0;

    if (HUD::gEditHeapMem)
    {
        RedDestroyHeap(HUD::gEditHeap);
        RedFreeToHeap(__RedDebugHeap, HUD::gEditHeapMem, 0);
        HUD::gEditHeapMem = 0;
        HUD::gEditHeap = -1;
    }

    RedSetCurrentHeap(iOldHeap);
}

// FUNCTION: SWBF2 0x00565c10
bool HUD::Manager::Write()
{
    HUD::File file;
    Manager::ConfigFile** ppConfig = gConfigFiles;
    for (Item* pItem = Item::GetFirst(); pItem; pItem = Item::GetNext(pItem))
    {
        if ((Item*)*ppConfig == pItem)
        {
            char acName[0x400];
            _snprintf(acName, sizeof(acName) - 1, "data\%s.hud", pItem->GetName());
            acName[sizeof(acName) - 1] = 0;
            if (file.IsOpen())
            {
                file.Close();
            }
            if (!file.OpenCreate(acName, false, true, false))
            {
                return false;
            }
            ++ppConfig;
        }
        if (file.IsOpen() && pItem->WriteEnabled())
        {
            pItem->Write(file, 0, 0);
        }
    }
    if (file.IsOpen())
    {
        file.Close();
    }
    return true;
}

// FUNCTION: SWBF2 0x00564930
unsigned int HUD::TranslateString(const char* const* ppStrings, unsigned int uCount,
                             const char* const pString)
{
    for (unsigned int i = 0; i < uCount; ++i)
    {
        if (!_strnicmp(ppStrings[i], pString, strlen(ppStrings[i])))
        {
            return i;
        }
    }
    return (unsigned int)-1;
}

static unsigned int sLoadNumViews;

void HUD::Manager::RefreshViewport()
{
    HUD::Element::SetupViewportDimensions(sLoadNumViews, false);
}

// FUNCTION: SWBF2 0x00565950
void HUD::Manager::Load(PblFileChunk& chunk)
{
    PblConfig config(chunk);

    unsigned int uNumViews = CameraManager::Instance()->m_iNumCam;
    sLoadNumViews = uNumViews;
    HUD::Element::SetupViewportDimensions(uNumViews, false);
    for (unsigned int i = 0; i < uNumViews; ++i)
    {
        if (!HUD::gScreenGroup[i + 1].GetGroup())
        {
            HUD::gScreenGroup[i + 1].SetZOrder(50);
            HUD::gScreenGroup[i + 1].m_fScreenRelativeX = 0.0f;
            HUD::gScreenGroup[i + 1].m_fScreenRelativeY = 0.0f;
            HUD::gScreenGroup[i + 1].Enable(true);
            HUD::gScreenGroup[i + 1].m_bUseSafeZone = false;
            HUD::gScreenGroup[i + 1].m_bUsePerspectiveHack = true;
            AdjustScreenGroupAspect(&HUD::gScreenGroup[i + 1]);
            FLRenderer::AddInterfaceElement(i, &HUD::gScreenGroup[i + 1], RedInterfaceScreen::LEFT,
                                            RedInterfaceScreen::TOP, false);
        }
        if (!HUD::gScreenGroupEdit[i + 1].GetGroup())
        {
            HUD::gScreenGroupEdit[i + 1].SetZOrder(50);
            HUD::gScreenGroupEdit[i + 1].m_fScreenRelativeX = 0.0f;
            HUD::gScreenGroupEdit[i + 1].m_fScreenRelativeY = 0.0f;
            HUD::gScreenGroupEdit[i + 1].Enable(HUD::gScreenGroupEditEnabled);
            HUD::gScreenGroupEdit[i + 1].m_bUseSafeZone = false;
            HUD::gScreenGroupEdit[i + 1].m_bUsePerspectiveHack = true;
            FLRenderer::AddInterfaceElement(i, &HUD::gScreenGroupEdit[i + 1], RedInterfaceScreen::LEFT,
                                            RedInterfaceScreen::TOP, false);
        }
    }
    AdjustScreenGroupAspect(HUD::gScreenGroup);

    while (!config.IsDone())
    {
        PblConfig::Data data = config.ReadNextData();
        unsigned int uId = data.GetId();
        HUD::Item::Factory* pFactory = HUD::Item::Factory::FindByHashID(uId);
        if (!pFactory)
        {
            continue;
        }

        HUD::Item* pItem = pFactory->CreateItem(data.GetNumArgs() ? data.GetStringArg(0) : "Unnamed", 0, 0);
        if (pItem)
        {
            PblConfig scope(config.ReadNextScope(), true);
            pItem->Read(scope, 0, 0);
        }

        if (uId == 0xD4E0C797)
        {
            HUD::Manager::ConfigFile* pFile = (HUD::Manager::ConfigFile*)pItem;
            if (!pFile->SplitSupported() || !pFile->ViewportsSupported() || !pFile->Widescreen())
            {
                delete pFile;
                return;
            }
            HUD::gConfigFiles[HUD::gNumConfigFiles++] = pFile;
        }
    }
}

#include "PebbleFL/Common/PblFile.h"
#include "Cloth.h"
#include "CollisionMesh.h"
#include "GameModel.h"
#include "HUDElement.h"
#include "Music.h"
#include "PebbleFL/Common/PblChunk.h"
#include "PebbleFL/Common/PblConfig.h"
#include "PebbleFL/Common/PblHashTable.h"
#include "RedEngineFL/Graphics/RedAmbientLight.h"
#include "RedEngineFL/Graphics/RedColor.h"
#include "RedEngineFL/Graphics/RedDirectionalLight.h"
#include "RedEngineFL/Graphics/RedLight.h"
#include "RedEngineFL/Memory/RedMemoryPool.h"
#include "RedEngineFL/Util/ext/basearray.h"
#include "RedEngineFL/Util/RedAnimation.h"
#include "Zephyr/Source/ZephyrAnimBank.h"
#include "Zephyr/Source/ZephyrAnimBankReader.h"
#include "Zephyr/Source/ZephyrAnimJoint.h"
#include "Zephyr/Source/ZephyrCommon.h"
#include "Zephyr/Source/ZephyrFile.h"
#include "Zephyr/Source/ZephyrSkeletonShared.h"
#include "Zephyr/Source/ZephyrSkinReader.h"
#include "Zephyr/Source/ZephyrSkinShared.h"

// FUNCTION: SWBF2 0x00565d20
void __fastcall DestroyPblFileForHUD(PblFile* pFile)
{
    pFile->~PblFile();
}

// FUNCTION: SWBF2 0x00565d30
unsigned int HUD::Objective::GetTextHashID() const
{
    return _textHashID;
}

// GLOBAL: SWBF2 0x01e56fc0
HUD::Manager::ConfigFile* HUD::gConfigFiles[256];

bool HUD::Manager::ConfigFile::ViewportsSupported() const
{
    for (unsigned int i = 0; i < mNumSupportedViewPorts; ++i)
    {
        if (mSupportedViewPorts[i] == CameraManager::Instance()->GetNumCameras())
        {
            return true;
        }
    }
    return mNumSupportedViewPorts == 0;
}
