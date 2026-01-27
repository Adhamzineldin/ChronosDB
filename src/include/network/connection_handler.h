#pragma once

#include <memory>
#include <string>
#include "network/protocol.h"
#include "execution/execution_engine.h"
#include "session_context.h"
#include "common/auth_manager.h"

namespace chronosdb {

    
    
    
    
    class ClientConnectionHandler {
    private:
        ExecutionEngine *engine_;
        std::shared_ptr<SessionContext> session_;
        AuthManager *auth_manager_;
        
        // The current output format (Default: TEXT)
        ProtocolType response_format_ = ProtocolType::TEXT;

        // Helper to format the result based on response_format_
        std::string SerializeResponse(const ExecutionResult& result);
        std::string SerializeError(const std::string& message);

    public:
        // Constructor
        ClientConnectionHandler(ExecutionEngine *engine, AuthManager *auth_manager);
        virtual ~ClientConnectionHandler() = default;

        // The main processing loop
        std::string ProcessRequest(const std::string &request);

        // Switch modes on the fly (Text -> JSON -> Binary)
        void SetResponseFormat(ProtocolType type) { response_format_ = type; }
        bool IsAuthenticated() const { return session_->is_authenticated; }
        std::string GetCurrentUser() const { return session_->current_user; }
        std::string GetCurrentDb() const { return session_->current_db; }

        // Accessor for Session (to keep it alive)
        std::shared_ptr<SessionContext> GetSession() const { return session_; }
    };

}