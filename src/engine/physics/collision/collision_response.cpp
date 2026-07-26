// Contact callback routing and impulse-response orchestration.

#include "engine/physics/collision/hms_collision.h"
#include "engine/physics/collision/hms_collision_manager.h"
#include "engine/physics/dynamics/hms_corpus.h"
#include "engine/physics/dynamics/hms_dyna.h"
#include "engine/physics/dynamics/hms_item.h"
#include "engine/physics/world/hms_zone.h"

void CHmsZoneDynamic::ComputeCollisionResponse() {
    ComputeCollisionResponse(collisionBuffer_);
}

void CHmsZoneDynamic::ComputeCollisionResponse(
        CHmsCollisionBuffer &collisionBuffer) {
    collisionBuffer.SortForCollisionResponse();

    u32 count = collisionBuffer.PhysicalCollisionCount();
    for (u32 collisionIndex = 0; collisionIndex < count; collisionIndex++) {
        SHmsPhysicalCollision *collision =
                collisionBuffer.PhysicalCollisionAtOrNull(collisionIndex);
        if (collision == nullptr) {
            continue;
        }

        CHmsCorpus *a = collision->CorpusA();
        CHmsCorpus *b = collision->CorpusB();
        const int contactHandlerA = a->WantsAbsorbContactForCollisionResponse();
        const int contactHandlerB = b->WantsAbsorbContactForCollisionResponse();

        const CHmsItem::SHmsCollisionGroupPair &pair =
                collision->CollisionGroupPair();
        CHmsCollisionContactSide side = pair.ContactSideForCollisionResponse(
                a->Item()->CollisionGroupForCollisionResponse());
        CHmsCollisionContactSide otherSide =
                CHmsItem::SHmsCollisionGroupPair::
                        OppositeContactSideForCollisionResponse(side);

        CHmsPhysicalContact contactA;
        CHmsPhysicalContact contactB;
        CHmsPhysicalContact *contactAPtr = nullptr;
        CHmsPhysicalContact *contactBPtr = nullptr;

        if (pair.ContactHandlerEnabledForResponse(side) && contactHandlerA != 0) {
            contactA.InitForCollisionAResponse(collision);
            contactAPtr = &contactA;
        }

        if (pair.ContactHandlerEnabledForResponse(otherSide) && contactHandlerB != 0) {
            contactB.InitForCollisionBResponse(collision);
            contactBPtr = &contactB;
        }

        if (pair.solveImpulse) {
            SolveImpulse(*collision, contactAPtr, contactBPtr);
            continue;
        }

        GmVec3 speedA = GmVec3::Zero();
        GmVec3 speedB = GmVec3::Zero();
        if (a->Dynamics() != 0) {
            a->Dynamics()->GetSpeed(collision->contactPoint, speedA);
        }
        if (b->Dynamics() != 0) {
            b->Dynamics()->GetSpeed(collision->contactPoint, speedB);
        }

        GmVec3 relativeSpeed = speedB - speedA;

        if (contactAPtr != 0) {
            contactAPtr->SetLocalRelativeSpeedForCollisionResponse(
                    a, relativeSpeed.Negated());
            a->RunAbsorbContactForCollisionResponse(contactAPtr);
        }

        if (contactBPtr != 0) {
            contactBPtr->SetLocalRelativeSpeedForCollisionResponse(b, relativeSpeed);
            b->RunAbsorbContactForCollisionResponse(contactBPtr);
        }
    }
}
