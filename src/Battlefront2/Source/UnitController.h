#pragma once

#include <stddef.h>

#include "PebbleFL/Common/PblRawStorage.h"

#include "Controller.h"
#include "CommandFsm.h"
#include "UnitCmdInfo.h"
#include "PebbleFL/Common/PblHandle.h"
#include "PebbleFL/Common/PblVector.h"
#include "RedEngineFL/Memory/RedMemoryPool.h"

class UnitAgent;
class AILowLevel;
class Character;
class GameObject;
namespace AI { class EventInfo; }
namespace SquadCommands { class SquadCommandOrder; }

class UnitController : public Controller, public CommandFsm, public PblHandled
{
public:

    enum
    {
        STATE_WAIT = 1,
        STATE_COMBAT = 2,
        STATE_WAIT_SECONDARY = 3,
        STATE_BOARD = 4,
        STATE_UNBOARD = 5,
        STATE_FLY = 6,
        STATE_REPAIR = 7,
        STATE_RANDOM = 8,
        STATE_CAPTURECP = 9,
        STATE_CTFOFFENSE = 10,
        STATE_DEATHMATCH = 11,
        STATE_DEFEND = 12,
        STATE_DESTROY = 13,
        STATE_SC_FOLLOW = 14,
        STATE_SC_HOLD = 15,
        STATE_FINAL = 16
    };

    UnitController(Controllable* pOwner);

    virtual bool IsRtti(unsigned int uRtti) const;
    virtual unsigned int GetDerivedRtti() const;
    virtual const char* GetDerivedRttiName() const;
    virtual void KillController();

    int SendEvent(AI::EventInfo* pInfo);
    int SendEvent(AI::EventInfo* pInfo) const;

    float GetUpdateRate() const;
    void UpdateHighLevel();
    void UpdateLowLevel(float dt);

    void AddFriendlyFire(const GameObject* pObject);

    void ResetFriendlyList();

    int GetFriendlyFireCount() const;
    const GameObject* GetFriendlyFire(int iIndex);
    bool IsInCombat() const;
    bool IsAgentDone();
    CmdInfo CopyCurCmd() const;
    const SquadCommands::SquadCommandOrder& GetLastSquadCommand();
    const GameObject* GetAttackHelperTarget() const;
    const GameObject* GetAttackHelperVisibleTarget() const;
    const PblVector3& GetAttackHelperTargetLastSeenPosition() const;
    int GetRandomSeed() const;
    void SetRandomSeed(int iSeed);
    void SetLastShotDist(float fDistSq);
    bool IsTargetLOSPossiblyBlocked();
    void EnterCombat();

    void ForceSetAttackHelperTarget(const GameObject* pTarget);

    bool IsLookAheadOnly() const;
    const PblVector3& GetOriginalAheadDir() const;

    static void operator delete(void* pBlock) { sMemoryPool.Free(pBlock); }

    static void* operator new(size_t uSize) { return sMemoryPool.Allocate(uSize); }
    static MemoryPool sMemoryPool;

    static unsigned int GetRtti() { return rttiHashUnitController; }
    static PblHash rttiHashUnitController;
    static char rttiNameUnitController[];

    AILowLevel& GetLowLevel() { return *reinterpret_cast<AILowLevel*>(mLowLevel); }
    const AILowLevel& GetLowLevel() const { return *reinterpret_cast<const AILowLevel*>(mLowLevel); }

protected:
    UnitController();
    virtual ~UnitController();

    virtual void EnterState();
    virtual void ExitState();
    virtual void UpdateState(float dt);

    const UnitCmdInfo* GetCurCmd() const;

    void DeleteAgent();
    void UpdateInvisibility();

    void UpdateLodState();
    void UseBuffs();
    void UseAutoTurrets();

public:
    PBL_RAW_ALIGN unsigned char mManagerNode[PBL_RAW_SIZE(20, 40)];
    float mNextUpdateTime;
    float mLastUpdateTime;
    float mUpdateRate;
    char mLowLevelUpdateSkips;
    float mLowLevelUpdateSkipDt;
    PBL_RAW_ALIGN unsigned char mThreatManager[PBL_RAW_SIZE(204, 256)];
    UnitAgent* mAgent;
    PBL_RAW_ALIGN unsigned char mLowLevel[PBL_RAW_SIZE(232, 296)];
    int mLodState;
    Character* mLodHumanPlayer;
    float mLodStateUpdateTime;
    PBL_RAW_ALIGN unsigned char mLastSquadCommand[PBL_RAW_SIZE(20, 32)];
    bool mJustEnteredVehicleGoToPickupAgent;
    bool mLookAheadOnly;
    PblVector3 mOriginalAheadDir;
    int mSeed;
    float mCrowdCheckTime;
    int mFriendlyCount;
    PBL_RAW_ALIGN unsigned char mFriendly[PBL_RAW_SIZE(64, 128)]; // 8 PblHandle<const GameObject>
    bool mSkip;
    int mVisionSkip;
    float mLastShotDistSq;
    PblVector3 mLastShotDistPos;
    float mLastShotDistTime;
};

UnitController* UnitControllerPtr(Controller* pController);
const UnitController* UnitControllerPtr(const Controller* pController);
UnitController* UnitControllerPtr(Controllable* pControllable);
const UnitController* UnitControllerPtr(const Controllable* pControllable);

 
 
 
 
 
 
 
 
