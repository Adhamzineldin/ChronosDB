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
#include "recovery/log_manager.h" // [FIX] Include LogManager
#include "recovery/checkpoint_manager.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <string>
#include <filesystem>
#include <chrono>
#include <thread>
#include <future>

namespace francodb {
    
    // Accept IBufferManager for polymorphic buffer pool usage
    // Works with both BufferPoolManager and PartitionedBufferPoolManager
    FrancoServer::FrancoServer(IBufferManager *bpm, Catalog *catalog, LogManager *log_manager)
        : bpm_(bpm), catalog_(catalog), log_manager_(log_manager) {
        try {
            // Initialize Registry FIRST
            registry_ = std::make_unique<DatabaseRegistry>();
            
            // Register the default DB
            registry_->RegisterExternal("default", bpm_, catalog_);

            // Initialize Auth & System Resources
            InitializeSystemResources();

            // Thread Pool
            unsigned int cores = std::thread::hardware_concurrency();
            int pool_size = (cores > 0) ? cores : 4;
            thread_pool_ = std::make_unique<ThreadPool>(pool_size);
            
        } catch (const std::exception &e) {
            std::cerr << "[CRITICAL] System Init Failed: " << e.what() << std::endl;
        }
    }

    FrancoServer::~FrancoServer() {
        std::cout << "[SHUTDOWN] Server destructor called..." << std::endl;
        
        // 1. Signal stop first
        running_ = false;
        is_running_ = false;
        
        // 2. Wait for auto-save thread with timeout
        if (auto_save_thread_.joinable()) {
            std::cout << "[SHUTDOWN] Waiting for auto-save thread..." << std::endl;
            
            // Give it a reasonable time to finish current checkpoint
            auto future = std::async(std::launch::async, [this]() {
                if (auto_save_thread_.joinable()) {
                    auto_save_thread_.join();
                }
            });
            
            // Wait max 5 seconds for the auto-save thread
            if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
                std::cerr << "[SHUTDOWN] Warning: Auto-save thread did not finish in time" << std::endl;
                // Thread will be abandoned - this is better than deadlock
            } else {
                std::cout << "[SHUTDOWN] Auto-save thread finished cleanly" << std::endl;
            }
        }
        
        // 3. Now do the shutdown flush
        Shutdown();
        
#ifdef _WIN32
        WSACleanup();
#endif
        std::cout << "[SHUTDOWN] Server destructor complete" << std::endl;
    }

    void FrancoServer::Start(int port) {
        is_running_ = true;

        socket_t s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCK) {
            std::cerr << "[ERROR] Failed to create socket" << std::endl;
            throw std::runtime_error("Socket creation failed");
        }

        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            std::cerr << "[ERROR] Server Bind Failed on port " << port << std::endl;
#ifdef _WIN32
            closesocket(s);
#else
            close(s);
#endif
            throw std::runtime_error("Bind failed");
        }

        listen(s, net::BACKLOG_QUEUE);
        listen_sock_ = (uintptr_t) s;
        running_ = true;

        auto_save_thread_ = std::thread(&FrancoServer::AutoSaveLoop, this);

        std::cout << "[READY] FrancoDB Server listening on port " << port << " (Pool Active)..." << std::endl;

        while (running_ && is_running_) {
            sockaddr_in client_addr{};
            socklen_t len = sizeof(client_addr);

#ifdef _WIN32
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(s, &readSet);
            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
            if (selectResult > 0 && FD_ISSET(s, &readSet)) {
#else
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(s, &readSet);
            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            int selectResult = select(s + 1, &readSet, nullptr, nullptr, &timeout);
            if (selectResult > 0 && FD_ISSET(s, &readSet)) { 
#endif
                socket_t client_sock = accept(s, (struct sockaddr *) &client_addr, &len);
                if (client_sock != INVALID_SOCK && running_) {
                    uintptr_t client_id = (uintptr_t) client_sock;

                    // Push client to Thread Pool
                    thread_pool_->Enqueue([this, client_id] {
                        this->HandleClient(client_id);
                    });
                }
            }
        }

#ifdef _WIN32
        closesocket((socket_t) listen_sock_);
#else
        close((socket_t) listen_sock_);
#endif
    }


    void FrancoServer::Shutdown() {
        std::cout << "[SHUTDOWN] Flushing buffers..." << std::endl;
        if (auth_manager_) auth_manager_->SaveUsers();
        if (system_catalog_) system_catalog_->SaveCatalog();
        if (system_bpm_) system_bpm_->FlushAllPages();
        if (registry_) registry_->FlushAllDatabases();
        if (catalog_) catalog_->SaveCatalog();
        if (bpm_) bpm_->FlushAllPages();
        
        // [NOTE] LogManager flush is handled in main.cpp usually, or can be added here
        if (log_manager_) log_manager_->Flush(true);

        running_ = false;
        is_running_ = false;
        if (listen_sock_ != 0) {
#ifdef _WIN32
            closesocket((socket_t) listen_sock_);
#else
            close((socket_t) listen_sock_);
#endif
            listen_sock_ = 0;
        }
    }


    void FrancoServer::AutoSaveLoop() {
        CheckpointManager cp_manager(bpm_, log_manager_);
        while (running_) {
            // Sleep in small increments to respond to shutdown quickly
            for (int i = 0; i < 300 && running_; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Check again after sleep - might have been signaled to stop
            if (!running_) {
                std::cout << "[AUTO-SAVE] Shutdown detected, exiting loop..." << std::endl;
                break;
            }
            
            std::cout << "[SERVER] Auto-Checkpoint Initiating..." << std::endl;
            
            // Try to acquire lock with timeout to avoid deadlock during shutdown
            {
                std::unique_lock<std::shared_mutex> lock(ExecutionEngine::global_lock_, std::defer_lock);
                
                // Try to lock with timeout
                auto start = std::chrono::steady_clock::now();
                bool acquired = false;
                while (!acquired && running_) {
                    acquired = lock.try_lock();
                    if (!acquired) {
                        auto elapsed = std::chrono::steady_clock::now() - start;
                        if (elapsed > std::chrono::seconds(5)) {
                            std::cerr << "[AUTO-SAVE] Could not acquire lock, skipping checkpoint" << std::endl;
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                }
                
                if (!acquired || !running_) {
                    continue;
                }
                
                if (bpm_) bpm_->FlushAllPages();
                if (catalog_) catalog_->SaveCatalog();
                if (system_bpm_) system_bpm_->FlushAllPages();
                if (system_catalog_) system_catalog_->SaveCatalog();
                
                cp_manager.BeginCheckpoint();
            
            } // Lock releases here -> Transactions resume automatically.

            std::cout << "[SERVER] Auto-Checkpoint Complete." << std::endl;
        }
        
        std::cout << "[AUTO-SAVE] Thread exiting cleanly" << std::endl;
    }

    void FrancoServer::InitializeSystemResources() {
        auto &config = ConfigManager::GetInstance();
        std::string data_dir = config.GetDataDirectory();
        
        std::filesystem::path system_dir = std::filesystem::path(data_dir) / "system";
        std::filesystem::path system_db_path = system_dir / "system.francodb";

        std::filesystem::create_directories(system_dir);

        if (std::filesystem::exists(system_db_path) && std::filesystem::file_size(system_db_path) < 4096) {
            std::cout << "[RECOVERY] System DB is too small. Wiping." << std::endl;
            std::filesystem::remove(system_db_path);
        }

        system_disk_ = std::make_unique<DiskManager>(system_db_path.string());
        
        if (config.IsEncryptionEnabled()) {
            system_disk_->SetEncryptionKey(config.GetEncryptionKey());
        }
        
        system_bpm_ = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, system_disk_.get());
        system_catalog_ = std::make_unique<Catalog>(system_bpm_.get());

        // [FIX] Pass log_manager_ to AuthManager so system internal queries are logged/ACID compliant
        auth_manager_ = std::make_unique<AuthManager>(
            system_bpm_.get(), 
            system_catalog_.get(), 
            registry_.get(),
            log_manager_
        );
    }


    std::string FrancoServer::DispatchCommand(const std::string &sql, ClientConnectionHandler *handler) {
        std::string upper_sql = sql;
        std::transform(upper_sql.begin(), upper_sql.end(), upper_sql.begin(), ::toupper);

        // Handle STOP/SHUTDOWN command - requires SUPERADMIN
        if (upper_sql.find("STOP") == 0 || upper_sql.find("SHUTDOWN") == 0 ||
            upper_sql.find("WA2AF") == 0 || upper_sql.find("2AFOL") == 0) {
            
            if (!handler->IsAuthenticated()) {
                return "ERROR: Authentication required to stop server.";
            }
            
            // Check for SUPERADMIN role
            auto session = handler->GetSession();
            if (!session || session->role != UserRole::SUPERADMIN) {
                return "ERROR: Permission denied. Only SUPERADMIN can stop the server.";
            }
            
            std::cout << "[STOP] Server shutdown requested by user: " << session->current_user << std::endl;
            
            // Start graceful shutdown in a separate thread to allow response to be sent
            std::thread([this]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Allow response to be sent
                this->Stop();
            }).detach();
            
            return "SHUTDOWN INITIATED. Server will stop in 500ms.";
        }

        if (upper_sql.find("WHOAMI") != std::string::npos) {
            return "Current User: " + (handler->IsAuthenticated() ? handler->GetCurrentUser() : "Guest");
        }

        if (upper_sql.find("SHOW DATABASES") != std::string::npos || upper_sql.find("WARINI DATABASE") != std::string::npos) {
            if (!handler->IsAuthenticated()) {
                return "ERROR: Authentication required.";
            }

            std::string user = handler->GetCurrentUser();
            std::stringstream ss;
            ss << "--- AVAILABLE DATABASES ---\n";

            if (auth_manager_->HasDatabaseAccess(user, "francodb")) {
                ss << "default\n";
            }

            auto &config = ConfigManager::GetInstance();
            std::filesystem::path data_path(config.GetDataDirectory());

            if (std::filesystem::exists(data_path)) {
                for (const auto &entry: std::filesystem::directory_iterator(data_path)) {
                    if (entry.is_directory()) {
                        std::string db_name = entry.path().filename().string();
                        if (db_name == "system" || db_name == "default") continue;

                        if (auth_manager_->HasDatabaseAccess(user, db_name)) {
                            ss << db_name << "\n";
                        }
                    }
                }
            }
            return ss.str();
        }

        return handler->ProcessRequest(sql);
    }

    void FrancoServer::Stop() {
        std::cout << "[STOP] Initiating graceful shutdown..." << std::endl;
        
        // 1. Signal all loops to stop FIRST (non-blocking)
        running_ = false;
        is_running_ = false;

        // 2. Close the listening socket to unblock accept()
        if (listen_sock_ != 0) {
#ifdef _WIN32
            // Shutdown first to unblock any blocking recv/send calls
            shutdown((socket_t) listen_sock_, SD_BOTH);
            closesocket((socket_t) listen_sock_);
#else
            shutdown((socket_t) listen_sock_, SHUT_RDWR);
            close((socket_t) listen_sock_);
#endif
            listen_sock_ = 0;
        }
        
        std::cout << "[STOP] Socket closed, waiting for threads to finish..." << std::endl;
    }


    void FrancoServer::HandleClient(uintptr_t client_socket) {
        socket_t sock = (socket_t) client_socket;
        
        // [FIX] Pass log_manager_ to ExecutionEngine
        auto engine = std::make_unique<ExecutionEngine>(
            bpm_, 
            catalog_, 
            auth_manager_.get(), 
            registry_.get(),
            log_manager_
        );
        
        auto handler = std::make_unique<ClientConnectionHandler>(engine.release(), auth_manager_.get());

        while (running_) {
            PacketHeader header;
            int bytes_read = recv(sock, (char *) &header, sizeof(header), MSG_WAITALL);
            if (bytes_read <= 0) break;

            uint32_t payload_len = ntohl(header.length);
            if (payload_len > 1024 * 1024 * 10) break; // 10MB Limit

            std::vector<char> payload(payload_len);
            recv(sock, payload.data(), payload_len, MSG_WAITALL);

            std::string sql(payload.begin(), payload.end());

            switch (header.type) {
                case MsgType::CMD_JSON: handler->SetResponseFormat(ProtocolType::JSON); break;
                case MsgType::CMD_BINARY: handler->SetResponseFormat(ProtocolType::BINARY); break;
                default: handler->SetResponseFormat(ProtocolType::TEXT); break;
            }

            std::string response = DispatchCommand(sql, handler.get());

            uint32_t resp_len = htonl(response.size());
            int sent_header = send(sock, (char*)&resp_len, sizeof(resp_len), 0);
            if (sent_header <= 0) break;

            size_t total_sent = 0;
            while (total_sent < response.size()) {
                int sent = send(sock, response.c_str() + total_sent, response.size() - total_sent, 0);
                if (sent <= 0) break;
                total_sent += sent;
            }
        }

#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
    }
}