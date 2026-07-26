#pragma once

#include "engine/core/engine_types.h"
#include "engine/core/gm_types.h"
#include "engine/physics/collision/hms_collision_manager.h"
#include "engine/physics/dynamics/hms_corpus_registry.h"
#include "engine/physics/dynamics/hms_dyna.h"
struct CHmsItem;
struct CHmsPhysicalContact;
struct CHmsCorpus;
struct CHmsZone;
struct CPlugTree;
struct SHmsPhysicalCollision;

void Zone_UpdateWaterHeights(CHmsCorpus *corpus);

struct CHmsCorpus {
    CHmsCorpus(void);
    virtual ~CHmsCorpus(void);
    void SetItem(CHmsItem *item);
    CHmsItem *Item(void) const { return item; }
    void SetDynamics(CHmsDyna *newDyna) { dyna = newDyna; }
    CHmsDyna *Dynamics(void) const { return dyna; }
    const GmIso4 &LocalLocation(void) const { return localIso; }
    void RestoreLocalLocation(const GmIso4 &location) noexcept {
        localIso = location;
    }
    void DetachFromWorld(void);
    CHmsZone *OwningZone(void) const;
    void BindCollisionManagerRegistration(
            CHmsCollisionManagerSZone &managerZone,
            CHmsCollisionManagerSGroup *managerGroup,
            CHmsCorpusId id);
    void ClearCollisionManagerRegistration(
            const CHmsCollisionManagerSZone &managerZone);
    CHmsCollisionManagerSZone *RegisteredCollisionManagerZone(void) const;
    CHmsCollisionManagerSGroup *RegisteredCollisionManagerGroup(void) const;
    void Reset(void);
    virtual const GmIso4 *GetLocation(void) const;
    void RefreshFromSolid(void);
    void SetLocation(
            const GmIso4 &location);
    void SetTranslation(
            const GmVec3 &translation);
    void RotateOf(
            const GmMat3 &rotation);
    const GmIso4 *LocationIso(void) const;
    const GmIso4 *VirtualLocationIso(void);
    CPlugTree *CollisionTree(void) const;
    int WaterGetPlaneEqInZone(GmVec4 &plane);
    CHmsCorpusId Id(void) const { return collisionManagerId; }
    int UsesLinearImpulseOnlyForSolveImpulse(void) const;
    GmIso4 LocationForSolveImpulse(void) const;
    GmVec3 CollisionPointVelocityForSolveImpulse(const GmVec3 &point) const;
    void ComputeOwnerForces(float dt);
    int IsExcludedFromInitialForcePass(void) const;
    void NotifyOwnerAfterContacts(void);
    GmIso4 LocationForCollisionResponse(void) const;
    GmVec3 TranslationForCollisionResponse(void) const;
    GmVec3 LocalImpulseNormalForCollisionResponse(
            const SHmsPhysicalCollision *collision,
            GmVec3 *beforeTranspose) const;
    GmVec3 LocalContactPointForCollisionResponse(
            const SHmsPhysicalCollision *collision,
            GmVec3 *beforeTranspose) const;
    int WantsAbsorbContactForCollisionResponse(void) const;
    int RunAbsorbContactForCollisionResponse(CHmsPhysicalContact *contact) const;
    void RunContactHandlerAndApplyReplacementForSolveImpulse(
            CHmsPhysicalContact *contact,
            GmVec3 *worldReplacement,
            GmVec3 *relativeSpeed,
            int sideAContact);
    void ApplyImpulseForSolveImpulse(
            CHmsDyna *targetDyna,
            SHmsPhysicalCollision *collision,
            GmVec3 speedAtPoint,
            float restitution,
            float materialFrictionProduct,
            int sideB);

private:
    GmIso4 localIso{};
    CHmsItem *item = nullptr;
    CHmsDyna *dyna = nullptr;
    CHmsCorpusId collisionManagerId;
    CHmsCollisionManagerSZone *collisionManagerZone = nullptr;
    CHmsCollisionManagerSGroup *collisionManagerGroup = nullptr;
    mutable GmIso4 queriedLocation{};
    CHmsZone *zone_ = nullptr;

    friend struct CHmsZone;
};
