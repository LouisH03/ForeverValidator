#ifndef FOREVERVALIDATOR_EXPERIMENTAL_PHYSICS_SANDBOX_H
#define FOREVERVALIDATOR_EXPERIMENTAL_PHYSICS_SANDBOX_H

// This API is experimental. It may change without compatibility guarantees.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <forevervalidator/input_state.h>
#include <forevervalidator/validation.h>

namespace forevervalidator::experimental {

namespace static_scene_test {
struct PhysicsSandboxStaticSceneTestAccess;
}
namespace cuda_test {
struct PhysicsSandboxCudaTestAccess;
}

class PhysicsSandboxCudaSearchSession;

enum class PhysicsSandboxErrorCode : std::uint8_t {
    InvalidSandbox,
    InvalidRequest,
    ReplayLoadingFailed,
    MapLoadingFailed,
    SimulationFailed,
    IncompatibleState,
    AllocationFailed,
    UnexpectedFailure,
};

struct PhysicsSandboxError {
    PhysicsSandboxErrorCode code = PhysicsSandboxErrorCode::UnexpectedFailure;
    ValidationError validationError{};
    std::string diagnostic;
};

template<typename T>
using PhysicsSandboxResult =
        DiscriminatedResult<T, PhysicsSandboxError>;

struct PhysicsSandboxOptions {
    SimulationBackend backend = SimulationBackend::Reference;
    std::uint32_t tickDurationMs = 10u;
    std::uint32_t prestartDurationMs = 2600u;
};

enum class PhysicsSandboxInputAction : std::uint8_t {
    Unmapped,
    Accelerate,
    Gas,
    Brake,
    Steer,
    SteerLeft,
    SteerRight,
    RaceRunning,
    FinishLine,
    Respawn,
};

enum class PhysicsSandboxInputValueKind : std::uint8_t {
    None,
    Switch,
    Analog,
};

enum class PhysicsSandboxSwitchState : std::uint8_t {
    Released,
    Pressed,
    NonCanonicalActive,
};

struct PhysicsSandboxInputValue {
    PhysicsSandboxInputValueKind kind = PhysicsSandboxInputValueKind::None;
    PhysicsSandboxSwitchState switchState =
            PhysicsSandboxSwitchState::Released;
    AnalogInputState analog = 0;
};

struct PhysicsSandboxInputEvent {
    std::int32_t timeMs = 0;
    PhysicsSandboxInputAction action = PhysicsSandboxInputAction::Unmapped;
    PhysicsSandboxInputValue value{};
};

struct PhysicsSandboxCarState {
    float rotationX = 0.0f;
    float rotationY = 0.0f;
    float rotationZ = 0.0f;
    float rotationW = 1.0f;
    Vector3 position{};
    Vector3 linearSpeed{};
    Vector3 angularSpeed{};
    Vector3 force{};
    Vector3 torque{};
};

struct PhysicsSandboxCollisionTriangle {
    Vector3 a{};
    Vector3 b{};
    Vector3 c{};
};

struct PhysicsSandboxEllipsoid {
    float rotationX = 0.0f;
    float rotationY = 0.0f;
    float rotationZ = 0.0f;
    float rotationW = 1.0f;
    Vector3 position{};
    Vector3 radii{};
};

struct PhysicsSandboxVector2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct PhysicsSandboxVector4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct PhysicsSandboxTransform {
    Vector3 basisX{1.0f, 0.0f, 0.0f};
    Vector3 basisY{0.0f, 1.0f, 0.0f};
    Vector3 basisZ{0.0f, 0.0f, 1.0f};
    Vector3 translation{};
};

enum class PhysicsSandboxScenePurpose : std::uint8_t {
    Environment,
    PlacedBlock,
    SubMobil,
    Clip,
    Helper,
    CheckpointTrigger,
    DedicatedInitialCollision,
    Pylon,
    Decoration,
    Terrain,
    Generated,
};

enum class PhysicsSandboxRenderLayer : std::uint8_t {
    World,
    Background,
};

struct PhysicsSandboxRenderProvenance {
    std::string blockName;
    std::string collection;
    std::string descriptorPath;
    std::string sceneObjectId;
    std::optional<std::uint64_t> placementIdentity;
    std::optional<std::uint32_t> blockInstanceId;
    std::optional<std::uint32_t> variant;
    std::uint32_t componentIndex = 0u;
    bool authored = false;
};

struct PhysicsSandboxRenderVertex {
    Vector3 position{};
    Vector3 normal{0.0f, 1.0f, 0.0f};
    PhysicsSandboxVector4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    PhysicsSandboxVector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    PhysicsSandboxVector2 uv0{};
    PhysicsSandboxVector2 uv1{};
};

struct PhysicsSandboxRenderSubset {
    std::uint32_t indexOffset = 0u;
    std::uint32_t indexCount = 0u;
    std::uint32_t materialSlot = 0u;
};

struct PhysicsSandboxRenderMesh {
    std::uint64_t id = 0u;
    std::vector<PhysicsSandboxRenderVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<PhysicsSandboxRenderSubset> subsets;
    Vector3 boundsMin{};
    Vector3 boundsMax{};
    bool hasNormals = false;
    bool hasTangents = false;
    bool hasVertexColors = false;
    bool hasUv0 = false;
    bool hasUv1 = false;
};

struct PhysicsSandboxMaterialBitmap {
    std::string samplerName;
    std::string sourcePath;
    std::uint32_t bitmapClassId = 0u;
    std::uint32_t renderClassId = 0u;
};

struct PhysicsSandboxRenderMaterial {
    std::uint64_t id = 0u;
    std::string sourcePath;
    std::string modelPath;
    std::string shaderPath;
    std::vector<PhysicsSandboxMaterialBitmap> bitmaps;
    std::uint32_t shaderFlags = 0u;
    std::uint8_t surfaceMaterialId = 0u;
    bool water = false;
    bool cubeMap = false;
    bool renderTarget = false;
};

struct PhysicsSandboxRenderInstance {
    std::uint64_t id = 0u;
    std::uint32_t meshIndex = 0u;
    std::uint32_t materialIndex = 0u;
    PhysicsSandboxTransform worldTransform{};
    PhysicsSandboxRenderProvenance provenance{};
    PhysicsSandboxScenePurpose purpose =
            PhysicsSandboxScenePurpose::Environment;
    PhysicsSandboxRenderLayer renderLayer =
            PhysicsSandboxRenderLayer::World;
    std::uint32_t lodLevel = 0u;
    float lodFarDistance = 0.0f;
    bool visible = true;
    bool castsShadows = true;
};

enum class PhysicsSandboxRenderDiagnosticCode : std::uint8_t {
    MissingUv,
    MissingNormal,
    MissingTangent,
    MissingMaterial,
    UnsupportedVisual,
    InvalidTopology,
    CollisionOnlyObjectSkipped,
};

struct PhysicsSandboxRenderDiagnostic {
    PhysicsSandboxRenderDiagnosticCode code =
            PhysicsSandboxRenderDiagnosticCode::UnsupportedVisual;
    std::string message;
    PhysicsSandboxRenderProvenance provenance{};
};

struct PhysicsSandboxRenderScene {
    std::vector<PhysicsSandboxRenderMesh> meshes;
    std::vector<PhysicsSandboxRenderMaterial> materials;
    std::vector<PhysicsSandboxRenderInstance> instances;
    std::vector<PhysicsSandboxRenderDiagnostic> diagnostics;
};

using PhysicsSandboxRenderSceneHandle =
        std::shared_ptr<const PhysicsSandboxRenderScene>;

struct PhysicsSandboxSceneView {
    std::vector<PhysicsSandboxCollisionTriangle> collisionTriangles;
    std::vector<PhysicsSandboxEllipsoid> carEllipsoids;
};

struct PhysicsSandboxStateView {
    std::uint64_t tick = 0u;
    std::uint64_t timeMs = 0u;
    std::uint64_t durationMs = 0u;
    MapEnvironment mapEnvironment = MapEnvironment::Unknown;
    VehicleModel vehicleModel = VehicleModel::Unknown;
    std::optional<PlayMode> playMode;
    PhysicsSandboxCarState car{};
    float accelerate = 0.0f;
    float brake = 0.0f;
    float steering = 0.0f;
    std::uint32_t checkpointsCollected = 0u;
    std::uint32_t checkpointsTotal = 0u;
    std::uint32_t completedLaps = 0u;
    std::uint32_t totalLaps = 1u;
    bool raceCompleted = false;
    std::optional<std::uint32_t> finishTimeMs;
    std::optional<FinishTimeEstimate> finishTime;
    std::uint32_t respawnCount = 0u;
    std::optional<std::uint32_t> stuntsScore;
};

struct PhysicsSandboxCudaModifierWindow {
    std::int64_t minimumTimeMs = 0;
    std::int64_t maximumTimeMs = 0;
    std::uint32_t seed = 0u;
};

struct PhysicsSandboxCudaRandomSteeringModifier {
    PhysicsSandboxCudaModifierWindow window{};
};

struct PhysicsSandboxCudaExistingEventModifier {
    PhysicsSandboxCudaModifierWindow window{};
    std::uint32_t minimumCount = 0u;
    std::uint32_t maximumCount = 0u;
    std::int64_t maximumTimeShiftMs = 0;
    bool absoluteSteering = false;
    AnalogInputState steeringDeltaMinimum = 0;
    AnalogInputState steeringDeltaMaximum = 0;
    AnalogInputState steeringAbsoluteMinimum = kAnalogInputMinimum;
    AnalogInputState steeringAbsoluteMaximum = kAnalogInputMaximum;
    bool toggleAccelerate = false;
    bool toggleBrake = false;
};

struct PhysicsSandboxCudaSmoothSteeringModifier {
    PhysicsSandboxCudaModifierWindow window{};
    std::uint32_t deformationCount = 0u;
    std::int64_t radiusMs = 0;
    AnalogInputState amplitudeMinimum = 0;
    AnalogInputState amplitudeMaximum = 0;
};

struct PhysicsSandboxCudaInsertionChannel {
    bool enabled = false;
    std::uint32_t minimumCount = 0u;
    std::uint32_t maximumCount = 0u;
    std::int64_t maximumHoldMs = 0;
};

struct PhysicsSandboxCudaInputInsertionModifier {
    PhysicsSandboxCudaModifierWindow window{};
    PhysicsSandboxCudaInsertionChannel steering{};
    PhysicsSandboxCudaInsertionChannel accelerate{};
    PhysicsSandboxCudaInsertionChannel brake{};
    bool steeringOffset = false;
    AnalogInputState steeringAbsoluteMinimum = kAnalogInputMinimum;
    AnalogInputState steeringAbsoluteMaximum = kAnalogInputMaximum;
    AnalogInputState steeringOffsetMinimum = 0;
    AnalogInputState steeringOffsetMaximum = 0;
};

struct PhysicsSandboxCudaDeletionChannel {
    bool enabled = false;
    std::uint32_t maximumCount = 0u;
};

struct PhysicsSandboxCudaInputDeletionModifier {
    PhysicsSandboxCudaModifierWindow window{};
    PhysicsSandboxCudaDeletionChannel steering{};
    PhysicsSandboxCudaDeletionChannel accelerate{};
    PhysicsSandboxCudaDeletionChannel brake{};
};

using PhysicsSandboxCudaModifier = std::variant<
        PhysicsSandboxCudaRandomSteeringModifier,
        PhysicsSandboxCudaExistingEventModifier,
        PhysicsSandboxCudaSmoothSteeringModifier,
        PhysicsSandboxCudaInputInsertionModifier,
        PhysicsSandboxCudaInputDeletionModifier>;

struct PhysicsSandboxCudaVector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct PhysicsSandboxCudaVelocityEvaluator {
    bool projected = false;
    bool alignmentEnabled = false;
    PhysicsSandboxCudaVector3 direction{};
    double minimumAlignment = -1.0;
};

struct PhysicsSandboxCudaPointEvaluator {
    PhysicsSandboxCudaVector3 target{};
};

struct PhysicsSandboxCudaPoseEvaluator {
    PhysicsSandboxCudaVector3 targetPosition{};
    double targetRotationX = 0.0;
    double targetRotationY = 0.0;
    double targetRotationZ = 0.0;
    double targetRotationW = 1.0;
    double rotationWeight = 0.5;
};

struct PhysicsSandboxCudaVolumeEntryEvaluator {
    PhysicsSandboxCudaVector3 minimum{};
    PhysicsSandboxCudaVector3 maximum{};
};

struct PhysicsSandboxCudaFinishTimeEvaluator {};

using PhysicsSandboxCudaEvaluator = std::variant<
        PhysicsSandboxCudaVelocityEvaluator,
        PhysicsSandboxCudaPointEvaluator,
        PhysicsSandboxCudaPoseEvaluator,
        PhysicsSandboxCudaVolumeEntryEvaluator,
        PhysicsSandboxCudaFinishTimeEvaluator>;

struct PhysicsSandboxCudaSearchConfiguration {
    std::uint32_t maximumBatchSize = 1u;
    std::int64_t earliestMutationTimeMs = 0;
    std::int64_t evaluationStartTimeMs = 0;
    std::int64_t evaluationEndTimeMs = 0;
    std::vector<PhysicsSandboxCudaModifier> modifiers;
    PhysicsSandboxCudaEvaluator evaluator =
            PhysicsSandboxCudaFinishTimeEvaluator{};
    // Retains the original materialization path for exact differential tests.
    bool useLegacyMutationPipelineForTesting = false;
};

// An opaque in-process runtime clone. States are not serializable and are not
// compatible across ForeverValidator builds.
class PhysicsSandboxState {
public:
    PhysicsSandboxState(const PhysicsSandboxState &);
    PhysicsSandboxState &operator=(const PhysicsSandboxState &);
    PhysicsSandboxState(PhysicsSandboxState &&) noexcept;
    PhysicsSandboxState &operator=(PhysicsSandboxState &&) noexcept;
    ~PhysicsSandboxState();

    const PhysicsSandboxStateView &View() const noexcept;

private:
    struct Impl;
    explicit PhysicsSandboxState(std::shared_ptr<const Impl> impl);
    std::shared_ptr<const Impl> impl_;
    friend class PhysicsSandbox;
    friend class PhysicsSandboxCudaSearchSession;
};

struct PhysicsSandboxCudaSearchMetrics {
    std::uint64_t residentDeviceBytes = 0u;
    std::uint64_t mutationDeviceBytes = 0u;
    std::uint64_t candidateInputDeviceBytes = 0u;
    std::uint64_t mutationScratchDeviceBytes = 0u;
    std::uint64_t hostToDeviceBytes = 0u;
    std::uint64_t deviceToHostBytes = 0u;
    double kernelMilliseconds = 0.0;
    double scoreInitializationKernelMilliseconds = 0.0;
    double mutationKernelMilliseconds = 0.0;
    double simulationKernelMilliseconds = 0.0;
    double winnerKernelMilliseconds = 0.0;
    double winnerReductionKernelMilliseconds = 0.0;
    double winnerStateCaptureKernelMilliseconds = 0.0;
    double finalizationKernelMilliseconds = 0.0;
    std::uint32_t simulationThreadsPerBlock = 0u;
    std::uint32_t simulationRegistersPerThread = 0u;
    std::uint64_t simulationLocalBytesPerThread = 0u;
    std::uint32_t simulationActiveBlocksPerMultiprocessor = 0u;
    double simulationTheoreticalOccupancy = 0.0;
};

struct PhysicsSandboxCudaSearchBatch {
    std::uint64_t firstCandidateId = 0u;
    std::uint32_t candidateCount = 0u;
    std::uint32_t evaluatedCandidateCount = 0u;
    std::uint64_t evaluatorCalls = 0u;
    std::uint64_t totalMutationCount = 0u;
    std::uint64_t mutationImprovementCount = 0u;
    bool cancelled = false;
    bool bestChanged = false;
    bool bestIsMutation = false;
    std::optional<std::uint64_t> bestCandidateId;
    std::size_t bestMutationCount = 0u;
    double bestScore = 0.0;
    double bestTimeMs = 0.0;
    double bestDetail0 = 0.0;
    double bestDetail1 = 0.0;
    PhysicsSandboxStateView bestState{};
    std::vector<PhysicsSandboxInputEvent> bestInputs;
    std::optional<PhysicsSandboxState> bestSnapshot;
    PhysicsSandboxCudaSearchMetrics metrics{};
};

class PhysicsSandbox {
public:
    PhysicsSandbox(PhysicsSandbox &&) noexcept;
    PhysicsSandbox &operator=(PhysicsSandbox &&) noexcept;
    ~PhysicsSandbox();
    PhysicsSandbox(const PhysicsSandbox &) = delete;
    PhysicsSandbox &operator=(const PhysicsSandbox &) = delete;

    SimulationBackend Backend() const noexcept;
    PhysicsSandboxResult<PhysicsSandboxStateView> LoadReplay(
            ByteView replayBytes,
            const ReplayIdentity &identity) noexcept;
    PhysicsSandboxResult<std::vector<PhysicsSandboxInputEvent>> ReadInputs()
            const noexcept;
    PhysicsSandboxResult<std::size_t> ReplaceInputs(
            std::vector<PhysicsSandboxInputEvent> events) noexcept;
    PhysicsSandboxResult<PhysicsSandboxStateView> AdvanceTicks(
            std::uint32_t count) noexcept;
    PhysicsSandboxResult<PhysicsSandboxState> CaptureState() const noexcept;
    PhysicsSandboxResult<PhysicsSandboxStateView> RestoreState(
            const PhysicsSandboxState &state) noexcept;
    PhysicsSandboxResult<PhysicsSandboxStateView> ReadState() const noexcept;
    PhysicsSandboxResult<PhysicsSandboxRenderSceneHandle> ReadRenderScene()
            const noexcept;
    PhysicsSandboxResult<PhysicsSandboxSceneView> ReadScene() const noexcept;

private:
    struct Impl;
    explicit PhysicsSandbox(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
    friend PhysicsSandboxResult<PhysicsSandbox> CreatePhysicsSandbox(
            AssetSource source,
            const PhysicsSandboxOptions &options) noexcept;
    friend std::vector<PhysicsSandboxResult<PhysicsSandboxStateView>>
            AdvancePhysicsSandboxes(
                    const std::vector<PhysicsSandbox *> &sandboxes,
                    std::uint32_t count) noexcept;
    friend struct static_scene_test::PhysicsSandboxStaticSceneTestAccess;
    friend struct cuda_test::PhysicsSandboxCudaTestAccess;
    friend class PhysicsSandboxCudaSearchSession;
    friend PhysicsSandboxResult<PhysicsSandboxCudaSearchSession>
            CreatePhysicsSandboxCudaSearchSession(
                    PhysicsSandbox &sandbox,
                    const PhysicsSandboxCudaSearchConfiguration
                            &configuration) noexcept;
};

class PhysicsSandboxCudaSearchSession {
public:
    PhysicsSandboxCudaSearchSession(
            PhysicsSandboxCudaSearchSession &&) noexcept;
    PhysicsSandboxCudaSearchSession &operator=(
            PhysicsSandboxCudaSearchSession &&) noexcept;
    ~PhysicsSandboxCudaSearchSession();
    PhysicsSandboxCudaSearchSession(
            const PhysicsSandboxCudaSearchSession &) = delete;
    PhysicsSandboxCudaSearchSession &operator=(
            const PhysicsSandboxCudaSearchSession &) = delete;

    PhysicsSandboxResult<PhysicsSandboxCudaSearchBatch> EvaluateBaseline()
            noexcept;
    PhysicsSandboxResult<PhysicsSandboxCudaSearchBatch> EvaluateBaseline(
            const std::function<bool()> &cancellationRequested) noexcept;
    PhysicsSandboxResult<PhysicsSandboxCudaSearchBatch> RunBatch(
            std::uint64_t firstCandidateId,
            std::uint32_t candidateCount,
            bool cancellationRequested = false) noexcept;
    PhysicsSandboxResult<PhysicsSandboxCudaSearchBatch> RunBatch(
            std::uint64_t firstCandidateId,
            std::uint32_t candidateCount,
            const std::function<bool()> &cancellationRequested) noexcept;
    PhysicsSandboxResult<std::uint32_t> ReserveBatchCapacity(
            std::uint32_t candidateCount) noexcept;

private:
    struct Impl;
    explicit PhysicsSandboxCudaSearchSession(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
    friend PhysicsSandboxResult<PhysicsSandboxCudaSearchSession>
            CreatePhysicsSandboxCudaSearchSession(
                    PhysicsSandbox &sandbox,
                    const PhysicsSandboxCudaSearchConfiguration
                            &configuration) noexcept;
};

PhysicsSandboxResult<PhysicsSandbox> CreatePhysicsSandbox(
        AssetSource source,
        const PhysicsSandboxOptions &options = {}) noexcept;

PhysicsSandboxResult<PhysicsSandboxCudaSearchSession>
CreatePhysicsSandboxCudaSearchSession(
        PhysicsSandbox &sandbox,
        const PhysicsSandboxCudaSearchConfiguration &configuration) noexcept;

std::vector<PhysicsSandboxResult<PhysicsSandboxStateView>>
AdvancePhysicsSandboxes(
        const std::vector<PhysicsSandbox *> &sandboxes,
        std::uint32_t count) noexcept;

}  // namespace forevervalidator::experimental

#endif
