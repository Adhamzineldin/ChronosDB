/**
 * execution_engine.cpp
 * 
 * Query Execution Coordinator - SOLID Compliant
 * 
 * This file has been completely refactored to follow SOLID principles:
 * - Single Responsibility: Each executor handles one category of operations
 * - Open/Closed: New statement types can be added via the dispatch map
 * - Dependency Inversion: Depends on executor abstractions
 * 
 * The ExecutionEngine now only handles:
 * 1. Concurrency gatekeeper (global lock management)
 * 2. Delegation via dispatch map (no switch/if-else chains)
 * 3. Recovery operations (CHECKPOINT, RECOVER)
 * 
 * @author ChronosDB Team
 */

#include "execution/execution_engine.h"

// Parser (for extended statement types)
#include "parser/statement.h"
#include "parser/extended_statements.h"

// Specialized Executors (SOLID - SRP)
#include "execution/ddl_executor.h"
#include "execution/dml_executor.h"
#include "execution/system_executor.h"
#include "execution/user_executor.h"
#include "execution/database_executor.h"
#include "execution/transaction_executor.h"

// Recovery
#include "recovery/checkpoint_manager.h"
#include "recovery/recovery_manager.h"
#include "recovery/time_travel_engine.h"

// AI Layer
#include "ai/ai_manager.h"
#include "ai/learning/learning_engine.h"
#include "ai/learning/bandit.h"

// Catalog
#include "catalog/table_metadata.h"
#include "catalog/index_info.h"

// Common
#include "common/exception.h"
#include "common/query_history.h"
#include "common/scheduler.h"
#include "common/chronos_net_config.h"
#include "network/replication.h"
#include "ai/index_advisor.h"
#include "ai/query_firewall.h"

// Parser (for inline SQL parsing in EXPORT/IMPORT/CALL)
#include "parser/lexer.h"
#include "parser/parser.h"

#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <filesystem>

namespace chronosdb {
    std::shared_mutex ExecutionEngine::global_lock_;

    // ============================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // ============================================================================

    ExecutionEngine::ExecutionEngine(IBufferManager *bpm, Catalog *catalog,
                                     AuthManager *auth_manager, DatabaseRegistry *db_registry,
                                     LogManager *log_manager, bool manage_ai)
        : bpm_(bpm),
          catalog_(catalog),
          auth_manager_(auth_manager),
          db_registry_(db_registry),
          log_manager_(log_manager),
          exec_ctx_(nullptr),
          manage_ai_(manage_ai),
          next_txn_id_(1) {

        // Create LockManager for row-level locking (CONCURRENCY FIX)
        lock_manager_ = std::make_unique<LockManager>();

        // Create executor context with LockManager
        exec_ctx_ = new ExecutorContext(bpm_, catalog_, nullptr, log_manager_, lock_manager_.get());

        // Initialize all specialized executors (SOLID - SRP)
        ddl_executor_ = std::make_unique<DDLExecutor>(catalog_, log_manager_);
        dml_executor_ = std::make_unique<DMLExecutor>(bpm_, catalog_, log_manager_);
        system_executor_ = std::make_unique<SystemExecutor>(catalog_, auth_manager_, db_registry_);
        user_executor_ = std::make_unique<UserExecutor>(auth_manager_);
        database_executor_ = std::make_unique<DatabaseExecutor>(auth_manager_, db_registry_, log_manager_);
        transaction_executor_ = std::make_unique<TransactionExecutor>(log_manager_, catalog_);

        // Share the atomic counter with transaction executor
        transaction_executor_->SetNextTxnId(&next_txn_id_);

        // Initialize the dispatch map (OCP - Open/Closed Principle)
        InitializeDispatchMap();

        // Initialize AI Layer (only for the server's main engine, not per-request engines)
        if (manage_ai_) {
            CheckpointManager* cp_mgr = nullptr; // Transient — created per-operation
            ai::AIManager::Instance().Initialize(catalog_, bpm_, log_manager_, cp_mgr);
        }
    }

    ExecutionEngine::~ExecutionEngine() {
        if (manage_ai_) {
            ai::AIManager::Instance().Shutdown();
        }
        delete exec_ctx_;
    }

    // ============================================================================
    // DISPATCH MAP INITIALIZATION (Open/Closed Principle)
    // ============================================================================
    // Adding a new statement type only requires adding one line to this map.
    // No modification to Execute() method needed!
    // ============================================================================

    void ExecutionEngine::InitializeDispatchMap() {
        // ----- DDL OPERATIONS -----
        dispatch_map_[StatementType::CREATE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return ddl_executor_->CreateTable(dynamic_cast<CreateStatement *>(s));
        };
        dispatch_map_[StatementType::CREATE_TABLE] = dispatch_map_[StatementType::CREATE];

        dispatch_map_[StatementType::CREATE_INDEX] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return ddl_executor_->CreateIndex(dynamic_cast<CreateIndexStatement *>(s));
        };

        dispatch_map_[StatementType::DROP] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return ddl_executor_->DropTable(dynamic_cast<DropStatement *>(s));
        };

        dispatch_map_[StatementType::DESCRIBE_TABLE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return ddl_executor_->DescribeTable(dynamic_cast<DescribeTableStatement *>(s));
        };

        dispatch_map_[StatementType::SHOW_CREATE_TABLE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return ddl_executor_->ShowCreateTable(dynamic_cast<ShowCreateTableStatement *>(s));
        };

        dispatch_map_[StatementType::ALTER_TABLE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return ddl_executor_->AlterTable(dynamic_cast<AlterTableStatement *>(s));
        };

        // ----- DML OPERATIONS -----
        dispatch_map_[StatementType::INSERT] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return dml_executor_->Insert(dynamic_cast<InsertStatement *>(s), t);
        };

        dispatch_map_[StatementType::SELECT] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return dml_executor_->Select(dynamic_cast<SelectStatement *>(s), ctx, t);
        };

        dispatch_map_[StatementType::UPDATE_CMD] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return dml_executor_->Update(dynamic_cast<UpdateStatement *>(s), t);
        };

        dispatch_map_[StatementType::DELETE_CMD] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return dml_executor_->Delete(dynamic_cast<DeleteStatement *>(s), t);
        };

        // ----- TRANSACTION OPERATIONS -----
        dispatch_map_[StatementType::BEGIN] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return transaction_executor_->Begin();
        };

        dispatch_map_[StatementType::COMMIT] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return transaction_executor_->Commit();
        };

        dispatch_map_[StatementType::ROLLBACK] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return transaction_executor_->Rollback();
        };

        // ----- DATABASE OPERATIONS -----
        dispatch_map_[StatementType::CREATE_DB] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return database_executor_->CreateDatabase(dynamic_cast<CreateDatabaseStatement *>(s), ctx);
        };

        dispatch_map_[StatementType::DROP_DB] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return database_executor_->DropDatabase(dynamic_cast<DropDatabaseStatement *>(s), ctx);
        };

        // ----- USER OPERATIONS -----
        dispatch_map_[StatementType::CREATE_USER] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return user_executor_->CreateUser(dynamic_cast<CreateUserStatement *>(s), ctx);
        };

        dispatch_map_[StatementType::ALTER_USER_ROLE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return user_executor_->AlterUserRole(dynamic_cast<AlterUserRoleStatement *>(s), ctx);
        };

        dispatch_map_[StatementType::DELETE_USER] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return user_executor_->DeleteUser(dynamic_cast<DeleteUserStatement *>(s), ctx);
        };

        // ----- SYSTEM OPERATIONS -----
        dispatch_map_[StatementType::SHOW_DATABASES] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return system_executor_->ShowDatabases(dynamic_cast<ShowDatabasesStatement *>(s), ctx);
        };

        dispatch_map_[StatementType::SHOW_TABLES] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return system_executor_->ShowTables(dynamic_cast<ShowTablesStatement *>(s), ctx);
        };

        dispatch_map_[StatementType::SHOW_STATUS] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return system_executor_->ShowStatus(dynamic_cast<ShowStatusStatement *>(s), ctx);
        };

        dispatch_map_[StatementType::SHOW_USERS] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return system_executor_->ShowUsers(dynamic_cast<ShowUsersStatement *>(s));
        };

        dispatch_map_[StatementType::WHOAMI] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return system_executor_->WhoAmI(dynamic_cast<WhoAmIStatement *>(s), ctx);
        };

        // ----- RECOVERY OPERATIONS -----
        dispatch_map_[StatementType::CHECKPOINT] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return ExecuteCheckpoint();
        };

        dispatch_map_[StatementType::RECOVER] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return ExecuteRecover(dynamic_cast<RecoverStatement *>(s));
        };
        
        // ----- AI LAYER -----
        dispatch_map_[StatementType::SHOW_AI_STATUS] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return system_executor_->ShowAIStatus(dynamic_cast<ShowAIStatusStatement *>(s));
        };

        dispatch_map_[StatementType::SHOW_ANOMALIES] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return system_executor_->ShowAnomalies(dynamic_cast<ShowAnomaliesStatement *>(s));
        };

        dispatch_map_[StatementType::SHOW_EXECUTION_STATS] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return system_executor_->ShowExecutionStats(dynamic_cast<ShowExecutionStatsStatement *>(s));
        };

        // ----- VIEW OPERATIONS -----
        dispatch_map_[StatementType::CREATE_VIEW] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* view_stmt = dynamic_cast<CreateViewStatement*>(s);
            if (!view_stmt) return ExecutionResult::Error("[DDL] Invalid CREATE VIEW statement");
            return ddl_executor_->CreateView(view_stmt->view_name_, view_stmt->select_query_);
        };

        dispatch_map_[StatementType::DROP_VIEW] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* view_stmt = dynamic_cast<DropViewStatement*>(s);
            if (!view_stmt) return ExecutionResult::Error("[DDL] Invalid DROP VIEW statement");
            return ddl_executor_->DropView(view_stmt->view_name_);
        };

        // ----- EXPLAIN / EXPLAIN ANALYZE -----
        dispatch_map_[StatementType::EXPLAIN] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* explain = dynamic_cast<ExplainStatement*>(s);
            if (!explain || !explain->query_statement_) return ExecutionResult::Error("[EXPLAIN] Invalid statement");

            StatementType inner_type = explain->query_statement_->GetType();
            auto rs = std::make_shared<ResultSet>();

            if (inner_type == StatementType::SELECT) {
                auto* select = dynamic_cast<SelectStatement*>(explain->query_statement_.get());
                if (!select) return ExecutionResult::Error("[EXPLAIN] Invalid SELECT");

                // Gather table metadata
                TableMetadata* table_info = catalog_->GetTable(select->table_name_);
                std::string est_rows = table_info ? "~?" : "?";

                // Determine scan strategy
                std::string scan_op = "SEQ SCAN";
                std::string scan_detail = select->table_name_;
                std::string ai_insight = "";

                // Check for index
                if (!select->where_clause_.empty() && select->where_clause_[0].op == "=") {
                    auto indexes = catalog_->GetTableIndexes(select->table_name_);
                    for (auto* idx : indexes) {
                        if (idx->col_name_ == select->where_clause_[0].column) {
                            std::string idx_type_str = (idx->index_type_ == IndexType::HASH) ? "Hash" : "B+ Tree";
                            scan_op = (idx->index_type_ == IndexType::HASH) ? "HASH SCAN" : "INDEX SCAN";
                            scan_detail = idx->name_ + " (" + idx_type_str + ")";
                            est_rows = "~1";
                            break;
                        }
                    }
                }

                // AI insights from UCB1 bandit
                {
                    auto& ai_mgr = ai::AIManager::Instance();
                    if (ai_mgr.IsInitialized() && ai_mgr.GetLearningEngine()) {
                        auto arm_stats = ai_mgr.GetLearningEngine()->GetArmStats();
                        for (const auto& arm : arm_stats) {
                            if (arm.strategy == ai::ScanStrategy::INDEX_SCAN && scan_op != "SEQ SCAN") {
                                std::ostringstream oss;
                                oss << std::fixed << std::setprecision(0)
                                    << "UCB1: " << (arm.average_reward * 100) << "% confidence";
                                ai_insight = oss.str();
                                break;
                            } else if (arm.strategy == ai::ScanStrategy::SEQUENTIAL_SCAN && scan_op == "SEQ SCAN") {
                                std::ostringstream oss;
                                oss << std::fixed << std::setprecision(0)
                                    << "UCB1: " << (arm.average_reward * 100) << "% confidence";
                                ai_insight = oss.str();
                                break;
                            }
                        }
                        if (ai_insight.empty()) ai_insight = "No AI data yet";
                    } else {
                        ai_insight = "AI inactive";
                    }
                }

                if (explain->analyze_) {
                    // EXPLAIN ANALYZE: actually run the query and measure timing
                    rs->column_names = {"Step", "Operation", "Est. Rows", "Act. Rows", "Time (ms)", "AI Insight"};

                    auto start = std::chrono::high_resolution_clock::now();
                    auto result = dml_executor_->Select(select, ctx, t);
                    auto end = std::chrono::high_resolution_clock::now();
                    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

                    std::string actual_rows = "0";
                    if (result.success && result.result_set) {
                        actual_rows = std::to_string(result.result_set->rows.size());
                    }

                    std::ostringstream time_oss;
                    time_oss << std::fixed << std::setprecision(2) << elapsed_ms;

                    rs->AddRow({"1", scan_op, est_rows, actual_rows, time_oss.str(), ai_insight});

                    if (!select->where_clause_.empty()) {
                        std::string filter_detail = select->where_clause_[0].column + " "
                                                    + select->where_clause_[0].op + " ?";
                        rs->AddRow({"2", "FILTER", est_rows, actual_rows, "0.00", "Selectivity: " + actual_rows + "/" + est_rows});
                    }

                    // Total row
                    rs->AddRow({"Total", "", "", actual_rows, time_oss.str() + "ms", ""});
                } else {
                    // EXPLAIN: just show the plan
                    rs->column_names = {"Step", "Operation", "Details", "Est. Rows", "AI Insight"};
                    rs->AddRow({"1", scan_op, scan_detail, est_rows, ai_insight});

                    if (!select->where_clause_.empty()) {
                        std::string filter_detail = select->where_clause_[0].column + " "
                                                    + select->where_clause_[0].op + " ?";
                        rs->AddRow({"2", "FILTER", filter_detail, est_rows, ""});
                    }
                }
            } else {
                rs->column_names = {"Step", "Operation", "Details"};
                rs->AddRow({"1", "EXECUTE", "Statement type " + std::to_string(static_cast<int>(inner_type))});
            }

            return ExecutionResult::Data(rs);
        };

        // ----- EXPORT/IMPORT -----
        dispatch_map_[StatementType::EXPORT_TABLE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<ExportStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid EXPORT statement");
            // Export table to CSV
            TableMetadata* table_info = catalog_->GetTable(stmt->table_name_);
            if (!table_info) return ExecutionResult::Error("Table not found: " + stmt->table_name_);

            std::ofstream file(stmt->file_path_);
            if (!file.is_open()) return ExecutionResult::Error("Cannot open file: " + stmt->file_path_);

            // Write header
            auto cols = table_info->schema_.GetColumns();
            for (size_t i = 0; i < cols.size(); i++) {
                if (i > 0) file << ",";
                file << cols[i].GetName();
            }
            file << "\n";

            // Write data using sequential scan
            auto select_sql = "SELECT * FROM " + stmt->table_name_ + ";";
            Lexer lex(select_sql);
            Parser parser(std::move(lex));
            auto select_stmt = parser.ParseQuery();
            if (select_stmt) {
                auto result = dml_executor_->Select(dynamic_cast<SelectStatement*>(select_stmt.get()), ctx, t);
                if (result.success && result.result_set) {
                    for (auto& row : result.result_set->rows) {
                        for (size_t i = 0; i < row.size(); i++) {
                            if (i > 0) file << ",";
                            // CSV escape: quote fields containing commas or quotes
                            std::string val = row[i];
                            if (val.find(',') != std::string::npos || val.find('"') != std::string::npos) {
                                std::string escaped;
                                for (char c : val) {
                                    if (c == '"') escaped += "\"\"";
                                    else escaped += c;
                                }
                                file << "\"" << escaped << "\"";
                            } else {
                                file << val;
                            }
                        }
                        file << "\n";
                    }
                }
            }
            file.close();
            return ExecutionResult::Message("Table '" + stmt->table_name_ + "' exported to " + stmt->file_path_);
        };

        dispatch_map_[StatementType::IMPORT_TABLE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<ImportStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid IMPORT statement");

            TableMetadata* table_info = catalog_->GetTable(stmt->table_name_);
            if (!table_info) return ExecutionResult::Error("Table not found: " + stmt->table_name_);

            std::ifstream file(stmt->file_path_);
            if (!file.is_open()) return ExecutionResult::Error("Cannot open file: " + stmt->file_path_);

            // Skip header line
            std::string header_line;
            std::getline(file, header_line);

            int imported = 0;
            std::string line;
            while (std::getline(file, line)) {
                if (line.empty()) continue;
                // Parse CSV line into values
                std::vector<std::string> values;
                std::string current;
                bool in_quotes = false;
                for (size_t i = 0; i < line.size(); i++) {
                    if (line[i] == '"') {
                        if (in_quotes && i + 1 < line.size() && line[i+1] == '"') {
                            current += '"';
                            i++;
                        } else {
                            in_quotes = !in_quotes;
                        }
                    } else if (line[i] == ',' && !in_quotes) {
                        values.push_back(current);
                        current.clear();
                    } else {
                        current += line[i];
                    }
                }
                values.push_back(current);

                // Build INSERT SQL
                std::string sql = "INSERT INTO " + stmt->table_name_ + " VALUES (";
                auto cols = table_info->schema_.GetColumns();
                for (size_t i = 0; i < values.size() && i < cols.size(); i++) {
                    if (i > 0) sql += ", ";
                    if (cols[i].GetType() == TypeId::VARCHAR) {
                        sql += "'" + values[i] + "'";
                    } else {
                        sql += values[i];
                    }
                }
                sql += ");";

                Lexer lex(sql);
                Parser parser(std::move(lex));
                auto insert_stmt = parser.ParseQuery();
                if (insert_stmt) {
                    dml_executor_->Insert(dynamic_cast<InsertStatement*>(insert_stmt.get()), t);
                    imported++;
                }
            }
            return ExecutionResult::Message("Imported " + std::to_string(imported) + " rows into " + stmt->table_name_);
        };

        // ----- BACKUP/RESTORE -----
        dispatch_map_[StatementType::BACKUP_DB] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<BackupStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid BACKUP statement");

            namespace fs = std::filesystem;
            try {
                // Flush everything first
                bpm_->FlushAllPages();
                if (catalog_) catalog_->SaveCatalog();
                if (log_manager_) log_manager_->Flush(true);

                fs::create_directories(stmt->file_path_);

                // Copy database files
                std::string db_name = ctx ? ctx->current_db : "chronosdb";
                auto& config = ConfigManager::GetInstance();
                fs::path src_dir = fs::path(config.GetDataDirectory()) / db_name;

                if (fs::exists(src_dir)) {
                    for (auto& entry : fs::recursive_directory_iterator(src_dir)) {
                        fs::path rel = fs::relative(entry.path(), src_dir);
                        fs::path dest = fs::path(stmt->file_path_) / rel;
                        if (entry.is_directory()) {
                            fs::create_directories(dest);
                        } else {
                            fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
                        }
                    }
                }
                return ExecutionResult::Message("Database backed up to: " + stmt->file_path_);
            } catch (const std::exception& e) {
                return ExecutionResult::Error("Backup failed: " + std::string(e.what()));
            }
        };

        dispatch_map_[StatementType::RESTORE_DB] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<RestoreStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid RESTORE statement");

            namespace fs = std::filesystem;
            try {
                std::string db_name = ctx ? ctx->current_db : "chronosdb";
                auto& config = ConfigManager::GetInstance();
                fs::path dest_dir = fs::path(config.GetDataDirectory()) / db_name;
                fs::path src_dir = fs::path(stmt->file_path_);

                if (!fs::exists(src_dir)) {
                    return ExecutionResult::Error("Backup path not found: " + stmt->file_path_);
                }

                for (auto& entry : fs::recursive_directory_iterator(src_dir)) {
                    fs::path rel = fs::relative(entry.path(), src_dir);
                    fs::path dest = dest_dir / rel;
                    if (entry.is_directory()) {
                        fs::create_directories(dest);
                    } else {
                        fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
                    }
                }

                // Reload catalog
                if (catalog_) catalog_->LoadCatalog();

                return ExecutionResult::Message("Database restored from: " + stmt->file_path_);
            } catch (const std::exception& e) {
                return ExecutionResult::Error("Restore failed: " + std::string(e.what()));
            }
        };

        // ----- STORED PROCEDURES -----
        dispatch_map_[StatementType::CREATE_PROCEDURE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<CreateProcedureStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid CREATE PROCEDURE statement");

            ProcedureInfo proc;
            proc.name = stmt->name_;
            for (auto& p : stmt->params_) {
                std::string type_str;
                switch (p.type) {
                    case TypeId::INTEGER: type_str = "INT"; break;
                    case TypeId::DECIMAL: type_str = "FLOAT"; break;
                    case TypeId::VARCHAR: type_str = "VARCHAR"; break;
                    case TypeId::BOOLEAN: type_str = "BOOLEAN"; break;
                    default: type_str = "VARCHAR"; break;
                }
                proc.parameters.emplace_back(p.name, type_str);
            }
            proc.body = stmt->body_;

            if (!catalog_->CreateProcedure(stmt->name_, proc)) {
                return ExecutionResult::Error("Procedure '" + stmt->name_ + "' already exists");
            }
            catalog_->SaveCatalog();
            return ExecutionResult::Message("Procedure '" + stmt->name_ + "' created");
        };

        dispatch_map_[StatementType::CALL_PROCEDURE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<CallProcedureStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid CALL statement");

            ProcedureInfo* proc = catalog_->GetProcedure(stmt->name_);
            if (!proc) return ExecutionResult::Error("Procedure not found: " + stmt->name_);

            // Execute the procedure body as a sequence of SQL statements
            // Split by semicolons and execute each
            std::string body = proc->body;
            std::istringstream stream(body);
            std::string line;
            ExecutionResult last_result = ExecutionResult::Message("OK");

            while (std::getline(stream, line, ';')) {
                // Trim whitespace
                size_t start = line.find_first_not_of(" \t\n\r");
                if (start == std::string::npos) continue;
                line = line.substr(start);
                if (line.empty()) continue;

                line += ";";
                try {
                    Lexer lex(line);
                    Parser parser(std::move(lex));
                    auto inner_stmt = parser.ParseQuery();
                    if (inner_stmt) {
                        last_result = this->Execute(inner_stmt.get(), ctx);
                    }
                } catch (...) {
                    // Continue executing remaining statements
                }
            }
            return last_result;
        };

        dispatch_map_[StatementType::DROP_PROCEDURE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<DropProcedureStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid DROP PROCEDURE statement");
            if (!catalog_->DropProcedure(stmt->name_)) {
                return ExecutionResult::Error("Procedure not found: " + stmt->name_);
            }
            catalog_->SaveCatalog();
            return ExecutionResult::Message("Procedure '" + stmt->name_ + "' dropped");
        };

        // ----- TRIGGERS -----
        dispatch_map_[StatementType::CREATE_TRIGGER] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<CreateTriggerStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid CREATE TRIGGER statement");

            TriggerInfo trig;
            trig.name = stmt->name_;
            trig.table_name = stmt->table_name_;
            trig.timing = stmt->timing_;
            trig.event = stmt->event_;
            trig.body = stmt->body_;

            if (!catalog_->CreateTrigger(stmt->name_, trig)) {
                return ExecutionResult::Error("Trigger '" + stmt->name_ + "' already exists");
            }
            catalog_->SaveCatalog();
            return ExecutionResult::Message("Trigger '" + stmt->name_ + "' created");
        };

        dispatch_map_[StatementType::DROP_TRIGGER] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<DropTriggerStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid DROP TRIGGER statement");
            if (!catalog_->DropTrigger(stmt->name_)) {
                return ExecutionResult::Error("Trigger not found: " + stmt->name_);
            }
            catalog_->SaveCatalog();
            return ExecutionResult::Message("Trigger '" + stmt->name_ + "' dropped");
        };

        // ----- QUERY HISTORY -----
        dispatch_map_[StatementType::SHOW_HISTORY] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<ShowHistoryStatement*>(s);
            int limit = (stmt && stmt->limit_ > 0) ? stmt->limit_ : 50;

            auto records = QueryHistory::Instance().GetRecent(limit);
            auto rs = std::make_shared<ResultSet>();
            rs->column_names = {"SQL", "User", "Database", "Success", "Time (ms)", "Timestamp"};

            for (auto& r : records) {
                std::ostringstream time_oss;
                time_oss << std::fixed << std::setprecision(2) << r.execution_time_ms;
                rs->AddRow({r.query, r.user, r.database,
                           r.success ? "YES" : "NO",
                           time_oss.str(),
                           std::to_string(r.timestamp_us)});
            }
            return ExecutionResult::Data(rs);
        };

        // ----- SCHEDULED JOBS -----
        dispatch_map_[StatementType::CREATE_SCHEDULE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<CreateScheduleStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid CREATE SCHEDULE statement");

            ScheduledJob job;
            job.name = stmt->name_;
            job.sql = stmt->sql_body_;
            job.interval_seconds = stmt->interval_seconds_;
            job.enabled = true;
            Scheduler::Instance().AddJob(job);

            // Also persist in catalog
            ScheduleInfo sched;
            sched.name = stmt->name_;
            sched.interval_seconds = stmt->interval_seconds_;
            sched.sql = stmt->sql_body_;
            catalog_->CreateSchedule(stmt->name_, sched);
            catalog_->SaveCatalog();

            return ExecutionResult::Message("Schedule '" + stmt->name_ + "' created (every " +
                                           std::to_string(stmt->interval_seconds_) + "s)");
        };

        dispatch_map_[StatementType::DROP_SCHEDULE] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<DropScheduleStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid DROP SCHEDULE statement");
            Scheduler::Instance().RemoveJob(stmt->name_);
            catalog_->DropSchedule(stmt->name_);
            catalog_->SaveCatalog();
            return ExecutionResult::Message("Schedule '" + stmt->name_ + "' dropped");
        };

        dispatch_map_[StatementType::SHOW_SCHEDULES] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto jobs = Scheduler::Instance().GetJobs();
            auto rs = std::make_shared<ResultSet>();
            rs->column_names = {"Name", "SQL", "Interval (s)", "Enabled", "Run Count", "Last Run"};
            for (auto& job : jobs) {
                rs->AddRow({job.name, job.sql, std::to_string(job.interval_seconds),
                           job.enabled ? "YES" : "NO", std::to_string(job.run_count),
                           std::to_string(job.last_run)});
            }
            return ExecutionResult::Data(rs);
        };

        // ----- REPLICATION -----
        dispatch_map_[StatementType::SET_REPLICATION] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<SetReplicationStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid replication statement");

            auto& repl = ReplicationManager::Instance();

            if (stmt->role_ == "PRIMARY") {
                repl.SetRole(ReplicationManager::Role::PRIMARY);
                return ExecutionResult::Message("Replication role set to PRIMARY");
            } else if (stmt->role_ == "REPLICA") {
                repl.SetRole(ReplicationManager::Role::REPLICA);
                if (!stmt->primary_host_.empty()) {
                    repl.ConnectToPrimary(stmt->primary_host_, stmt->primary_port_);
                }
                return ExecutionResult::Message("Replication role set to REPLICA");
            } else if (stmt->role_ == "STANDALONE") {
                repl.SetRole(ReplicationManager::Role::STANDALONE);
                return ExecutionResult::Message("Replication role set to STANDALONE");
            }
            return ExecutionResult::Error("Unknown replication role: " + stmt->role_);
        };

        dispatch_map_[StatementType::SHOW_REPLICATION_STATUS] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto status = ReplicationManager::Instance().GetStatus();
            auto rs = std::make_shared<ResultSet>();
            rs->column_names = {"Property", "Value"};

            std::string role_str;
            switch (status.role) {
                case ReplicationManager::Role::STANDALONE: role_str = "STANDALONE"; break;
                case ReplicationManager::Role::PRIMARY: role_str = "PRIMARY"; break;
                case ReplicationManager::Role::REPLICA: role_str = "REPLICA"; break;
            }
            rs->AddRow({"Role", role_str});
            rs->AddRow({"Replica Count", std::to_string(status.replica_count)});
            rs->AddRow({"Last WAL LSN", std::to_string(status.last_wal_lsn)});
            rs->AddRow({"Primary Alive", status.primary_alive ? "YES" : "NO"});
            rs->AddRow({"Primary Host", status.primary_host});

            for (auto& host : status.replica_hosts) {
                rs->AddRow({"Replica", host});
            }
            return ExecutionResult::Data(rs);
        };

        // ----- AI: INDEX ADVISOR -----
        dispatch_map_[StatementType::SHOW_INDEX_SUGGESTIONS] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto suggestions = ai::IndexAdvisor::Instance().GetSuggestions(catalog_);
            auto rs = std::make_shared<ResultSet>();
            rs->column_names = {"Table", "Column", "Type", "Reason", "Query Count", "Suggested SQL"};
            for (auto& sug : suggestions) {
                rs->AddRow({sug.table, sug.column, sug.index_type, sug.reason,
                           std::to_string(sug.query_count), sug.suggested_sql});
            }
            if (suggestions.empty()) {
                rs->AddRow({"(none)", "", "", "Not enough query data yet", "0", ""});
            }
            return ExecutionResult::Data(rs);
        };

        // ----- AI: QUERY FIREWALL -----
        dispatch_map_[StatementType::SHOW_BLOCKED_QUERIES] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto blocked = ai::QueryFirewall::Instance().GetBlocked();
            auto rs = std::make_shared<ResultSet>();
            rs->column_names = {"ID", "SQL", "User", "Reason", "Timestamp", "Approved"};
            for (auto& bq : blocked) {
                rs->AddRow({std::to_string(bq.id), bq.sql, bq.user, bq.reason,
                           std::to_string(bq.timestamp), bq.approved ? "YES" : "NO"});
            }
            if (blocked.empty()) {
                rs->AddRow({"0", "(none)", "", "No blocked queries", "0", ""});
            }
            return ExecutionResult::Data(rs);
        };

        dispatch_map_[StatementType::APPROVE_QUERY] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            auto* stmt = dynamic_cast<ApproveQueryStatement*>(s);
            if (!stmt) return ExecutionResult::Error("Invalid APPROVE QUERY statement");
            if (ai::QueryFirewall::Instance().Approve(stmt->query_id_)) {
                return ExecutionResult::Message("Query " + std::to_string(stmt->query_id_) + " approved");
            }
            return ExecutionResult::Error("Query ID " + std::to_string(stmt->query_id_) + " not found");
        };

        // ----- SERVER CONTROL -----
        dispatch_map_[StatementType::STOP_SERVER] = [this](Statement *s, SessionContext *ctx, Transaction *t) {
            return ExecuteStopServer(ctx);
        };
    }

    // ============================================================================
    // TRANSACTION ACCESS (Delegates to TransactionExecutor)
    // ============================================================================

    Transaction *ExecutionEngine::GetCurrentTransaction() {
        return transaction_executor_->GetCurrentTransaction();
    }

    Transaction *ExecutionEngine::GetCurrentTransactionForWrite() {
        return transaction_executor_->GetCurrentTransactionForWrite();
    }

    // ============================================================================
    // MAIN EXECUTE METHOD - Clean Dispatch (No Switch/If-Else!)
    // ============================================================================

    ExecutionResult ExecutionEngine::Execute(Statement *stmt, SessionContext *session) {
        if (stmt == nullptr) {
            return ExecutionResult::Error("Empty Statement");
        }

        // ==========================================================================
        // CONCURRENCY GATEKEEPER
        // ==========================================================================
        std::unique_lock<std::shared_mutex> exclusive_lock;
        std::shared_lock<std::shared_mutex> shared_lock;

        StatementType type = stmt->GetType();

        bool requires_exclusive = (type == StatementType::RECOVER || type == StatementType::CHECKPOINT);

        if (requires_exclusive) {
            exclusive_lock = std::unique_lock<std::shared_mutex>(global_lock_);
        } else {
            shared_lock = std::shared_lock<std::shared_mutex>(global_lock_);
        }

        // ==========================================================================
        // PERMISSION CHECKS (Before dispatch)
        // ==========================================================================
        if (auto error = CheckPermissions(type, session); !error.empty()) {
            return ExecutionResult::Error(error);
        }

        // ==========================================================================
        // DISPATCH TO HANDLER (No switch/if-else chain!)
        // ==========================================================================
        try {
            Transaction *txn = GetCurrentTransactionForWrite();

            // Special handling for USE DATABASE (needs to update engine state)
            if (type == StatementType::USE_DB) {
                return HandleUseDatabase(dynamic_cast<UseDatabaseStatement *>(stmt), session, txn);
            }

            // Look up handler in dispatch map
            auto it = dispatch_map_.find(type);
            if (it == dispatch_map_.end()) {
                return ExecutionResult::Error("Unknown Statement Type");
            }

            // Execute the handler with timing for query history
            auto start_time = std::chrono::high_resolution_clock::now();
            ExecutionResult res = it->second(stmt, session, txn);
            auto end_time = std::chrono::high_resolution_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            // Record in query history
            {
                QueryRecord record;
                record.query = stmt->sql_text_.empty() ?
                    ("stmt_type:" + std::to_string(static_cast<int>(type))) : stmt->sql_text_;
                record.user = session ? session->current_user : "system";
                record.database = session ? session->current_db : "chronosdb";
                record.success = res.success;
                record.execution_time_ms = elapsed_ms;
                record.timestamp_us = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                QueryHistory::Instance().Record(record);
            }

            // Auto-commit for single DML statements
            if (type == StatementType::INSERT ||
                type == StatementType::UPDATE_CMD ||
                type == StatementType::DELETE_CMD) {
                transaction_executor_->AutoCommitIfNeeded();
            }

            return res;
        } catch (const std::exception &e) {
            // Force rollback on error
            if (transaction_executor_->GetCurrentTransaction() &&
                transaction_executor_->GetCurrentTransaction()->GetState() == Transaction::TransactionState::RUNNING) {
                transaction_executor_->Rollback();
            }
            return ExecutionResult::Error(e.what());
        }
    }

    // ============================================================================
    // PERMISSION CHECKS (Extracted for cleanliness)
    // ============================================================================

    std::string ExecutionEngine::CheckPermissions(StatementType type, SessionContext *session) {
        if (!session) return "";

        // Reserved database protections
        bool is_reserved_db = (session->current_db == "chronosdb" || session->current_db == "system");
        bool is_superadmin = (session->role == UserRole::SUPERADMIN);

        if (is_reserved_db && !is_superadmin) {
            if (type == StatementType::CREATE || type == StatementType::CREATE_TABLE) {
                return "Cannot create tables in reserved database";
            }
            if (type == StatementType::DROP) {
                return "Cannot drop tables in system database";
            }
            if (type == StatementType::INSERT || type == StatementType::UPDATE_CMD || type ==
                StatementType::DELETE_CMD) {
                return "Cannot modify system database tables";
            }
        }

        return ""; // No error
    }

    // ============================================================================
    // SPECIAL HANDLERS (Complex operations that need extra logic)
    // ============================================================================

    ExecutionResult ExecutionEngine::HandleUseDatabase(UseDatabaseStatement *stmt,
                                                       SessionContext *session,
                                                       Transaction *txn) {
        IBufferManager *new_bpm = nullptr;
        Catalog *new_catalog = nullptr;

        ExecutionResult res = database_executor_->UseDatabase(stmt, session, &new_bpm, &new_catalog);

        // Update engine state after USE DATABASE
        if (res.success && new_bpm && new_catalog) {
            bpm_ = new_bpm;
            catalog_ = new_catalog;

            // Update executor context with LockManager
            delete exec_ctx_;
            exec_ctx_ = new ExecutorContext(bpm_, catalog_, txn, log_manager_, lock_manager_.get());

            // Update executors with new catalog
            transaction_executor_->SetCatalog(catalog_);

            // Reinitialize DDL/DML executors with new catalog
            ddl_executor_ = std::make_unique<DDLExecutor>(catalog_, log_manager_);
            dml_executor_ = std::make_unique<DMLExecutor>(bpm_, catalog_, log_manager_);
        }

        return res;
    }

    // ============================================================================
    // HELPER METHODS
    // ============================================================================

    std::string ExecutionEngine::ValueToString(const Value &v) {
        std::ostringstream oss;
        oss << v;
        return oss.str();
    }

    // ============================================================================
    // RECOVERY OPERATIONS
    // ============================================================================

    ExecutionResult ExecutionEngine::ExecuteCheckpoint() {
        CheckpointManager cp_mgr(bpm_, log_manager_);
        cp_mgr.BeginCheckpoint();
        return ExecutionResult::Message("CHECKPOINT SUCCESS");
    }

    ExecutionResult ExecutionEngine::ExecuteRecover(RecoverStatement *stmt) {
        std::cout << "[SYSTEM] Preparing for Time Travel (Reverse Delta Strategy)..." << std::endl;

        uint64_t target_time = stmt->timestamp_;

        // Special case: UINT64_MAX means "recover to latest"
        bool recover_to_latest = (target_time == UINT64_MAX);

        if (recover_to_latest) {
            std::cout << "[SYSTEM] Recovering to LATEST state..." << std::endl;
        } else {
            uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            // Allow timestamps up to 1 minute in the future (treat as "now")
            uint64_t one_minute = 60ULL * 1000000ULL;  // microseconds
            if (target_time > now + one_minute) {
                return ExecutionResult::Error("Cannot travel to the future! Use 'RECOVER TO LATEST' for current state.");
            }

            if (target_time == 0) {
                return ExecutionResult::Error("Invalid timestamp (0). Use 'RECOVER TO LATEST' for current state.");
            }

            std::cout << "[SYSTEM] Initiating Time Travel to: " << target_time << std::endl;
        }

        // Force Buffer Pool Flush before recovery
        bpm_->FlushAllPages();
        log_manager_->Flush(true);

        try {
            // ================================================================
            // USE REVERSE DELTA TIME TRAVEL ENGINE
            //
            // The TimeTravelEngine provides:
            // - ATOMIC recovery (all-or-nothing)
            // - REVERSE DELTA strategy for recent queries (O(delta) not O(N))
            // - Automatic fallback to FORWARD REPLAY for distant past
            // ================================================================

            CheckpointManager cp_mgr(bpm_, log_manager_);
            cp_mgr.SetCatalog(catalog_);

            TimeTravelEngine time_travel(log_manager_, catalog_, bpm_, &cp_mgr);

            std::string db_name = log_manager_->GetCurrentDatabase();
            auto result = time_travel.RecoverTo(target_time, db_name);

            if (!result.success) {
                return ExecutionResult::Error(std::string("Recovery Failed: ") + result.error_message);
            }

            std::cout << "[SYSTEM] Strategy used: "
                      << (result.strategy_used == TimeTravelEngine::Strategy::REVERSE_DELTA
                          ? "REVERSE_DELTA" : "FORWARD_REPLAY") << std::endl;
            std::cout << "[SYSTEM] Records processed: " << result.records_processed << std::endl;
            std::cout << "[SYSTEM] Time elapsed: " << result.elapsed_ms << "ms" << std::endl;

            // Flush after recovery to persist changes
            bpm_->FlushAllPages();
            log_manager_->Flush(true);

            // Save catalog to persist table metadata changes
            if (catalog_) {
                catalog_->SaveCatalog();
            }
        } catch (const std::exception &e) {
            return ExecutionResult::Error(std::string("Recovery Failed: ") + e.what());
        }

        std::cout << "[SYSTEM] Time Travel Complete. Resuming normal operations." << std::endl;

        if (recover_to_latest) {
            return ExecutionResult::Message("RECOVERED TO LATEST. System state restored to most recent.");
        }
        return ExecutionResult::Message("TIME TRAVEL COMPLETE. System state reverted using Reverse Delta strategy.");
    }
    
    ExecutionResult ExecutionEngine::ExecuteStopServer(SessionContext* session) {
        // Only SUPERADMIN can stop the server
        if (session && session->role != UserRole::SUPERADMIN) {
            return ExecutionResult::Error("Permission denied. Only SUPERADMIN can stop the server.");
        }
        
        std::cout << "[STOP] Server shutdown requested by user: " 
                  << (session ? session->current_user : "unknown") << std::endl;
        
        // Set the shutdown flag - this will be checked by the server
        shutdown_requested_ = true;
        
        return ExecutionResult::Message("SHUTDOWN INITIATED. Server will stop after completing current operations.");
    }
} // namespace chronosdb
