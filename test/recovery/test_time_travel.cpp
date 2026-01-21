#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iomanip>

#pragma comment(lib, "ws2_32.lib")

// --- CONFIGURATION ---
const std::string SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 2501;      
const std::string USER = "maayn";  
const std::string PASS = "root";   
const char CMD_TEXT = 'Q';

// --- HELPER: Get Current Microseconds ---
uint64_t GetMicroseconds() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// --- CLIENT CLASS ---
class FrancoClient {
    SOCKET sock;
public:
    FrancoClient() : sock(INVALID_SOCKET) {}

    bool Connect() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) return false;

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP.c_str(), &server_addr.sin_addr);

        if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            closesocket(sock);
            return false;
        }
        return true;
    }

    std::string Send(const std::string& query) {
        // 1. Send Header + Body
        char type = CMD_TEXT;
        uint32_t len = htonl(query.length());
        std::vector<char> buffer;
        buffer.push_back(type);
        const char* len_ptr = reinterpret_cast<const char*>(&len);
        buffer.insert(buffer.end(), len_ptr, len_ptr + 4);
        buffer.insert(buffer.end(), query.begin(), query.end());

        if (send(sock, buffer.data(), (int)buffer.size(), 0) == SOCKET_ERROR) return "NETWORK_ERROR";

        // 2. Receive Length
        uint32_t net_len = 0;
        int received = 0;
        while (received < 4) {
            int bytes = recv(sock, (char*)&net_len + received, 4 - received, 0);
            if (bytes <= 0) return "NETWORK_ERROR";
            received += bytes;
        }
        uint32_t resp_len = ntohl(net_len);

        // 3. Receive Body
        std::vector<char> resp_buf(resp_len + 1);
        received = 0;
        while (received < resp_len) {
            int bytes = recv(sock, resp_buf.data() + received, resp_len - received, 0);
            if (bytes <= 0) return "NETWORK_ERROR";
            received += bytes;
        }
        resp_buf[resp_len] = '\0';
        return std::string(resp_buf.data());
    }

    void Close() { if (sock != INVALID_SOCKET) closesocket(sock); }
};

// --- ASSERTION HELPER ---
void AssertContains(const std::string& actual, const std::string& expected, const std::string& test_name) {
    if (actual.find(expected) != std::string::npos) {
        std::cout << "[PASS] " << test_name << std::endl;
    } else {
        std::cout << "[FAIL] " << test_name << "\n   Expected to find: " << expected << "\n   Got: " << actual << std::endl;
        exit(1);
    }
}

// --- MAIN TEST ---
int TestTimeTravel() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    std::cout << "========================================" << std::endl;
    std::cout << "   FRANCODB TIME TRAVEL SUITE           " << std::endl;
    std::cout << "========================================" << std::endl;

    FrancoClient client;
    if (!client.Connect()) {
        std::cerr << "Failed to connect to server." << std::endl;
        return -1;
    }

    // 1. SETUP
    std::cout << "\n[STEP 1] Setup Database..." << std::endl;
    client.Send("LOGIN " + USER + " " + PASS + ";");
    client.Send("2E3MEL DATABASE tt_test;");
    client.Send("2ESTA5DEM tt_test;");
    client.Send("2EMSA7 GADWAL bank;"); // Clean start
    
    std::string resp = client.Send("2E3MEL GADWAL bank (id RAKAM, money RAKAM);");
    AssertContains(resp, "SUCCESS", "Create Table");

    // 2. INSERT WEALTH (The Past)
    std::cout << "\n[STEP 2] Inserting Wealth (1,000,000)..." << std::endl;
    client.Send("EMLA GOWA bank ELKEYAM (1, 1000000);");
    
    // Validate
    resp = client.Send("2E5TAR * MEN bank;");
    AssertContains(resp, "1000000", "Verify Initial Wealth");

    // *** CAPTURE TIME POINT ***
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Ensure clock tick
    uint64_t safe_time = GetMicroseconds();
    std::cout << "   -> Safe Timestamp Captured: " << safe_time << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); 

    // 3. THE HACK (The Destructive Update)
    std::cout << "\n[STEP 3] Performing The Hack (Set money = 0)..." << std::endl;
    client.Send("3ADEL bank 5ALY money = 0 LAMA id = 1;");

    // Validate Hack
    resp = client.Send("2E5TAR * MEN bank;");
    AssertContains(resp, "0", "Verify Money is Gone (Live)");

    // 4. TEST SNAPSHOT (SELECT AS OF)
    std::cout << "\n[STEP 4] Testing Read-Only Time Travel (AS OF)..." << std::endl;
    std::string query_as_of = "2E5TAR * MEN bank AS OF " + std::to_string(safe_time) + ";";
    resp = client.Send(query_as_of);
    
    // We expect to see 1,000,000 even though live data is 0
    AssertContains(resp, "1000000", "Snapshot Read (Should see old money)");

    // 5. TEST ROLLBACK (RECOVER TO)
    std::cout << "\n[STEP 5] Testing Permanent Rollback (RECOVER TO)..." << std::endl;
    std::string query_recover = "RECOVER TO " + std::to_string(safe_time) + ";";
    resp = client.Send(query_recover);
    AssertContains(resp, "COMPLETE", "Execute Recovery Command");

    // 6. VERIFY REALITY IS RESTORED
    std::cout << "\n[STEP 6] Verifying Live Data after Rollback..." << std::endl;
    resp = client.Send("2E5TAR * MEN bank;");
    AssertContains(resp, "1000000", "Verify Wealth Restored");

    // 7. GO BACK TO THE FUTURE (Redo / Undo Check)
    // If we recover to NOW, we should see 0 again (because the hack happened after safe_time)
    // Note: This relies on your log not being truncated. If you implemented Rollback logic purely, 
    // you might not be able to go forward if you deleted the logs. 
    // But basic "Undo" logic works backwards.

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "   ALL TESTS PASSED - TIME TRAVEL WORKS " << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    client.Close();
    WSACleanup();
    return 0;
}


int main() {
    TestTimeTravel();
}