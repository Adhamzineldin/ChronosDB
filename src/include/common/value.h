#pragma once

#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include "common/type.h"
#include "common/exception.h"

namespace francodb {

/**
 * Value is a generic container for any type of data in the database.
 * It handles serialization (writing to tuple) and comparisons.
 */
class Value {
public:
    // --- CONSTRUCTORS ---
    
    // Invalid / Null Value
    Value() : type_id_(TypeId::INVALID) { value_.integer_ = 0; }

    // Integer
    Value(TypeId type, int32_t i) : type_id_(type) {
        if (type == TypeId::INTEGER) {
            value_.integer_ = i;
            size_ = 4;
        } else if (type == TypeId::BOOLEAN) {
            value_.boolean_ = (i != 0);
            size_ = 1;
        }
    }

    // String (Varchar)
    Value(TypeId type, const std::string &val) : type_id_(type) {
        size_ = val.length();
        // Allocate memory for string
        char *data = new char[size_ + 1];
        memcpy(data, val.c_str(), size_);
        data[size_] = '\0';
        value_.varlen_ = data;
    }

    // Copy Constructor (Deep Copy for Strings)
    Value(const Value &other) {
        type_id_ = other.type_id_;
        size_ = other.size_;
        if (type_id_ == TypeId::VARCHAR) {
            char *data = new char[size_ + 1];
            memcpy(data, other.value_.varlen_, size_);
            data[size_] = '\0';
            value_.varlen_ = data;
        } else {
            value_ = other.value_;
        }
    }

    // Destructor (Clean up string memory)
    ~Value() {
        if (type_id_ == TypeId::VARCHAR && value_.varlen_ != nullptr) {
            delete[] value_.varlen_;
        }
    }

    // --- ACCESSORS ---
    
    TypeId GetTypeId() const { return type_id_; }

    int32_t GetAsInteger() const {
        return value_.integer_;
    }

    std::string GetAsString() const {
        if (type_id_ == TypeId::VARCHAR) {
            return std::string(value_.varlen_, size_);
        } else if (type_id_ == TypeId::INTEGER) {
            return std::to_string(value_.integer_);
        } else if (type_id_ == TypeId::BOOLEAN) {
            return value_.boolean_ ? "true" : "false";
        }
        return "";
    }

    // --- SERIALIZATION ---
    
    // Write this Value into the raw tuple data at specific offset
    void SerializeTo(char *storage) const {
        if (type_id_ == TypeId::VARCHAR) {
            // For Varchar, we write: [Length (4B)] [Chars...]
            uint32_t len = size_;
            memcpy(storage, &len, sizeof(uint32_t));
            memcpy(storage + sizeof(uint32_t), value_.varlen_, size_);
        } else {
            // Fixed length (Int/Bool)
            // Just copy the union bytes directly
            if (type_id_ == TypeId::INTEGER) {
                memcpy(storage, &value_.integer_, sizeof(int32_t));
            } else if (type_id_ == TypeId::BOOLEAN) {
                int8_t b = value_.boolean_ ? 1 : 0;
                memcpy(storage, &b, sizeof(int8_t));
            }
        }
    }

    // Read a Value from raw tuple data
    static Value DeserializeFrom(const char *storage, TypeId type_id) {
        if (type_id == TypeId::INTEGER) {
            int32_t val = *reinterpret_cast<const int32_t *>(storage);
            return Value(TypeId::INTEGER, val);
        } else if (type_id == TypeId::BOOLEAN) {
            int8_t val = *reinterpret_cast<const int8_t *>(storage);
            return Value(TypeId::BOOLEAN, (int32_t)val);
        } else if (type_id == TypeId::VARCHAR) {
            // Read Length first
            uint32_t len = *reinterpret_cast<const uint32_t *>(storage);
            // Read Chars
            std::string str(storage + sizeof(uint32_t), len);
            return Value(TypeId::VARCHAR, str);
        }
        return Value();
    }
    
    // --- COMPARISON ---
    // Returns true if this == other
    bool CompareEquals(const Value &other) const {
        if (type_id_ != other.type_id_) return false;
        
        if (type_id_ == TypeId::INTEGER) {
            return value_.integer_ == other.value_.integer_;
        }
        if (type_id_ == TypeId::VARCHAR) {
            // String compare
            return GetAsString() == other.GetAsString();
        }
        return false;
    }

private:
    TypeId type_id_;
    uint32_t size_; // Length of data

    // The Union holds the raw data efficiently
    union Val {
        int32_t integer_;
        bool boolean_;
        double decimal_;
        char *varlen_; // Pointer for strings
    } value_;
};

} // namespace francodb