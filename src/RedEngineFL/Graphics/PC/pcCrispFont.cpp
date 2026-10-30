#include <windows.h>
#include <d3d9.h>
#include <string.h>

#include "pcCrispFont.h"

#include "pcDisplayLogic.h"
#include "pcRedRenderer.h"
#include "pcRedTexture.h"
#include "RedEngineFL/Util/RedLog.h"

namespace CrispFont
{

namespace
{

// The coverage (0..255 per texel) of a page's mip level uiLevel (uiW x uiH)
// from its A4R4G4B4 or A8R8G8B8 alpha.
bool ReadCoverage(IDirect3DTexture9* pTexture, D3DFORMAT eFormat, unsigned int uiLevel, unsigned int uiW,
                  unsigned int uiH, unsigned char* pucOut)
{
    if (eFormat != D3DFMT_A4R4G4B4 && eFormat != D3DFMT_A8R8G8B8)
    {
        return false;
    }
    D3DLOCKED_RECT kLocked;
    if (FAILED(pTexture->LockRect(uiLevel, &kLocked, 0, D3DLOCK_READONLY)))
    {
        return false;
    }
    for (unsigned int uiY = 0; uiY < uiH; ++uiY)
    {
        const unsigned char* pucRow = (const unsigned char*)kLocked.pBits + uiY * kLocked.Pitch;
        for (unsigned int uiX = 0; uiX < uiW; ++uiX)
        {
            if (eFormat == D3DFMT_A4R4G4B4)
            {
                const unsigned short usTexel = ((const unsigned short*)pucRow)[uiX];
                pucOut[uiY * uiW + uiX] = (unsigned char)((usTexel >> 12) * 17);
            }
            else
            {
                pucOut[uiY * uiW + uiX] = pucRow[uiX * 4 + 3];
            }
        }
    }
    pTexture->UnlockRect(uiLevel);
    return true;
}

// A glyph's box on its page at 1x (uiTexW x uiTexH), if it lies on the page:
// the texels it shows, its UVs being half a texel in (DisplayLogic::TexelInset).
bool GlyphBox(const RedFont::Glyph& g, unsigned int uiTexW, unsigned int uiTexH, int& iX, int& iY, int& iW,
              int& iH)
{
    const float fX = g.uLeft * (float)uiTexW;
    const float fY = g.vTop * (float)uiTexH;
    iX = (int)(fX - DisplayLogic::TexelInset(fX) + 0.5f);
    iY = (int)(fY - DisplayLogic::TexelInset(fY) + 0.5f);
    iW = (int)((g.uRight - g.uLeft) * (float)uiTexW + 0.5f);
    iH = (int)((g.vBottom - g.vTop) * (float)uiTexH + 0.5f);
    return iW > 0 && iH > 0 && iX >= 0 && iY >= 0 && iX + iW <= (int)uiTexW && iY + iH <= (int)uiTexH;
}

// Bilinear upscale of the original coverage, then steepened a little so the
// edges stay firm: the fallback for anything GDI doesn't draw.
void Upscale(const unsigned char* pucSrc, unsigned int uiW, unsigned int uiH, unsigned int uiK,
             unsigned char* pucDst)
{
    const unsigned int uiDstW = uiW * uiK;
    const unsigned int uiDstH = uiH * uiK;
    for (unsigned int uiY = 0; uiY < uiDstH; ++uiY)
    {
        float fY = ((float)uiY + 0.5f) / (float)uiK - 0.5f;
        if (fY < 0.0f)
        {
            fY = 0.0f;
        }
        unsigned int uiY0 = (unsigned int)fY;
        unsigned int uiY1 = uiY0 + 1 < uiH ? uiY0 + 1 : uiY0;
        const float fTy = fY - (float)uiY0;
        for (unsigned int uiX = 0; uiX < uiDstW; ++uiX)
        {
            float fX = ((float)uiX + 0.5f) / (float)uiK - 0.5f;
            if (fX < 0.0f)
            {
                fX = 0.0f;
            }
            unsigned int uiX0 = (unsigned int)fX;
            unsigned int uiX1 = uiX0 + 1 < uiW ? uiX0 + 1 : uiX0;
            const float fTx = fX - (float)uiX0;
            const float fTop = pucSrc[uiY0 * uiW + uiX0] * (1.0f - fTx) + pucSrc[uiY0 * uiW + uiX1] * fTx;
            const float fBottom = pucSrc[uiY1 * uiW + uiX0] * (1.0f - fTx) + pucSrc[uiY1 * uiW + uiX1] * fTx;
            float fValue = (fTop * (1.0f - fTy) + fBottom * fTy) / 255.0f;
            fValue = (fValue - 0.5f) * 1.6f + 0.5f;
            fValue = fValue < 0.0f ? 0.0f : (fValue > 1.0f ? 1.0f : fValue);
            pucDst[uiY * uiDstW + uiX] = (unsigned char)(fValue * 255.0f + 0.5f);
        }
    }
}

const MAT2 kIdentity = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };

// GDI's 16.16 fixed point.
FIXED ToFixed(float fValue)
{
    const long lValue = (long)(fValue * 65536.0f + 0.5f);
    FIXED kOut;
    kOut.value = (short)(lValue >> 16);
    kOut.fract = (WORD)(lValue & 0xFFFF);
    return kOut;
}

// A glyph's GDI bitmap (0..64 per pixel) under a transform, into pucScratch.
bool GetBitmap(HDC hDC, unsigned short usValue, const MAT2& rMatrix, GLYPHMETRICS& rMetrics,
               unsigned char* pucScratch, unsigned int uiScratchSize)
{
    const DWORD dwSize = GetGlyphOutlineW(hDC, usValue, GGO_GRAY8_BITMAP, &rMetrics, 0, 0, &rMatrix);
    return dwSize != GDI_ERROR && dwSize != 0 && dwSize <= uiScratchSize &&
           GetGlyphOutlineW(hDC, usValue, GGO_GRAY8_BITMAP, &rMetrics, dwSize, pucScratch, &rMatrix) !=
               GDI_ERROR;
}

// An Arial Black GDI font sized so 'H' is fCapHeight pixels tall, or 0.
HFONT MakeFont(HDC hDC, float fCapHeight)
{
    const int iProbeEm = 400;
    HFONT hProbe = CreateFontW(-iProbeEm, 0, 0, 0, FW_BLACK, FALSE, FALSE, FALSE, ANSI_CHARSET,
                               OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                               DEFAULT_PITCH | FF_SWISS, L"Arial Black");
    if (!hProbe)
    {
        return 0;
    }
    HGDIOBJ hOld = SelectObject(hDC, hProbe);
    wchar_t awcFace[64] = { 0 };
    GetTextFaceW(hDC, 64, awcFace);
    GLYPHMETRICS kMetrics;
    const DWORD dwSize = GetGlyphOutlineW(hDC, L'H', GGO_METRICS, &kMetrics, 0, 0, &kIdentity);
    SelectObject(hDC, hOld);
    DeleteObject(hProbe);
    if (dwSize == GDI_ERROR || kMetrics.gmBlackBoxY == 0 || _wcsicmp(awcFace, L"Arial Black") != 0)
    {
        return 0;
    }
    const int iEm = (int)(((float)iProbeEm * fCapHeight) / (float)kMetrics.gmBlackBoxY + 0.5f);
    return CreateFontW(-iEm, 0, 0, 0, FW_BLACK, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial Black");
}

// Draws one glyph with GDI into its box at K (cleared first), as wide as the
// atlas glyph (pucSrc, the 1x page) and on its ink's middle. False if GDI has
// no outline for it, leaving the box as it was.
bool DrawGlyph(HDC hDC, const RedFont::Glyph& g, const unsigned char* pucSrc, unsigned int uiTexW,
               unsigned int uiTexH, unsigned int uiK, float fBaseline, unsigned char* pucDst,
               unsigned char* pucScratch, unsigned int uiScratchSize, float& rfStretch)
{
    int iSrcX, iSrcY, iBoxW, iBoxH;
    if (!GlyphBox(g, uiTexW, uiTexH, iSrcX, iSrcY, iBoxW, iBoxH))
    {
        return false;
    }

    GLYPHMETRICS kMetrics;
    if (!GetBitmap(hDC, g.value, kIdentity, kMetrics, pucScratch, uiScratchSize))
    {
        return false;
    }
    const DisplayLogic::InkSpan kAtlas =
        DisplayLogic::MeasureInk(pucSrc + iSrcY * uiTexW + iSrcX, iBoxW, iBoxH, (int)uiTexW, 255);
    DisplayLogic::InkSpan kInk = DisplayLogic::MeasureInk(
        pucScratch, (int)kMetrics.gmBlackBoxX, (int)kMetrics.gmBlackBoxY, ((int)kMetrics.gmBlackBoxX + 3) & ~3, 64);
    const float fStretch = DisplayLogic::CrispStretch(kAtlas.fWidth, kInk.fWidth, (int)uiK);
    rfStretch = fStretch;
    if (fStretch != 1.0f)
    {
        MAT2 kWide = kIdentity;
        kWide.eM11 = ToFixed(fStretch);
        if (!GetBitmap(hDC, g.value, kWide, kMetrics, pucScratch, uiScratchSize))
        {
            return false;
        }
        kInk = DisplayLogic::MeasureInk(pucScratch, (int)kMetrics.gmBlackBoxX, (int)kMetrics.gmBlackBoxY,
                                        ((int)kMetrics.gmBlackBoxX + 3) & ~3, 64);
    }

    const unsigned int uiDstW = uiTexW * uiK;
    const int iBoxX = iSrcX * (int)uiK;
    const int iBoxY = iSrcY * (int)uiK;
    int iDstX, iDstY;
    DisplayLogic::CrispPlace(iBoxX, iBoxY, (int)uiK, g.top, fBaseline, kMetrics.gmptGlyphOrigin.y,
                             kAtlas.fWidth > 0.0f ? kAtlas.fCentre : (float)iBoxW * 0.5f,
                             kInk.fWidth > 0.0f ? kInk.fCentre : (float)kMetrics.gmBlackBoxX * 0.5f, iDstX,
                             iDstY);

    const int iBoxX1 = iBoxX + iBoxW * (int)uiK;
    const int iBoxY1 = iBoxY + iBoxH * (int)uiK;
    for (int iY = iBoxY; iY < iBoxY1; ++iY)
    {
        memset(pucDst + iY * uiDstW + iBoxX, 0, iBoxX1 - iBoxX);
    }
    const int iPitch = ((int)kMetrics.gmBlackBoxX + 3) & ~3;
    for (int iRow = 0; iRow < (int)kMetrics.gmBlackBoxY; ++iRow)
    {
        const int iY = iDstY + iRow;
        if (iY < iBoxY || iY >= iBoxY1)
        {
            continue;
        }
        for (int iCol = 0; iCol < (int)kMetrics.gmBlackBoxX; ++iCol)
        {
            const int iX = iDstX + iCol;
            if (iX < iBoxX || iX >= iBoxX1)
            {
                continue;
            }
            const unsigned int uiGray = pucScratch[iRow * iPitch + iCol]; // 0..64
            pucDst[iY * uiDstW + iX] = (unsigned char)(uiGray >= 64 ? 255 : uiGray * 4);
        }
    }
    return true;
}

}

bool IsRedrawn(const RedTexture* pPage)
{
    return pPage && pPage->m_format == D3DFMT_A8R8G8B8 && pPage->m_uiNumMipMaps > 1;
}

void RebaseGlyphs(RedFont::Glyph* aGlyphs, unsigned int uiNumGlyphs, RedTexture* const* apPages,
                  unsigned int uiNumPages)
{
    for (unsigned int i = 0; i < uiNumGlyphs; ++i)
    {
        RedFont::Glyph& g = aGlyphs[i];
        const RedTexture* pPage = g.texnum < uiNumPages ? apPages[g.texnum] : 0;
        if (!IsRedrawn(pPage))
        {
            continue;
        }
        const float fW = (float)(pPage->m_uiWidth >> (pPage->m_uiNumMipMaps - 1));
        const float fH = (float)(pPage->m_uiHeight >> (pPage->m_uiNumMipMaps - 1));
        const float fU = DisplayLogic::TexelInset(g.uLeft * fW) / fW;
        const float fV = DisplayLogic::TexelInset(g.vTop * fH) / fH;
        g.uLeft -= fU;
        g.uRight -= fU;
        g.vTop -= fV;
        g.vBottom -= fV;
    }
}

bool FindCapLine(RedTexture* const* apPages, unsigned int uiNumPages, const RedFont::Glyph* aGlyphs,
                 unsigned int uiNumGlyphs, CapLine& rCap)
{
    const unsigned short ausRef[2] = { 'H', 'A' };
    for (int r = 0; r < 2; ++r)
    {
        for (unsigned int i = 0; i < uiNumGlyphs; ++i)
        {
            const RedFont::Glyph& g = aGlyphs[i];
            RedTexture* pPage = g.texnum < uiNumPages ? apPages[g.texnum] : 0;
            if (g.value != ausRef[r] || !pPage || !pPage->m_pTexture)
            {
                continue;
            }
            const unsigned int uiLevel = IsRedrawn(pPage) ? pPage->m_uiNumMipMaps - 1 : 0;
            const unsigned int uiW = pPage->m_uiWidth >> uiLevel;
            const unsigned int uiH = pPage->m_uiHeight >> uiLevel;
            int iX, iY, iW, iH;
            if (uiW == 0 || uiH == 0 || uiW > 1024 || uiH > 1024 || !GlyphBox(g, uiW, uiH, iX, iY, iW, iH))
            {
                return false;
            }
            unsigned char* pucPage = new unsigned char[uiW * uiH];
            const bool bRead =
                ReadCoverage((IDirect3DTexture9*)pPage->m_pTexture, pPage->m_format, uiLevel, uiW, uiH, pucPage);
            const DisplayLogic::InkSpan kInk =
                bRead ? DisplayLogic::MeasureInkRows(pucPage + iY * uiW + iX, iW, iH, (int)uiW, 255)
                      : DisplayLogic::InkSpan();
            delete[] pucPage;
            if (!bRead || kInk.fWidth < 2.0f)
            {
                return false;
            }
            rCap.fHeight = kInk.fWidth;
            rCap.fBaseline = (float)g.top + kInk.fCentre + kInk.fWidth * 0.5f;
            return true;
        }
    }
    return false;
}

bool RedrawPage(RedTexture* pPage, const RedFont::Glyph* aGlyphs, unsigned int uiNumGlyphs,
                unsigned int uiPage, unsigned int uiK, const CapLine* pCap)
{
    IDirect3DDevice9* pDevice = RedRenderer::pcGetDevice();
    if (!pPage || !pPage->m_pTexture || !pDevice || uiK < 2 || pPage->m_uiWidth == 0 ||
        pPage->m_uiHeight == 0 || pPage->m_uiWidth > 1024 || pPage->m_uiHeight > 1024)
    {
        return false;
    }
    const unsigned int uiW = pPage->m_uiWidth;
    const unsigned int uiH = pPage->m_uiHeight;
    const unsigned int uiDstW = uiW * uiK;
    const unsigned int uiDstH = uiH * uiK;

    unsigned char* pucSrc = new unsigned char[uiW * uiH];
    if (!ReadCoverage((IDirect3DTexture9*)pPage->m_pTexture, pPage->m_format, 0, uiW, uiH, pucSrc))
    {
        delete[] pucSrc;
        return false;
    }
    unsigned char* pucDst = new unsigned char[uiDstW * uiDstH];
    Upscale(pucSrc, uiW, uiH, uiK, pucDst);

    // redraw the printable glyphs of this page with GDI
    int iDrawn = 0;
    float fStretchSum = 0.0f;
    if (pCap)
    {
        HDC hDC = CreateCompatibleDC(0);
        HFONT hFont = hDC ? MakeFont(hDC, pCap->fHeight * (float)uiK) : 0;
        if (hFont)
        {
            HGDIOBJ hOld = SelectObject(hDC, hFont);
            const unsigned int uiScratchSize = 256 * 256;
            unsigned char* pucScratch = new unsigned char[uiScratchSize];
            for (unsigned int i = 0; i < uiNumGlyphs; ++i)
            {
                const RedFont::Glyph& g = aGlyphs[i];
                float fStretch = 1.0f;
                const bool bPrintable = (g.value > 0x20 && g.value < 0x7F) || (g.value > 0xA0 && g.value <= 0xFF);
                if (g.texnum == uiPage && bPrintable &&
                    DrawGlyph(hDC, g, pucSrc, uiW, uiH, uiK, pCap->fBaseline, pucDst, pucScratch,
                              uiScratchSize, fStretch))
                {
                    ++iDrawn;
                    fStretchSum += fStretch;
                }
            }
            delete[] pucScratch;
            SelectObject(hDC, hOld);
            DeleteObject(hFont);
        }
        if (hDC)
        {
            DeleteDC(hDC);
        }
    }
    delete[] pucSrc;

    // the K x page with a mip chain down to 1x, white with the coverage as alpha
    unsigned int uiLevels = 1;
    while ((1u << (uiLevels - 1)) < uiK)
    {
        ++uiLevels;
    }
    IDirect3DTexture9* pTexture = 0;
    if (FAILED(pDevice->CreateTexture(uiDstW, uiDstH, uiLevels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                      &pTexture, 0)) ||
        !pTexture)
    {
        delete[] pucDst;
        return false;
    }
    unsigned char* pucLevel = pucDst;
    unsigned int uiLevelW = uiDstW;
    unsigned int uiLevelH = uiDstH;
    for (unsigned int uiLevel = 0; uiLevel < uiLevels; ++uiLevel)
    {
        if (uiLevel > 0)
        {
            const unsigned int uiNextW = uiLevelW / 2;
            const unsigned int uiNextH = uiLevelH / 2;
            unsigned char* pucNext = new unsigned char[uiNextW * uiNextH];
            for (unsigned int uiY = 0; uiY < uiNextH; ++uiY)
            {
                for (unsigned int uiX = 0; uiX < uiNextW; ++uiX)
                {
                    const unsigned char* p = pucLevel + (uiY * 2) * uiLevelW + uiX * 2;
                    pucNext[uiY * uiNextW + uiX] =
                        (unsigned char)((p[0] + p[1] + p[uiLevelW] + p[uiLevelW + 1] + 2) / 4);
                }
            }
            if (pucLevel != pucDst)
            {
                delete[] pucLevel;
            }
            pucLevel = pucNext;
            uiLevelW = uiNextW;
            uiLevelH = uiNextH;
        }
        D3DLOCKED_RECT kLocked;
        if (SUCCEEDED(pTexture->LockRect(uiLevel, &kLocked, 0, 0)))
        {
            for (unsigned int uiY = 0; uiY < uiLevelH; ++uiY)
            {
                unsigned int* puiRow = (unsigned int*)((unsigned char*)kLocked.pBits + uiY * kLocked.Pitch);
                for (unsigned int uiX = 0; uiX < uiLevelW; ++uiX)
                {
                    puiRow[uiX] = ((unsigned int)pucLevel[uiY * uiLevelW + uiX] << 24) | 0x00FFFFFFu;
                }
            }
            pTexture->UnlockRect(uiLevel);
        }
    }
    if (pucLevel != pucDst)
    {
        delete[] pucLevel;
    }
    delete[] pucDst;

    pPage->m_pTexture->Release();
    pPage->m_pTexture = pTexture;
    pPage->m_uiWidth = uiDstW;
    pPage->m_uiHeight = uiDstH;
    pPage->m_uiNumMipMaps = uiLevels;
    pPage->m_format = D3DFMT_A8R8G8B8;
    pPage->m_pool = D3DPOOL_MANAGED;
    pPage->m_sizeInBytes = uiDstW * uiDstH * 4;
    LOG(LogRender, Inform, "OPENSWBF2 CRISPFONT page %ux%u -> %ux%u glyphs=%d stretch=%.2f cap=%.2f baseline=%.2f", uiW,
        uiH, uiDstW, uiDstH, iDrawn, iDrawn ? fStretchSum / (float)iDrawn : 1.0f, pCap ? pCap->fHeight : 0.0f,
        pCap ? pCap->fBaseline : 0.0f);
    return true;
}

}
