#ifndef TMNF_STATIC_SOLID_ARCHIVE_VEHICLE_STRUCT_READER_H
#define TMNF_STATIC_SOLID_ARCHIVE_VEHICLE_STRUCT_READER_H

#include "engine/core/engine_types.h"

class CGameCtnReplayStaticSolidArchiveByteStream;
struct CGameCtnReplayStaticSolidArchiveChunkDispatchContext;

struct CSceneVehicleStructArchiveState {
    u32 visualVehicleCount = 0u;
    int hasVisualVehicleCount = 0;

    void Reset();
};

struct CGameCtnReplayStaticSolidArchiveVehicleStructReader {
    static int ParseChunk(
            const CGameCtnReplayStaticSolidArchiveChunkDispatchContext &context,
            u32 classId,
            u32 chunkId);
};

#endif
