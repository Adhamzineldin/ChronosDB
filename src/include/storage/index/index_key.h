#pragma once
#include <cstring>
#include "common/value.h"

namespace francodb {

    // A fixed-size key container for B+Tree
    template <size_t KeySize>
    struct GenericKey {
        char data[KeySize];

        void SetFromValue(const Value& v) {
            std::memset(data, 0, KeySize); // Clear garbage
            if (v.GetTypeId() == TypeId::INTEGER) {
                int32_t val = v.GetAsInteger();
                std::memcpy(data, &val, sizeof(int32_t));
            } else if (v.GetTypeId() == TypeId::DECIMAL) {
                double val = v.GetAsDouble();
                std::memcpy(data, &val, sizeof(double));
            }
        }

        // Operators for B+Tree sorting
        bool operator<(const GenericKey& other) const { return std::memcmp(data, other.data, KeySize) < 0; }
        bool operator>(const GenericKey& other) const { return std::memcmp(data, other.data, KeySize) > 0; }
        bool operator==(const GenericKey& other) const { return std::memcmp(data, other.data, KeySize) == 0; }
    };

    // Comparator helper for B+Tree
    template <size_t KeySize>
    class GenericComparator {
    public:
        GenericComparator(TypeId type = TypeId::INVALID) : type_(type) {} // Default arg fixes constructor issues
        int operator()(const GenericKey<KeySize> &lhs, const GenericKey<KeySize> &rhs) const {
            if (lhs < rhs) return -1;
            if (lhs > rhs) return 1;
            return 0;
        }
        TypeId type_;
    };

} // namespace francodb