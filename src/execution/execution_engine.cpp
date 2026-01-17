#include "execution/execution_engine.h"
#include "execution/executors/insert_executor.h"
#include "execution/executors/seq_scan_executor.h"
#include "execution/executors/delete_executor.h"
#include "execution/executors/index_scan_executor.h"
#include "execution/executors/update_executor.h"
#include "common/exception.h"
#include <algorithm>
#include <sstream>
#include <filesystem>

#include "common/auth_manager.h"
#include "../include/network/session_context.h"

namespace francodb {
    struct SessionContext;

    SessionContext *g_session_context = nullptr;

    ExecutionEngine::ExecutionEngine(BufferPoolManager *bpm, Catalog *catalog, AuthManager *auth_manager)
        : catalog_(catalog), bpm_(bpm), auth_manager_(auth_manager),
          exec_ctx_(new ExecutorContext(catalog, bpm)),
          current_transaction_(nullptr), next_txn_id_(1), in_explicit_transaction_(false) {
    }

    ExecutionEngine::~ExecutionEngine() {
        if (current_transaction_) delete current_transaction_;
        delete exec_ctx_;
    }

    Transaction *ExecutionEngine::GetCurrentTransaction() { return current_transaction_; }

    Transaction *ExecutionEngine::GetCurrentTransactionForWrite() {
        if (current_transaction_ == nullptr) {
            current_transaction_ = new Transaction(next_txn_id_++);
        }
        return current_transaction_;
    }

    void ExecutionEngine::AutoCommitIfNeeded() {
        if (!in_explicit_transaction_ && current_transaction_ != nullptr &&
            current_transaction_->GetState() == Transaction::TransactionState::RUNNING) {
            ExecuteCommit(); // We can ignore result of auto-commit
        }
    }

    // --- MAIN EXECUTE DISPATCHER ---
    // --- MAIN EXECUTE DISPATCHER ---
    ExecutionResult ExecutionEngine::Execute(Statement *stmt, SessionContext *session) {
        if (stmt == nullptr) return ExecutionResult::Error("Empty Statement");

        try {
            ExecutionResult res;
            switch (stmt->GetType()) {
                // --- DDL ---
                case StatementType::CREATE_INDEX: res = ExecuteCreateIndex(dynamic_cast<CreateIndexStatement *>(stmt)); break;
                case StatementType::CREATE: res = ExecuteCreate(dynamic_cast<CreateStatement *>(stmt)); break;
                case StatementType::DROP: res = ExecuteDrop(dynamic_cast<DropStatement *>(stmt)); break;
                
                // --- USER MANAGEMENT ---
                case StatementType::CREATE_USER: 
                    res = ExecuteCreateUser(dynamic_cast<CreateUserStatement *>(stmt)); 
                    break;
                case StatementType::ALTER_USER_ROLE: // [FIX] Added Case
                    res = ExecuteAlterUserRole(dynamic_cast<AlterUserRoleStatement *>(stmt)); 
                    break;
                case StatementType::DELETE_USER:
                    res = ExecuteDeleteUser(dynamic_cast<DeleteUserStatement *>(stmt));
                    break;

                // --- DML ---
                case StatementType::INSERT: res = ExecuteInsert(dynamic_cast<InsertStatement *>(stmt)); break;
                case StatementType::SELECT: res = ExecuteSelect(dynamic_cast<SelectStatement *>(stmt)); break;
                case StatementType::DELETE_CMD: res = ExecuteDelete(dynamic_cast<DeleteStatement *>(stmt)); break;
                case StatementType::UPDATE_CMD: res = ExecuteUpdate(dynamic_cast<UpdateStatement *>(stmt)); break;

                // --- SYSTEM & METADATA ---
                case StatementType::SHOW_DATABASES: res = ExecuteShowDatabases(dynamic_cast<ShowDatabasesStatement *>(stmt), session); break;
                case StatementType::SHOW_TABLES: res = ExecuteShowTables(dynamic_cast<ShowTablesStatement *>(stmt), session); break;
                case StatementType::SHOW_STATUS: res = ExecuteShowStatus(dynamic_cast<ShowStatusStatement *>(stmt), session); break;
                case StatementType::WHOAMI: res = ExecuteWhoAmI(dynamic_cast<WhoAmIStatement *>(stmt), session); break;
                case StatementType::SHOW_USERS: res = ExecuteShowUsers(dynamic_cast<ShowUsersStatement *>(stmt)); break;

                // --- TRANSACTIONS ---
                case StatementType::BEGIN: res = ExecuteBegin(); break;
                case StatementType::ROLLBACK: res = ExecuteRollback(); break;
                case StatementType::COMMIT: res = ExecuteCommit(); break;
                
                // --- DB MGMT ---
                case StatementType::CREATE_DB:
                     // Usually handled in connection handler, but if here:
                     res = ExecutionResult::Message("Create DB should be handled by System/Handler level for now.");
                     break;
                case StatementType::USE_DB:
                     res = ExecutionResult::Message("Use DB should be handled by Connection Handler.");
                     break;

                default: return ExecutionResult::Error("Unknown Statement Type in Engine.");
            }

            // Auto-commit
            if (stmt->GetType() == StatementType::INSERT || stmt->GetType() == StatementType::UPDATE_CMD || stmt->GetType() == StatementType::DELETE_CMD) {
                AutoCommitIfNeeded();
            }
            return res;
        } catch (const std::exception &e) {
            return ExecutionResult::Error(e.what());
        }
    }

    std::string ExecutionEngine::ValueToString(const Value &v) {
        std::ostringstream oss;
        oss << v;
        return oss.str();
    }


    ExecutionResult ExecutionEngine::ExecuteCreate(CreateStatement *stmt) {
        Schema schema(stmt->columns_);
        bool success = catalog_->CreateTable(stmt->table_name_, schema);
        if (!success) return ExecutionResult::Error("Table already exists: " + stmt->table_name_);
        return ExecutionResult::Message("CREATE TABLE SUCCESS");
    }

    ExecutionResult ExecutionEngine::ExecuteCreateIndex(CreateIndexStatement *stmt) {
        auto *index = catalog_->CreateIndex(stmt->index_name_, stmt->table_name_, stmt->column_name_);
        if (index == nullptr) return ExecutionResult::Error("Failed to create index");
        return ExecutionResult::Message("CREATE INDEX SUCCESS");
    }

    ExecutionResult ExecutionEngine::ExecuteInsert(InsertStatement *stmt) {
        InsertExecutor executor(exec_ctx_, stmt, GetCurrentTransactionForWrite());
        executor.Init();
        Tuple t;
        int count = 0;
        while (executor.Next(&t)) count++; // Usually 1 for single insert
        return ExecutionResult::Message("INSERT 1"); // Assuming simple insert
    }

    ExecutionResult ExecutionEngine::ExecuteSelect(SelectStatement *stmt) {
        AbstractExecutor *executor = nullptr;
        bool use_index = false;

        // Optimizer Logic (Simplified)
        if (!stmt->where_clause_.empty() && stmt->where_clause_[0].op == "=") {
            auto &cond = stmt->where_clause_[0];
            auto indexes = catalog_->GetTableIndexes(stmt->table_name_);
            for (auto *idx: indexes) {
                if (idx->col_name_ == cond.column && idx->b_plus_tree_) {
                    try {
                        executor = new IndexScanExecutor(exec_ctx_, stmt, idx, cond.value, GetCurrentTransaction());
                        use_index = true;
                        break;
                    } catch (...) {
                    }
                }
            }
        }

        if (!use_index) {
            executor = new SeqScanExecutor(exec_ctx_, stmt, GetCurrentTransaction());
        }

        try {
            executor->Init();
        } catch (...) {
            if (use_index) {
                // Fallback
                delete executor;
                executor = new SeqScanExecutor(exec_ctx_, stmt, GetCurrentTransaction());
                executor->Init();
            } else {
                throw;
            }
        }

        // --- POPULATE RESULT SET ---
        auto rs = std::make_shared<ResultSet>();
        const Schema *output_schema = executor->GetOutputSchema();

        // 1. Column Headers
        for (const auto &col: output_schema->GetColumns()) {
            rs->column_names.push_back(col.GetName());
        }

        // 2. Rows
        Tuple t;
        while (executor->Next(&t)) {
            std::vector<std::string> row_strings;
            for (uint32_t i = 0; i < output_schema->GetColumnCount(); ++i) {
                row_strings.push_back(ValueToString(t.GetValue(*output_schema, i)));
            }
            rs->AddRow(row_strings);
        }
        delete executor;

        return ExecutionResult::Data(rs);
    }

    ExecutionResult ExecutionEngine::ExecuteDrop(DropStatement *stmt) {
        if (!catalog_->DropTable(stmt->table_name_)) return ExecutionResult::Error("Table not found");
        return ExecutionResult::Message("DROP TABLE SUCCESS");
    }

    ExecutionResult ExecutionEngine::ExecuteDelete(DeleteStatement *stmt) {
        DeleteExecutor executor(exec_ctx_, stmt, GetCurrentTransactionForWrite());
        executor.Init();
        Tuple t;
        executor.Next(&t); // The executor prints log internally currently, but we can change that later
        return ExecutionResult::Message("DELETE SUCCESS");
    }

    ExecutionResult ExecutionEngine::ExecuteUpdate(UpdateStatement *stmt) {
        UpdateExecutor executor(exec_ctx_, stmt, GetCurrentTransactionForWrite());
        executor.Init();
        Tuple t;
        executor.Next(&t);
        return ExecutionResult::Message("UPDATE SUCCESS");
    }

    ExecutionResult ExecutionEngine::ExecuteBegin() {
        if (current_transaction_ && in_explicit_transaction_) return ExecutionResult::Error("Transaction in progress");
        if (current_transaction_) ExecuteCommit();
        current_transaction_ = new Transaction(next_txn_id_++);
        in_explicit_transaction_ = true;
        return ExecutionResult::Message(
            "BEGIN TRANSACTION " + std::to_string(current_transaction_->GetTransactionId()));
    }

    ExecutionResult ExecutionEngine::ExecuteRollback() {
        if (!current_transaction_ || !in_explicit_transaction_)
            return ExecutionResult::Error(
                "No transaction to rollback");

        // ... (Keep existing rollback logic here, copied from your previous file) ...
        // ... Copy the reverse iteration logic here ...
        const auto &modifications = current_transaction_->GetModifications();
        std::vector<std::pair<RID, Transaction::TupleModification> > mods_vec;
        for (const auto &[rid, mod]: modifications) mods_vec.push_back({rid, mod});
        std::reverse(mods_vec.begin(), mods_vec.end());

        for (const auto &[rid, mod]: mods_vec) {
            if (mod.table_name.empty()) continue;
            TableMetadata *table_info = catalog_->GetTable(mod.table_name);
            if (!table_info) continue;

            if (mod.is_deleted) {
                // Undo Delete
                table_info->table_heap_->UnmarkDelete(rid, nullptr);
                auto indexes = catalog_->GetTableIndexes(mod.table_name);
                for (auto *index: indexes) {
                    int col_idx = table_info->schema_.GetColIdx(index->col_name_);
                    if (col_idx >= 0) {
                        GenericKey<8> key;
                        key.SetFromValue(mod.old_tuple.GetValue(table_info->schema_, col_idx));
                        index->b_plus_tree_->Insert(key, rid, nullptr);
                    }
                }
            } else if (mod.old_tuple.GetLength() == 0) {
                // Undo Insert
                Tuple current;
                if (table_info->table_heap_->GetTuple(rid, &current, nullptr)) {
                    auto indexes = catalog_->GetTableIndexes(mod.table_name);
                    for (auto *index: indexes) {
                        int col_idx = table_info->schema_.GetColIdx(index->col_name_);
                        if (col_idx >= 0) {
                            GenericKey<8> key;
                            key.SetFromValue(current.GetValue(table_info->schema_, col_idx));
                            index->b_plus_tree_->Remove(key, nullptr);
                        }
                    }
                }
                table_info->table_heap_->MarkDelete(rid, nullptr);
            } else {
                // Undo Update
                table_info->table_heap_->UnmarkDelete(rid, nullptr);
                // Note: In a real system we'd restore values. 
                // For now we just unmark delete assuming update was delete+insert
            }
        }

        current_transaction_->SetState(Transaction::TransactionState::ABORTED);
        delete current_transaction_;
        current_transaction_ = nullptr;
        in_explicit_transaction_ = false;
        return ExecutionResult::Message("ROLLBACK SUCCESS");
    }

    ExecutionResult ExecutionEngine::ExecuteCommit() {
        if (current_transaction_) {
            current_transaction_->SetState(Transaction::TransactionState::COMMITTED);
            delete current_transaction_;
            current_transaction_ = nullptr;
        }
        in_explicit_transaction_ = false;
        return ExecutionResult::Message("COMMIT SUCCESS");
    }

    ExecutionResult ExecutionEngine::ExecuteWhoAmI(WhoAmIStatement *stmt, SessionContext *session) {
        auto rs = std::make_shared<ResultSet>();
        rs->column_names = {"Current User", "Current DB", "Role"};

        std::string role_str = "USER";
        if (session->role == UserRole::SUPERADMIN) role_str = "SUPERADMIN";
        else if (session->role == UserRole::ADMIN) role_str = "ADMIN";
        else if (session->role == UserRole::READONLY) role_str = "READONLY";

        rs->AddRow({session->current_user, session->current_db, role_str});
        return ExecutionResult::Data(rs);
    }

    ExecutionResult ExecutionEngine::ExecuteShowDatabases(ShowDatabasesStatement *stmt, SessionContext *session) {
        auto rs = std::make_shared<ResultSet>();
        rs->column_names.push_back("Database");

        // 1. Always show 'default' if access allowed
        if (auth_manager_->HasDatabaseAccess(session->current_user, "default")) {
            rs->AddRow({"default"});
        }

        // 2. Scan Directory
        namespace fs = std::filesystem;
        auto &config = ConfigManager::GetInstance();
        std::string data_dir = config.GetDataDirectory();

        if (fs::exists(data_dir)) {
            for (const auto &entry: fs::directory_iterator(data_dir)) {
                if (entry.is_directory()) {
                    std::string db_name = entry.path().filename().string();
                    if (db_name == "system" || db_name == "default") continue;

                    // 3. Security Check
                    if (auth_manager_->HasDatabaseAccess(session->current_user, db_name)) {
                        rs->AddRow({db_name});
                    }
                }
            }
        }
        return ExecutionResult::Data(rs);
    }

    ExecutionResult ExecutionEngine::ExecuteShowUsers(ShowUsersStatement *stmt) {
        std::vector<UserInfo> users = auth_manager_->GetAllUsers();
        auto rs = std::make_shared<ResultSet>();
        rs->column_names = {"Username", "Roles"};

        for (const auto &user: users) {
            std::string roles_desc;
            for (const auto &[db, role]: user.db_roles) {
                if (!roles_desc.empty()) roles_desc += ", ";
                roles_desc += db + ":";
                switch (role) {
                    case UserRole::SUPERADMIN: roles_desc += "SUPER"; break;
                    case UserRole::ADMIN: roles_desc += "ADMIN"; break;
                    case UserRole::NORMAL: roles_desc += "NORMAL"; break;
                    case UserRole::READONLY: roles_desc += "READONLY"; break;
                    case UserRole::DENIED: roles_desc += "DENIED"; break;
                    default: roles_desc += "UNKNOWN"; break;
                }
            }
            rs->AddRow({user.username, roles_desc});
        }
        return ExecutionResult::Data(rs);
    }

    ExecutionResult ExecutionEngine::ExecuteCreateUser(CreateUserStatement *stmt) {
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


    ExecutionResult ExecutionEngine::ExecuteShowStatus(ShowStatusStatement *stmt, SessionContext *session) {
        auto rs = std::make_shared<ResultSet>();
        rs->column_names = {"Variable", "Value"};

        // 1. Current User
        rs->AddRow({"Current User", session->current_user.empty() ? "Guest" : session->current_user});

        // 2. Current Database
        rs->AddRow({"Current Database", session->current_db});

        // 3. Current Role
        std::string role_str;
        switch (session->role) {
            case UserRole::SUPERADMIN: role_str = "SUPERADMIN (Full Access)";
                break;
            case UserRole::ADMIN: role_str = "ADMIN (Read/Write)";
                break;
            case UserRole::NORMAL: role_str = "NORMAL (Read/Write)";
                break;
            case UserRole::READONLY: role_str = "READONLY (Select Only)";
                break;
            case UserRole::DENIED: role_str = "DENIED (No Access)";
                break;
        }
        rs->AddRow({"Current Role", role_str});

        // 4. Auth Status
        rs->AddRow({"Authenticated", session->is_authenticated ? "Yes" : "No"});

        return ExecutionResult::Data(rs);
    }

    ExecutionResult ExecutionEngine::ExecuteShowTables(ShowTablesStatement *stmt, SessionContext *session) {
        auto rs = std::make_shared<ResultSet>();
        rs->column_names = {"Tables_in_" + session->current_db};

        // Strategy: We scan the physical directory of the CURRENT database.
        // This is robust because it reflects the 'USE DB' command immediately.

        auto &config = ConfigManager::GetInstance();
        std::filesystem::path db_path = std::filesystem::path(config.GetDataDirectory()) / session->current_db;

        if (!std::filesystem::exists(db_path)) {
            // Fallback: If using default/system, check catalog memory
            if (session->current_db == "default") {
                std::vector<std::string> tables = catalog_->GetAllTableNames();
                for (const auto &tbl: tables) rs->AddRow({tbl});
                return ExecutionResult::Data(rs);
            }
            return ExecutionResult::Error("Database directory not found: " + session->current_db);
        }

        // Scan the folder for .table files (or however your DiskManager stores them)
        // Assuming file structure: data/dbname/tablename.table
        try {
            std::vector<std::string> table_names;
            for (const auto &entry: std::filesystem::directory_iterator(db_path)) {
                if (entry.is_regular_file()) {
                    std::string fname = entry.path().filename().string();
                    // Assuming tables are stored as "tablename.table" or just pages
                    // If you store metadata separately, adjust this filter.
                    // For now, we assume any file in the DB folder represents a table object
                    // OR check against the catalog if catalog supports multi-db.

                    // Simple filter: Ignore .log or .meta files
                    if (fname.find(".log") == std::string::npos && fname.find(".meta") == std::string::npos) {
                        // Strip extension if present
                        size_t lastindex = fname.find_last_of(".");
                        std::string raw_name = (lastindex == std::string::npos) ? fname : fname.substr(0, lastindex);
                        table_names.push_back(raw_name);
                    }
                }
            }

            // Sort for nice output
            std::sort(table_names.begin(), table_names.end());
            for (const auto &name: table_names) {
                rs->AddRow({name});
            }
        } catch (...) {
            return ExecutionResult::Error("Failed to scan table directory.");
        }

        return ExecutionResult::Data(rs);
    }
    
    
    
    ExecutionResult ExecutionEngine::ExecuteAlterUserRole(AlterUserRoleStatement *stmt) {
        // 1. Convert String Role to Enum
        UserRole r = UserRole::NORMAL; // Default
        
        std::string role_upper = stmt->role_;
        std::transform(role_upper.begin(), role_upper.end(), role_upper.begin(), ::toupper);

        if (role_upper == "SUPERADMIN") r = UserRole::SUPERADMIN;
        else if (role_upper == "ADMIN") r = UserRole::ADMIN;
        else if (role_upper == "NORMAL") r = UserRole::NORMAL;
        else if (role_upper == "READONLY") r = UserRole::READONLY;
        else if (role_upper == "DENIED") r = UserRole::DENIED;
        else return ExecutionResult::Error("Invalid Role: " + stmt->role_);

        // 2. Determine Target DB (Default to "default" if not specified)
        std::string target_db = stmt->db_name_.empty() ? "default" : stmt->db_name_;

        // 3. Call AuthManager
        if (auth_manager_->SetUserRole(stmt->username_, target_db, r)) {
            return ExecutionResult::Message("User role updated successfully for DB: " + target_db);
        }
        
        return ExecutionResult::Error("Failed to update user role (User might not exist or is Root).");
    }
    
    ExecutionResult ExecutionEngine::ExecuteDeleteUser(DeleteUserStatement *stmt) {
        // Safety: Prevent deleting the current user to avoid locking yourself out?
        // (Optional check, but AuthManager::DeleteUser handles Root protection already)
        
        if (auth_manager_->DeleteUser(stmt->username_)) {
            return ExecutionResult::Message("User '" + stmt->username_ + "' deleted successfully.");
        }
        return ExecutionResult::Error("Failed to delete user (User might not exist or is Root).");
    }
    
    
    
    
} // namespace francodb
