#pragma once

#include <string>
#include <vector>
#include "common/rid.h"

namespace chronosdb {

/**
 * ForeignKeyConstraint: Manages referential integrity
 * 
 * SOLID Principles:
 * - Single Responsibility: Only manages FK constraints
 * - Open/Closed: Can be extended with new actions
 * - Interface Segregation: Minimal public interface
 */
class ForeignKeyConstraint {
public:
    enum class Action {
        RESTRICT,      // Prevent deletion/update
        CASCADE,       // Cascade delete/update
        SET_NULL,      // Set to NULL on delete/update
        SET_DEFAULT,   // Set to default value
        NO_ACTION      // Deferred check
    };

    explicit ForeignKeyConstraint(const std::string &name)
        : constraint_name_(name) {}

    // Configuration
    ForeignKeyConstraint& SetColumns(const std::string &local_col, const std::string &ref_col) {
        local_column_ = local_col;
        referenced_column_ = ref_col;
        return *this;
    }

    ForeignKeyConstraint& SetReferencedTable(const std::string &table) {
        referenced_table_ = table;
        return *this;
    }

    ForeignKeyConstraint& SetOnDelete(Action action) {
        on_delete_action_ = action;
        return *this;
    }

    ForeignKeyConstraint& SetOnUpdate(Action action) {
        on_update_action_ = action;
        return *this;
    }

    // Accessors
    const std::string& GetName() const { return constraint_name_; }
    const std::string& GetLocalColumn() const { return local_column_; }
    const std::string& GetReferencedColumn() const { return referenced_column_; }
    const std::string& GetReferencedTable() const { return referenced_table_; }
    Action GetOnDeleteAction() const { return on_delete_action_; }
    Action GetOnUpdateAction() const { return on_update_action_; }

    bool IsValid() const {
        return !constraint_name_.empty() && 
               !local_column_.empty() && 
               !referenced_table_.empty() &&
               !referenced_column_.empty();
    }

private:
    std::string constraint_name_;
    std::string local_column_;
    std::string referenced_table_;
    std::string referenced_column_;
    Action on_delete_action_ = Action::RESTRICT;
    Action on_update_action_ = Action::RESTRICT;
};

} // namespace chronosdb

