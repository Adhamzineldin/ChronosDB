/**
 * user_executor.cpp
 * 
 * Implementation of User Management Operations
 * Extracted from ExecutionEngine to satisfy Single Responsibility Principle.
 * 
 * @author FrancoDB Team
 */

#include "execution/user_executor.h"
#include "parser/statement.h"
#include <algorithm>

namespace francodb {

// ============================================================================
// CREATE USER
// ============================================================================
ExecutionResult UserExecutor::CreateUser(CreateUserStatement* stmt) {
    if (!stmt) {
        return ExecutionResult::Error("[User] Invalid CREATE USER statement");
    }
    
    UserRole r = UserRole::NORMAL;
    std::string role_upper = stmt->role_;
    std::transform(role_upper.begin(), role_upper.end(), role_upper.begin(), ::toupper);

    if (role_upper == "ADMIN") r = UserRole::ADMIN;
    else if (role_upper == "SUPERADMIN") r = UserRole::SUPERADMIN;
    else if (role_upper == "READONLY") r = UserRole::READONLY;

    if (auth_manager_->CreateUser(stmt->username_, stmt->password_, r)) {
        return ExecutionResult::Message("User created successfully.");
    }
    return ExecutionResult::Error("User creation failed.");
}

// ============================================================================
// ALTER USER ROLE
// ============================================================================
ExecutionResult UserExecutor::AlterUserRole(AlterUserRoleStatement* stmt) {
    if (!stmt) {
        return ExecutionResult::Error("[User] Invalid ALTER USER statement");
    }
    
    UserRole r = UserRole::NORMAL;
    std::string role_upper = stmt->role_;
    std::transform(role_upper.begin(), role_upper.end(), role_upper.begin(), ::toupper);

    if (role_upper == "SUPERADMIN") r = UserRole::SUPERADMIN;
    else if (role_upper == "ADMIN") r = UserRole::ADMIN;
    else if (role_upper == "NORMAL") r = UserRole::NORMAL;
    else if (role_upper == "READONLY") r = UserRole::READONLY;
    else if (role_upper == "DENIED") r = UserRole::DENIED;
    else return ExecutionResult::Error("Invalid Role: " + stmt->role_);

    // Default to "francodb" if no database specified
    std::string target_db = stmt->db_name_.empty() ? "francodb" : stmt->db_name_;

    if (auth_manager_->SetUserRole(stmt->username_, target_db, r)) {
        return ExecutionResult::Message("User role updated successfully for DB: " + target_db);
    }

    return ExecutionResult::Error("Failed to update user role (User might not exist or is Root).");
}

// ============================================================================
// DELETE USER
// ============================================================================
ExecutionResult UserExecutor::DeleteUser(DeleteUserStatement* stmt) {
    if (!stmt) {
        return ExecutionResult::Error("[User] Invalid DELETE USER statement");
    }
    
    if (auth_manager_->DeleteUser(stmt->username_)) {
        return ExecutionResult::Message("User '" + stmt->username_ + "' deleted successfully.");
    }
    return ExecutionResult::Error("Failed to delete user (User might not exist or is Root).");
}

} // namespace francodb

