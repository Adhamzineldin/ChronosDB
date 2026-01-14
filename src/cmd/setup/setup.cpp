// setup.cpp - FrancoDB Initial Setup Utility
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <random>

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
#include <arpa/inet.h>
typedef int socket_t;
#define INVALID_SOCK -1
#endif

// Check if a port is available
bool IsPortAvailable(int port) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif

    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCK) {
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bool available = (bind(s, (struct sockaddr*)&addr, sizeof(addr)) == 0);

#ifdef _WIN32
    closesocket(s);
    WSACleanup();
#else
    close(s);
#endif

    return available;
}

// Find a free port starting from the given port
int FindFreePort(int startPort = 2501) {
    for (int port = startPort; port < startPort + 1000; ++port) {
        if (IsPortAvailable(port)) {
            return port;
        }
    }
    return -1; // No free port found
}

// Generate encryption key
std::string GenerateEncryptionKey() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    return ss.str();
}

// Create configuration file
bool CreateConfigFile(const std::string& configPath, int port, 
                     const std::string& rootUsername, const std::string& rootPassword,
                     const std::string& dataDir, bool encryptionEnabled, 
                     const std::string& encryptionKey, int autosaveInterval) {
    std::ofstream file(configPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# FrancoDB Configuration File\n";
    file << "# Generated automatically by setup utility\n\n";
    
    file << "port = " << port << "\n";
    file << "root_username = \"" << rootUsername << "\"\n";
    file << "root_password = \"" << rootPassword << "\"\n";
    file << "data_directory = \"" << dataDir << "\"\n";
    file << "encryption_enabled = " << (encryptionEnabled ? "true" : "false") << "\n";
    if (!encryptionKey.empty()) {
        file << "encryption_key = \"" << encryptionKey << "\"\n";
    }
    file << "autosave_interval = " << autosaveInterval << "\n";
    
    return true;
}

int main(int argc, char* argv[]) {
    std::cout << "==========================================" << std::endl;
    std::cout << "  FrancoDB Initial Setup" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << std::endl;

    // Default values
    int port = 2501;
    std::string rootUsername = "maayn";
    std::string rootPassword = "root";
    std::string dataDir = "./data";
    bool encryptionEnabled = false;
    std::string encryptionKey = "";
    int autosaveInterval = 30;
    
    // Parse command line arguments
    bool interactive = true;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
            interactive = false;
        } else if (arg == "--username" && i + 1 < argc) {
            rootUsername = argv[++i];
            interactive = false;
        } else if (arg == "--password" && i + 1 < argc) {
            rootPassword = argv[++i];
            interactive = false;
        } else if (arg == "--data-dir" && i + 1 < argc) {
            dataDir = argv[++i];
            interactive = false;
        } else if (arg == "--encryption") {
            encryptionEnabled = true;
            interactive = false;
        } else if (arg == "--non-interactive") {
            interactive = false;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: francodb_setup [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --port PORT          Server port (default: 2501)" << std::endl;
            std::cout << "  --username USER      Root username (default: maayn)" << std::endl;
            std::cout << "  --password PASS      Root password (default: root)" << std::endl;
            std::cout << "  --data-dir DIR       Data directory (default: data)" << std::endl;
            std::cout << "  --encryption         Enable encryption" << std::endl;
            std::cout << "  --non-interactive    Use defaults without prompts" << std::endl;
            std::cout << "  --help, -h           Show this help message" << std::endl;
            return 0;
        }
    }

    // Check if port is available
    std::cout << "Checking port availability..." << std::endl;
    if (!IsPortAvailable(port)) {
        std::cout << "[WARNING] Port " << port << " is not available." << std::endl;
        int freePort = FindFreePort(port);
        if (freePort != -1) {
            std::cout << "[INFO] Found free port: " << freePort << std::endl;
            if (interactive) {
                std::cout << "Use port " << freePort << " instead? (y/n) [y]: ";
                std::string response;
                std::getline(std::cin, response);
                if (response.empty() || response[0] == 'y' || response[0] == 'Y') {
                    port = freePort;
                } else {
                    std::cout << "[ERROR] Cannot proceed without a free port." << std::endl;
                    return 1;
                }
            } else {
                port = freePort;
                std::cout << "[INFO] Using port " << port << " instead." << std::endl;
            }
        } else {
            std::cout << "[ERROR] No free port found in range " << port << "-" << (port + 1000) << std::endl;
            return 1;
        }
    } else {
        std::cout << "[OK] Port " << port << " is available." << std::endl;
    }

    // Interactive configuration
    if (interactive) {
        std::cout << std::endl;
        std::cout << "Server port [" << port << "]: ";
        std::string input;
        std::getline(std::cin, input);
        if (!input.empty()) {
            try {
                int newPort = std::stoi(input);
                if (IsPortAvailable(newPort)) {
                    port = newPort;
                } else {
                    std::cout << "[WARNING] Port " << newPort << " is not available. Using " << port << " instead." << std::endl;
                }
            } catch (...) {
                std::cout << "[WARNING] Invalid port. Using " << port << " instead." << std::endl;
            }
        }

        std::cout << "Root username [" << rootUsername << "]: ";
        std::getline(std::cin, input);
        if (!input.empty()) {
            rootUsername = input;
        }

        std::cout << "Root password [" << rootPassword << "]: ";
        std::getline(std::cin, input);
        if (!input.empty()) {
            rootPassword = input;
        }

        std::cout << "Data directory [" << dataDir << "]: ";
        std::getline(std::cin, input);
        if (!input.empty()) {
            dataDir = input;
        }

        std::cout << "Enable encryption? (y/n) [n]: ";
        std::getline(std::cin, input);
        if (!input.empty() && (input[0] == 'y' || input[0] == 'Y')) {
            encryptionEnabled = true;
        }
    }

    // Generate encryption key if enabled
    if (encryptionEnabled && encryptionKey.empty()) {
        encryptionKey = GenerateEncryptionKey();
        std::cout << std::endl;
        std::cout << "[INFO] Generated encryption key: " << encryptionKey << std::endl;
        std::cout << "[WARNING] Save this key securely! You'll need it to access encrypted databases." << std::endl;
    }

    // Create data directory
    std::cout << std::endl;
    std::cout << "Creating data directory..." << std::endl;
    try {
        std::filesystem::create_directories(dataDir);
        std::cout << "[OK] Data directory created: " << dataDir << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[ERROR] Failed to create data directory: " << e.what() << std::endl;
        return 1;
    }

    // Create configuration file
    std::string configPath = "francodb.conf";
    std::cout << "Creating configuration file..." << std::endl;
    if (CreateConfigFile(configPath, port, rootUsername, rootPassword, dataDir, 
                        encryptionEnabled, encryptionKey, autosaveInterval)) {
        std::cout << "[OK] Configuration file created: " << configPath << std::endl;
    } else {
        std::cout << "[ERROR] Failed to create configuration file." << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "  Setup Complete!" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Port: " << port << std::endl;
    std::cout << "  Username: " << rootUsername << std::endl;
    std::cout << "  Data Directory: " << dataDir << std::endl;
    std::cout << "  Encryption: " << (encryptionEnabled ? "Enabled" : "Disabled") << std::endl;
    std::cout << std::endl;
    std::cout << "You can now start the server with: francodb_server.exe" << std::endl;
    std::cout << "Or use the shell with: francodb_shell.exe" << std::endl;

    return 0;
}
