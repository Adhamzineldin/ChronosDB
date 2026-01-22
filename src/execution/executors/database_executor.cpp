/**
 * database_executor.cpp
 * 
 * Implementation of Database Management Operations
 * Extracted from ExecutionEngine to satisfy Single Responsibility Principle.
 * 
 * @author FrancoDB Team
 */

#include "execution/database_executor.h"
#include "parser/statement.h"
#include "network/session_context.h"
#include "storage/storage_interface.h"  // For IBufferManager
#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"
#include "common/franco_net_config.h"
#include <filesystem>
#include <algorithm>

namespace francodb {

// ============================================================================
// CREATE DATABASE
// ============================================================================
ExecutionResult DatabaseExecutor::CreateDatabase(CreateDatabaseStatement* stmt, SessionContext* session) {
    if (!stmt) {
        return ExecutionResult::Error("[Database] Invalid CREATE DATABASE statement");
    }
    
    // Check permissions
    if (!auth_manager_->HasPermission(session->role, StatementType::CREATE_DB)) {
        return ExecutionResult::Error("Permission denied: CREATE DATABASE requires ADMIN role");
    }

    try {
        // Protect system/reserved database names
        std::string db_lower = stmt->db_name_;
        std::transform(db_lower.begin(), db_lower.end(), db_lower.begin(), ::tolower);
        if (db_lower == "system" || db_lower == "francodb") {
            return ExecutionResult::Error("Cannot create database with reserved name: " + stmt->db_name_);
        }

        // Check if database already exists
        if (db_registry_->Get(stmt->db_name_) != nullptr) {
            return ExecutionResult::Error("Database '" + stmt->db_name_ + "' already exists");
        }

        // Create the database (initializes DiskManager, BufferPool, Catalog)
        auto entry = db_registry_->GetOrCreate(stmt->db_name_);

        if (!entry) {
            return ExecutionResult::Error("Failed to create database '" + stmt->db_name_ + "'");
        }

        // Create the WAL log directory for this database
        if (log_manager_) {
            log_manager_->CreateDatabaseLog(stmt->db_name_);
        }

        // Grant creator ADMIN rights on this database
        UserRole creator_role = (session->role == UserRole::SUPERADMIN)
                                    ? UserRole::SUPERADMIN
                                    : UserRole::ADMIN;
        auth_manager_->SetUserRole(session->current_user, stmt->db_name_, creator_role);

        return ExecutionResult::Message("Database '" + stmt->db_name_ + "' created successfully");
    } catch (const std::exception& e) {
        return ExecutionResult::Error("Failed to create database: " + std::string(e.what()));
    }
}

// ============================================================================
// USE DATABASE
// ============================================================================
ExecutionResult DatabaseExecutor::UseDatabase(UseDatabaseStatement* stmt, 
                                               SessionContext* session,
                                               IBufferManager** bpm_out,
                                               Catalog** catalog_out) {
    if (!stmt) {
        return ExecutionResult::Error("[Database] Invalid USE DATABASE statement");
    }
    
    // Check access permissions
    if (!auth_manager_->HasDatabaseAccess(session->current_user, stmt->db_name_)) {
        return ExecutionResult::Error("Access denied to database '" + stmt->db_name_ + "'");
    }

    try {
        // Try registry first
        auto entry = db_registry_->Get(stmt->db_name_);

        // If not already loaded, try to load from disk
        if (!entry) {
            namespace fs = std::filesystem;
            auto& config = ConfigManager::GetInstance();
            fs::path db_dir = fs::path(config.GetDataDirectory()) / stmt->db_name_;
            fs::path db_file = db_dir / (stmt->db_name_ + ".francodb");
            if (fs::exists(db_file)) {
                entry = db_registry_->GetOrCreate(stmt->db_name_);
            }
        }

        if (!entry) {
            return ExecutionResult::Error("Database '" + stmt->db_name_ + "' does not exist");
        }

        // Update output pointers
        if (db_registry_->ExternalBpm(stmt->db_name_)) {
            *bpm_out = db_registry_->ExternalBpm(stmt->db_name_);
            *catalog_out = db_registry_->ExternalCatalog(stmt->db_name_);
        } else {
            *bpm_out = entry->bpm.get();
            *catalog_out = entry->catalog.get();
        }

        // Switch the log manager to the new database's WAL file
        if (log_manager_) {
            log_manager_->SwitchDatabase(stmt->db_name_);
        }

        // Update session context
        session->current_db = stmt->db_name_;
        session->role = auth_manager_->GetUserRole(session->current_user, stmt->db_name_);

        if (auth_manager_->IsSuperAdmin(session->current_user)) {
            session->role = UserRole::SUPERADMIN;
        }

        return ExecutionResult::Message("Now using database: " + stmt->db_name_);
    } catch (const std::exception& e) {
        return ExecutionResult::Error("Failed to switch database: " + std::string(e.what()));
    }
}

// ============================================================================
// DROP DATABASE
// ============================================================================
ExecutionResult DatabaseExecutor::DropDatabase(DropDatabaseStatement* stmt, SessionContext* session) {
    if (!stmt) {
        return ExecutionResult::Error("[Database] Invalid DROP DATABASE statement");
    }
    
    // Check permissions
    if (!auth_manager_->HasPermission(session->role, StatementType::DROP_DB)) {
        return ExecutionResult::Error("Permission denied: DROP DATABASE requires ADMIN role");
    }

    try {
        // Protect system/reserved databases
        std::string db_lower = stmt->db_name_;
        std::transform(db_lower.begin(), db_lower.end(), db_lower.begin(), ::tolower);
        if (db_lower == "system" || db_lower == "francodb") {
            return ExecutionResult::Error("Cannot drop system database: " + stmt->db_name_);
        }

        auto entry = db_registry_->Get(stmt->db_name_);

        if (!entry) {
            return ExecutionResult::Error("Database '" + stmt->db_name_ + "' does not exist");
        }

        // Prevent dropping currently active database
        if (session->current_db == stmt->db_name_) {
            return ExecutionResult::Error(
                "Cannot drop currently active database. Switch to another database first.");
        }

        // Flush and close the database
        if (entry->bpm) {
            entry->bpm->FlushAllPages();
        }
        if (entry->catalog) {
            entry->catalog->SaveCatalog();
        }

        // Remove from registry
        db_registry_->Remove(stmt->db_name_);

        // Delete the entire database directory
        auto& config = ConfigManager::GetInstance();
        std::string data_dir = config.GetDataDirectory();
        std::filesystem::path db_dir = std::filesystem::path(data_dir) / stmt->db_name_;

        if (std::filesystem::exists(db_dir) && std::filesystem::is_directory(db_dir)) {
            std::filesystem::remove_all(db_dir);
        }

        return ExecutionResult::Message("Database '" + stmt->db_name_ + "' dropped successfully");
    } catch (const std::exception& e) {
        return ExecutionResult::Error("Failed to drop database: " + std::string(e.what()));
    }
}

} // namespace francodb

