#include "common/encryption.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace chronosdb {

void Encryption::EncryptXOR(const std::string& key, char* data, size_t size) {
    if (key.empty()) return; // No encryption if key is empty
    
    auto key_bytes = DeriveKey(key, 32);
    size_t key_len = key_bytes.size();
    
    for (size_t i = 0; i < size; ++i) {
        data[i] ^= key_bytes[i % key_len];
    }
}

void Encryption::DecryptXOR(const std::string& key, char* data, size_t size) {
    // XOR is symmetric - encryption and decryption are the same
    EncryptXOR(key, data, size);
}

std::vector<uint8_t> Encryption::HexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::string Encryption::BytesToHex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    for (uint8_t byte : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<uint8_t> Encryption::DeriveKey(const std::string& key_str, size_t key_size) {
    std::vector<uint8_t> key(key_size);
    
    if (key_str.length() >= key_size) {
        // Use first key_size bytes
        std::memcpy(key.data(), key_str.data(), key_size);
    } else {
        // Repeat key to fill key_size
        size_t pos = 0;
        for (size_t i = 0; i < key_size; ++i) {
            key[i] = static_cast<uint8_t>(key_str[pos]);
            pos = (pos + 1) % key_str.length();
        }
    }
    
    return key;
}

} // namespace chronosdb
