#pragma once

#include "RedEngineFL/Interface/RedFont.h"

class RedTexture;

// OpenSWBF2: sharp text above 1080p. The game's fonts are small bitmap atlases
// (13 to 22 px tall) magnified 2x in game and 3.6x in the shell at 4K. Their
// glyph boxes are stored as fractions of the atlas and text layout only uses
// the glyph metrics, so an atlas page can be swapped for one K times the size
// with every glyph redrawn in the same box - sharper, with identical layout.
// Glyphs are drawn with GDI in Arial Black (the face the atlases were made
// from) at the atlas's cap height, each as wide as the atlas glyph, on its
// ink's middle and the font's baseline; anything GDI can't draw keeps a
// smooth upscale of the original. A mip chain covers every UI scale up to K.
// The sizes and placement are DisplayLogic::CrispFactor / CrispStretch /
// CrispPlace.
namespace CrispFont
{

// A font's capitals at 1x: how tall their ink is and how far below the line
// top its bottom (the baseline) is.
struct CapLine
{
    float fHeight;
    float fBaseline;
};

// Measures the cap line from the ink of the font's 'H' (or 'A') on its page
// (apPages, the font's pages). False if there is none or it can't be read.
bool FindCapLine(RedTexture* const* apPages, unsigned int uiNumPages, const RedFont::Glyph* aGlyphs,
                 unsigned int uiNumGlyphs, CapLine& rCap);

// Redraws page uiPage of a font (its glyphs aGlyphs) at uiK, in place; with
// no cap line it is only upscaled. Returns false (page untouched) if the page
// can't be read or made.
bool RedrawPage(RedTexture* pPage, const RedFont::Glyph* aGlyphs, unsigned int uiNumGlyphs,
                unsigned int uiPage, unsigned int uiK, const CapLine* pCap);

// A redrawn page (the only atlases with a mip chain; the last level is 1x).
bool IsRedrawn(const RedTexture* pPage);

// The atlases' UVs sit half a texel in, on texel centres, which D3D9 maps to
// pixel centres when the 1x page is drawn 1:1. Redrawn pages are drawn with
// D3D9's half-pixel offset instead (RedGlyphCacheElement::RenderUsingContext),
// exact at every whole-number scale, so their glyphs' UVs move to the texel
// edges. Call once per font, after its pages are redrawn.
void RebaseGlyphs(RedFont::Glyph* aGlyphs, unsigned int uiNumGlyphs, RedTexture* const* apPages,
                  unsigned int uiNumPages);

}
