// database_registry.cpp
#include "network/database_registry.h"
#include "common/config_manager.h"
#include <iostream>

namespace francodb {

    void DatabaseRegistry::FlushAllDatabases() {
        // 1. Flush "Owned" and "Managed" Databases
        // These are the ones loaded via CREATE DATABASE or USE
        for (auto& [name, entry] : registry_) {
            if (entry) {
                std::cout << "[REGISTRY] Flushing database: " << name << std::endl;
                if (entry->catalog) {
                    entry->catalog->SaveCatalog();
                }
                if (entry->bpm) {
                    entry->bpm->FlushAllPages();
                }
            }
        }

        // 2. Flush "External" Pointers 
        // This handles the 'default' DB or system-level DBs passed in manually.
        // We use a check to make sure we don't double-flush if they are also in the registry.
        for (auto& [name, catalog] : external_catalog_) {
            // If it's not in the main registry, flush it here
            if (registry_.find(name) == registry_.end() && catalog) {
                catalog->SaveCatalog();
            }
        }

        for (auto& [name, bpm] : external_bpm_) {
            if (registry_.find(name) == registry_.end() && bpm) {
                bpm->FlushAllPages();
            }
        }
    
        std::cout << "[REGISTRY] All databases persisted to disk." << std::endl;
    }

} // namespace francodb