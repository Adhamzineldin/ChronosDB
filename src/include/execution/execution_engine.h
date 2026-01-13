#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include "execution/executor_context.h"
#include "parser/statement.h"
#include "execution/executors/insert_executor.h"
#include "execution/executors/seq_scan_executor.h"
#include "common/exception.h"
#include "executors/delete_executor.h"
#include "executors/index_scan_executor.h"
#include "executors/update_executor.h"

namespace francodb {
    class ExecutionEngine {
    public:
        ExecutionEngine(BufferPoolManager *bpm, Catalog *catalog)
            : catalog_(catalog), exec_ctx_(new ExecutorContext(catalog, bpm)) {
        }

        ~ExecutionEngine() { delete exec_ctx_; }

        void Execute(Statement *stmt) {
            if (stmt == nullptr) {
                return;
            }

            switch (stmt->GetType()) {
                case StatementType::CREATE_INDEX: {
                    auto *idx_stmt = dynamic_cast<CreateIndexStatement *>(stmt);
                    ExecuteCreateIndex(idx_stmt);
                    break;
                }

                case StatementType::CREATE: {
                    // Cast the generic Statement to a specific CreateStatement
                    auto *create_stmt = dynamic_cast<CreateStatement *>(stmt);
                    ExecuteCreate(create_stmt);
                    break;
                }
                case StatementType::INSERT: {
                    auto *insert_stmt = dynamic_cast<InsertStatement *>(stmt);
                    ExecuteInsert(insert_stmt);
                    break;
                }
                case StatementType::SELECT: {
                    auto *select_stmt = dynamic_cast<SelectStatement *>(stmt);
                    ExecuteSelect(select_stmt);
                    break;
                }
                case StatementType::DROP: {
                    auto *drop_stmt = dynamic_cast<DropStatement *>(stmt);
                    ExecuteDrop(drop_stmt);
                    break;
                }
                case StatementType::DELETE_CMD: {
                    auto *del_stmt = dynamic_cast<DeleteStatement *>(stmt);
                    ExecuteDelete(del_stmt);
                    break;
                }
                case StatementType::UPDATE_CMD: {
                    auto *upd_stmt = dynamic_cast<UpdateStatement *>(stmt);
                    ExecuteUpdate(upd_stmt);
                    break;
                }
                default: {
                    throw Exception(ExceptionType::EXECUTION, "Unknown Statement Type.");
                }
            }
        }

    private:
        // --- 1. CREATE HANDLER ---
        void ExecuteCreate(CreateStatement *stmt) {
            Schema schema(stmt->columns_);
            bool success = catalog_->CreateTable(stmt->table_name_, schema);
            if (!success) {
                throw Exception(ExceptionType::EXECUTION, "Table already exists: " + stmt->table_name_);
            }
            std::cout << "[EXEC] Created Table: " << stmt->table_name_ << std::endl;
        }

        void ExecuteCreateIndex(CreateIndexStatement *stmt) {
            auto *index = catalog_->CreateIndex(stmt->index_name_, stmt->table_name_, stmt->column_name_);
            if (index == nullptr) {
                throw Exception(ExceptionType::EXECUTION, "Failed to create index (Table exists? Column exists?)");
            }
            std::cout << "[EXEC] Created Index: " << stmt->index_name_ << " on " << stmt->table_name_ << std::endl;
        }


        // --- 2. INSERT HANDLER ---
        void ExecuteInsert(InsertStatement *stmt) {
            InsertExecutor executor(exec_ctx_, stmt);
            executor.Init();
            Tuple t;
            executor.Next(&t);
            std::cout << "[EXEC] Insert successful." << std::endl;
        }

        // --- 3. SELECT HANDLER ---
        void ExecuteSelect(SelectStatement *stmt) {
            AbstractExecutor *executor = nullptr;

            // --- OPTIMIZER LOGIC START ---
            // 1. Check if we have a simple equality filter (e.g., "id = 100")
            bool use_index = false;
            std::string index_col_name;
            Value index_search_value;

            // We only optimize simple cases: "WHERE col = val"
            if (!stmt->where_clause_.empty()) {
                // Check the first condition
                auto &cond = stmt->where_clause_[0];
                if (cond.op == "=") {
                    // 2. Ask Catalog: "Is there an index on this column?"
                    auto indexes = catalog_->GetTableIndexes(stmt->table_name_);
                    for (auto *idx: indexes) {
                        if (idx->col_name_ == cond.column) {
                            // FOUND A MATCHING INDEX!
                            use_index = true;
                            index_col_name = idx->name_;
                            index_search_value = cond.value;

                            // Create the specialized IndexScanExecutor
                            executor = new IndexScanExecutor(exec_ctx_, stmt, idx, index_search_value);
                            std::cout << "[OPTIMIZER] Using Index: " << idx->name_ << std::endl;
                            break;
                        }
                    }
                }
            }
            // --- OPTIMIZER LOGIC END ---

            // Fallback: If no index found, use SeqScan
            if (!use_index) {
                executor = new SeqScanExecutor(exec_ctx_, stmt);
                std::cout << "[OPTIMIZER] Using Sequential Scan" << std::endl;
            }

            // --- EXECUTION (Same as before) ---
            executor->Init();
            Tuple t;
            int count = 0;
            const Schema *output_schema = executor->GetOutputSchema();

            std::cout << "\n=== QUERY RESULT ===" << std::endl;
            for (const auto &col: output_schema->GetColumns()) {
                std::cout << col.GetName() << "\t| ";
            }
            std::cout << "\n--------------------" << std::endl;

            while (executor->Next(&t)) {
                for (uint32_t i = 0; i < output_schema->GetColumnCount(); ++i) {
                    Value v = t.GetValue(*output_schema, i);
                    std::cout << v << "\t\t| ";
                }
                std::cout << std::endl;
                count++;
            }
            std::cout << "====================" << std::endl;
            std::cout << "Rows returned: " << count << "\n" << std::endl;

            delete executor; // Cleanup
        }

        // --- 4. DROP HANDLER ---
        void ExecuteDrop(DropStatement *stmt) {
            bool success = catalog_->DropTable(stmt->table_name_);
            if (!success) {
                throw Exception(ExceptionType::EXECUTION, "Table not found: " + stmt->table_name_);
            }
            std::cout << "[EXEC] Dropped Table: " << stmt->table_name_ << std::endl;
        }

        // --- 5. DELETE HANDLER ---
        void ExecuteDelete(DeleteStatement *stmt) {
            DeleteExecutor executor(exec_ctx_, stmt);
            executor.Init();
            Tuple t;
            executor.Next(&t);
        }

        // --- 6. UPDATE HANDLER ---
        void ExecuteUpdate(UpdateStatement *stmt) {
            UpdateExecutor executor(exec_ctx_, stmt);
            executor.Init();
            Tuple t;
            executor.Next(&t);
        }

        Catalog *catalog_;
        ExecutorContext *exec_ctx_;
    };
} // namespace francodb
