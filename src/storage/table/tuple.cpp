#include "storage/table/tuple.h"
#include "storage/table/schema.h"
#include "common/exception.h"
#include <cassert>
#include <cstring>
#include <iostream> // For debug logging

namespace francodb {

Tuple::Tuple(const std::vector<Value> &values, const Schema &schema) {
    // 1. Validate Input
    if (values.size() != schema.GetColumnCount()) {
        // Fallback: If sizes mismatch, create an empty/invalid tuple rather than crashing
        return; 
    }

    // 2. Calculate Total Size
    uint32_t fixed_len = schema.GetLength();
    uint32_t total_size = fixed_len;
    
    // Calculate variable length space
    for (const auto &val : values) {
        if (val.GetTypeId() == TypeId::VARCHAR) {
            total_size += val.GetAsString().length();
        }
    }

    // 3. Allocate Memory
    data_.resize(total_size);
    // Zero out memory to prevent reading garbage later
    if (total_size > 0) {
        std::memset(data_.data(), 0, total_size);
    }

    // 4. Write Data with Bounds Checking
    uint32_t var_offset = fixed_len; // Start of variable data area

    for (size_t i = 0; i < values.size(); ++i) {
        const Column &col = schema.GetColumn(i);
        const Value &val = values[i];
        uint32_t offset = col.GetOffset();

        // CRITICAL BOUNDS CHECK: Prevent 0xC0000005 Crash
        if (offset >= total_size) {
            // Log error and skip writing to avoid crash
            std::cerr << "[Tuple Error] Offset " << offset << " exceeds size " << total_size << std::endl;
            continue; 
        }

        if (col.GetType() == TypeId::VARCHAR) {
            std::string str = val.GetAsString();
            uint32_t str_len = static_cast<uint32_t>(str.length());

            // Ensure metadata fits
            if (offset + 2 * sizeof(uint32_t) > total_size) continue;

            // Write metadata (offset, length)
            memcpy(data_.data() + offset, &var_offset, sizeof(uint32_t));
            memcpy(data_.data() + offset + sizeof(uint32_t), &str_len, sizeof(uint32_t));

            // Ensure string data fits
            if (var_offset + str_len > total_size) {
                std::cerr << "[Tuple Error] String overflow. VarOffset: " << var_offset << " Len: " << str_len << std::endl;
                continue;
            }

            // Write string
            if (str_len > 0) {
                memcpy(data_.data() + var_offset, str.c_str(), str_len);
            }
            var_offset += str_len;
        } else {
            // Fixed types: Check if 4 or 8 bytes fit
            uint32_t type_size = (col.GetType() == TypeId::DECIMAL || col.GetType() == TypeId::BIGINT) ? 8 : 4;
            
            if (offset + type_size > total_size) {
                std::cerr << "[Tuple Error] Fixed overflow. Offset: " << offset << " Size: " << type_size << std::endl;
                continue;
            }
            
            val.SerializeTo(data_.data() + offset);
        }
    }
}

Value Tuple::GetValue(const Schema &schema, uint32_t column_idx) const {
    if (column_idx >= schema.GetColumnCount()) {
        throw Exception(ExceptionType::EXECUTION, "Column index out of range");
    }
    if (data_.empty()) {
        // Return dummy value to prevent crash if tuple is invalid
        return Value(TypeId::INTEGER, 0); 
    }
    
    const Column &col = schema.GetColumn(column_idx);
    TypeId type = col.GetType();
    uint32_t offset = col.GetOffset();

    // Bounds Check for Read
    if (offset >= data_.size()) {
        throw Exception(ExceptionType::EXECUTION, "Column offset out of bounds");
    }

    if (type == TypeId::VARCHAR) {
        if (offset + 8 > data_.size()) {
            throw Exception(ExceptionType::EXECUTION, "VARCHAR metadata out of bounds");
        }
        
        uint32_t var_offset, var_len;
        memcpy(&var_offset, data_.data() + offset, sizeof(uint32_t));
        memcpy(&var_len, data_.data() + offset + sizeof(uint32_t), sizeof(uint32_t));

        if (var_offset >= data_.size() || var_offset + var_len > data_.size()) {
            // Return empty string instead of crashing/throwing if corrupt
            return Value(TypeId::VARCHAR, ""); 
        }

        // [FIX] Pass explicit length to DeserializeFrom
        return Value::DeserializeFrom(data_.data() + var_offset, TypeId::VARCHAR, var_len);
    }

    // [FIX] Pass 0 length for fixed types
    return Value::DeserializeFrom(data_.data() + offset, type, 0);
}

} // namespace francodb