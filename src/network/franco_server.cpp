// network/franco_server.cpp (updated)
// Platform networking headers
#include "network/packet.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define INVALID_SOCK INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCK -1
#endif

#include "network/franco_server.h"
#include "network/protocol.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "storage/disk/disk_manager.h"
#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"
#include "common/config.h"
#include "common/franco_net_config.h"
#include "network/database_registry.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <string>
#include <filesystem>
#include <chrono>
#include <thread>

namespace francodb {
    FrancoServer::FrancoServer(BufferPoolManager* bpm, Catalog* catalog)
        : bpm_(bpm), catalog_(catalog) {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

        // Create system database for authentication (system.francodb)
        // Use configured data directory
        auto& config = ConfigManager::GetInstance();
        std::string data_dir = config.GetDataDirectory();
        std::filesystem::create_directories(data_dir);
        
        // Use filesystem::path for cross-platform path handling (Windows uses \, Linux uses /)
        std::filesystem::path system_db_path = std::filesystem::path(data_dir) / "system";
        system_disk_ = std::make_unique<DiskManager>(system_db_path.string());
        
        // Apply encryption to system database if enabled
        if (config.IsEncryptionEnabled() && !config.GetEncryptionKey().empty()) {
            system_disk_->SetEncryptionKey(config.GetEncryptionKey());
        }
        
        system_bpm_ = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, system_disk_.get());
        system_catalog_ = std::make_unique<Catalog>(system_bpm_.get());
        // Load existing catalog if it exists, otherwise it will be created
        system_catalog_->LoadCatalog();
        
        // Initialize AuthManager (this will create system tables and default user if needed)
        auth_manager_ = std::make_unique<AuthManager>(system_bpm_.get(), system_catalog_.get());
        
        // CRITICAL: Save system catalog after initialization to ensure system files are created
        // This ensures that even if config exists but system files don't, they get created
        system_catalog_->SaveCatalog();
        system_bpm_->FlushAllPages();

        // Register default database (points to provided bpm/catalog; not owned)
        registry_ = std::make_unique<DatabaseRegistry>();
        registry_->RegisterExternal("default", bpm_, catalog_);
    }

    FrancoServer::~FrancoServer() {
        // Stop auto-save thread
        running_ = false;
        if (auto_save_thread_.joinable()) {
            auto_save_thread_.join();
        }
        
        Shutdown();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void FrancoServer::Start(int port) {
        socket_t s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCK) {
            std::cerr << "[ERROR] Failed to create socket" << std::endl;
            return;
        }

        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "[ERROR] Server Bind Failed on port " << port << std::endl;
#ifdef _WIN32
            closesocket(s);
#else
            close(s);
#endif
            return;
        }

        listen(s, net::BACKLOG_QUEUE);
        listen_sock_ = (uintptr_t)s;
        running_ = true;
        
        // Start auto-save thread (saves data every 30 seconds)
        auto_save_thread_ = std::thread(&FrancoServer::AutoSaveLoop, this);

        std::cout << "[READY] FrancoDB Server listening on port " << port << "..." << std::endl;
        std::cout << "[INFO] Auto-save enabled (every 30 seconds)." << std::endl;

        while (running_) {
            sockaddr_in client_addr{};
#ifdef _WIN32
            int len = sizeof(client_addr);
            // Use select with timeout to make accept() interruptible
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(s, &readSet);
            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
            if (selectResult > 0 && FD_ISSET(s, &readSet)) {
                socket_t client_sock = accept(s, (struct sockaddr*)&client_addr, &len);
                if (client_sock != INVALID_SOCK) {
                    uintptr_t client_id = (uintptr_t)client_sock;
                    client_threads_[client_id] = std::thread(&FrancoServer::HandleClient, this, client_id);
                    client_threads_[client_id].detach();
                }
            }
#else
            socklen_t len = sizeof(client_addr);
            // Use select with timeout to make accept() interruptible
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(s, &readSet);
            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            int selectResult = select(s + 1, &readSet, nullptr, nullptr, &timeout);
            if (selectResult > 0 && FD_ISSET(s, &readSet)) {
                socket_t client_sock = accept(s, (struct sockaddr*)&client_addr, &len);
                if (client_sock != INVALID_SOCK) {
                    uintptr_t client_id = (uintptr_t)client_sock;
                    client_threads_[client_id] = std::thread(&FrancoServer::HandleClient, this, client_id);
                    client_threads_[client_id].detach();
                }
            }
#endif
        }

#ifdef _WIN32
        closesocket((socket_t)listen_sock_);
#else
        close((socket_t)listen_sock_);
#endif
    }

    void FrancoServer::Shutdown() {
        std::cout << "[SHUTDOWN] Saving all data to disk..." << std::endl;
        std::cout.flush();
        
        // Save all users
        if (auth_manager_) {
            auth_manager_->SaveUsers();
        }
        
        // Save system catalog
        if (system_catalog_) {
            system_catalog_->SaveCatalog();
        }
        
        // Flush system buffer pool
        if (system_bpm_) {
            system_bpm_->FlushAllPages();
        }
        
        // Flush all databases in registry
        if (registry_) {
            registry_->FlushAllDatabases();
        }
        
        // Flush default database
        if (catalog_) {
            catalog_->SaveCatalog();
        }
        if (bpm_) {
            bpm_->FlushAllPages();
        }
        
        std::cout << "[SHUTDOWN] All data saved successfully." << std::endl;
        std::cout.flush();
        running_ = false;
        if (listen_sock_ != 0) {
#ifdef _WIN32
            closesocket((socket_t)listen_sock_);
#else
            close((socket_t)listen_sock_);
#endif
            listen_sock_ = 0;
        }
        // Wait for client threads to finish
        for (auto& [id, thread] : client_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        client_threads_.clear();
    }

    void FrancoServer::AutoSaveLoop() {
        const int AUTO_SAVE_INTERVAL_SECONDS = 30; // Save every 30 seconds
        
        while (running_) {
            // Sleep for the interval, but check running_ flag periodically
            for (int i = 0; i < AUTO_SAVE_INTERVAL_SECONDS * 10 && running_; ++i) {
#ifdef _WIN32
                Sleep(100); // Sleep 100ms at a time
#else
                usleep(100000); // Sleep 100ms at a time
#endif
            }
            
            // Only save if server is still running
            if (running_) {
                try {
                    // Save all users
                    if (auth_manager_) {
                        auth_manager_->SaveUsers();
                    }
                    
                    // Save system catalog
                    if (system_catalog_) {
                        system_catalog_->SaveCatalog();
                    }
                    
                    // Flush system buffer pool
                    if (system_bpm_) {
                        system_bpm_->FlushAllPages();
                    }
                    
                    // Flush all databases in registry
                    if (registry_) {
                        registry_->FlushAllDatabases();
                    }
                    
                    // Flush default database
                    if (catalog_) {
                        catalog_->SaveCatalog();
                    }
                    if (bpm_) {
                        bpm_->FlushAllPages();
                    }
                    
                    std::cout << "[AUTO-SAVE] Data saved automatically." << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[AUTO-SAVE] Error: " << e.what() << std::endl;
                }
            }
        }
    }

    ProtocolType FrancoServer::DetectProtocol(const std::string& initial_data) {
        if (initial_data.empty()) return ProtocolType::TEXT;
        
        if (initial_data[0] == '{' || initial_data.substr(0, 4) == "POST") {
            return ProtocolType::JSON;
        } else if (initial_data[0] == 0x01 || initial_data[0] == 0x02) {
            return ProtocolType::BINARY;
        }
        
        return ProtocolType::TEXT;
    }

    std::pair<Catalog*, BufferPoolManager*> FrancoServer::GetOrCreateDb(const std::string &db_name) {
        auto entry = registry_->Get(db_name);
        if (!entry) {
            entry = registry_->GetOrCreate(db_name);
        }
        if (!entry) return {nullptr, nullptr};
        if (entry->catalog && entry->bpm) {
            return {entry->catalog.get(), entry->bpm.get()};
        }
        // external?
        auto ext_bpm = registry_->ExternalBpm(db_name);
        auto ext_cat = registry_->ExternalCatalog(db_name);
        return {ext_cat, ext_bpm};
    }

    void FrancoServer::HandleClient(uintptr_t client_socket) {
        socket_t sock = (socket_t)client_socket;
        
        while (running_) {
            // 1. READ HEADER (5 Bytes)
            PacketHeader header;
            int bytes_read = recv(sock, (char*)&header, sizeof(header), MSG_WAITALL);
            
            if (bytes_read <= 0) break; // Client disconnected

            // Convert length back from network order
            uint32_t payload_len = ntohl(header.length);

            // 2. READ PAYLOAD
            // Allocate exact size or use a safe buffer check
            if (payload_len > net::MAX_PACKET_SIZE) { 
                // Handle error: packet too big
                break; 
            }
            
            std::vector<char> payload(payload_len);
            recv(sock, payload.data(), payload_len, MSG_WAITALL);
            
            std::string sql(payload.begin(), payload.end());

            // 3. PROCESS BASED ON TYPE
            ProtocolType proto_mode;
            switch (header.type) {
                case MsgType::CMD_JSON:   proto_mode = ProtocolType::JSON; break;
                case MsgType::CMD_BINARY: proto_mode = ProtocolType::BINARY; break;
                default:                  proto_mode = ProtocolType::TEXT; break;
            }
            
            // Create Handler on the fly (or reuse one and just set the mode)
            auto handler = CreateHandler(proto_mode, client_socket);
            std::string response = handler->ProcessRequest(sql);

            send(sock, response.c_str(), response.size(), 0);
        }
        
        // ... cleanup ...
    }
    
    std::unique_ptr<ConnectionHandler> FrancoServer::CreateHandler(
        ProtocolType type, uintptr_t client_socket) {
        
        auto engine = std::make_unique<ExecutionEngine>(bpm_, catalog_);
        
        switch (type) {
            case ProtocolType::JSON:
                return std::make_unique<ApiConnectionHandler>(engine.release(), auth_manager_.get());
            case ProtocolType::BINARY:
                return std::make_unique<BinaryConnectionHandler>(engine.release(), auth_manager_.get());
            case ProtocolType::TEXT:
            default:
                return std::make_unique<ClientConnectionHandler>(engine.release(), auth_manager_.get());
        }
    }
}