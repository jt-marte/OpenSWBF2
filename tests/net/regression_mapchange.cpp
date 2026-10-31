#include "third_party/doctest/doctest.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "testutil.h"
#include "nettest.h"

// OpenSWBF2: a dedicated server crashed in NetPktGroup::PopFront a couple of seconds into the map
// after an rcon /restart. The late-state ring had grown to 32 slots but the map-change cleanup
// still freed 8, so the rest kept links into a packet pool the next mission rebuilt.

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

} // namespace

TEST_CASE("a packet group whose count runs past its list is dropped and not followed" *
          doctest::test_suite("regression"))
{
    ExpectLog expect("OPENSWBF2 NET packet group corrupt");
    NetTestReset();
    LogCapture capture;

    unsigned char acPayload[4] = {1, 2, 3, 4};
    NetPktGroup group;
    group.Reset();
    group.PushBack(NetMakePacket(acPayload, 4));
    group.pktCount = 3; // what a stale group looks like after the pool was rebuilt

    CHECK(!group.PopFront());
    CHECK(group.pktCount == 0);
    CHECK(group.firstPkt == -1);
    CHECK(group.lastPkt == -1);
    CHECK(capture.Contains("OPENSWBF2 NET packet group corrupt"));
}

TEST_CASE("a new packet group starts empty" * doctest::test_suite("regression"))
{
    NetPktGroup group;
    CHECK(group.pktCount == 0);
    CHECK(group.firstPkt == -1);
    CHECK(group.lastPkt == -1);
    CHECK(!group.PopFront());
}

TEST_CASE("a map change clears the whole late-state ring" * doctest::test_suite("regression"))
{
    const std::string sNet = ReadSource("../src/Battlefront2/Source/NetGame.cpp");

    // Both cleanups cover every slot, and nothing hard-codes retail's 8 any more
    CHECK(sNet.find("for (unsigned int i = 0; i < kLateRing; i++)\n    {\n        lateState[i].Free();") !=
          std::string::npos);
    CHECK(sNet.find("for (unsigned int i = 0; i < kLateRing; ++i)\n        {\n            lateState[i].Free();") !=
          std::string::npos);
    CHECK(sNet.find("i < 8; i++)\n    {\n        lateState") == std::string::npos);
    CHECK(sNet.find("i < 8; ++i)\n        {\n            lateState") == std::string::npos);

    // ...and whatever survived is forgotten right after the pool is rebuilt
    const size_t uCreate = sNet.find("NetComm::Create(bMission);");
    const size_t uForget = sNet.find("lateState[i].Reset();");
    REQUIRE(uCreate != std::string::npos);
    REQUIRE(uForget != std::string::npos);
    CHECK(uCreate < uForget);
    CHECK(uForget < sNet.find("NetSetup::Create(bMission);"));
}

TEST_CASE("an admin keeps its session and pushes across a map change" * doctest::test_suite("regression"))
{
    const std::string sAdmin = ReadSource("../src/Battlefront2/Source/NetAdmin.cpp");

    // The clock restarts each mission: times from the last map count as just now, instead of
    // retail's dropping the session
    CHECK(sAdmin.find("static void RebaseAdminTimes(float fNow)") != std::string::npos);
    CHECK(sAdmin.find("const float fNow = NetComm::GetTime();\n    RebaseAdminTimes(fNow);") != std::string::npos);
    CHECK(sAdmin.find("admin.lastRecvTime = 0.0f;") == std::string::npos);

    // /restart replays the running map's own entry
    const std::string sList = ReadSource("../src/Battlefront2/Source/MissionPlayList.cpp");
    CHECK(sList.find("sPlaylistPosn = siCurrentEntry;") != std::string::npos);

    // Score pushes name the teams for maps from the command line or rcon too
    CHECK(sAdmin.find("pusTeam1 = Team::sTeams[1]->GetName();") != std::string::npos);
}

TEST_CASE("nobody keeps a deleted vehicle and bots made from players get the right team" *
          doctest::test_suite("regression"))
{
    // A dedicated server crashed in Character::Update a few seconds after a map change: four bot
    // gunners kept their multi-seat vehicle after it was deleted
    const std::string sCtl = ReadSource("../src/Battlefront2/Source/Controllable.cpp");
    const size_t uDtor = sCtl.find("Controllable::~Controllable()");
    REQUIRE(uDtor != std::string::npos);
    CHECK(sCtl.find("Character::ForgetControllable(this);", uDtor) < sCtl.find("void Controllable::Kill()"));

    const std::string sChar = ReadSource("../src/Battlefront2/Source/Character.cpp");
    CHECK(sChar.find("void Character::ForgetControllable(const Controllable* pControllable)") != std::string::npos);
    // MakeAI picks the team from the index in elements, not Win32's 432-byte stride
    CHECK(sChar.find("const int iIndex = pBase ? (int)(this - pBase)") != std::string::npos);

    // A script mod's skin naming a low-res model the map didn't load keeps the old one (it
    // crashed a dedicated server loading nab2c_con)
    const std::string sSoldier = ReadSource("../src/Battlefront2/Source/EntitySoldier.cpp");
    CHECK(sSoldier.find("if (pLowRes)\n        {\n            mModelLowRes = pLowRes;") != std::string::npos);
    CHECK(sSoldier.find("mModelLowRes ? mModelLowRes->GetRedModel(0) : 0") != std::string::npos);
}

TEST_CASE("a soldier addon without a loaded model is not drawn" * doctest::test_suite("regression"))
{
    // A client crashed drawing a soldier on geo1c_con: the user's skin changer had pointed an
    // animated addon at a model the map didn't load
    const std::string sAddon = ReadSource("../src/Battlefront2/Source/AnimatedAddon.cpp");
    CHECK(sAddon.find("if (pModel)\n        {\n            mModel = pModel;") != std::string::npos);
    CHECK(sAddon.find("if (!pJoint || !mModel || !mModel->GetRedModel(0))") != std::string::npos);
    CHECK(sAddon.find("if (mModel && mModel->GetRedModel(0))") != std::string::npos);

    // The droideka's low-res models the same way as the soldier's
    const std::string sDroideka = ReadSource("../src/Battlefront2/Source/EntityDroideka.cpp");
    CHECK(sDroideka.find("mUprightLowRes = pLowRes;") != std::string::npos);
    CHECK(sDroideka.find("mBallLowRes = pLowRes;") != std::string::npos);
    CHECK(sDroideka.find("mUprightLowRes = const_cast<GameModel*>(GameModel::Find(") == std::string::npos);

    // An animated prop's skeleton reads its pose through a set pointer: headless games (every
    // dedicated server) crashed on tat3 (Jabba's Palace)
    const std::string sProp = ReadSource("../src/Battlefront2/Source/EntityPropAnimated.cpp");
    CHECK(sProp.find("mZephyrSkeleton.Open(*mAnimObj->GetZephyrSkeletonShared(), mZephyrPoseStatic);") != std::string::npos);
    const std::string sSkel = ReadSource("../src/Zephyr/Source/ZephyrSkeleton.cpp");
    CHECK(sSkel.find("if (!m_pPoseFinal || !m_pShared)") != std::string::npos);

    // With auto-assign on, both team rows on team select stay readable (one looked blank)
    const std::string sSide = ReadSource("../data/scripts/ingame/ifs_sideselect.lua");
    CHECK(sSide.find("IFFlashyText_fnSetTextColor(button.label, 150, 150, 150)") != std::string::npos);
}

TEST_CASE("BF1 pack Bespin maps load and play on server and client" * doctest::test_suite("regression"))
{
    // Cloud City Hero Assault crashed loading: a chunk property found no chunk of its own class
    const std::string sGeo = ReadSource("../src/Battlefront2/Source/EntityGeometry.cpp");
    CHECK(sGeo.find("ChunkDescriptor* pChunk = (unsigned int)sCurrentChunk <= 9 ? mChunkDescriptors[sCurrentChunk] : 0;") !=
          std::string::npos);
    CHECK(sGeo.find("mChunkDescriptors[sCurrentChunk]->mSmokeEffect") == std::string::npos);
    CHECK(sGeo.find("mChunkDescriptors[sCurrentChunk]->mKeepSoldierCollisions") == std::string::npos);

    // A client crashed on a door update for a door it doesn't have (Platforms Classic Conquest)
    const std::string sNet = ReadSource("../src/Battlefront2/Source/NetGame.cpp");
    CHECK(sNet.find("EntityDoor* door = EntityDoor::Find(event.u.updateDoorState.mHashID);\n    if (door)") !=
          std::string::npos);

    // A melee weapon's "no animation" index 255 reads as retail's flat Win32 table did, and as
    // nothing past it (Platforms Hero Assault crashed server and clients)
    const std::string sAnim = ReadSource("../src/Battlefront2/Source/SoldierAnimatorClass.cpp");
    CHECK(sAnim.find("const SoldierAnimation* SoldierAnimatorClass::AnimationSlot(int iMap, int iSlot) const") !=
          std::string::npos);
    CHECK(sAnim.find("return mMap[eMap].mAnimation[i - NUM_WEAPON_ANIMATIONS_TOTAL];") == std::string::npos);
    CHECK(sAnim.find("const int iFlat = iMap * ANIMATION_MAP_SIZE + iSlot;") != std::string::npos);
}

TEST_CASE("BF1 pack Rhen Var and AT4 load and play on server and client" * doctest::test_suite("regression"))
{
    // Every client crashed joining Rhen Var: Harbor: an armed building with an empty turret slot
    const std::string sNet = ReadSource("../src/Battlefront2/Source/NetState.cpp");
    const size_t uMove = sNet.find("void NetEntityBuildingArmed::MoveToClient()");
    REQUIRE(uMove != std::string::npos);
    CHECK(sNet.find("Controllable* control = mTurret[i];\n        if (!control)", uMove) <
          sNet.find("void NetCommandBuilding::MoveToClient()"));
    const std::string sArmed = ReadSource("../src/Battlefront2/Source/EntityBuildingArmed.cpp");
    CHECK(sArmed.find("    {\n        mTurret[i]->Kill();") == std::string::npos);
    const std::string sAnimArmed = ReadSource("../src/Battlefront2/Source/EntityBuildingAnimatedArmed.cpp");
    CHECK(sAnimArmed.find("    {\n        mTurret[i]->Kill();") == std::string::npos);

    // A dedicated server crashed spawning AT4's walker, whose animation never loaded
    const std::string sWalker = ReadSource("../src/Battlefront2/Source/EntityWalker.cpp");
    const size_t uBuild = sWalker.find("Entity* EntityWalkerClass::Build(const EntityDesc& desc)");
    REQUIRE(uBuild != std::string::npos);
    CHECK(sWalker.find("if (!m_pZephyrSkeletonShared)", uBuild) < sWalker.find("new EntityWalker(this, desc)", uBuild));
    CHECK(sWalker.find("RedAnimation::Find(PblHash(pszValue))->GetZephyrSkeletonShared()") == std::string::npos);

    // A server crashed on Naboo Holocron: a point off the terrain read outside its patch table
    const std::string sTerrain = ReadSource("../src/RedEngineFL/Application/RedTerrain.cpp");
    const size_t uHole = sTerrain.find("bool RedTerrain::PointInHole(const PblVector3& vPos)");
    REQUIRE(uHole != std::string::npos);
    CHECK(sTerrain.find("if (iX < 0 || iZ < 0 || iX >= _iPointsPerDimension || iZ >= _iPointsPerDimension)", uHole) <
          sTerrain.find("_pHeightExpand[iPatch].uiFlags", uHole));

    // Utapau Hunt in the Galactic Civil War is listed by the pack but doesn't exist
    const std::string sList = ReadSource("../data/scripts/shell/missionlist.lua");
    CHECK(sList.find("{ map = \"uta1\", key = \"mode_hunt_g\" }") != std::string::npos);
    CHECK(sList.find("missionlist_DropMissing(mp_missionselect_listbox_contents)") != std::string::npos);
}
