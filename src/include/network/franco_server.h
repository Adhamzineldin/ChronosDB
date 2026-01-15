#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
#include <utility>

#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"
#include "execution/execution_engine.h"
#include "network/connection_handler.h"
#include "common/auth_manager.h"
#include "network/database_registry.h"
#include "network/protocol.h"  // Ensure we see the definition from here

namespace francodb {

    // REMOVED: enum class ProtocolType definition (it is already in protocol.h)

    class FrancoServer {
    public:
        FrancoServer(BufferPoolManager* bpm, Catalog* catalog);
        ~FrancoServer();

        void Start(int port);
        void Shutdown();
        void RequestShutdown() { running_ = false; }
        
        // Accessors for System components (needed for main.cpp)
        BufferPoolManager* GetSystemBpm() { return system_bpm_.get(); }
        Catalog* GetSystemCatalog() { return system_catalog_.get(); }
        AuthManager* GetAuthManager() { return auth_manager_.get(); }

    private:
        void HandleClient(uintptr_t client_socket);
        void AutoSaveLoop();
        ProtocolType DetectProtocol(const std::string& initial_data);
        std::pair<Catalog*, BufferPoolManager*> GetOrCreateDb(const std::string &db_name);

        // Core Components
        BufferPoolManager* bpm_;
        Catalog* catalog_;
        
        // System Components
        std::unique_ptr<DiskManager> system_disk_;
        std::unique_ptr<BufferPoolManager> system_bpm_;
        std::unique_ptr<Catalog> system_catalog_;
        std::unique_ptr<AuthManager> auth_manager_;
        std::unique_ptr<DatabaseRegistry> registry_;

        std::atomic<bool> running_{false};
        uintptr_t listen_sock_ = 0;
        std::map<uintptr_t, std::thread> client_threads_;
        std::thread auto_save_thread_;
    };

}