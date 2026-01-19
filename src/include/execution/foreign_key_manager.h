#pragma once

#include <string>
#include <vector>
#include <memory>
#include "parser/advanced_statements.h"
#include "storage/table/tuple.h"
#include "catalog/catalog.h"
#include "common/exception.h"

namespace francodb {

    /**
     * ForeignKeyManager: Enforces referential integrity
     * Follows Single Responsibility Principle - only manages FK constraints
     * Dependency Injection: receives Catalog as dependency
     */
    class ForeignKeyManager {
    public:
        explicit ForeignKeyManager(Catalog* catalog) : catalog_(catalog) {
            if (!catalog_) {
                throw Exception(ExceptionType::EXECUTION, "Catalog is required for FK management");
            }
        }

        /**
         * ValidateInsert: Check if INSERT violates any foreign key constraints
         * Returns: true if valid, false if violates constraint
         */
        bool ValidateInsert(const std::string& table_name, const Tuple& tuple) const;

        /**
         * ValidateUpdate: Check if UPDATE violates any foreign key constraints
         * Returns: true if valid, false if violates constraint
         */
        bool ValidateUpdate(const std::string& table_name, const Tuple& old_tuple, 
                           const Tuple& new_tuple) const;

        /**
         * ValidateDelete: Check if DELETE violates any foreign key constraints
         * Returns: true if valid, false if violates constraint
         */
        bool ValidateDelete(const std::string& table_name, const Tuple& tuple) const;

        /**
         * HandleCascadeDelete: Automatically delete related rows on DELETE
         * Used when ON DELETE CASCADE is specified
         */
        bool HandleCascadeDelete(const std::string& table_name, const Tuple& tuple);

        /**
         * HandleCascadeUpdate: Automatically update related rows on UPDATE
         * Used when ON UPDATE CASCADE is specified
         */
        bool HandleCascadeUpdate(const std::string& table_name, const Tuple& old_tuple,
                                const Tuple& new_tuple);

    private:
        Catalog* catalog_;

        // Helper methods
        bool ForeignKeyReferenced(const std::string& table_name, const Tuple& tuple,
                                 const ForeignKeyConstraint& fk) const;
        
        bool ReferencedRowExists(const std::string& ref_table, const std::string& ref_column,
                                const Value& value) const;

        std::vector<ForeignKeyConstraint> GetTableForeignKeys(const std::string& table_name) const;
        
        std::vector<ForeignKeyConstraint> GetReferencingForeignKeys(
            const std::string& table_name) const;
    };

} // namespace francodb

