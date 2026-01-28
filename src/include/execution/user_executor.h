#pragma once

#include "execution/execution_result.h"
#include "common/auth_manager.h"

namespace chronosdb {

// Forward declarations
class CreateUserStatement;
class AlterUserRoleStatement;
class DeleteUserStatement;
struct SessionContext;

/**
 * UserExecutor - User management operations
 *
 * SOLID PRINCIPLE: Single Responsibility
 * This class handles all user account operations:
 * - CREATE USER
 * - ALTER USER ROLE
 * - DELETE USER
 *
 * Security: Only SUPERADMIN users can execute these operations.
 *
 * @author ChronosDB Team
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
     * Requires SUPERADMIN permission.
     */
    ExecutionResult CreateUser(CreateUserStatement* stmt, SessionContext* session);

    /**
     * Alter a user's role for a specific database.
     * Requires SUPERADMIN permission.
     */
    ExecutionResult AlterUserRole(AlterUserRoleStatement* stmt, SessionContext* session);

    /**
     * Delete a user account.
     * Requires SUPERADMIN permission.
     */
    ExecutionResult DeleteUser(DeleteUserStatement* stmt, SessionContext* session);

private:
    AuthManager* auth_manager_;

    /**
     * Check if session has SUPERADMIN permission.
     */
    bool HasSuperAdminPermission(SessionContext* session) const;
};

} // namespace chronosdb

