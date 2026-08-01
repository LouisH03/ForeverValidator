#pragma once

#include <optional>

#include "engine/game/replay_vehicle_solid_definition.h"
struct CPlugFilePack;
struct InstalledVehicleAssetGraph;
class DefaultVehicleSolidArchive {
public:
    static std::optional<ReplayVehicleSolidDefinition> LoadFromPack(
            CPlugFilePack &pack);
    static std::optional<ReplayVehicleSolidDefinition> LoadFromPack(
            CPlugFilePack &pack,
            const InstalledVehicleAssetGraph &assets);
};
