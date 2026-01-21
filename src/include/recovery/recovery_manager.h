#pragma once

#include "recovery/log_manager.h"
#include "recovery/log_record.h"
#include "catalog/catalog.h"              // [NEW]
#include "buffer/buffer_pool_manager.h"   // [NEW]
#include <map>

namespace francodb {

    class RecoveryManager {
    public:
        RecoveryManager(LogManager* log_manager, Catalog* catalog, BufferPoolManager* bpm) 
            : log_manager_(log_manager), catalog_(catalog), bpm_(bpm) {}

        void ARIES();

        void RollbackToTime(uint64_t target_time);

        void RecoverToTime(uint64_t target_time);

        void ReplayIntoHeap(TableHeap *target_heap, std::string target_table_name, uint64_t target_time);

    private:
        void RunRecoveryLoop(uint64_t stop_at_time, uint64_t start_offset);

        LogManager* log_manager_;
        Catalog* catalog_;             
        BufferPoolManager* bpm_;     
    };

} // namespace francodb