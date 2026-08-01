#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "engine/core/cmw_nod.h"
#include "engine/core/engine_types.h"
#include "engine/core/gm_types.h"
struct CPlugMaterial;
class CPlugShader;
struct CPlugSurface;
struct CPlugTree;
struct CPlugVisual;
struct SPlugTreeOptimGroup;

enum class PlugVisualOptimizationMode : u32 {
    Default = 0u,
};

enum class PlugVisualWeldOptions : u32 {
    None = 0u,
};

class PlugTreeOptimizationBatch {
public:
    explicit PlugTreeOptimizationBatch(
            unsigned long expectedGroupCount = 0ul);
    ~PlugTreeOptimizationBatch(void);

    PlugTreeOptimizationBatch(const PlugTreeOptimizationBatch &) = delete;
    PlugTreeOptimizationBatch &operator=(
            const PlugTreeOptimizationBatch &) = delete;
    PlugTreeOptimizationBatch(PlugTreeOptimizationBatch &&) = delete;
    PlugTreeOptimizationBatch &operator=(
            PlugTreeOptimizationBatch &&) = delete;

    SPlugTreeOptimGroup &CreateGroup(
            std::vector<SPlugTreeOptimGroup *> &view);

private:
    std::vector<std::unique_ptr<SPlugTreeOptimGroup>> ownedGroups_;
};

struct SPlugTreeOptimCriteria {
    bool copyGroupProperties = false;
    std::optional<u32> mergedVertexLimit;
    std::optional<float> boxExtentLimit;
    bool markGroupedVisualDirty = false;
    bool selfWeld = false;
    bool providerOptimization = false;
    PlugVisualOptimizationMode providerOptimizationMode =
            PlugVisualOptimizationMode::Default;
    float selfWeldAngle = 0.0f;
    float selfWeldDistance = 0.0f;
    float selfWeldPrecision = 0.0f;
    float selfWeldTolerance = 0.0f;
    PlugVisualWeldOptions selfWeldOptions = PlugVisualWeldOptions::None;
    u32 standaloneVertexLimit = 0u;

    bool CopiesGroupProperties(void) const;
    bool EnforcesMergedVertexLimit(void) const;
    bool EnforcesBoxExtentLimit(void) const;
    bool MarksGroupedVisualDirty(void) const;
    bool WantsSelfWeld(void) const;
    bool WantsProviderOptimization(void) const;
    bool NeedsProviderPostProcess(void) const;
    u32 MergedVertexLimit(void) const;
    u32 StandaloneVertexLimit(void) const;
    float BoxExtentLimit(void) const;
    u32 ProviderOptimizeMode(void) const;
    float SelfWeldAngle(void) const;
    float SelfWeldDistance(void) const;
    float SelfWeldPrecision(void) const;
    float SelfWeldTolerance(void) const;
    u32 SelfWeldFlags(void) const;
};

struct SPlugTreeOptimTravel {
    explicit SPlugTreeOptimTravel(PlugTreeOptimizationBatch &batch);

    bool hasIso = false;
    GmIso4 iso{};

    SPlugTreeOptimTravel WithTreeLocalIso(const CPlugTree &tree) const;
    SPlugTreeOptimTravel WithOptimizationBatch(
            PlugTreeOptimizationBatch &batch) const;
    SPlugTreeOptimGroup &CreateGroup(
            std::vector<SPlugTreeOptimGroup *> &groups) const;

private:
    PlugTreeOptimizationBatch *batch_ = nullptr;
};

struct SPlugTreeOptimTransf {
    CPlugTree *tree = nullptr;
    bool hasIso = false;
    GmIso4 iso{};
};

struct SPlugTreeOptimProperties {
    bool optimizationBoundary = false;
    bool visualInheritance = false;
    bool shaderInheritance = false;
    bool materialInheritance = false;
    bool visible = false;
    bool modelInstance = false;
    bool castsShadows = false;
    bool rooted = false;
};

struct SPlugTreeOptimGroup {
    enum class Kind : u32 {
        MergedGeometry,
        SingleTree,
        RecursiveTree,
        GeneratedMip,
    };

    Kind kind = Kind::MergedGeometry;
    SPlugTreeOptimProperties properties{};
    GmBoxAligned box{};
    u32 indexCount = 0u;
    u32 texCoordCount = 0u;
    u32 vertexCount = 0u;
    std::vector<SPlugTreeOptimTransf> transforms;
    std::unique_ptr<CPlugTree> generatedTree;

    SPlugTreeOptimGroup(void);
    ~SPlugTreeOptimGroup(void);
    CPlugMaterial *Material(void) const;
    CPlugShader *Shader(void) const;
    CPlugSurface *Surface(void) const;
    void SetMaterial(CPlugMaterial *material);
    void SetShader(CPlugShader *shader);
    void SetSurface(CPlugSurface *surface);
    void AdoptMissingTreeState(const CPlugTree &tree);
    void InitCommon(CPlugTree &tree, Kind kind);
    void InitAsSingleTree(CPlugTree &tree,
                          const SPlugTreeOptimTravel &travel);
    void InitAsMergeTree(CPlugTree &tree,
                         const SPlugTreeOptimTravel &travel);
    void InitAsGeneratedMip(std::unique_ptr<CPlugTree> tree,
                            const SPlugTreeOptimProperties &properties);
    void AddTransform(const SPlugTreeOptimTransf &transform);
    void AddTreeTransform(CPlugTree &tree,
                          const SPlugTreeOptimTravel &travel);
    void CopyTreeBox(const CPlugTree &tree);
    u32 TransformCount(void) const;
    SPlugTreeOptimTransf *TransformAt(u32 index);
    const SPlugTreeOptimTransf *TransformAt(u32 index) const;
    SPlugTreeOptimTransf *FirstTransform(void);
    CPlugVisual *FirstVisual(void) const;
    CPlugVisual *FirstTransformVisual(void) const;
    bool HasMatchingVertexFormat(const CPlugVisual *visual) const;
    bool IsSingleTreeGroup(void) const;
    bool HasSurfaceConflict(const CPlugTree &tree) const;
    bool HasMaterialMismatch(const CPlugTree &tree) const;
    bool HasShaderMismatch(const CPlugTree &tree) const;
    void ExpandBoxWith(const CPlugTree &tree);
    void AddVisualCounts(CPlugVisual &visual, u32 vertexCount);
    void AdoptGeneratedTree(std::unique_ptr<CPlugTree> tree);
    std::unique_ptr<CPlugTree> TakeGeneratedTree(void);

private:
    CMwNodRef<CPlugMaterial> material_;
    CMwNodRef<CPlugShader> shader_;
    CMwNodRef<CPlugSurface> surface_;
};
