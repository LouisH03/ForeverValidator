#pragma once

#include "engine/rendering/plug_material.h"
#include "engine/physics/geometry/plug_surface.h"
#include "engine/rendering/plug_tree.h"
#include "engine/core/cmw_nod.h"
#include "engine/core/engine_types.h"
#include "format/static_solid/static_solid_geometry_decoder.h"
#include "format/static_solid/static_solid_archive_identity.h"
#include "format/static_solid/static_solid_resource_index.h"
#include "format/static_solid/static_solid_material_remaps.h"
#include "format/static_solid/static_solid_archive_id.h"
#include <vector>

class CGameCtnReplayStaticSolidArchiveNodeDirectory;
class CGameCtnReplayStaticSolidArchiveSurfaceGraph;
class CGameCtnReplayStaticSolidArchiveMaterialDefinition;
class CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition;
class CGameCtnReplayStaticSolidArchiveSurfaceMaterialEntry;
class CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition;

class StaticSolidArchiveVisualCollection {
public:
    void Clear();
    int Allocate(u32 count);
    StaticSolidArchiveVisual *At(u32 index);
    u32 Count() const;

private:
    std::vector<CMwNodRef<StaticSolidArchiveVisual>>
            providers;
};

class StaticSolidSurfaceAssembler {
public:
    void Free();
    void EnsureDefaultSurfaceMaterials();
    int MaterializeArchiveGraphWithDefaultMaterials(
            const CGameCtnReplayStaticSolidArchiveNodeDirectory &nodes,
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaceGraph,
            const StaticSolidDecodedPayloads
                    &decodedPayloads);
    int MaterializeArchiveGraph(
            const CGameCtnReplayStaticSolidArchiveNodeDirectory &nodes,
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaceGraph,
            const StaticSolidDecodedPayloads
                    &decodedPayloads);

    CPlugSurface *SurfaceForPayload(
            StaticSolidArchiveId payload);
    CPlugSurface *SurfaceForNode(
            CGameCtnReplayStaticSolidArchiveNodeIdentity surface);
    CPlugMaterial *MaterialForNode(
            CGameCtnReplayStaticSolidArchiveNodeIdentity material);
    StaticSolidArchiveVisual *VisualProviderForNode(
            CGameCtnReplayStaticSolidArchiveNodeIdentity visual);
    void ApplyMaterialRemapToTree(
            CPlugTree *tree,
            StaticSolidArchiveId payload,
            StaticSolidMaterialRemap kind);

    u32 SurfaceCount() const;
    u32 SurfaceGeomCount() const;
    u32 VisualProviderCount() const;
    u32 MaterialCount() const;

private:
    StaticSolidNodes<CPlugSurface> surfaces;
    StaticSolidNodes<CPlugSurfaceGeom> surfaceGeoms;
    StaticSolidNodes<CPlugMaterial> materials;
    StaticSolidArchiveVisualCollection visualProviders;
    StaticSolidResourceIndex resources;
    StaticSolidMaterialRemaps materialRemaps;
    u32 surfaceCount = 0u;
    u32 surfaceGeomCount = 0u;
    u32 materialCount = 0u;

    int AllocateForGraph(
            const CGameCtnReplayStaticSolidArchiveNodeDirectory &nodes,
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaces);
    void ConstructSurfacesAndMaterials(
            const CGameCtnReplayStaticSolidArchiveNodeDirectory &nodes);
    void ApplyMaterialDefinitions(
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaces);
    int InstallMaterialRemaps(
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaces);
    int MaterializeSurfaceGeometryDefinition(
            u32 surfaceGeomIndex,
            const CGameCtnReplayStaticSolidArchiveSurfaceGeometryDefinition &definition,
            const StaticSolidDecodedPayloads
                    &decodedPayloads);
    int MaterializeSurfaceGeometryDefinitions(
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaces,
            const StaticSolidDecodedPayloads
                    &decodedPayloads);
    int LinkSurfaceToGeom(
            CGameCtnReplayStaticSolidArchiveNodeIdentity surface,
            CGameCtnReplayStaticSolidArchiveNodeIdentity geom,
            u32 materialSlotCount);
    int LinkSurfacesToGeoms(
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaces);
    int ApplySurfaceMaterialEntry(
            CGameCtnReplayStaticSolidArchiveNodeIdentity surface,
            const CGameCtnReplayStaticSolidArchiveSurfaceMaterialEntry &entry);
    int ApplySurfaceMaterialEntries(
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaces);
    void MaterializeArchiveVisualProvider(
            u32 visualProviderIndex,
            const CGameCtnReplayStaticSolidArchiveVisualGeometryDefinition &definition,
            const StaticSolidDecodedPayloads &decodedPayloads);
    void MaterializeVisualProviderGeometry(
            u32 visualProviderIndex,
            CGameCtnReplayStaticSolidArchiveNodeIdentity visualProvider,
            StaticSolidDecodedVisualGeometry geometry);
    void MaterializeVisualProviderDefinitions(
            const CGameCtnReplayStaticSolidArchiveSurfaceGraph &surfaces,
            const StaticSolidDecodedPayloads
                    &decodedPayloads);
    CPlugSurface *SurfaceAt(u32 index) {
        return index < surfaceCount ? surfaces.At(index) : nullptr;
    }
    CPlugMaterial *MaterialAt(u32 index) {
        return index < materialCount ? materials.At(index) : nullptr;
    }
    StaticSolidArchiveVisual *VisualProviderAt(u32 index) {
        return visualProviders.At(index);
    }
    u32 SurfaceIndexForNode(
            CGameCtnReplayStaticSolidArchiveNodeIdentity surface) const {
        return resources.Surface(surface);
    }
    u32 SurfaceGeomIndexForNode(
            CGameCtnReplayStaticSolidArchiveNodeIdentity geom) const {
        return resources.SurfaceGeom(geom);
    }
    u32 VisualProviderIndexForNode(
            CGameCtnReplayStaticSolidArchiveNodeIdentity visual) const {
        return resources.VisualProvider(visual);
    }
    u32 MaterialIndexForNode(
            CGameCtnReplayStaticSolidArchiveNodeIdentity material) const {
        return resources.Material(material);
    }
    CPlugMaterial *DefaultSurfaceMaterial(u32 materialId);
    CPlugSurfaceGeom *SurfaceGeomAt(u32 index) {
        return index < surfaceGeomCount ? surfaceGeoms.At(index) : nullptr;
    }
    CPlugMaterial *MaterialRemapForSource(
            StaticSolidArchiveId payload,
            StaticSolidMaterialRemap kind,
            const CPlugMaterial *source);
    CPlugMaterial *RemapMaterialRef(
            StaticSolidArchiveId payload,
            StaticSolidMaterialRemap kind,
            CPlugMaterial *material);
    void ApplyMaterialRemapToSurface(
            CPlugSurface *surface,
            StaticSolidArchiveId payload,
            StaticSolidMaterialRemap kind);
    void ApplyMaterialRemapToSubtree(
            CPlugTree *tree,
            StaticSolidArchiveId payload,
            StaticSolidMaterialRemap kind);
};
