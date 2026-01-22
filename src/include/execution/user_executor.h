#pragma once

#include "execution/execution_result.h"
#include "common/auth_manager.h"

namespace francodb {

// Forward declarations
class CreateUserStatement;
class AlterUserRoleStatement;
class DeleteUserStatement;

/**
 * UserExecutor - User management operations
 * 
 * SOLID PRINCIPLE: Single Responsibility
 * This class handles all user account operations:
 * - CREATE USER
 * - ALTER USER ROLE
 * - DELETE USER
 * 
 * @author FrancoDB Team
 */
class UserExecutor {
public:
    explicit UserExecutor(AuthManager* auth_manager)
        : auth_manager_(auth_manager) {}
    
    // ========================================================================
    // USER MANAGEMENT
    // ========================================================================
    
    /**
     * Create a new user account.
     */
    ExecutionResult CreateUser(CreateUserStatement* stmt);
    
    /**
     * Alter a user's role for a specific database.
     */
    ExecutionResult AlterUserRole(AlterUserRoleStatement* stmt);
    
    /**
     * Delete a user account.
     */
    ExecutionResult DeleteUser(DeleteUserStatement* stmt);

private:
    AuthManager* auth_manager_;
};

} // namespace francodb

