#ifndef TMNF_STATIC_SOLID_ARCHIVE_VEHICLE_TUNING_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_VEHICLE_TUNING_READER_H


#include "engine/core/engine_types.h"
#include "format/static_solid/static_solid_archive_node_ref_reader.h"
#include "format/static_solid/static_solid_archive_stream_roles.h"
class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveCMwIdState;

struct CGameCtnReplayStaticSolidArchiveVehicleTuningReader {
    static int ParseVehicleTuningChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            u32 classId,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CGameCtnReplayStaticSolidArchiveFeedbackSink *feedbackSink);

private:
    static int ApplyVehicleTuningIdFeedback(
            const char *tuningId,
            CGameCtnReplayStaticSolidArchiveFeedbackSink *feedbackSink);
    static int ParseFastBufferNodPtrs(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ParseSceneVehicleTuningBaseChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CGameCtnReplayStaticSolidArchiveFeedbackSink *feedbackSink);
    static int ParseFuncKeysRealChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            u32 chunkId);
    static int ParseSceneVehicleTuningsChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ParseCarTuningSchemaChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ParseLegacyCarTuningM6CompositeChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ParseLegacyCarTuningChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs);
    static int ParseSceneVehicleCarTuningChunk(
            CGameCtnReplayStaticSolidArchiveByteStream *byteStream,
            CGameCtnReplayStaticSolidArchiveCMwIdState *cmwIdState,
            u32 chunkId,
            CGameCtnReplayStaticSolidArchiveNodeRefReader *nodeRefs,
            CGameCtnReplayStaticSolidArchiveFeedbackSink *feedbackSink);
};

#endif
