/**
 * user_executor.cpp
 *
 * Implementation of User Management Operations
 * Extracted from ExecutionEngine to satisfy Single Responsibility Principle.
 *
 * Security: All user management operations require SUPERADMIN permission.
 *
 * @author ChronosDB Team
 */

#include "execution/user_executor.h"
#include "parser/statement.h"
#include "network/session_context.h"
#include "common/config_manager.h"
#include <algorithm>

namespace chronosdb {

// ============================================================================
// PERMISSION CHECK
// ============================================================================
bool UserExecutor::HasSuperAdminPermission(SessionContext* session) const {
    if (!session) return false;
    if (!session->is_authenticated) return false;
    return session->role == UserRole::SUPERADMIN;
}

// ============================================================================
// CREATE USER
// ============================================================================
ExecutionResult UserExecutor::CreateUser(CreateUserStatement* stmt, SessionContext* session) {
    if (!stmt) {
        return ExecutionResult::Error("[User] Invalid CREATE USER statement");
    }

    // Permission check - only SUPERADMIN can create users
    if (!HasSuperAdminPermission(session)) {
        return ExecutionResult::Error("[User] Permission denied: CREATE USER requires SUPERADMIN role");
    }

    if (!auth_manager_) {
        return ExecutionResult::Error("[User] Auth manager not initialized");
    }

    // Check if user already exists
    if (auth_manager_->CheckUserExists(stmt->username_)) {
        return ExecutionResult::Error("[User] User '" + stmt->username_ + "' already exists");
    }

    // Parse role string (case-insensitive)
    UserRole r = UserRole::NORMAL;
    std::string role_upper = stmt->role_;
    std::transform(role_upper.begin(), role_upper.end(), role_upper.begin(), ::toupper);

    if (role_upper == "SUPERADMIN") r = UserRole::SUPERADMIN;
    else if (role_upper == "ADMIN") r = UserRole::ADMIN;
    else if (role_upper == "READONLY") r = UserRole::READONLY;
    else if (role_upper == "DENIED") r = UserRole::DENIED;
    else if (role_upper == "USER" || role_upper == "NORMAL" || role_upper.empty()) r = UserRole::NORMAL;
    else {
        return ExecutionResult::Error("[User] Invalid role: " + stmt->role_ +
            ". Valid roles: SUPERADMIN, ADMIN, NORMAL, READONLY, DENIED");
    }

    if (auth_manager_->CreateUser(stmt->username_, stmt->password_, r)) {
        return ExecutionResult::Message("User '" + stmt->username_ + "' created successfully with role " + role_upper + ".");
    }
    return ExecutionResult::Error("[User] Failed to create user '" + stmt->username_ + "'. Internal error.");
}

// ============================================================================
// ALTER USER ROLE
// ============================================================================
ExecutionResult UserExecutor::AlterUserRole(AlterUserRoleStatement* stmt, SessionContext* session) {
    if (!stmt) {
        return ExecutionResult::Error("[User] Invalid ALTER USER statement");
    }

    // Permission check - only SUPERADMIN can alter user roles
    if (!HasSuperAdminPermission(session)) {
        return ExecutionResult::Error("[User] Permission denied: ALTER USER ROLE requires SUPERADMIN role");
    }

    if (!auth_manager_) {
        return ExecutionResult::Error("[User] Auth manager not initialized");
    }

    // Check if target user exists
    if (!auth_manager_->CheckUserExists(stmt->username_)) {
        return ExecutionResult::Error("[User] User '" + stmt->username_ + "' does not exist");
    }

    // Check if trying to modify root user
    std::string root_username = ConfigManager::GetInstance().GetRootUsername();
    if (stmt->username_ == root_username) {
        return ExecutionResult::Error("[User] Cannot modify the root user's role");
    }

    // Parse role string (case-insensitive)
    UserRole r = UserRole::NORMAL;
    std::string role_upper = stmt->role_;
    std::transform(role_upper.begin(), role_upper.end(), role_upper.begin(), ::toupper);

    if (role_upper == "SUPERADMIN") r = UserRole::SUPERADMIN;
    else if (role_upper == "ADMIN") r = UserRole::ADMIN;
    else if (role_upper == "NORMAL" || role_upper == "USER") r = UserRole::NORMAL;
    else if (role_upper == "READONLY") r = UserRole::READONLY;
    else if (role_upper == "DENIED") r = UserRole::DENIED;
    else {
        return ExecutionResult::Error("[User] Invalid role: " + stmt->role_ +
            ". Valid roles: SUPERADMIN, ADMIN, NORMAL, READONLY, DENIED");
    }

    // Use session's current database if no database specified, fallback to "default"
    std::string target_db = stmt->db_name_;
    if (target_db.empty()) {
        target_db = (session && !session->current_db.empty()) ? session->current_db : "default";
    }

    if (auth_manager_->SetUserRole(stmt->username_, target_db, r)) {
        return ExecutionResult::Message("User '" + stmt->username_ + "' role updated to " +
            role_upper + " for database '" + target_db + "'.");
    }

    return ExecutionResult::Error("[User] Failed to update user role. Internal error.");
}

// ============================================================================
// DELETE USER
// ============================================================================
ExecutionResult UserExecutor::DeleteUser(DeleteUserStatement* stmt, SessionContext* session) {
    if (!stmt) {
        return ExecutionResult::Error("[User] Invalid DELETE USER statement");
    }

    // Permission check - only SUPERADMIN can delete users
    if (!HasSuperAdminPermission(session)) {
        return ExecutionResult::Error("[User] Permission denied: DELETE USER requires SUPERADMIN role");
    }

    if (!auth_manager_) {
        return ExecutionResult::Error("[User] Auth manager not initialized");
    }

    // Check if user exists
    if (!auth_manager_->CheckUserExists(stmt->username_)) {
        return ExecutionResult::Error("[User] User '" + stmt->username_ + "' does not exist");
    }

    if (auth_manager_->DeleteUser(stmt->username_)) {
        return ExecutionResult::Message("User '" + stmt->username_ + "' deleted successfully.");
    }
    return ExecutionResult::Error("[User] Cannot delete root user or internal error occurred.");
}

} // namespace chronosdb
