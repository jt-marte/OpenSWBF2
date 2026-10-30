#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>

#include "pcPadPrompts.h"

#include "PebbleFL/Common/PblHashTable.h"
#include "RedEngineFL/Graphics/PC/pcRedRenderer.h"
#include "RedEngineFL/Graphics/PC/pcRedTexture.h"
#include "RedEngineFL/input/pc/pcXInputPad.h"
#include "RedEngineFL/Util/RedLog.h"

namespace PadPrompts
{

namespace
{

const int kSize = 64;

enum Shape
{
    kShapeCircle,
    kShapeBumper,
    kShapeTrigger,
    kShapeDPad
};

struct Style
{
    int iShape;
    unsigned int uiText; // 0xRRGGBB
    bool bRing;          // a lighter ring: the sticks
    int iArm;            // D-pad: highlighted arm, 0 up 1 right 2 down 3 left
};

const unsigned int kFace = 0x202020;
const unsigned int kWhite = 0xFFFFFF;

const Style kStyles[XInputPad::kGlyphCount] = {
    { kShapeCircle, 0x6CC24A, false, -1 }, // A
    { kShapeCircle, 0xE0453A, false, -1 }, // B
    { kShapeCircle, 0x3E8EDE, false, -1 }, // X
    { kShapeCircle, 0xF2C12E, false, -1 }, // Y
    { kShapeBumper, kWhite, false, -1 },   // LB
    { kShapeBumper, kWhite, false, -1 },   // RB
    { kShapeTrigger, kWhite, false, -1 },  // LT
    { kShapeTrigger, kWhite, false, -1 },  // RT
    { kShapeCircle, kWhite, true, -1 },    // LS (click)
    { kShapeCircle, kWhite, true, -1 },    // RS (click)
    { kShapeBumper, kWhite, false, -1 },   // Back
    { kShapeBumper, kWhite, false, -1 },   // Start
    { kShapeDPad, kWhite, false, 0 },
    { kShapeDPad, kWhite, false, 1 },
    { kShapeDPad, kWhite, false, 2 },
    { kShapeDPad, kWhite, false, 3 },
    { kShapeCircle, kWhite, true, -1 },    // left stick
    { kShapeCircle, kWhite, true, -1 },    // right stick
};

bool InRoundRect(float fX, float fY, float fX0, float fY0, float fX1, float fY1, float fR)
{
    const float fCx = fX < fX0 + fR ? fX0 + fR : (fX > fX1 - fR ? fX1 - fR : fX);
    const float fCy = fY < fY0 + fR ? fY0 + fR : (fY > fY1 - fR ? fY1 - fR : fY);
    const float fDx = fX - fCx;
    const float fDy = fY - fCy;
    return fX >= fX0 && fX <= fX1 && fY >= fY0 && fY <= fY1 && fDx * fDx + fDy * fDy <= fR * fR;
}

bool InShape(int iShape, float fX, float fY)
{
    switch (iShape)
    {
    case kShapeCircle:
        return (fX - 32.0f) * (fX - 32.0f) + (fY - 32.0f) * (fY - 32.0f) <= 30.0f * 30.0f;
    case kShapeBumper:
        return InRoundRect(fX, fY, 1.0f, 13.0f, 63.0f, 51.0f, 12.0f);
    case kShapeTrigger:
        return InRoundRect(fX, fY, 9.0f, 2.0f, 55.0f, 62.0f, 14.0f);
    case kShapeDPad:
        return InRoundRect(fX, fY, 22.0f, 2.0f, 42.0f, 62.0f, 5.0f) ||
               InRoundRect(fX, fY, 2.0f, 22.0f, 62.0f, 42.0f, 5.0f);
    }
    return false;
}

// The lit part of a style: a ring round the sticks, one arm of the D-pad.
bool InAccent(const Style& rStyle, float fX, float fY)
{
    if (rStyle.bRing)
    {
        const float fD2 = (fX - 32.0f) * (fX - 32.0f) + (fY - 32.0f) * (fY - 32.0f);
        return fD2 <= 30.0f * 30.0f && fD2 >= 25.0f * 25.0f;
    }
    switch (rStyle.iArm)
    {
    case 0: return InRoundRect(fX, fY, 25.0f, 5.0f, 39.0f, 22.0f, 3.0f);
    case 1: return InRoundRect(fX, fY, 42.0f, 25.0f, 59.0f, 39.0f, 3.0f);
    case 2: return InRoundRect(fX, fY, 25.0f, 42.0f, 39.0f, 59.0f, 3.0f);
    case 3: return InRoundRect(fX, fY, 5.0f, 25.0f, 22.0f, 39.0f, 3.0f);
    }
    return false;
}

// 4x4 supersampled coverage of the shape and of its accent for one pixel.
void Coverage(const Style& rStyle, int iX, int iY, float& fShape, float& fAccent)
{
    int iShape = 0;
    int iAccent = 0;
    for (int iSy = 0; iSy < 4; ++iSy)
    {
        for (int iSx = 0; iSx < 4; ++iSx)
        {
            const float fX = (float)iX + ((float)iSx + 0.5f) * 0.25f;
            const float fY = (float)iY + ((float)iSy + 0.5f) * 0.25f;
            if (InShape(rStyle.iShape, fX, fY))
            {
                ++iShape;
                if (InAccent(rStyle, fX, fY))
                {
                    ++iAccent;
                }
            }
        }
    }
    fShape = (float)iShape / 16.0f;
    fAccent = (float)iAccent / 16.0f;
}

// The label's coverage (0..255 per pixel), drawn white on black with GDI.
void DrawLabel(const char* pcText, unsigned char* pucCoverage)
{
    memset(pucCoverage, 0, kSize * kSize);
    const size_t uiLen = strlen(pcText);
    if (!uiLen)
    {
        return;
    }

    BITMAPINFO kInfo;
    memset(&kInfo, 0, sizeof(kInfo));
    kInfo.bmiHeader.biSize = sizeof(kInfo.bmiHeader);
    kInfo.bmiHeader.biWidth = kSize;
    kInfo.bmiHeader.biHeight = -kSize;
    kInfo.bmiHeader.biPlanes = 1;
    kInfo.bmiHeader.biBitCount = 32;
    kInfo.bmiHeader.biCompression = BI_RGB;

    HDC hDC = CreateCompatibleDC(0);
    void* pvBits = 0;
    HBITMAP hBitmap = CreateDIBSection(hDC, &kInfo, DIB_RGB_COLORS, &pvBits, 0, 0);
    if (!hDC || !hBitmap || !pvBits)
    {
        if (hBitmap)
        {
            DeleteObject(hBitmap);
        }
        if (hDC)
        {
            DeleteDC(hDC);
        }
        return;
    }
    HGDIOBJ hOldBitmap = SelectObject(hDC, hBitmap);
    memset(pvBits, 0, kSize * kSize * 4);

    const int iHeight = uiLen == 1 ? 40 : (uiLen == 2 ? 28 : 14);
    HFONT hFont = CreateFontA(-iHeight, 0, 0, 0, FW_BLACK, FALSE, FALSE, FALSE, ANSI_CHARSET,
                              OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH | FF_SWISS, "Arial Black");
    HGDIOBJ hOldFont = SelectObject(hDC, hFont);
    SetTextColor(hDC, RGB(255, 255, 255));
    SetBkMode(hDC, TRANSPARENT);
    RECT kRect = { 0, 0, kSize, kSize };
    DrawTextA(hDC, pcText, (int)uiLen, &kRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    GdiFlush();

    const unsigned char* pucBits = (const unsigned char*)pvBits;
    for (int i = 0; i < kSize * kSize; ++i)
    {
        pucCoverage[i] = pucBits[i * 4 + 2];
    }

    SelectObject(hDC, hOldFont);
    DeleteObject(hFont);
    SelectObject(hDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hDC);
}

unsigned char Lerp(unsigned int uiA, unsigned int uiB, float fT)
{
    return (unsigned char)((float)uiA + ((float)uiB - (float)uiA) * fT + 0.5f);
}

void DrawIcon(int iGlyph, unsigned int* puiPixels)
{
    const Style& rStyle = kStyles[iGlyph];
    unsigned char aucLabel[kSize * kSize];
    DrawLabel(XInputPad::GlyphText(iGlyph), aucLabel);

    for (int iY = 0; iY < kSize; ++iY)
    {
        for (int iX = 0; iX < kSize; ++iX)
        {
            float fShape, fAccent;
            Coverage(rStyle, iX, iY, fShape, fAccent);
            const float fText = (float)aucLabel[iY * kSize + iX] / 255.0f;
            const float fLit = fAccent > fText ? fAccent : fText;
            const unsigned int uiLit = fAccent > fText ? 0xC8C8C8 : rStyle.uiText;

            const unsigned char ucR = Lerp((kFace >> 16) & 0xFF, (uiLit >> 16) & 0xFF, fLit);
            const unsigned char ucG = Lerp((kFace >> 8) & 0xFF, (uiLit >> 8) & 0xFF, fLit);
            const unsigned char ucB = Lerp(kFace & 0xFF, uiLit & 0xFF, fLit);
            float fAlpha = fShape * 0.9f;
            if (fText > fAlpha)
            {
                fAlpha = fText;
            }
            const unsigned char ucA = (unsigned char)(fAlpha * 255.0f + 0.5f);
            puiPixels[iY * kSize + iX] =
                ((unsigned int)ucA << 24) | ((unsigned int)ucR << 16) | ((unsigned int)ucG << 8) | ucB;
        }
    }
}

}

unsigned int IconHash(int iGlyph)
{
    char acName[32];
    sprintf(acName, "openswbf2_pad%d", iGlyph);
    return PblHash(acName);
}

void CreateIcons()
{
    if (!RedRenderer::pcGetDevice())
    {
        return;
    }
    static unsigned int sauiPixels[kSize * kSize];
    int iMade = 0;
    int iKept = 0;
    for (int iGlyph = 0; iGlyph < XInputPad::kGlyphCount; ++iGlyph)
    {
        if (RedTexture::s_textureHashTable.Find(IconHash(iGlyph)))
        {
            ++iKept;
            continue;
        }
        RedTexture* pTexture =
            new RedTexture(kSize, kSize, D3DFMT_A8R8G8B8, 1, 0, D3DPOOL_MANAGED, IconHash(iGlyph));
        IDirect3DTexture9* pD3DTexture = (IDirect3DTexture9*)pTexture->m_pTexture;
        D3DLOCKED_RECT kLocked;
        if (!pD3DTexture || FAILED(pD3DTexture->LockRect(0, &kLocked, 0, 0)))
        {
            continue;
        }
        DrawIcon(iGlyph, sauiPixels);
        for (int iY = 0; iY < kSize; ++iY)
        {
            memcpy((unsigned char*)kLocked.pBits + iY * kLocked.Pitch, &sauiPixels[iY * kSize], kSize * 4);
        }
        pD3DTexture->UnlockRect(0);
        ++iMade;
    }
    LOG(LogRender, Inform, "OPENSWBF2 PADPROMPTS icons made=%d kept=%d", iMade, iKept);
}

}
