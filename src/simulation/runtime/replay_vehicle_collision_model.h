#ifndef TMNF_REPLAY_VEHICLE_COLLISION_MODEL_H
#define TMNF_REPLAY_VEHICLE_COLLISION_MODEL_H

#include <array>
#include <memory>

#include "engine/game/replay_vehicle_collision_definition.h"
struct CPlugTree;

class ReplayVehicleCollisionModel {
public:
    bool Build(const VehicleCollisionModelDefinition &definition);
    CPlugTree *Shape(VehicleCollisionRole role) const;
    std::unique_ptr<CPlugTree> ReleaseTree();

private:
    std::unique_ptr<CPlugTree> tree;
    std::array<CPlugTree *, VehicleCollisionRolesInDetectorOrder.size()>
            shapes{};
};

#endif
