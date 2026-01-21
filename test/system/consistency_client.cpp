#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// --- CONFIG ---
const std::string SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 2501;      
const std::string USER = "maayn";  
const std::string PASS = "root";   

const int NUM_THREADS = 8;
const int OPS_PER_THREAD = 500; // Lower count, but higher quality checks

const char CMD_TEXT = 'Q';

static std::atomic<int> success_count{0};
std::atomic<int> data_errors{0};
std::mutex log_mutex;

static void Log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << msg << std::endl;
}

void LogError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cerr << "[DATA ERROR] " << msg << std::endl;
}

class FrancoClient {
    SOCKET sock;
public:
    FrancoClient() : sock(INVALID_SOCKET) {}

    bool Connect() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) return false;
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP.c_str(), &addr.sin_addr);
        return connect(sock, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR;
    }

    std::string Send(const std::string& query) {
        char type = CMD_TEXT;
        uint32_t len = htonl(query.length());
        std::vector<char> buffer;
        buffer.push_back(type);
        const char* len_ptr = reinterpret_cast<const char*>(&len);
        buffer.insert(buffer.end(), len_ptr, len_ptr + 4);
        buffer.insert(buffer.end(), query.begin(), query.end());

        if (send(sock, buffer.data(), (int)buffer.size(), 0) == SOCKET_ERROR) return "NET_ERR";

        char recv_buf[8192];
        int bytes = recv(sock, recv_buf, 8192 - 1, 0);
        if (bytes > 0) {
            recv_buf[bytes] = '\0';
            return std::string(recv_buf);
        }
        return "NET_ERR";
    }

    void Close() { if (sock != INVALID_SOCKET) closesocket(sock); }
};

void Worker(int thread_id) {
    FrancoClient client;
    if (!client.Connect()) return;

    // Login & Setup
    if (client.Send("LOGIN " + USER + " " + PASS + ";").find("ERROR") != std::string::npos) return;
    client.Send("2ESTA5DEM verify_db;");

    for (int i = 0; i < OPS_PER_THREAD; ++i) {
        int unique_id = (thread_id * 10000) + i;
        std::string val_v1 = "T" + std::to_string(thread_id) + "_VAL_" + std::to_string(i);
        std::string val_v2 = "UPDATED_" + std::to_string(i);

        // --- STEP 1: INSERT ---
        // (This part is fine, you are checking it)
        std::string q_ins = "EMLA GOWA verify_table ELKEYAM (" + std::to_string(unique_id) + ", '" + val_v1 + "');";
        std::string r_ins = client.Send(q_ins);
        if (r_ins.find("SUCCESS") == std::string::npos && r_ins.find("INSERT") == std::string::npos) {
            LogError("Insert Failed: " + r_ins);
            data_errors++; continue;
        }

        // --- STEP 2: VERIFY INSERT ---
        // (This part is fine)

        // --- STEP 3: UPDATE (FIXED) ---
        std::string q_upd = "3ADEL verify_table 5ALY val = '" + val_v2 + "' LAMA id = " + std::to_string(unique_id) + ";";
        std::string r_upd = client.Send(q_upd); // <--- CAPTURE THE RESPONSE

        // CHECK IT!
        if (r_upd.find("SUCCESS") == std::string::npos && r_upd.find("UPDATE") == std::string::npos) {
            // Log the actual error from the server
            LogError("Update Failed for ID " + std::to_string(unique_id) + " Server said: " + r_upd);
            data_errors++; 
            continue; // Skip verification if update failed
        }

        // --- STEP 4: VERIFY UPDATE ---
        std::string q_sel = "2E5TAR * MEN verify_table LAMA id = " + std::to_string(unique_id) + ";";
        std::string r_sel2 = client.Send(q_sel);
        if (r_sel2.find(val_v2) == std::string::npos) {
            LogError("Thread " + std::to_string(thread_id) + " Updated to '" + val_v2 + "' but Read: " + r_sel2);
            data_errors++; continue;
        }

        success_count++;
    }
    client.Close();
}

void TestConsistencyClient() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    Log("=== FRANCODB DATA INTEGRITY TEST ===");

    // 1. Admin Setup
    {
        FrancoClient admin;
        admin.Connect();
        admin.Send("LOGIN " + USER + " " + PASS + ";");
        admin.Send("2E3MEL DATABASE verify_db;");
        admin.Send("2ESTA5DEM verify_db;");
        admin.Send("2EMSA7 GADWAL verify_table;");
        admin.Send("2E3MEL GADWAL verify_table (id RAKAM, val GOMLA);");
        // Create Index to ensure Reads are fast and test B+Tree correctness
        admin.Send("2E3MEL FEHRIS idx_id 3ALA verify_table (id);");
        admin.Close();
    }

    // 2. Run Threads
    Log("-> Launching " + std::to_string(NUM_THREADS) + " threads validating Read-After-Write consistency...");
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) threads.emplace_back(Worker, i + 1);
    for (auto& t : threads) t.join();

    Log("\n=== INTEGRITY REPORT ===");
    Log("Successful Cycles: " + std::to_string(success_count));
    Log("Data Corruptions:  " + std::to_string(data_errors));

    WSACleanup();
    
}
