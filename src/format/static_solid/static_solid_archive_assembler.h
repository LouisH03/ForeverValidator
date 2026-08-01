#pragma once

#include <optional>

#include "format/static_solid/static_solid_archive_id.h"
#include "format/static_solid/static_solid_decorator_assembler.h"
#include "format/static_solid/static_solid_surface_assembler.h"
#include "format/static_solid/static_solid_tree_assembler.h"
class CGameCtnReplayStaticSolidArchiveGraph;
class StaticSolidArchiveLoadSession;
class CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition;

class StaticSolidArchiveAssembler {
public:
    void Clear();
    bool Assemble(const CGameCtnReplayStaticSolidArchiveGraph &archive,
                  StaticSolidArchiveLoadSession &payloads);

    CPlugTree *CollisionRoot(StaticSolidArchiveId archiveId);
    CPlugTree *ModelRoot(StaticSolidArchiveId archiveId,
                         std::optional<u32> treeNodeIndex);
    const CGameCtnReplayStaticSolidArchiveSolidPhysicsDefinition *Physics(
            StaticSolidArchiveId archiveId);

    void ApplyReplacementMaterials(CPlugTree *tree,
                                   StaticSolidArchiveId archiveId);
    void ApplyDecorationSkinMaterials(CPlugTree *tree,
                                      StaticSolidArchiveId archiveId);
    void ApplyWarpDecoration(CPlugTree *tree);

private:
    StaticSolidTreeAssembler trees_;
    StaticSolidSurfaceAssembler surfaces_;
    StaticSolidDecoratorAssembler decorators_;
};
