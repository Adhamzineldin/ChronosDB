#include "storage/table/column.h"
#include "common/type.h"
#include <sstream>

namespace chronosdb {

Column::Column(std::string name, TypeId type, bool is_primary_key,
               bool is_nullable, bool is_unique)
    : name_(std::move(name)), type_(type), length_(Type::GetTypeSize(type)), 
      column_offset_(0), is_primary_key_(is_primary_key), 
      is_nullable_(is_nullable), is_unique_(is_unique) {
    // Primary keys are NOT nullable by default
    if (is_primary_key_) {
        is_nullable_ = false;
    }
}

Column::Column(std::string name, TypeId type, uint32_t length, 
               bool is_primary_key, bool is_nullable, bool is_unique)
    : name_(std::move(name)), type_(type), length_(length), 
      column_offset_(0), is_primary_key_(is_primary_key), 
      is_nullable_(is_nullable), is_unique_(is_unique) {
    // Primary keys are NOT nullable by default
    if (is_primary_key_) {
        is_nullable_ = false;
    }
}

std::string Column::ToString() const {
    std::stringstream ss;
    ss << name_ << ":" << Type::TypeToString(type_);
    
    if (is_primary_key_) {
        ss << " (PRIMARY KEY)";
    }
    if (is_unique_) {
        ss << " (UNIQUE)";
    }
    if (!is_nullable_) {
        ss << " (NOT NULL)";
    }
    if (default_value_.has_value()) {
        ss << " DEFAULT " << default_value_.value().GetAsString();
    }
    
    return ss.str();
}

bool Column::ValidateValue(const Value& value) const {
    // Check NULL constraint
    if (!is_nullable_ && value.GetTypeId() == TypeId::VARCHAR && value.GetAsString().empty()) {
        return false;
    }
    
    // Check type compatibility
    if (value.GetTypeId() != type_) {
        return false;
    }
    
    return true;
}

} // namespace chronosdb

