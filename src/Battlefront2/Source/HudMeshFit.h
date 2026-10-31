#pragma once

// OpenSWBF2: a HUD weapon slot places each weapon model with that model's own MeshInfo
// (rotation, scale, position). A mod weapon whose HUDTag names a model the stock HUD doesn't
// list, like the Conversion Pack's, fell back to the slot's default matrix and was drawn huge in
// the middle of the screen. Such a model now borrows the MeshInfo of its weapon class or an
// ancestor, refitted so its bounding box fills the screen area that entry gives its own model.
// No engine dependencies, so the unit tests can call it.
namespace HudMeshFit
{
struct Box
{
    float vMin[3];
    float vMax[3];
};

inline bool IsEmpty(const Box& box)
{
    return !(box.vMin[0] <= box.vMax[0] && box.vMin[1] <= box.vMax[1] && box.vMin[2] <= box.vMax[2]);
}

// The box a model takes up in the slot: its corners through the 3x3 part of a row-vector matrix
// (v * m), with the model's y mirrored first as Red3DModelElementLite::LocalToWorld does.
inline Box Place(const Box& box, const float m[4][4])
{
    Box out;
    for (int j = 0; j < 3; ++j)
    {
        out.vMin[j] = out.vMax[j] = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float fLo = i == 1 ? -box.vMax[i] : box.vMin[i];
            const float fHi = i == 1 ? -box.vMin[i] : box.vMax[i];
            const float a = fLo * m[i][j];
            const float b = fHi * m[i][j];
            out.vMin[j] += a < b ? a : b;
            out.vMax[j] += a < b ? b : a;
        }
    }
    return out;
}

// The matrix that puts a model with box `box` where mRef puts a model with box `refBox`: the same
// rotation, scaled evenly to fit inside the reference's area on both screen axes (x and y), with
// the centres matched. The renderer multiplies a HUD model's translation by fTranslateScale (the
// depth multiplier times the depth), so a model-space offset d moves the translation by
// d / fTranslateScale. Returns false, leaving mOut alone, when a box or the scale can't be used.
inline bool Fit(const float mRef[4][4], const Box& refBox, const Box& box, float fTranslateScale,
                float mOut[4][4])
{
    if (IsEmpty(refBox) || IsEmpty(box) || !(fTranslateScale > 0.0f))
    {
        return false;
    }

    const Box ref = Place(refBox, mRef);
    const Box own = Place(box, mRef);
    float fScale = 0.0f;
    for (int j = 0; j < 2; ++j)
    {
        const float fOwn = own.vMax[j] - own.vMin[j];
        if (fOwn > 1e-6f)
        {
            const float fAxis = (ref.vMax[j] - ref.vMin[j]) / fOwn;
            fScale = fScale == 0.0f || fAxis < fScale ? fAxis : fScale;
        }
    }
    if (!(fScale > 0.0f))
    {
        return false;
    }

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            mOut[i][j] = mRef[i][j] * fScale;
        }
        mOut[i][3] = mRef[i][3];
    }
    for (int j = 0; j < 3; ++j)
    {
        const float fRefCentre = (ref.vMin[j] + ref.vMax[j]) * 0.5f;
        const float fOwnCentre = (own.vMin[j] + own.vMax[j]) * 0.5f * fScale;
        mOut[3][j] = mRef[3][j] + (fRefCentre - fOwnCentre) / fTranslateScale;
    }
    mOut[3][3] = mRef[3][3];
    return true;
}
}
