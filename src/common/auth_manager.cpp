#include "common/auth_manager.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "storage/table/schema.h"
#include "common/exception.h"
#include "common/franco_net_config.h"
#include <sstream>
#include <functional>
#include <iomanip>
#include <algorithm> // For std::transform

namespace francodb {
    // Bcrypt-style password hashing (iterated with secret pepper from config).
    // NOTE: This is not a full bcrypt implementation, but it mimics the idea:
    //  - combine password + secret pepper
    //  - run through many hash iterations to slow down brute-force attacks
    std::string AuthManager::HashPassword(const std::string &password) {
        std::hash<std::string> hasher;

        // Combine password with secret pepper from config
        std::string data = password + net::PASSWORD_PEPPER;
        size_t hash = 0;

        // Cost factor (number of iterations) – can be tuned
        const int kCost = 10000;
        for (int i = 0; i < kCost; i++) {
            // Mix in previous hash value to make each round depend on the last
            hash = hasher(data + std::to_string(hash));
        }

        std::ostringstream oss;
        oss << std::hex << hash;
        return oss.str();
    }

    AuthManager::AuthManager(BufferPoolManager *system_bpm, Catalog *system_catalog)
        : system_bpm_(system_bpm), system_catalog_(system_catalog), initialized_(false) {
        system_engine_ = new ExecutionEngine(system_bpm_, system_catalog_);
        InitializeSystemDatabase();
        LoadUsers();
    }

    AuthManager::~AuthManager() {
        SaveUsers();
        delete system_engine_;
    }

    bool AuthManager::CheckUserExists(const std::string &username) {
        std::string select_sql = "2E5TAR * MEN franco_users WHERE username = '" + username + "';";
        Lexer lexer(select_sql);
        Parser parser(std::move(lexer));
        try {
            auto stmt = parser.ParseQuery();
            if (!stmt) return false;
            ExecutionResult res = system_engine_->Execute(stmt.get());
            return (res.success && res.result_set && !res.result_set->rows.empty());
        } catch (...) {
            return false;
        }
    }

    void AuthManager::InitializeSystemDatabase() {
        if (initialized_) return;

        if (system_catalog_->GetTable("franco_users") == nullptr) {
            // --- DEFINE SCHEMA HERE ---
            std::vector<Column> user_cols;
            user_cols.emplace_back("username", TypeId::VARCHAR, static_cast<uint32_t>(64), true);
            user_cols.emplace_back("password_hash", TypeId::VARCHAR, static_cast<uint32_t>(128), false);
            user_cols.emplace_back("db_name", TypeId::VARCHAR, static_cast<uint32_t>(64), false);
            user_cols.emplace_back("role", TypeId::VARCHAR, static_cast<uint32_t>(16), false);
            Schema user_schema(user_cols);

            if (system_catalog_->CreateTable("franco_users", user_schema) == nullptr) {
                throw Exception(ExceptionType::EXECUTION, "Failed to create franco_users table");
            }

            system_catalog_->SaveCatalog();
            system_bpm_->FlushAllPages();
        }

        auto &config = ConfigManager::GetInstance();
        std::string root_user = config.GetRootUsername();

        if (!CheckUserExists(root_user)) {
            std::string root_pass = config.GetRootPassword();
            std::string admin_hash = HashPassword(root_pass);
            std::string insert_sql = "EMLA GOWA franco_users ELKEYAM ('" + root_user + "', '" + admin_hash +
                                     "', 'default', 'SUPERADMIN');";

            Lexer lexer(insert_sql);
            Parser parser(std::move(lexer));
            auto stmt = parser.ParseQuery();
            if (stmt) {
                system_engine_->Execute(stmt.get());
            }

            system_catalog_->SaveCatalog();
            system_bpm_->FlushAllPages();
        }

        initialized_ = true;
    }

    void AuthManager::LoadUsers() {
        users_cache_.clear();

        try {
            std::string select_sql = "2E5TAR * MEN franco_users;";
            Lexer lexer(select_sql);
            Parser parser(std::move(lexer));
            auto stmt = parser.ParseQuery();

            if (!stmt) return;

            // If the table is physically broken, Execute will throw an exception
            // instead of letting the program crash with 0xc0000005.
            ExecutionResult res = system_engine_->Execute(stmt.get());

            if (!res.success || !res.result_set) return;

            for (const auto &row: res.result_set->rows) {
                if (row.size() < 4) continue;
                // ... (your existing row parsing logic) ...
            }
        } catch (const std::exception &e) {
            std::cerr << "[CRITICAL] AuthManager failed to load users: " << e.what() << std::endl;
            // If we can't load users, we shouldn't crash; we just have an empty cache.
            // The root user will still work because Authenticate() checks IsRoot() first.
        }
    }

    void AuthManager::SaveUsers() {
        // We no longer call "2EMSA7 MEN franco_users" (DELETE) here.
        // That was the "Corruption Window."

        for (const auto &[username, user]: users_cache_) {
            for (const auto &[db, role]: user.db_roles) {
                std::string role_str;
                switch (role) {
                    case UserRole::SUPERADMIN: role_str = "SUPERADMIN";
                        break;
                    case UserRole::ADMIN: role_str = "ADMIN";
                        break;
                    case UserRole::USER: role_str = "USER";
                        break;
                    case UserRole::READONLY: role_str = "READONLY";
                        break;
                    case UserRole::DENIED: role_str = "DENIED";
                        break;
                }

                // USE "UPSERT" LOGIC: Try to insert, if it fails because user exists, it's fine.
                // In a more advanced engine, we would use UPDATE franco_users SET role = ...
                std::string insert_sql = "EMLA GOWA franco_users ELKEYAM ('" +
                                         username + "', '" +
                                         user.password_hash + "', '" +
                                         db + "', '" +
                                         role_str + "');";

                try {
                    Lexer lexer(insert_sql);
                    Parser parser(std::move(lexer));
                    auto stmt = parser.ParseQuery();
                    if (stmt) {
                        system_engine_->Execute(stmt.get());
                    }
                } catch (...) {
                    // If it fails (e.g. duplicate key), we just move on.
                }
            }
        }

        // CRITICAL: Flush immediately after saving users. 
        // Don't wait for the destructor!
        system_catalog_->SaveCatalog();
        system_bpm_->FlushAllPages();
    }

    // Helper: check if user is root/superadmin (from config)
    static bool IsRoot(const std::string &username) {
        auto &config = ConfigManager::GetInstance();
        return username == config.GetRootUsername();
    }

    // Updated Authenticate: only checks password
    bool AuthManager::Authenticate(const std::string &username, const std::string &password, UserRole &out_role) {
        // Check if root user first (before loading users)
        if (IsRoot(username)) {
            auto &config = ConfigManager::GetInstance();
            std::string input_hash = HashPassword(password);
            std::string expected_hash = HashPassword(config.GetRootPassword());
            if (input_hash == expected_hash) {
                out_role = UserRole::SUPERADMIN; // Root is always SUPERADMIN
                return true;
            }
            return false;
        }
        LoadUsers();
        auto it = users_cache_.find(username);
        if (it == users_cache_.end()) return false;
        std::string input_hash = HashPassword(password);
        if (input_hash != it->second.password_hash) return false;
        out_role = UserRole::READONLY; // role is per-db, so default here
        return true;
    }

    // Check if user is SUPERADMIN
    bool AuthManager::IsSuperAdmin(const std::string &username) {
        if (IsRoot(username)) return true; // maayn is always SUPERADMIN
        LoadUsers();
        auto it = users_cache_.find(username);
        if (it == users_cache_.end()) return false;
        // Check if user has SUPERADMIN role in any database
        for (const auto &[db, role]: it->second.db_roles) {
            if (role == UserRole::SUPERADMIN) return true;
        }
        return false;
    }

    // Per-db role getter
    UserRole AuthManager::GetUserRole(const std::string &username, const std::string &db_name) {
        if (IsSuperAdmin(username)) return UserRole::SUPERADMIN; // SUPERADMIN has SUPERADMIN role in all databases
        LoadUsers();
        auto it = users_cache_.find(username);
        if (it == users_cache_.end()) return UserRole::DENIED;
        auto role_it = it->second.db_roles.find(db_name);
        if (role_it == it->second.db_roles.end()) return UserRole::DENIED;
        return role_it->second;
    }

    // Per-db role setter
    bool AuthManager::SetUserRole(const std::string &username, const std::string &db_name, UserRole role) {
        if (IsRoot(username)) return false; // maayn (superadmin) cannot be changed
        LoadUsers();
        auto it = users_cache_.find(username);
        if (it == users_cache_.end()) {
            // User doesn't exist in cache, create entry
            UserInfo new_user;
            new_user.username = username;
            new_user.password_hash = ""; // Will be set when user is created
            users_cache_[username] = new_user;
            it = users_cache_.find(username);
        }
        it->second.db_roles[db_name] = role;
        SaveUsers();
        return true;
    }

    // CreateUser: set default role for 'default' db
    bool AuthManager::CreateUser(const std::string &username, const std::string &password, UserRole role) {
        LoadUsers();
        if (users_cache_.find(username) != users_cache_.end()) return false;
        UserInfo new_user;
        new_user.username = username;
        new_user.password_hash = HashPassword(password);
        new_user.db_roles["default"] = role;
        users_cache_[username] = new_user;
        SaveUsers();
        return true;
    }

    // GetAllUsers: return all users from cache
    std::vector<UserInfo> AuthManager::GetAllUsers() {
        LoadUsers();
        std::vector<UserInfo> result;
        for (const auto &[username, user]: users_cache_) {
            result.push_back(user);
        }
        return result;
    }

    // DeleteUser: remove user from system
    bool AuthManager::DeleteUser(const std::string &username) {
        if (IsRoot(username)) return false; // Cannot delete root/superadmin
        LoadUsers();
        auto it = users_cache_.find(username);
        if (it == users_cache_.end()) return false;
        users_cache_.erase(it);
        SaveUsers();
        return true;
    }

    // SetUserRole: set role for current/default database (single parameter version)
    bool AuthManager::SetUserRole(const std::string &username, UserRole new_role) {
        if (IsRoot(username)) return false; // Cannot change root
        LoadUsers();
        auto it = users_cache_.find(username);
        if (it == users_cache_.end()) return false;
        // Set role for "default" database
        it->second.db_roles["default"] = new_role;
        SaveUsers();
        return true;
    }

    // GetUserRole: get role for default database (single parameter version)
    UserRole AuthManager::GetUserRole(const std::string &username) {
        return GetUserRole(username, "default");
    }

    // Check if user has access to a database (SUPERADMIN always has access)
    bool AuthManager::HasDatabaseAccess(const std::string &username, const std::string &db_name) {
        if (IsSuperAdmin(username)) return true; // SUPERADMIN has access to all databases
        UserRole role = GetUserRole(username, db_name);
        return role != UserRole::DENIED;
    }

    // HasPermission: deny all for DENIED, always allow for SUPERADMIN and ADMIN
    bool AuthManager::HasPermission(UserRole role, StatementType stmt_type) {
        if (role == UserRole::DENIED) return false;
        if (role == UserRole::SUPERADMIN || role == UserRole::ADMIN) return true;
        switch (role) {
            case UserRole::USER:
                switch (stmt_type) {
                    case StatementType::SELECT:
                    case StatementType::INSERT:
                    case StatementType::UPDATE_CMD:
                    case StatementType::CREATE_INDEX:
                    case StatementType::BEGIN:
                    case StatementType::COMMIT:
                    case StatementType::ROLLBACK:
                        return true;
                    case StatementType::DROP:
                    case StatementType::DELETE_CMD:
                    case StatementType::CREATE:
                    case StatementType::CREATE_DB:
                        return false;
                    default:
                        return false;
                }
            case UserRole::READONLY:
                return (stmt_type == StatementType::SELECT);
            default:
                return false;
        }
    }
} // namespace francodb
