#pragma once

#include <string>
#include <vector>
#include <cstring>
#include "common/rid.h"
#include "common/value.h" // <--- CRITICAL: Need this for Value
#include "storage/table/schema.h" // <--- CRITICAL: Need this for Schema

namespace francodb {
    class Tuple {
    public:
        Tuple() = default;

        // The S-Grade constructors
        Tuple(const std::vector<Value> &values, const Schema &schema);

        Value GetValue(const Schema &schema, uint32_t column_idx) const;

        RID GetRid() const { return rid_; }
        void SetRid(RID rid) { rid_ = rid; }
        char *GetData() { return data_.data(); }
        const char *GetData() const { return data_.data(); }
        uint32_t GetLength() const { return data_.size(); }

        void SerializeTo(char *storage) const { memcpy(storage, data_.data(), data_.size()); }

        void DeserializeFrom(const char *storage, uint32_t size) {
            data_.resize(size);
            memcpy(data_.data(), storage, size);
        }

    private:
        RID rid_;
        std::vector<char> data_;
    };
}
