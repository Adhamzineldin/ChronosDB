#include "storage/table/tuple.h"
#include "storage/table/schema.h"
#include <cassert>

namespace francodb {

Tuple::Tuple(const std::vector<Value> &values, const Schema &schema) {
    assert(values.size() == schema.GetColumnCount());

    // 1. Calculate Total Size
    // Fixed Size (calculated by Schema) + Size of all Variable Data
    uint32_t tuple_size = schema.GetLength();
    for (const auto &val : values) {
        if (val.GetTypeId() == TypeId::VARCHAR) {
            tuple_size += val.GetAsString().length();
        }
    }

    // 2. Allocate Memory
    data_.resize(tuple_size);
    std::fill(data_.begin(), data_.end(), 0);

    // 3. Write Data
    // 'var_offset' tracks where the next string goes (starting at end of fixed area)
    uint32_t var_offset = schema.GetLength(); 

    for (size_t i = 0; i < values.size(); ++i) {
        const auto &col = schema.GetColumn(i);
        const auto &val = values[i];

        if (col.GetType() == TypeId::VARCHAR) {
            // VARIABLE LENGTH LOGIC:
            // 1. Get string data
            std::string str = val.GetAsString();
            uint32_t str_len = str.length();

            // 2. Write (Offset, Length) pair in the Fixed Area
            uint32_t offset_val = var_offset;
            memcpy(data_.data() + col.GetOffset(), &offset_val, sizeof(uint32_t));
            memcpy(data_.data() + col.GetOffset() + 4, &str_len, sizeof(uint32_t));

            // 3. Write actual string in the Variable Area
            memcpy(data_.data() + var_offset, str.c_str(), str_len);

            // 4. Update Heap Pointer
            var_offset += str_len;

        } else {
            // FIXED LENGTH LOGIC:
            // Just write directly to the offset
            val.SerializeTo(data_.data() + col.GetOffset());
        }
    }
}

Value Tuple::GetValue(const Schema &schema, uint32_t column_idx) const {
    const Column &col = schema.GetColumn(column_idx);
    TypeId type = col.GetType();
    uint32_t offset = col.GetOffset();

    if (type == TypeId::VARCHAR) {
        // Read (Offset, Length) from Fixed Area
        uint32_t var_offset = *reinterpret_cast<const uint32_t *>(data_.data() + offset);
        uint32_t var_len = *reinterpret_cast<const uint32_t *>(data_.data() + offset + 4);

        // Read string from Variable Area
        std::string val_str(data_.data() + var_offset, var_len);
        return Value(TypeId::VARCHAR, val_str);
    } 
    
    // Fixed Type
    return Value::DeserializeFrom(data_.data() + offset, type);
}

} // namespace francodb