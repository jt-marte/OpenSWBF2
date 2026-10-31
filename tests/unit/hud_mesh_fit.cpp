#include "third_party/doctest/doctest.h"

#include <stdio.h>
#include <string>

#include "Battlefront2/Source/HudMeshFit.h"

// OpenSWBF2: a HUD weapon slot showed a mod weapon's model (the Conversion Pack's) huge in the
// middle of the screen, because the slot has no MeshInfo for it. It now borrows the entry of the
// weapon's class or an ancestor, refitted to the model's size.

namespace
{

std::string ReadSource(const char* pcPath)
{
    std::string sOut;
    FILE* pFile = fopen(pcPath, "rb");
    REQUIRE(pFile);
    int c;
    while ((c = fgetc(pFile)) != EOF)
    {
        if (c != '\r')
        {
            sOut += (char)c;
        }
    }
    fclose(pFile);
    return sOut;
}

HudMeshFit::Box MakeBox(float x0, float y0, float z0, float x1, float y1, float z1)
{
    HudMeshFit::Box box = {{x0, y0, z0}, {x1, y1, z1}};
    return box;
}

void SetMatrix(float m[4][4], const float r0[3], const float r1[3], const float r2[3], float tx,
               float ty, float tz)
{
    const float* rows[3] = {r0, r1, r2};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            m[i][j] = rows[i][j];
        }
        m[i][3] = 0.0f;
    }
    m[3][0] = tx;
    m[3][1] = ty;
    m[3][2] = tz;
    m[3][3] = 1.0f;
}

// Where a model's box ends up before the group transform: the placed box moved by the
// translation the renderer uses (translation times fTranslateScale).
HudMeshFit::Box OnScreen(const HudMeshFit::Box& box, const float m[4][4], float fTranslateScale)
{
    HudMeshFit::Box out = HudMeshFit::Place(box, m);
    for (int j = 0; j < 3; ++j)
    {
        out.vMin[j] += m[3][j] * fTranslateScale;
        out.vMax[j] += m[3][j] * fTranslateScale;
    }
    return out;
}

} // namespace

TEST_CASE("HudMeshFit: a model fills the reference's box centred and keeps its proportions" *
          doctest::test_suite("unit"))
{
    const float r0[3] = {1.0f, 0.0f, 0.0f};
    const float r1[3] = {0.0f, 1.0f, 0.0f};
    const float r2[3] = {0.0f, 0.0f, 1.0f};
    float mRef[4][4];
    SetMatrix(mRef, r0, r1, r2, 100.0f, 200.0f, 0.0f);
    const HudMeshFit::Box refBox = MakeBox(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f);
    const HudMeshFit::Box box = MakeBox(0.0f, 0.0f, 0.0f, 4.0f, 1.0f, 1.0f);

    float mOut[4][4];
    REQUIRE(HudMeshFit::Fit(mRef, refBox, box, 0.01f, mOut));

    // 4 wide and 1 high into 2 x 2: the width limits it, so half size
    CHECK(mOut[0][0] == doctest::Approx(0.5f));
    CHECK(mOut[1][1] == doctest::Approx(0.5f));
    CHECK(mOut[2][2] == doctest::Approx(0.5f));
    CHECK(mOut[3][3] == doctest::Approx(1.0f));

    const HudMeshFit::Box ref = OnScreen(refBox, mRef, 0.01f);
    const HudMeshFit::Box own = OnScreen(box, mOut, 0.01f);
    CHECK(own.vMax[0] - own.vMin[0] == doctest::Approx(ref.vMax[0] - ref.vMin[0]));
    CHECK(own.vMax[1] - own.vMin[1] == doctest::Approx(0.5f));
    for (int j = 0; j < 3; ++j)
    {
        CHECK(own.vMin[j] + own.vMax[j] == doctest::Approx(ref.vMin[j] + ref.vMax[j]));
    }
}

TEST_CASE("HudMeshFit: a turned and scaled slot entry fits a shorter rifle into the stock one's place" *
          doctest::test_suite("unit"))
{
    // The stock HUD shows rifles side on: a quarter turn about y, scaled 0.8
    const float r0[3] = {0.0f, 0.0f, -0.8f};
    const float r1[3] = {0.0f, 0.8f, 0.0f};
    const float r2[3] = {0.8f, 0.0f, 0.0f};
    float mRef[4][4];
    SetMatrix(mRef, r0, r1, r2, -843.74f, 787.29f, 0.0f);
    // The stock rifle and the Conversion Pack's HUD rifle, both long in z
    const HudMeshFit::Box refBox = MakeBox(-0.082f, -0.134f, -0.521f, 0.050f, 0.185f, 1.212f);
    const HudMeshFit::Box box = MakeBox(-0.066f, -0.129f, -0.206f, 0.036f, 0.107f, 0.545f);
    const float fTranslateScale = 640.0f / 3840.0f * 0.01f;

    float mOut[4][4];
    REQUIRE(HudMeshFit::Fit(mRef, refBox, box, fTranslateScale, mOut));

    // It's chunkier than the stock rifle, so its height decides: as tall as the stock one, not
    // as long, so it stays clear of the rows above and below
    const HudMeshFit::Box ref = OnScreen(refBox, mRef, fTranslateScale);
    const HudMeshFit::Box own = OnScreen(box, mOut, fTranslateScale);
    CHECK(own.vMax[1] - own.vMin[1] == doctest::Approx(ref.vMax[1] - ref.vMin[1]));
    CHECK(own.vMax[0] - own.vMin[0] < ref.vMax[0] - ref.vMin[0]);
    CHECK(own.vMax[0] - own.vMin[0] > 0.5f * (ref.vMax[0] - ref.vMin[0]));
    for (int j = 0; j < 3; ++j)
    {
        CHECK(own.vMin[j] + own.vMax[j] == doctest::Approx(ref.vMin[j] + ref.vMax[j]).epsilon(1e-4));
    }
    // Same turn, larger: the rows keep their direction
    CHECK(mOut[0][2] < -0.8f);
    CHECK(mOut[0][0] == doctest::Approx(0.0f));
}

TEST_CASE("HudMeshFit: the model's y is mirrored as the renderer does" * doctest::test_suite("unit"))
{
    const float r0[3] = {1.0f, 0.0f, 0.0f};
    const float r1[3] = {0.0f, 1.0f, 0.0f};
    const float r2[3] = {0.0f, 0.0f, 1.0f};
    float m[4][4];
    SetMatrix(m, r0, r1, r2, 0.0f, 0.0f, 0.0f);
    const HudMeshFit::Box placed = HudMeshFit::Place(MakeBox(0.0f, 1.0f, 0.0f, 1.0f, 3.0f, 1.0f), m);
    CHECK(placed.vMin[1] == doctest::Approx(-3.0f));
    CHECK(placed.vMax[1] == doctest::Approx(-1.0f));
}

TEST_CASE("HudMeshFit: a flat model fits on the axis it has; unusable input changes nothing" *
          doctest::test_suite("unit"))
{
    const float r0[3] = {1.0f, 0.0f, 0.0f};
    const float r1[3] = {0.0f, 1.0f, 0.0f};
    const float r2[3] = {0.0f, 0.0f, 1.0f};
    float mRef[4][4];
    SetMatrix(mRef, r0, r1, r2, 0.0f, 0.0f, 0.0f);
    const HudMeshFit::Box refBox = MakeBox(-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f);

    float mOut[4][4];
    REQUIRE(HudMeshFit::Fit(mRef, refBox, MakeBox(-0.5f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f), 1.0f, mOut));
    CHECK(mOut[0][0] == doctest::Approx(2.0f));

    float mKeep[4][4] = {{7.0f}};
    CHECK_FALSE(HudMeshFit::Fit(mRef, refBox, MakeBox(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f), 1.0f, mKeep));
    CHECK_FALSE(HudMeshFit::Fit(mRef, refBox, MakeBox(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f), 1.0f, mKeep));
    CHECK_FALSE(HudMeshFit::Fit(mRef, refBox, refBox, 0.0f, mKeep));
    CHECK(mKeep[0][0] == 7.0f);
}

TEST_CASE("HudMeshFit: the weapon slots use the fallback" * doctest::test_suite("regression"))
{
    const std::string sModel = ReadSource("../src/Battlefront2/Source/HUDElementModel3D.cpp");
    const std::string sEvents = ReadSource("../src/Battlefront2/Source/HUDGameEvents.cpp");

    // A model without its own entry tries the fallback before the default matrix
    CHECK(sModel.find("if (pMeshInfo == &mDefault && SetFallbackMesh(pModel))") != std::string::npos);
    // ...but a mod overlay that hides unlisted models stays hidden
    CHECK(sModel.find("bDefaultHidden") != std::string::npos);
    CHECK(sModel.find("HudMeshFit::Fit(") != std::string::npos);

    // The candidates are set before the slot gets the model
    const size_t uSet = sEvents.find("SetWeaponMeshFallbacks(uWeaponHash, pWeaponClass);");
    const size_t uSend = sEvents.find("InitializeEventWithData(&change, weaponEvents.change, payload)->Send();");
    REQUIRE(uSet != std::string::npos);
    REQUIRE(uSend != std::string::npos);
    CHECK(uSet < uSend);
    // The class itself, then its ancestors
    CHECK(sEvents.find("auCandidate[0] = p->mId;") != std::string::npos);
    CHECK(sEvents.find("p = p->mParent") != std::string::npos);
}
