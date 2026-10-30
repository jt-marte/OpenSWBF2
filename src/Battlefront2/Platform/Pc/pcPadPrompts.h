#pragma once

// OpenSWBF2: Xbox-style button icons for the pad's button prompts
// (XInputPad::PadGlyph), drawn at startup into textures the SpecialFont
// glyphs XInputPad::kGlyphBase + n use (FLInputManager::InitCustomFontData).
namespace PadPrompts
{

// Texture name hash of the icon for a glyph.
unsigned int IconHash(int iGlyph);

// Draws the icons; call once the device exists, before RedFont::PostCommonInit.
void CreateIcons();

}
