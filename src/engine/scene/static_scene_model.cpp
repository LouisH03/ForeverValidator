#include "engine/scene/static_scene_model.h"
#include <utility>
#include <new>

StaticSceneModel::StaticSceneModel(StaticSolidPrototype prototype,
                                   const GmIso4 &worldIso,
                                   StaticScenePurpose purpose)
        : prototype_(std::move(prototype)),
          worldIso_(worldIso),
          purpose_(purpose) {}

const StaticSolidPrototype &StaticSceneModel::Prototype() const {
    return prototype_;
}

const GmIso4 &StaticSceneModel::WorldIso() const {
    return worldIso_;
}

StaticScenePurpose StaticSceneModel::Purpose() const {
    return purpose_;
}

void StaticSceneModel::SetProvenance(StaticSceneProvenance provenance) {
    provenance_ = std::move(provenance);
}

const StaticSceneProvenance &StaticSceneModel::Provenance() const {
    return provenance_;
}

void StaticSceneModel::SetItemProperties(
        const CHmsItem::Properties &properties) {
    itemProperties_ = properties;
}

const std::optional<CHmsItem::Properties> &
StaticSceneModel::ItemProperties() const {
    return itemProperties_;
}

void StaticSceneModel::SetCheckpoint(
        CGameCtnReplayCheckpointTrigger::RaceCheckpointIdentity identity,
        std::optional<GmIso4> spawnIso) {
    checkpointIdentity_ = identity;
    checkpointSpawnIso_ = std::move(spawnIso);
}

const std::optional<
        CGameCtnReplayCheckpointTrigger::RaceCheckpointIdentity> &
StaticSceneModel::CheckpointIdentity() const {
    return checkpointIdentity_;
}

const std::optional<GmIso4> &StaticSceneModel::CheckpointSpawnIso() const {
    return checkpointSpawnIso_;
}

void StaticSceneModelCollection::Clear() {
    models_.clear();
    complete_ = true;
}

bool StaticSceneModelCollection::Reserve(std::size_t count) {
    try {
        models_.reserve(count);
        return true;
    } catch (const std::bad_alloc &) {
        models_.clear();
        return false;
    }
}

bool StaticSceneModelCollection::Add(StaticSceneModel model) {
    try {
        models_.push_back(std::move(model));
        return true;
    } catch (const std::bad_alloc &) {
        return false;
    }
}

const std::vector<StaticSceneModel> &StaticSceneModelCollection::Models()
        const {
    return models_;
}

bool StaticSceneModelCollection::Empty() const {
    return models_.empty();
}

void StaticSceneModelCollection::MarkIncomplete() {
    complete_ = false;
}

bool StaticSceneModelCollection::IsComplete() const {
    return complete_;
}
