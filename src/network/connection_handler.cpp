#include "network/connection_handler.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "common/result_formatter.h"
#include "storage/disk/disk_manager.h"
#include "network/protocol.h" // Ensure this is included

#include <sstream>
#include <algorithm>
#include <filesystem>

namespace chronosdb {
    ClientConnectionHandler::ClientConnectionHandler(ExecutionEngine *engine, AuthManager *auth_manager)
        : engine_(engine),
          session_(std::make_shared<SessionContext>()),
          auth_manager_(auth_manager) {
    }

    std::string ClientConnectionHandler::SerializeResponse(const ExecutionResult &result) {
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

    std::string ClientConnectionHandler::SerializeError(const std::string &message) {
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

        // --- LOGIN HANDLING (Keep this here - it's auth, not execution) ---
        if (stmt->GetType() == StatementType::LOGIN) {
            auto *login = dynamic_cast<LoginStatement *>(stmt.get());
            if (!login) return SerializeError("Invalid LOGIN");

            UserRole login_role;
            if (auth_manager_->Authenticate(login->username_, login->password_, login_role)) {
                session_->is_authenticated = true;
                session_->current_user = login->username_;
                session_->current_db = ""; // No database selected yet

                // Set initial role
                session_->role = login_role;

                std::string role_str = (session_->role == UserRole::SUPERADMIN)
                                           ? "SUPERADMIN"
                                           : (session_->role == UserRole::ADMIN)
                                                 ? "ADMIN"
                                                 : "USER";
                return SerializeResponse(ExecutionResult::Message("LOGIN OK (Role: " + role_str + ")"));
            }
            return SerializeError("Authentication failed");
        }

        // --- AUTH CHECK ---
        if (!session_->is_authenticated) {
            return SerializeError("Authentication required. Use LOGIN");
        }

        // --- EXECUTE EVERYTHING ELSE IN ENGINE ---
        // Refresh role in case permissions changed
        if (!session_->current_db.empty()) {
            session_->role = auth_manager_->GetUserRole(session_->current_user, session_->current_db);
            if (auth_manager_->IsSuperAdmin(session_->current_user)) {
                session_->role = UserRole::SUPERADMIN;
            }
        }

        // Let ExecutionEngine handle ALL statements including CREATE_DB and USE_DB
        ExecutionResult res = engine_->Execute(stmt.get(), session_.get());

        return SerializeResponse(res);
        
    } catch (const std::exception &e) {
        return SerializeError("SYSTEM ERROR: " + std::string(e.what()));
    }
}
}
