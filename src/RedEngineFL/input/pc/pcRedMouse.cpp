#include <string.h>

#include "pcRedInputManager.h"
#include "RedEngineFL/Application/pc/pcFLRender.h"
#include "RedEngineFL/Interface/RedInterfaceScreen.h"
#include "RedEngineFL/Interface/RedBitmapElement.h"
#include "RedEngineFL/Graphics/PC/pcDisplayLogic.h"
#include <d3dx9.h>
#include "RedEngineFL/Graphics/PC/pcRedRenderer.h"
#include "RedEngineFL/Graphics/PC/pcRedTexture.h"
#include "RedEngineFL/Graphics/PC/pcDisplaySettings.h"
#include "RedEngineFL/Util/RedLog.h"
#include "pcXInputPad.h"

extern HWND hGlobalWnd;
extern bool gAppActive;
extern bool g_bNoRender;

static DWORD sdwCoopFlags = DISCL_EXCLUSIVE | DISCL_FOREGROUND;
static DisplayLogic::CursorPlan sPlan = {false, true, false, false};
static bool sbAttached;
static const unsigned int kCursorSrcMax = 64;
static unsigned int sauiSrc[kCursorSrcMax * kCursorSrcMax];
static unsigned int suiSrcW;
static unsigned int suiSrcH;
static bool sbHaveSrc;
static HCURSOR shCursor;
static unsigned int suiBuiltW;
static unsigned int suiBuiltH;
static bool sbCreateLogged;
static RECT sClipRect;
static bool sbClipped;
static unsigned int suiRawPrevDown;
static unsigned int suiLatched;

static HCURSOR MakeCursorFromCache(unsigned int uiW, unsigned int uiH)
{
    BITMAPV5HEADER kV5;
    memset(&kV5, 0, sizeof(kV5));
    kV5.bV5Size = sizeof(BITMAPV5HEADER);
    kV5.bV5Width = (LONG)uiW;
    kV5.bV5Height = -(LONG)uiH;
    kV5.bV5Planes = 1;
    kV5.bV5BitCount = 32;
    kV5.bV5Compression = BI_BITFIELDS;
    kV5.bV5RedMask = 0x00FF0000;
    kV5.bV5GreenMask = 0x0000FF00;
    kV5.bV5BlueMask = 0x000000FF;
    kV5.bV5AlphaMask = 0xFF000000;

    void* pvColor = 0;
    HBITMAP hColor = CreateDIBSection(0, (BITMAPINFO*)&kV5, DIB_RGB_COLORS, &pvColor, 0, 0);

    unsigned char aucInfo[sizeof(BITMAPINFOHEADER) + 2 * sizeof(RGBQUAD)];
    memset(aucInfo, 0, sizeof(aucInfo));
    BITMAPINFO* pInfo = (BITMAPINFO*)aucInfo;
    pInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pInfo->bmiHeader.biWidth = (LONG)uiW;
    pInfo->bmiHeader.biHeight = -(LONG)uiH;
    pInfo->bmiHeader.biPlanes = 1;
    pInfo->bmiHeader.biBitCount = 1;
    pInfo->bmiHeader.biCompression = BI_RGB;
    pInfo->bmiColors[1].rgbRed = 0xff;
    pInfo->bmiColors[1].rgbGreen = 0xff;
    pInfo->bmiColors[1].rgbBlue = 0xff;
    void* pvMask = 0;
    HBITMAP hMask = CreateDIBSection(0, pInfo, DIB_RGB_COLORS, &pvMask, 0, 0);

    HCURSOR hCursor = 0;
    if (hColor && hMask && pvColor && pvMask)
    {
        DisplayLogic::CursorScaleBgra(sauiSrc, suiSrcW, suiSrcH, (unsigned int*)pvColor, uiW, uiH);
        DisplayLogic::CursorMaskFromBgra((const unsigned int*)pvColor, uiW, uiH,
                                         (unsigned char*)pvMask);
        GdiFlush();
        ICONINFO kInfo;
        kInfo.fIcon = FALSE;
        kInfo.xHotspot = 0;
        kInfo.yHotspot = 0;
        kInfo.hbmMask = hMask;
        kInfo.hbmColor = hColor;
        hCursor = (HCURSOR)CreateIconIndirect(&kInfo);
    }
    if (hColor)
    {
        DeleteObject(hColor);
    }
    if (hMask)
    {
        DeleteObject(hMask);
    }
    return hCursor;
}

static void SetBuiltCursor(unsigned int uiW, unsigned int uiH)
{
    suiBuiltW = uiW;
    suiBuiltH = uiH;
    HCURSOR hNew = MakeCursorFromCache(uiW, uiH);
    if (hNew)
    {
        if (shCursor && GetCursor() == shCursor)
        {
            ::SetCursor(hNew);
        }
        if (shCursor)
        {
            DestroyIcon((HICON)shCursor);
        }
        shCursor = hNew;
    }
    else if (!sbCreateLogged)
    {
        LOG(LogCore, Warning, "OPENSWBF2 CURSOR fallback reason=%s", "create");
        sbCreateLogged = true;
    }
}

static void WantedSize(unsigned int& uiW, unsigned int& uiH)
{
    RECT kRc = { 0, 0, 0, 0 };
    if (hGlobalWnd)
    {
        GetClientRect(hGlobalWnd, &kRc);
    }
    DisplayLogic::CursorSize(suiSrcW, suiSrcH, RedInterfaceScreen::GetScreenWidth(),
                             RedInterfaceScreen::GetScreenHeight(), (unsigned int)kRc.right,
                             (unsigned int)kRc.bottom, uiW, uiH);
}

static const char* ReadCursorTexture(unsigned int uiTextureHash)
{
    const RedTexture* pTexture = RedTexture::Find(uiTextureHash);
    if (!pTexture || !pTexture->m_pTexture)
    {
        return "notexture";
    }
    if (pTexture->m_type != RedTexture::PC_REDTEXTURE_TYPE_TEXTURE)
    {
        return "type";
    }
    IDirect3DTexture9* pTex = (IDirect3DTexture9*)pTexture->m_pTexture;
    D3DSURFACE_DESC kDesc;
    if (FAILED(pTex->GetLevelDesc(0, &kDesc)))
    {
        return "readback";
    }
    if (kDesc.Width == 0 || kDesc.Height == 0 || kDesc.Width > kCursorSrcMax
        || kDesc.Height > kCursorSrcMax)
    {
        return "size";
    }
    IDirect3DDevice9* pDevice = RedRenderer::pcGetDevice();
    if (!pDevice)
    {
        return "readback";
    }

    const char* pcReason = "readback";
    IDirect3DSurface9* pSrc = 0;
    IDirect3DSurface9* pDst = 0;
    if (SUCCEEDED(pTex->GetSurfaceLevel(0, &pSrc))
        && SUCCEEDED(pDevice->CreateOffscreenPlainSurface(kDesc.Width, kDesc.Height,
                                                          D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH,
                                                          &pDst, 0))
        && SUCCEEDED(D3DXLoadSurfaceFromSurface(pDst, 0, 0, pSrc, 0, 0, D3DX_FILTER_NONE, 0)))
    {
        D3DLOCKED_RECT kLr;
        if (SUCCEEDED(pDst->LockRect(&kLr, 0, D3DLOCK_READONLY)))
        {
            for (unsigned int y = 0; y < kDesc.Height; y++)
            {
                memcpy(&sauiSrc[y * kDesc.Width], (unsigned char*)kLr.pBits + kLr.Pitch * y,
                       kDesc.Width * sizeof(unsigned int));
            }
            pDst->UnlockRect();
            suiSrcW = kDesc.Width;
            suiSrcH = kDesc.Height;
            sbHaveSrc = true;
            pcReason = 0;
        }
    }
    if (pDst)
    {
        pDst->Release();
    }
    if (pSrc)
    {
        pSrc->Release();
    }
    return pcReason;
}

bool HwCursor::Software()
{
    return DisplayCfg::gDisplay.bSoftwareCursor;
}

bool HwCursor::Shown()
{
    return sPlan.bHcursor;
}

HCURSOR HwCursor::Current()
{
    if (!sPlan.bHcursor || XInputPad::gPadOwnsCursor)
    {
        return 0;
    }
    if (shCursor)
    {
        return shCursor;
    }
    return LoadCursorA(0, IDC_ARROW);
}

void HwCursor::ReleaseClip()
{
    if (!sbClipped)
    {
        return;
    }
    ClipCursor(0);
    sbClipped = false;
}

void HwCursor::Destroy()
{
    sbAttached = false;
    ReleaseClip();
    if (sPlan.bHcursor)
    {
        ::SetCursor(0);
        sPlan.bHcursor = false;
    }
    if (shCursor)
    {
        DestroyIcon((HICON)shCursor);
    }
    shCursor = 0;
    suiBuiltW = 0;
    suiBuiltH = 0;
    sbHaveSrc = false;
}

void HwCursor::Build(unsigned int uiTextureHash)
{
    Destroy();
    sbAttached = true;
    sbCreateLogged = false;
    if (!Software())
    {
        FLRenderer::mCursorImage.Enable(false);
    }
    if (Software() || g_bNoRender)
    {
        LOG(LogCore, Inform, "OPENSWBF2 CURSOR mode=%s image=skipped", Software() ? "sw" : "hw");
        return;
    }
    const char* pcReason = ReadCursorTexture(uiTextureHash);
    if (pcReason)
    {
        LOG(LogCore, Warning, "OPENSWBF2 CURSOR fallback reason=%s", pcReason);
        LOG(LogCore, Inform, "OPENSWBF2 CURSOR mode=hw image=fallback");
        return;
    }
    unsigned int uiW = 0;
    unsigned int uiH = 0;
    WantedSize(uiW, uiH);
    SetBuiltCursor(uiW, uiH);
    if (shCursor)
    {
        LOG(LogCore, Inform, "OPENSWBF2 CURSOR mode=hw image=built");
    }
    else
    {
        LOG(LogCore, Inform, "OPENSWBF2 CURSOR mode=hw image=fallback");
    }
}

// FUNCTION: SWBF2 0x006da310
bool RedMouse::Exists()
{
    return pc_device != 0;
}

// FUNCTION: SWBF2 0x006da4b0
void RedMouse::Remove()
{
    pc_device->Unacquire();
    pc_device->SetCooperativeLevel(hGlobalWnd, sdwCoopFlags);
    m_connected = false;
}

// FUNCTION: SWBF2 0x006da320
void RedMouse::Init(unsigned char ucPortNum)
{
    pc_device = 0;
    m_portNum = ucPortNum;
    m_connected = false;
    pCursorImage = 0;

    POINT point;
    GetCursorPos(&point);
    int iCw = 0;
    int iCh = 0;
    if (hGlobalWnd)
    {
        RECT rc;
        ScreenToClient(hGlobalWnd, &point);
        GetClientRect(hGlobalWnd, &rc);
        iCw = rc.right;
        iCh = rc.bottom;
    }
    const int iSw = (int)RedInterfaceScreen::GetScreenWidth();
    const int iSh = (int)RedInterfaceScreen::GetScreenHeight();
    DisplayLogic::MapClientToBackBuffer(point.x, point.y, (unsigned int)iCw, (unsigned int)iCh,
                                        (unsigned int)iSw, (unsigned int)iSh, m_X, m_Y);
    m_X = (m_X < 0) ? 0 : (m_X > iSw ? iSw : m_X);
    m_Y = (m_Y < 0) ? 0 : (m_Y > iSh ? iSh : m_Y);
    m_Z = 0;
    m_dZ = 0;
    m_dY = 0;
    m_dX = 0;
    m_uiDownButtons = 0;
    m_uiPressedButtons = 0;
    m_uiReleasedButtons = 0;

    IDirectInput8A* pDI = RedInputManager::pcGetDIInterface();

    if (pc_device)
    {
        return;
    }

    if (pDI->CreateDevice(GUID_SysMouse, &pc_device, 0) != DI_OK)
    {
        return;
    }

    HRESULT hrDataFormat = pc_device->SetDataFormat(&c_dfDIMouse2);

    DIPROPRANGE diprg;
    diprg.diph.dwSize = sizeof(DIPROPRANGE);
    diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    diprg.diph.dwObj = 0;
    diprg.diph.dwHow = DIPH_DEVICE;
    diprg.lMin = (LONG)0x80000000;
    diprg.lMax = 0x7FFFFFFF;
    pc_device->GetProperty(DIPROP_RANGE, &diprg.diph);
    pc_device->SetProperty(DIPROP_RANGE, &diprg.diph);

    if (FAILED(hrDataFormat)
        || FAILED(pc_device->SetCooperativeLevel(hGlobalWnd,
                                                 DISCL_EXCLUSIVE | DISCL_FOREGROUND)))
    {
        pc_device->Release();
        pc_device = 0;
    }
}

// FUNCTION: SWBF2 0x006da450
void RedMouse::Insert()
{
    if (m_connected)
    {
        return;
    }

    memset(&pc_state, 0, sizeof(pc_state));

    pc_device->Unacquire();
    pc_device->SetCooperativeLevel(hGlobalWnd, sdwCoopFlags);

    HRESULT hr = pc_device->Acquire();
    while (hr == DIERR_INPUTLOST)
    {
        hr = pc_device->Acquire();
    }

    m_connected = (hr == DI_OK);
}

// FUNCTION: SWBF2 0x006da290
RedMouse::RedMouse()
{
}

// FUNCTION: SWBF2 0x006da2a0
RedMouse::~RedMouse()
{
}

// FUNCTION: SWBF2 0x006da4e0
void RedMouse::SetCursorPos(int iX, int iY)
{
    m_X = iX;
    m_Y = iY;

    const int iWidth = (int)RedInterfaceScreen::GetScreenWidth();
    const int iHeight = (int)RedInterfaceScreen::GetScreenHeight();
    m_X = (iX < 0) ? 0 : (iX > iWidth ? iWidth : iX);
    m_Y = (iY < 0) ? 0 : (iY > iHeight ? iHeight : iY);
    FLRenderer::SetCursorPos((float)m_X, (float)m_Y);

    if (gAppActive && hGlobalWnd)
    {
        RECT rc;
        int iCx = 0;
        int iCy = 0;
        GetClientRect(hGlobalWnd, &rc);
        DisplayLogic::MapBackBufferToClient(m_X, m_Y, (unsigned int)iWidth, (unsigned int)iHeight,
                                            (unsigned int)rc.right, (unsigned int)rc.bottom, iCx,
                                            iCy);
        POINT pt = { iCx, iCy };
        ClientToScreen(hGlobalWnd, &pt);
        ::SetCursorPos(pt.x, pt.y);
    }
}

// FUNCTION: SWBF2 0x006da2e0
void RedMouse::Cleanup()
{
    if (pc_device)
    {
        pc_device->Unacquire();
        pc_device->Release();
        pc_device = 0;
    }
    m_connected = false;
}

static void ReconcileCursor(RedMouse& kMouse)
{
    const bool bShown = sbAttached && FLRenderer::mCursorGroup.IsEnabled();
    const bool bActive = gAppActive && hGlobalWnd && !IsIconic(hGlobalWnd);
    const DisplayLogic::CursorPlan kPlan = DisplayLogic::CursorPlanFor(
        bShown, RedRenderer::GetDisplayMode(), bActive, HwCursor::Software(), g_bNoRender);

    const DWORD dwWant = kPlan.bExclusive ? (DWORD)(DISCL_EXCLUSIVE | DISCL_FOREGROUND)
                                          : (DWORD)(DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
    if (dwWant != sdwCoopFlags)
    {
        sdwCoopFlags = dwWant;
        if (kMouse.Exists())
        {
            kMouse.Remove();
            kMouse.Insert();
        }
    }

    if (kPlan.bHideDrawn && sbAttached)
    {
        FLRenderer::mCursorImage.Enable(false);
    }

    if (kPlan.bHcursor && sbHaveSrc)
    {
        unsigned int uiW = 0;
        unsigned int uiH = 0;
        WantedSize(uiW, uiH);
        if (uiW != suiBuiltW || uiH != suiBuiltH)
        {
            SetBuiltCursor(uiW, uiH);
        }
    }

    const DisplayLogic::CursorPlan kPrev = sPlan;
    sPlan = kPlan;
    if (kPlan.bHcursor && !kPrev.bHcursor && kPrev.bExclusive)
    {
        kMouse.SetCursorPos(kMouse.m_X, kMouse.m_Y);
    }
    if (kPlan.bHcursor && !kPrev.bHcursor)
    {
        POINT kPt;
        if (GetCursorPos(&kPt) && WindowFromPoint(kPt) == hGlobalWnd)
        {
            ::SetCursor(HwCursor::Current());
        }
    }
    if (!kPlan.bHcursor && kPrev.bHcursor)
    {
        ::SetCursor(0);
    }

    // Hide the cursor while a pad drives the menus. Visual only: mouse
    // exclusivity and clipping are left to the plan above.
    static bool sbPadHidden;
    if (XInputPad::gPadOwnsCursor != sbPadHidden)
    {
        sbPadHidden = XInputPad::gPadOwnsCursor;
        if (kPlan.bHcursor)
        {
            POINT kPt;
            if (GetCursorPos(&kPt) && WindowFromPoint(kPt) == hGlobalWnd)
            {
                ::SetCursor(HwCursor::Current());
            }
        }
        else if (HwCursor::Software() && sbAttached)
        {
            FLRenderer::mCursorImage.Enable(!sbPadHidden);
        }
    }

    if (kPlan.bClip && hGlobalWnd)
    {
        RECT kClient;
        GetClientRect(hGlobalWnd, &kClient);
        POINT kTl = { kClient.left, kClient.top };
        POINT kBr = { kClient.right, kClient.bottom };
        ClientToScreen(hGlobalWnd, &kTl);
        ClientToScreen(hGlobalWnd, &kBr);
        RECT kRc = { kTl.x, kTl.y, kBr.x, kBr.y };
        RECT kCur;
        if (!GetClipCursor(&kCur) || !EqualRect(&kCur, &kRc))
        {
            ClipCursor(&kRc);
            sClipRect = kRc;
            sbClipped = true;
        }
    }
    else
    {
        HwCursor::ReleaseClip();
    }
}

// FUNCTION: SWBF2 0x006da560
void RedMouse::Update(float fDeltaTime)
{
    ReconcileCursor(*this);

    if (!pc_device)
    {
        return;
    }

    HRESULT hr = pc_device->Poll();
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
    {
        Remove();
        Insert();
        if (FAILED(pc_device->Poll()))
        {
            return;
        }
    }

    if (FAILED(pc_device->GetDeviceState(sizeof(DIMOUSESTATE2), &pc_state)))
    {
        return;
    }

    m_dX = pc_state.lX;
    m_dY = pc_state.lY;
    m_dZ = pc_state.lZ;
    if (abs(m_dX) >= 3 || abs(m_dY) >= 3)
    {
        pCursorImage = 0;
        XInputPad::MouseTookMenus();
    }

    bool bBlockNew = false;
    if (sPlan.bHcursor && hGlobalWnd)
    {
        POINT kScreen = { 0, 0 };
        RECT kRc = { 0, 0, 0, 0 };
        if (GetCursorPos(&kScreen) && GetClientRect(hGlobalWnd, &kRc))
        {
            POINT kClientPt = kScreen;
            ScreenToClient(hGlobalWnd, &kClientPt);
            const bool bInside = kClientPt.x >= 0 && kClientPt.x < kRc.right && kClientPt.y >= 0
                                 && kClientPt.y < kRc.bottom
                                 && WindowFromPoint(kScreen) == hGlobalWnd;
            bBlockNew = RedRenderer::GetDisplayMode() != DisplayLogic::kModeFullscreen && !bInside;
            DisplayLogic::MapClientToBackBuffer(
                kClientPt.x, kClientPt.y, (unsigned int)kRc.right, (unsigned int)kRc.bottom,
                RedInterfaceScreen::GetScreenWidth(), RedInterfaceScreen::GetScreenHeight(), m_X,
                m_Y);
        }
        else
        {
            bBlockNew = RedRenderer::GetDisplayMode() != DisplayLogic::kModeFullscreen;
        }
    }
    else
    {
        m_X += m_dX;
        m_Y += m_dY;
    }
    if (bBlockNew)
    {
        m_dZ = 0;
    }
    m_Z += m_dZ;

    const int iWidth = (int)RedInterfaceScreen::GetScreenWidth();
    const int iHeight = (int)RedInterfaceScreen::GetScreenHeight();
    m_X = (m_X < 0) ? 0 : (m_X > iWidth ? iWidth : m_X);
    m_Y = (m_Y < 0) ? 0 : (m_Y > iHeight ? iHeight : m_Y);
    FLRenderer::SetCursorPos((float)m_X, (float)m_Y);

    const unsigned int uiPrevDown = m_uiDownButtons;
    unsigned int uiRaw = 0;
    for (int i = 0; i < 8; i++)
    {
        if (pc_state.rgbButtons[i] & 0x80)
        {
            uiRaw |= 1 << i;
        }
    }
    m_uiDownButtons = DisplayLogic::CursorButtonLatch(uiRaw, suiRawPrevDown, bBlockNew, suiLatched);
    suiRawPrevDown = uiRaw;
    const unsigned int uiChanged = uiPrevDown ^ m_uiDownButtons;
    m_uiPressedButtons = m_uiDownButtons & uiChanged;
    m_uiReleasedButtons = uiPrevDown & uiChanged;
    if (m_uiPressedButtons)
    {
        XInputPad::MouseTookMenus();
    }
}
