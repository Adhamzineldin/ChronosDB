#include "common/config_manager.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace chronosdb {

bool ConfigManager::LoadConfig(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Parse key=value pairs
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Remove quotes if present
        if (value.size() >= 2 && value[0] == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        
        // Set values
        if (key == "port") {
            try { port_ = std::stoi(value); } catch (...) {}
        } else if (key == "root_username") {
            root_username_ = value;
        } else if (key == "root_password") {
            root_password_ = value;
        } else if (key == "data_directory") {
            data_directory_ = value;
        } else if (key == "encryption_enabled") {
            encryption_enabled_ = (value == "true" || value == "1" || value == "yes");
        } else if (key == "encryption_key") {
            encryption_key_ = value;
        } else if (key == "autosave_interval") {
            try { autosave_interval_ = std::stoi(value); } catch (...) {}
        }
    }
    
    return true;
}

bool ConfigManager::SaveConfig(const std::string& config_path) {
    std::ofstream file(config_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# ChronosDB Configuration File\n";
    file << "# Generated automatically\n\n";
    
    file << "port = " << port_ << "\n";
    file << "root_username = \"" << root_username_ << "\"\n";
    file << "root_password = \"" << root_password_ << "\"\n";
    file << "data_directory = \"" << data_directory_ << "\"\n";
    file << "encryption_enabled = " << (encryption_enabled_ ? "true" : "false") << "\n";
    if (!encryption_key_.empty()) {
        file << "encryption_key = \"" << encryption_key_ << "\"\n";
    }
    file << "autosave_interval = " << autosave_interval_ << "\n";
    
    return true;
}

bool ConfigManager::CreateDefaultConfig(const std::string& config_path) {
    if (std::filesystem::exists(config_path)) {
        return false; // Config already exists
    }
    
    return SaveConfig(config_path);
}

std::string ConfigManager::ReadPassword(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();
    
    std::string password;
    
#ifdef _WIN32
    char ch;
    while ((ch = _getch()) != '\r') {
        if (ch == '\b') {
            if (!password.empty()) {
                password.pop_back();
                std::cout << "\b \b";
            }
        } else {
            password += ch;
            std::cout << '*';
        }
    }
    std::cout << std::endl;
#else
    struct termios old_termios, new_termios;
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    new_termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    
    std::getline(std::cin, password);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    std::cout << std::endl;
#endif
    
    return password;
}

std::string ConfigManager::GenerateEncryptionKey() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    return ss.str();
}

void ConfigManager::InteractiveConfig() {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  ChronosDB Configuration Setup" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "\nPlease configure ChronosDB settings:\n" << std::endl;
    
    // Port
    std::cout << "Server port [" << port_ << "]: ";
    std::string input;
    std::getline(std::cin, input);
    if (!input.empty()) {
        try { port_ = std::stoi(input); } catch (...) {}
    }
    
    // Root username
    std::cout << "Root username [" << root_username_ << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        root_username_ = input;
    }
    
    // Root password
    std::string password = ReadPassword("Root password (hidden): ");
    if (!password.empty()) {
        root_password_ = password;
    }
    
    // Data directory
    std::cout << "Data directory [" << data_directory_ << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        data_directory_ = input;
    }
    
    // Encryption
    std::cout << "Enable encryption? (y/n) [" << (encryption_enabled_ ? "y" : "n") << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        encryption_enabled_ = (input[0] == 'y' || input[0] == 'Y');
    }
    
    if (encryption_enabled_) {
        std::cout << "Generate encryption key automatically? (y/n) [y]: ";
        std::getline(std::cin, input);
        if (input.empty() || input[0] == 'y' || input[0] == 'Y') {
            encryption_key_ = GenerateEncryptionKey();
            std::cout << "Generated encryption key: " << encryption_key_ << std::endl;
            std::cout << "WARNING: Save this key securely! You'll need it to access encrypted databases." << std::endl;
        } else {
            encryption_key_ = ReadPassword("Enter encryption key (32 hex characters): ");
        }
    }
    
    // Auto-save interval
    std::cout << "Auto-save interval (seconds) [" << autosave_interval_ << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        try { autosave_interval_ = std::stoi(input); } catch (...) {}
    }
    
    std::cout << "\nConfiguration saved!" << std::endl;
}

} // namespace chronosdb
