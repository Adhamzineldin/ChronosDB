#pragma once
#include "recovery/recovery_manager.h"
#include "storage/table/table_heap.h"

namespace francodb {
    class SnapshotManager {
    public:
        // Builds a temporary table heap containing data exactly as it was at 'target_time'
        static std::unique_ptr<TableHeap> BuildSnapshot(
            std::string table_name,
            uint64_t target_time, 
            BufferPoolManager* bpm, 
            LogManager* log_manager,
            Catalog* catalog) 
        {
            // 1. Create a FRESH, EMPTY Shadow Table
            // (Transaction = nullptr means no logging for this temp table)
            auto shadow_heap = std::make_unique<TableHeap>(bpm, nullptr); 

            // 2. Use RecoveryManager to fill it
            // We pass the Shadow Heap explicitly so RecoveryManager writes to IT, not the Catalog.
            RecoveryManager recovery(log_manager, catalog, bpm);
            
            // This functions needs to be added to RecoveryManager (see below)
            recovery.ReplayIntoHeap(shadow_heap.get(), table_name, target_time);
            
            return shadow_heap;
        }
    };
}