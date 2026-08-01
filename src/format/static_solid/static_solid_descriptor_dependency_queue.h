#ifndef TMNF_STATIC_SOLID_DESCRIPTOR_DEPENDENCY_QUEUE_H
#define TMNF_STATIC_SOLID_DESCRIPTOR_DEPENDENCY_QUEUE_H

#include <string>
#include <vector>

#include "engine/core/engine_types.h"
#include "format/static_solid/static_solid_external_node_paths.h"
struct CGameCtnReplayMapInput;
class CGameCtnReplayArchiveStaticModelCollection;
struct CatalogDecorationSizeDefinition;
class ReplaySceneBlockPlacements;
struct StaticSolidArchiveCatalog;
class StaticSolidArchiveLoadSession;
class StaticSolidArchiveReferenceCatalog;
struct CPlugFilePack;
struct SceneDescriptorFolderPaths;

enum class CGameCtnReplayStaticSolidDescriptorDependencyPriority : u32 {
    Optional = 0u,
    Required = 1u
};

struct CGameCtnReplayStaticSolidDescriptorDependency {
    std::string selectedDescriptorPath;
    CGameCtnReplayStaticSolidDescriptorDependencyPriority priority =
            CGameCtnReplayStaticSolidDescriptorDependencyPriority::Optional;

    const char *SelectedDescriptorPath() const;
    int IsRequired() const;
};

struct CGameCtnReplayStaticSolidDescriptorDependencyQueue {
private:
    std::vector<CGameCtnReplayStaticSolidDescriptorDependency> dependencies;

    int Has(const char *selectedDescriptorPath) const;
    int QueueDescriptor(
            const char *selectedDescriptorPath,
            CGameCtnReplayStaticSolidDescriptorDependencyPriority priority);

public:
    void Free();
    int RequireDescriptor(const char *selectedDescriptorPath);
    int RequestOptionalDescriptor(const char *selectedDescriptorPath);
    int RequestExternalRefDescriptors(
            const SceneDescriptorFolderPaths &externalFolders,
            const CPlugFilePack &pack,
            const StaticSolidArchiveLoadSession &archive,
            const std::vector<CGameCtnReplayStaticSolidExternalNodeRef>
                    &nodeRefs);
    int SeedFromReplayStaticInputs(
            const CGameCtnReplayMapInput *mapInput,
            const ReplaySceneBlockPlacements *placements,
            const CGameCtnReplayArchiveStaticModelCollection *archiveModels,
            const StaticSolidArchiveCatalog *inventory,
            const StaticSolidArchiveReferenceCatalog *references,
            const CatalogDecorationSizeDefinition *decorationSize);
    int SeedFromReplayMapStaticInputs(
            const ReplaySceneBlockPlacements *placements,
            const CGameCtnReplayArchiveStaticModelCollection *archiveModels,
            const StaticSolidArchiveReferenceCatalog *references);
    int SeedFromReplayDecorationStaticInputs(
            const StaticSolidArchiveCatalog *inventory,
            const CatalogDecorationSizeDefinition *decorationSize);
    int RequireMissingDescriptor(
            const StaticSolidArchiveLoadSession *store,
            const char *selectedDescriptorPath);
    int DecodeReachablePayloadGraph(
            const StaticSolidArchiveCatalog *inventory,
            StaticSolidArchiveLoadSession *store,
            CGameCtnReplayArchiveStaticModelCollection *archiveModels,
            u32 *selectedMissingOut);
};

#endif
