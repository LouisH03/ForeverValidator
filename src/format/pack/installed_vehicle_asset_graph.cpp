#include "format/pack/installed_vehicle_asset_graph.h"

#include <array>
#include <cstdint>
#include <new>
#include <utility>

#include "format/archive/archive_class_ids.h"
#include "format/archive/tmnf_gbx_body_reader.h"
#include "format/pack/installed/byte_buffer.h"
#include "format/pack/installed/plug_file_pack.h"
#include "format/static_solid/static_scene_archive_loader.h"
#include "format/static_solid/static_solid_archive_payload_stream.h"
#include "format/vehicle_tuning/default_vehicle_archive_schema.h"

namespace {

bool ParseReferenceTable(
        CPlugFilePack &pack,
        const InstalledVehicleAssetReference &asset,
        GbxBodyReferenceTable *references) {
    if (references == nullptr || !asset.IsValid()) {
        return false;
    }
    const CPlugFileFidContainer_SFileDesc *file =
            pack.FindFileDescByPath(asset.selectedPath.c_str());
    if (file == nullptr || file->classId != asset.classId) {
        return false;
    }

    ByteBuffer extracted;
    u32 classId = 0u;
    if (pack.ExtractPath(asset.selectedPath.c_str(), &extracted) &&
        extracted.Size() <= UINT32_MAX &&
        GbxBodyOffsetReader::TryParseWithReferences(
                extracted.Data(),
                static_cast<u32>(extracted.Size()),
                &classId,
                references)) {
        return classId == asset.classId;
    }
    if (!file->IsEncryptedPayload()) {
        return false;
    }

    StaticSolidArchivePayload payload;
    if (!payload.BuildEncryptedPackFilePayload({
                *file,
                pack.dataStart,
                0u,
                0u,
                asset.logicalPath.c_str(),
                asset.selectedPath.c_str()})) {
        return false;
    }
    const std::size_t payloadOffset = payload.PackPayloadOffset();
    if (payloadOffset > pack.bytes.Size() ||
        payload.RawByteCount() > pack.bytes.Size() - payloadOffset) {
        return false;
    }
    CGameCtnReplayStaticSolidDecodedPayload header;
    if (!CGameCtnReplayStaticSolidArchivePayloadReader::
                DecodeReferenceTablePrefixWithStreamFeedback(
                        pack.key,
                        payload,
                        StaticSolidArchiveRawBytes::FromBytes(
                                pack.bytes.Data() + payloadOffset,
                                payload.RawByteCount()),
                        &header) ||
        !GbxBodyOffsetReader::TryParseWithReferences(
                header.Bytes(),
                header.ByteCount(),
                &classId,
                references)) {
        return false;
    }
    return classId == asset.classId;
}

bool FindUniquePackClass(
        CPlugFilePack &pack,
        u32 classId,
        InstalledVehicleAssetReference *out) {
    if (out == nullptr) {
        return false;
    }
    *out = InstalledVehicleAssetReference{};
    const CPlugFileFidContainer_SFileDesc *found = nullptr;
    for (const CPlugFileFidContainer_SFileDesc &file : pack.files) {
        if (file.classId != classId) {
            continue;
        }
        if (found != nullptr) {
            return false;
        }
        found = &file;
    }
    std::array<char, 512u> path{};
    if (found == nullptr ||
        !pack.FileDescPlainPath(found, path.data(), path.size())) {
        return false;
    }
    try {
        out->logicalPath = path.data();
        out->selectedPath = path.data();
        out->classId = classId;
    } catch (const std::bad_alloc &) {
        *out = InstalledVehicleAssetReference{};
        return false;
    }
    return true;
}

bool ResolveUniqueDeclaredClass(
        CPlugFilePack &pack,
        const InstalledVehicleAssetReference &source,
        const GbxBodyReferenceTable &references,
        u32 targetClassId,
        InstalledVehicleAssetReference *out) {
    if (out == nullptr) {
        return false;
    }
    *out = InstalledVehicleAssetReference{};
    bool found = false;
    for (const GbxBodyExternalReference &reference :
         references.externalReferences) {
        if (reference.IsResource()) {
            continue;
        }
        std::string logicalPath;
        std::array<char, 512u> selectedPath{};
        if (!references.ResolvePlainPathForReference(
                    source.logicalPath, reference, &logicalPath) ||
            !pack.SelectedPathForPlainRef(
                    logicalPath.c_str(),
                    selectedPath.data(),
                    selectedPath.size())) {
            continue;
        }
        const CPlugFileFidContainer_SFileDesc *file =
                pack.FindFileDescByPath(selectedPath.data());
        if (file == nullptr || file->classId != targetClassId) {
            continue;
        }
        if (found) {
            return false;
        }
        try {
            out->logicalPath = std::move(logicalPath);
            out->selectedPath = selectedPath.data();
            out->classId = targetClassId;
        } catch (const std::bad_alloc &) {
            *out = InstalledVehicleAssetReference{};
            return false;
        }
        found = true;
    }
    return found;
}

bool IsRaceCameraClass(u32 classId) {
    return classId == TMNF_CLASS_CGameControlCameraTrackManiaRace ||
           classId == TMNF_CLASS_CGameControlCameraTrackManiaRace2 ||
           classId == TMNF_CLASS_CGameControlCameraTrackManiaRace3;
}

bool ResolveDeclaredRaceCameras(
        CPlugFilePack &pack,
        const InstalledVehicleAssetReference &source,
        const GbxBodyReferenceTable &references,
        std::vector<InstalledVehicleAssetReference> *out) {
    if (out == nullptr) {
        return false;
    }
    out->clear();
    for (const GbxBodyExternalReference &reference :
         references.externalReferences) {
        if (reference.IsResource()) {
            continue;
        }
        std::string logicalPath;
        std::array<char, 512u> selectedPath{};
        if (!references.ResolvePlainPathForReference(
                    source.logicalPath, reference, &logicalPath) ||
            !pack.SelectedPathForPlainRef(
                    logicalPath.c_str(), selectedPath.data(),
                    selectedPath.size())) {
            continue;
        }
        const CPlugFileFidContainer_SFileDesc *file =
                pack.FindFileDescByPath(selectedPath.data());
        if (file == nullptr || !IsRaceCameraClass(file->classId)) {
            continue;
        }
        InstalledVehicleAssetReference camera;
        camera.logicalPath = std::move(logicalPath);
        camera.selectedPath = selectedPath.data();
        camera.classId = file->classId;
        out->push_back(std::move(camera));
    }
    return true;
}

}  // namespace

std::optional<InstalledVehicleAssetGraph>
InstalledVehicleAssetGraph::ResolveFromPack(CPlugFilePack &pack) {
    try {
        InstalledVehicleAssetGraph graph;
        GbxBodyReferenceTable collectorReferences;
        GbxBodyReferenceTable mobilReferences;
        GbxBodyReferenceTable materialReferences;
        GbxBodyReferenceTable bitmapReferences;
        const bool resolved =
                FindUniquePackClass(
                        pack,
                        TMNF_CLASS_CGameCtnCollectorVehicle,
                        &graph.collector) &&
                ParseReferenceTable(
                        pack, graph.collector, &collectorReferences) &&
                ResolveDeclaredRaceCameras(
                        pack, graph.collector, collectorReferences,
                        &graph.raceCameras) &&
                ResolveUniqueDeclaredClass(
                        pack,
                        graph.collector,
                        collectorReferences,
                        TMNF_CLASS_CSceneVehicleCar,
                        &graph.mobil) &&
                ParseReferenceTable(pack, graph.mobil, &mobilReferences) &&
                ResolveUniqueDeclaredClass(
                        pack,
                        graph.mobil,
                        mobilReferences,
                        TMNF_CLASS_CPlugSolid,
                        &graph.solid) &&
                ResolveUniqueDeclaredClass(
                        pack,
                        graph.mobil,
                        mobilReferences,
                        TMNF_CLASS_CSceneVehicleTunings,
                        &graph.tuning) &&
                ResolveUniqueDeclaredClass(
                        pack,
                        graph.mobil,
                        mobilReferences,
                        TMNF_CLASS_CMwRefBuffer,
                        &graph.materials) &&
                ResolveUniqueDeclaredClass(
                        pack,
                        graph.mobil,
                        mobilReferences,
                        TMNF_CLASS_CSceneVehicleStruct,
                        &graph.vehicleStruct) &&
                ParseReferenceTable(
                        pack, graph.materials, &materialReferences) &&
                ResolveUniqueDeclaredClass(
                        pack,
                        graph.materials,
                        materialReferences,
                        TMNF_CLASS_CPlugBitmap,
                        &graph.fakeContactBitmap) &&
                ParseReferenceTable(
                        pack, graph.fakeContactBitmap, &bitmapReferences) &&
                ResolveUniqueDeclaredClass(
                        pack,
                        graph.fakeContactBitmap,
                        bitmapReferences,
                        TMNF_CLASS_CPlugFileTga,
                        &graph.fakeContactTexture);
        if (!resolved || !graph.IsComplete()) {
            return std::nullopt;
        }
        return graph;
    } catch (const std::bad_alloc &) {
        return std::nullopt;
    }
}

std::optional<std::vector<InstalledVehicleAssetReference>>
InstalledVehicleAssetGraph::ResolveRaceCamerasFromPack(CPlugFilePack &pack) {
    try {
        InstalledVehicleAssetReference collector;
        GbxBodyReferenceTable references;
        std::vector<InstalledVehicleAssetReference> cameras;
        if (!FindUniquePackClass(
                    pack, TMNF_CLASS_CGameCtnCollectorVehicle, &collector) ||
            !ParseReferenceTable(pack, collector, &references) ||
            !ResolveDeclaredRaceCameras(
                    pack, collector, references, &cameras) ||
            cameras.empty()) {
            return std::nullopt;
        }
        return cameras;
    } catch (const std::bad_alloc &) {
        return std::nullopt;
    }
}

bool InstalledVehicleAssetGraph::IsComplete() const {
    return collector.IsValid() &&
           collector.classId == TMNF_CLASS_CGameCtnCollectorVehicle &&
           mobil.IsValid() && mobil.classId == TMNF_CLASS_CSceneVehicleCar &&
           solid.IsValid() && solid.classId == TMNF_CLASS_CPlugSolid &&
           tuning.IsValid() &&
           tuning.classId == TMNF_CLASS_CSceneVehicleTunings &&
           materials.IsValid() && materials.classId == TMNF_CLASS_CMwRefBuffer &&
           vehicleStruct.IsValid() &&
           vehicleStruct.classId == TMNF_CLASS_CSceneVehicleStruct &&
           fakeContactBitmap.IsValid() &&
           fakeContactBitmap.classId == TMNF_CLASS_CPlugBitmap &&
           fakeContactTexture.IsValid() &&
           fakeContactTexture.classId == TMNF_CLASS_CPlugFileTga;
}
