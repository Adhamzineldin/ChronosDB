#include "network/connection_handler.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "common/result_formatter.h"
#include "storage/disk/disk_manager.h"
#include "network/protocol.h" // Ensure this is included

#include <sstream>
#include <algorithm>
#include <filesystem>

namespace francodb {

    ClientConnectionHandler::ClientConnectionHandler(ExecutionEngine *engine, AuthManager *auth_manager)
        : engine_(engine),
          session_(std::make_shared<SessionContext>()),
          auth_manager_(auth_manager) {
    }

    std::string ClientConnectionHandler::SerializeResponse(const ExecutionResult& result) {
        // Create temporary instances to format the output
        if (response_format_ == ProtocolType::JSON) {
            JsonProtocol p;
            return p.Serialize(result);
        } else if (response_format_ == ProtocolType::BINARY) {
            BinaryProtocol p;
            return p.Serialize(result);
        } else {
            TextProtocol p;
            return p.Serialize(result);
        }
    }

    std::string ClientConnectionHandler::SerializeError(const std::string& message) {
        return SerializeResponse(ExecutionResult::Error(message));
    }

    std::string ClientConnectionHandler::ProcessRequest(const std::string &request) {
        std::string sql = request;
        sql.erase(std::remove(sql.begin(), sql.end(), '\n'), sql.end());
        sql.erase(std::remove(sql.begin(), sql.end(), '\r'), sql.end());
        
        if (sql.empty()) return "";
        if (sql == "exit" || sql == "quit") return "Goodbye!\n";

        try {
            Lexer lexer(sql);
            Parser parser(std::move(lexer));
            auto stmt = parser.ParseQuery();

            if (!stmt) return SerializeError("Failed to parse query");

            // --- LOGIN ---
            if (stmt->GetType() == StatementType::LOGIN) {
                auto *login = dynamic_cast<LoginStatement *>(stmt.get());
                if (!login) return SerializeError("Invalid LOGIN");
                
                UserRole login_role;
                if (auth_manager_->Authenticate(login->username_, login->password_, login_role)) {
                    session_->is_authenticated = true;
                    session_->current_user = login->username_;
                    session_->current_db = "default";
                    session_->role = auth_manager_->GetUserRole(session_->current_user, session_->current_db);
                    if (login_role == UserRole::SUPERADMIN) session_->role = UserRole::SUPERADMIN;

                    std::string role_str = (session_->role == UserRole::SUPERADMIN) ? "SUPERADMIN" : 
                                           (session_->role == UserRole::ADMIN) ? "ADMIN" : "USER";
                    return SerializeResponse(ExecutionResult::Message("LOGIN OK (Role: " + role_str + ")"));
                }
                return SerializeError("Authentication failed");
            }

            // --- AUTH CHECK ---
            if (!session_->is_authenticated) return SerializeError("Authentication required. Use LOGIN");

            // --- USE DB ---
            if (stmt->GetType() == StatementType::USE_DB) {
                auto *use_db = dynamic_cast<UseDatabaseStatement *>(stmt.get());
                if (!use_db) return SerializeError("Invalid USE");
                
                if (!auth_manager_->HasDatabaseAccess(session_->current_user, use_db->db_name_)) {
                    return SerializeError("Access denied to " + use_db->db_name_);
                }
                session_->current_db = use_db->db_name_;
                session_->role = auth_manager_->GetUserRole(session_->current_user, session_->current_db);
                if (auth_manager_->IsSuperAdmin(session_->current_user)) session_->role = UserRole::SUPERADMIN;
                return SerializeResponse(ExecutionResult::Message("Using database: " + use_db->db_name_));
            }

            // --- CREATE DB ---
            if (stmt->GetType() == StatementType::CREATE_DB) {
                if (!auth_manager_->HasPermission(session_->role, StatementType::CREATE_DB)) 
                    return SerializeError("Permission denied.");
                auto *create_db = dynamic_cast<CreateDatabaseStatement *>(stmt.get());
                
                try {
                    std::filesystem::create_directories("data");
                    DiskManager new_db("data/" + create_db->db_name_);
                    UserRole creator_role = (session_->role == UserRole::SUPERADMIN) ? UserRole::SUPERADMIN : UserRole::ADMIN;
                    auth_manager_->SetUserRole(session_->current_user, create_db->db_name_, creator_role);
                    return SerializeResponse(ExecutionResult::Message("CREATE DATABASE " + create_db->db_name_ + " OK"));
                } catch (const std::exception &e) {
                    return SerializeError("Failed: " + std::string(e.what()));
                }
            }

            // --- GENERAL QUERY ---
            session_->role = auth_manager_->GetUserRole(session_->current_user, session_->current_db);
            if (!auth_manager_->HasPermission(session_->role, stmt->GetType())) {
                return SerializeError("Permission denied.");
            }

            ExecutionResult res = engine_->Execute(stmt.get());
            return SerializeResponse(res);

        } catch (const std::exception &e) {
            return SerializeError("SYSTEM ERROR: " + std::string(e.what()));
        }
    }
}