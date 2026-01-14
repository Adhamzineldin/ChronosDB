#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace francodb {

class Encryption {
public:
    // Simple XOR encryption (for basic obfuscation)
    // For production, consider using AES-256
    static void EncryptXOR(const std::string& key, char* data, size_t size);
    static void DecryptXOR(const std::string& key, char* data, size_t size);
    
    // Convert hex string to bytes
    static std::vector<uint8_t> HexToBytes(const std::string& hex);
    static std::string BytesToHex(const std::vector<uint8_t>& bytes);
    
    // Generate encryption key from string (for XOR, we use the key directly)
    static std::vector<uint8_t> DeriveKey(const std::string& key_str, size_t key_size = 32);
};

} // namespace francodb
