#include <iostream>
#include <string>
#include <algorithm>
#include "network/franco_client.h"
#include "common/franco_net_config.h"

using namespace francodb;

void PrintWelcome() {
    std::cout << "==========================================" << std::endl;
    std::cout << "            FrancoDB Shell v2.0           " << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Connection string format:" << std::endl;
    std::cout << "  maayn://user:pass@host:port/dbname" << std::endl;
    std::cout << "  maayn://user:pass@host/dbname     (default port 2501)" << std::endl;
    std::cout << "  maayn://user:pass@host            (no database)" << std::endl;
    std::cout << "Or enter credentials manually." << std::endl;
    std::cout << "Commands: exit | USE <db>; | CREATE DATABASE <db>; | SELECT/INSERT/..." << std::endl;
}

int main(int argc, char* argv[]) {
    FrancoClient db_client;
    std::string username = net::DEFAULT_ADMIN_USERNAME;
    std::string current_db = "default";
    bool connected = false;

    PrintWelcome();

    // Check if connection string provided as argument
    if (argc > 1) {
        std::string conn_str = argv[1];
        // Trim leading colons if present
        while (!conn_str.empty() && (conn_str.front() == ':' || conn_str.front() == ' ' || conn_str.front() == '\t')) {
            conn_str.erase(0, 1);
        }
        
        // Find connection string if it appears anywhere
        size_t conn_start = conn_str.find("maayn://");
        if (conn_start != std::string::npos) {
            // Extract the connection string
            if (conn_start != 0) {
                conn_str = conn_str.substr(conn_start);
            }
            
            // Try to connect - if fails, exit (NO fallback)
            if (!db_client.ConnectFromString(conn_str)) {
                std::cerr << "[FATAL] Invalid connection string or connection failed." << std::endl;
                std::cerr << "Make sure:" << std::endl;
                std::cerr << "  1. The server is running (francodb_server)" << std::endl;
                std::cerr << "  2. The connection string format is correct" << std::endl;
                std::cerr << "  3. The server is listening on port 2501" << std::endl;
                return 1;
            }
            
            connected = true;
            // Extract username from connection string for display
            size_t user_start = conn_str.find("://") + 3;
            size_t user_end = conn_str.find('@', user_start);
            if (user_end != std::string::npos) {
                username = conn_str.substr(user_start, user_end - user_start);
                size_t colon = username.find(':');
                if (colon != std::string::npos) {
                    username = username.substr(0, colon);
                }
            }
            // Extract database from connection string
            size_t at_pos = conn_str.find('@');
            size_t db_start = conn_str.find_last_of('/');
            if (at_pos != std::string::npos &&
                db_start != std::string::npos &&
                db_start > at_pos &&
                db_start + 1 < conn_str.length()) {
                current_db = conn_str.substr(db_start + 1);
            }
        } else {
            // Argument provided but not a connection string - fail
            std::cerr << "[FATAL] Invalid connection string format: " << argv[1] << std::endl;
            std::cerr << "Expected format: maayn://user:pass@host:port/dbname" << std::endl;
            return 1;
        }
    }

    // Manual connection if not connected via argument
    if (!connected) {
        // No argument provided - go directly to manual entry (skip connection string prompt)
        std::string password;
        std::string host;
        std::string port_str;
        int port = net::DEFAULT_PORT;
        std::cout << "\nUsername: ";
        if (!std::getline(std::cin, username)) {
            std::cerr << "\n[INFO] Connection cancelled." << std::endl;
            return 0;
        }
        std::cout << "Password: ";
        if (!std::getline(std::cin, password)) {
            std::cerr << "\n[INFO] Connection cancelled." << std::endl;
            return 0;
        }
        std::cout << "Host (empty = default): ";
        if (!std::getline(std::cin, host)) {
            std::cerr << "\n[INFO] Connection cancelled." << std::endl;
            return 0;
        }
        if (host.empty()) host = net::DEFAULT_SERVER_IP;
        std::cout << "Port (empty = default): ";
        if (!std::getline(std::cin, port_str)) {
            std::cerr << "\n[INFO] Connection cancelled." << std::endl;
            return 0;
        }
        if (!port_str.empty()) {
            try {
                port = std::stoi(port_str);
            } catch (...) {
                std::cerr << "[WARN] Invalid port, using default." << std::endl;
                port = net::DEFAULT_PORT;
            }
        }
        if (!db_client.Connect(host, port, username, password)) {
            std::cerr << "[FATAL] Could not connect/authenticate to FrancoDB server." << std::endl;
            return 1;
        }
        connected = true;
    }

    std::string input;
    while (true) {
        std::cout << username << "@" << current_db << "> ";
        std::cout.flush(); // Ensure prompt is displayed
        if (!std::getline(std::cin, input)) {
            // Handle EOF/Ctrl+C gracefully
            std::cout << "\nGoodbye!" << std::endl;
            break;
        }
        if (input == "exit" || input == "quit") {
            std::cout << "Goodbye!" << std::endl;
            break;
        }
        if (input.empty()) continue;

        // Track USE <db>;
        if (input.rfind("USE ", 0) == 0 || input.rfind("2ESTA5DEM ", 0) == 0) {
            // naive parse: USE <db>;
            std::string db = input.substr(input.find(' ') + 1);
            if (!db.empty() && db.back() == ';') db.pop_back();
            // Trim whitespace
            while (!db.empty() && (db.front() == ' ' || db.front() == '\t')) db.erase(0, 1);
            while (!db.empty() && (db.back() == ' ' || db.back() == '\t')) db.pop_back();
            current_db = db;
        }

        // Use the Driver API
        std::string response = db_client.Query(input);
        
        // Update current_db if USE command was successful
        if (response.rfind("Using database: ", 0) == 0) {
            current_db = response.substr(16); // "Using database: ".length()
            current_db.erase(std::remove(current_db.begin(), current_db.end(), '\n'), current_db.end());
        }
        
        std::cout << response << std::endl;
    }

    db_client.Disconnect();
    return 0;
}