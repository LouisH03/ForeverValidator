#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/cmw_nod.h"
#include "engine/core/engine_types.h"
#include "engine/core/gm_types.h"
#include "engine/physics/dynamics/hms_corpus_registry.h"
struct CHmsCorpus;
struct CHmsDyna;
struct CHmsPhysicalContact;
struct CHmsPortal;
struct CHmsZone;
struct CPlugSolid;
class CSceneMobil;

enum CHmsCollisionContactSide {
    CHmsCollisionContactSide_GroupA = 0u,
    CHmsCollisionContactSide_GroupB = 1u,
};

struct SPlugVisibleId {
    u32 value = 0u;
    constexpr u32 Value(void) const { return value; }
};

struct CHmsItem : CMwNod {
    enum ECallback : u32 {
        ECallback_ComputeForces = 0u,
        ECallback_AfterContacts = 1u,
        ECallback_AbsorbContact = 2u,
        ECallback_Count = 3u,
    };

    struct CCallback {
        virtual ~CCallback(void);
        virtual ECallback GetType(void) const = 0;
    };

    struct CCallbackComputeForces : CCallback {
        ECallback GetType(void) const final {
            return ECallback_ComputeForces;
        }
        virtual void ComputeForces(CHmsItem *item, float dt) = 0;
    };

    struct CCallbackAfterContacts : CCallback {
        ECallback GetType(void) const final {
            return ECallback_AfterContacts;
        }
        virtual void AfterContacts(CHmsItem *item) = 0;
    };

    struct CCallbackAbsorbContact : CCallback {
        ECallback GetType(void) const final {
            return ECallback_AbsorbContact;
        }
        virtual void AbsorbContact(CHmsItem *item,
                                   CHmsPhysicalContact &contact) = 0;
    };

    struct SHmsCollisionGroupPair {
        u32 groupA;
        u32 groupB;
        bool solveImpulse;
        bool contactHandlerForGroupA;
        bool contactHandlerForGroupB;

        bool ContactHandlerEnabledForResponse(
                CHmsCollisionContactSide side) const;
        CHmsCollisionContactSide ContactSideForCollisionResponse(
                u32 itemCollisionGroup) const;
        static CHmsCollisionContactSide OppositeContactSideForCollisionResponse(
                CHmsCollisionContactSide side);
    };

    enum EContactInterest : u32 {
        EContactInterest_None = 0u,
        EContactInterest_Basic = 1u,
        EContactInterest_Local = 2u,
        EContactInterest_Detailed = 3u,
    };

    enum ECollisionGroup : u32 {
        ECollisionGroup_None = 0u,
        ECollisionGroup_Dynamic = 3u,
        ECollisionGroup_Static = 4u,
    };

    enum EDynamicType : u32 {
        EDynamicType_Static = 0u,
        EDynamicType_LowPriority = 1u,
        EDynamicType_Normal = 2u,
        EDynamicType_HighPriority = 3u,
    };

    struct Properties {
        EContactInterest contactInterest = EContactInterest_None;
        ECollisionGroup collisionGroup = ECollisionGroup_None;
        EDynamicType dynamicType = EDynamicType_Static;
        bool active = true;
        bool collisionStatic = false;
        bool kinematicOnly = false;
        bool forcePointDynamicCollisionResponse = false;
        bool visionStatic = false;
        bool background = false;
        bool zombie = false;
        bool occluderForLightMap = false;
        bool shadowTexCastedEnabled = true;
        bool shadowFakeEnabled = true;
        bool lightLensFlareEnabled = true;
        bool lightEmitter = false;
        uint8_t shadowTexCastedCount = 0u;
        u32 shadowCasterGroupMask = 0u;
        u32 shadowReceiverGroupMask = 0xfffu;
    };

    CHmsItem(void);
    ~CHmsItem(void) override;
    void CallbackSet(ECallback type, CCallback *callback);
    CCallback *CallbackGet(ECallback type) const;
    void RunComputeForcesCallback(float dt);
    void RunAfterContactsCallback(void);
    int WantsAbsorbContactCallback(void) const;
    int RunAbsorbContactCallback(CHmsPhysicalContact *contact);
    void SetContactInterest(
            EContactInterest value);
    void SetIsCollisionStatic(
            int enabled);
    void SetIsKinematicOnly(
            int enabled);
    void SetIsForcePointDynamicCollisionResponse(
            int enabled);
    void SetIsVisionStatic(
            int enabled);
    void SetShadowCasterGroupMask(
            unsigned long mask);
    void SetShadowReceiverGroupMask(
            unsigned long mask);
    void SetShadowFakeEnable(
            int enabled);
    void SetLightLensFlareEnable(
            int enabled);
    void SetOccluderForLightMap(
            int enabled);
    void AddCorpus(
            CHmsCorpus *corpus);
    void RemoveCorpus(
            CHmsCorpus *corpus);
    EHmsCorpusCat GetCorpusCat(
            void);
    CHmsCorpus *GetCorpus(
            const CHmsZone *zone) const;
    unsigned long GetCorpusIndex(
            const CHmsZone *zone) const;
    void SetSolid(
            CPlugSolid *solid);
    CPlugSolid *Solid(void) const;
    void RefreshCorpusesFromSolid(void);
    void SetCollisionGroup(
            ECollisionGroup group);
    void SetDynamicType(
            EDynamicType dynamicType);
    void SetLightEmitter(
            int enabled);
    void VisibleIdSet(
            const SPlugVisibleId &visibleId);
    void SetLocation(
            const GmIso4 &location,
            const CHmsZone *zone);
    void SetCountShadowTexCasted(
            uint8_t count,
            int enabled);
    void SetIsBackground(
            int enabled);
    const CHmsZone *GetZone(
            unsigned long index);
    void GetLinearSpeed(
            GmVec3 &out) const;
    void GetAngularSpeed(
            GmVec3 &out) const;
    void GetForce(
            GmVec3 &out);
    void SetLinearSpeed(
            const GmVec3 &speed);
    void SetAngularSpeed(
            const GmVec3 &speed);
    void AddForce(
            const GmVec3 &force);
    void AddTorque(
            const GmVec3 &torque);
    void AddForce(
            const GmVec3 &force,
            const GmVec3 &point);
    void SetForce(
            const GmVec3 &force);
    void SetTorque(
            const GmVec3 &torque);
    void AddImpulse(
            const GmVec3 &impulse,
            const GmVec3 &point);
    void AddImpulse(
            const GmVec3 &impulse);
    void SetProperties(const Properties &properties);
    const Properties &GetProperties(void) const;
    void ApplyBlockMobilDefaults(void);
    void SetSceneMobilOwner(CSceneMobil *owner);
    CSceneMobil *SceneMobilOwner(void) const;
    CHmsCorpus *CorpusAt(u32 index) const;
    u32 CorpusCount(void) const;
    bool HasPortals(void) const;
    CHmsDyna *FirstDyna(void) const;
    static const SHmsCollisionGroupPair *DefaultCollisionGroupPairs(u32 *countOut);
    u32 CollisionGroup(void) const;
    u32 CollisionGroupForCollisionResponse(void) const;
    u32 ContactInterestForCollisionResponse(void) const;
    int RequestsLocalContactPayloadForCollisionResponse(void) const;
    int HasStaticCollisionTree(void) const;
    int IsZombieCorpus(void) const;
    u32 DynamicTypeForSolveImpulse(void) const;

protected:
    void UpdateCorpusCat(void);

private:
    CMwNodRef<CPlugSolid> solid;
    Properties properties;
    std::vector<CHmsPortal *> portals;
    std::vector<CHmsCorpus *> corpuses;
    CSceneMobil *sceneMobilOwner = nullptr;
    CCallbackComputeForces *computeForcesCallback = nullptr;
    CCallbackAfterContacts *afterContactsCallback = nullptr;
    CCallbackAbsorbContact *absorbContactCallback = nullptr;
};
