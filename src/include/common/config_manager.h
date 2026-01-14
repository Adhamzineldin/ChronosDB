#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <filesystem>

namespace francodb {

class ConfigManager {
public:
    static ConfigManager& GetInstance() {
        static ConfigManager instance;
        return instance;
    }

    // Load configuration from file
    bool LoadConfig(const std::string& config_path = "francodb.conf");
    
    // Save configuration to file
    bool SaveConfig(const std::string& config_path = "francodb.conf");
    
    // Create default config file
    bool CreateDefaultConfig(const std::string& config_path = "francodb.conf");
    
    // Getters
    int GetPort() const { return port_; }
    std::string GetRootUsername() const { return root_username_; }
    std::string GetRootPassword() const { return root_password_; }
    std::string GetDataDirectory() const { return data_directory_; }
    bool IsEncryptionEnabled() const { return encryption_enabled_; }
    std::string GetEncryptionKey() const { return encryption_key_; }
    int GetAutoSaveInterval() const { return autosave_interval_; }
    
    // Setters
    void SetPort(int port) { port_ = port; }
    void SetRootUsername(const std::string& username) { root_username_ = username; }
    void SetRootPassword(const std::string& password) { root_password_ = password; }
    void SetDataDirectory(const std::string& dir) { data_directory_ = dir; }
    void SetEncryptionEnabled(bool enabled) { encryption_enabled_ = enabled; }
    void SetEncryptionKey(const std::string& key) { encryption_key_ = key; }
    void SetAutoSaveInterval(int seconds) { autosave_interval_ = seconds; }
    
    // Interactive configuration
    void InteractiveConfig();

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    int port_ = 2501;
    std::string root_username_ = "maayn";
    std::string root_password_ = "root";
    std::string data_directory_ = "data";
    bool encryption_enabled_ = false;
    std::string encryption_key_ = "";
    int autosave_interval_ = 30;
    
    std::string ReadPassword(const std::string& prompt);
    std::string GenerateEncryptionKey();
};

} // namespace francodb
