#include "SimDigest.h"

#include "AIGoalManager.h"
#include "Character.h"
#include "CommandLine.h"
#include "CommandPost.h"
#include "Controllable.h"
#include "Damageable.h"
#include "Entity.h"
#include "EntityGeometry.h"
#include "Explosion.h"
#include "Factory.h"
#include "GameLoop.h"
#include "LocalLimits.h"
#include "GameObject.h"
#include "Ordnance.h"
#include "SimDigestCore.h"
#include "Team.h"
#include "Weapon.h"
#include "PebbleFL/Common/PblHashTable.h"
#include "PebbleFL/Common/PblRandom.h"
#include "RedEngineFL/Util/RedLog.h"

static unsigned int suTraceTurn;
static bool sbFactoriesLogged;

static unsigned int AddVector(unsigned int uHash, const PblVector3& vValue)
{
    uHash = SimDigestCore::AddFloat(uHash, vValue.x);
    uHash = SimDigestCore::AddFloat(uHash, vValue.y);
    return SimDigestCore::AddFloat(uHash, vValue.z);
}

static unsigned int AddObject(unsigned int uHash, const GameObject* pObject)
{
    if (!pObject)
    {
        return SimDigestCore::AddU32(uHash, 0);
    }
    uHash = SimDigestCore::AddU32(uHash, 1);
    uHash = AddVector(uHash, pObject->GetPosition());
    uHash = AddVector(uHash, pObject->GetVelocity());
    uHash = SimDigestCore::AddFloat(uHash, pObject->mCurHealth);
    return SimDigestCore::AddInt(uHash, pObject->GetTeam());
}

static unsigned int AddCharacters(unsigned int uHash)
{
    int iLimit = Character::sCharacters ? Character::sCharacterLimit : 0;
    uHash = SimDigestCore::AddInt(uHash, iLimit);
    for (int i = 0; i < iLimit; ++i)
    {
        const Character& kCharacter = Character::sCharacters[i];
        uHash = SimDigestCore::AddInt(uHash, kCharacter.mTeamNumber);
        uHash = SimDigestCore::AddInt(uHash, kCharacter.mClassIndex);
        const Controllable* pControllable = kCharacter.GetControllable();
        uHash = AddObject(uHash, pControllable ? pControllable->GetGameObject() : 0);
    }
    return uHash;
}

static unsigned int AddTeams(unsigned int uHash)
{
    int iCount = Team::sTeams ? Team::GetCount() : 0;
    uHash = SimDigestCore::AddInt(uHash, iCount);
    for (int i = 0; i < iCount; ++i)
    {
        const Team* pTeam = Team::sTeams[i];
        if (!pTeam)
        {
            uHash = SimDigestCore::AddU32(uHash, 0);
            continue;
        }
        uHash = SimDigestCore::AddU32(uHash, 1);
        uHash = SimDigestCore::AddInt(uHash, pTeam->GetPoints());
        uHash = SimDigestCore::AddInt(uHash, pTeam->GetCurReinforcements());
    }
    return uHash;
}

static unsigned int AddPosts(unsigned int uHash)
{
    int iCount = CommandPost::sPostCount ? *CommandPost::sPostCount : 0;
    uHash = SimDigestCore::AddInt(uHash, iCount);
    for (int i = 0; i < iCount && i < LocalLimits::kMaxPosts; ++i)
    {
        const CommandPost* pPost = CommandPost::FindPostByIndex(i);
        if (!pPost)
        {
            uHash = SimDigestCore::AddU32(uHash, 0);
            continue;
        }
        uHash = SimDigestCore::AddU32(uHash, 1);
        uHash = SimDigestCore::AddInt(uHash, pPost->GetIndex());
        const GameObject* pObject = pPost->GetGameObject();
        uHash = SimDigestCore::AddInt(uHash, pObject ? pObject->GetTeam() : -1);
        uHash = SimDigestCore::AddFloat(uHash, pPost->mCaptureTimer);
        uHash = SimDigestCore::AddFloat(uHash, pPost->mNeutralizeTimer);
    }
    return SimDigestCore::AddInt(uHash, CommandPost::GetCount());
}

static unsigned int AddGoals(unsigned int uHash)
{
    const AI::AIGoalManager* pManager = AI::AIGoalManager::Instance();
    if (!pManager)
    {
        return SimDigestCore::AddU32(uHash, 0);
    }
    int iCount = 0;
    for (int i = 0; i < AI::AIGoalManager::MAX_GOALS; ++i)
    {
        if (pManager->mGoals[i])
        {
            ++iCount;
        }
    }
    uHash = SimDigestCore::AddInt(uHash, iCount);
    for (int i = 0; i < AI::AIGoalManager::MAX_GOALS; ++i)
    {
        if (pManager->mGoals[i])
        {
            uHash = SimDigestCore::AddInt(uHash, i);
        }
    }
    return uHash;
}

static unsigned int AddWorldObjects(unsigned int uHash)
{
    const PblHashTable<EntityEx, 1024>& kMap = EntityEx::GetEntityMap();
    int iVisited = 0;
    for (int i = 0; i < 1024; ++i)
    {
        unsigned int uKey = kMap._table[i];
        if (!uKey)
        {
            continue;
        }
        const EntityEx* pEntity = (const EntityEx*)kMap._table[1024 + i];
        if (!pEntity || !pEntity->IsRtti(GameObject::GetRtti()))
        {
            continue;
        }
        ++iVisited;
        uHash = SimDigestCore::AddU32(uHash, uKey);
        uHash = AddObject(uHash, static_cast<const GameObject*>(pEntity));
    }
    return SimDigestCore::AddInt(uHash, iVisited);
}

unsigned int SimDigest::StateHash()
{
    unsigned int uHash = SimDigestCore::kOffsetBasis;
    uHash = SimDigestCore::AddFloat(uHash, GameLoop::GetMissionTime());
    uHash = AddCharacters(uHash);
    uHash = AddTeams(uHash);
    uHash = AddPosts(uHash);
    uHash = AddGoals(uHash);
    uHash = AddWorldObjects(uHash);
    return uHash;
}

void SimDigest::TraceTurn()
{
    ++suTraceTurn;
    if (gDigestTrace > 0 && suTraceTurn <= (unsigned int)gDigestTrace)
    {
        LOG(LogSim, Inform, "OPENSWBF2 DIGEST TRACE turn=%u state=0x%08X rand=%u/%u", suTraceTurn, StateHash(),
            PblRandom::_Global._uiSeed, PblRandom::_Sim._uiSeed);
    }
}

template <class TYPE, class CLASS, class DESC>
static void LogFactoryList(const char* pcTag)
{
    for (typename PblList<Factory<TYPE, CLASS, DESC> >::Itor it(Factory<TYPE, CLASS, DESC>::sList); it; ++it)
    {
        const Factory<TYPE, CLASS, DESC>* pFactory = it;
        unsigned int uParent = pFactory->mParent ? pFactory->mParent->mId : 0;
        LOG(LogSim, Inform, "OPENSWBF2 FACTORY %s idx=%d id=0x%08X parent=0x%08X", pcTag, pFactory->mNetIndex,
            pFactory->mId, uParent);
    }
}

void SimDigest::Reset()
{
    suTraceTurn = 0;
    sbFactoriesLogged = false;
}

void SimDigest::LogFactories()
{
    if (!gFactoryLog || sbFactoriesLogged)
    {
        return;
    }
    sbFactoriesLogged = true;
    LogFactoryList<Entity, EntityClass, EntityDesc>("entity");
    LogFactoryList<Ordnance, OrdnanceClass, OrdnanceDesc>("ordnance");
    LogFactoryList<Weapon, WeaponClass, WeaponDesc>("weapon");
    LogFactoryList<Ordnance, ExplosionClass, OrdnanceDesc>("explosion");
    LogFactoryCounts();
}

void SimDigest::LogFactoryCounts()
{
    if (!gFactoryLog)
    {
        return;
    }
    LOG(LogSim, Inform, "OPENSWBF2 FACTORY COUNTS entity=%u ordnance=%u weapon=%u explosion=%u",
        Factory<Entity, EntityClass, EntityDesc>::sCounter, Factory<Ordnance, OrdnanceClass, OrdnanceDesc>::sCounter,
        Factory<Weapon, WeaponClass, WeaponDesc>::sCounter, Factory<Ordnance, ExplosionClass, OrdnanceDesc>::sCounter);
}
