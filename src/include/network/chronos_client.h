// network/chronos_client.h
#pragma once

#include <string>
#include <memory>

#include "common/chronos_net_config.h"
#include "network/protocol.h"

namespace chronosdb {

    class ChronosClient {
    private:
        uintptr_t sock_{0};
        bool is_connected_{false};
        std::unique_ptr<ProtocolSerializer> protocol_;
        ProtocolType protocol_type_;

    public:
        explicit ChronosClient(ProtocolType protocol = ProtocolType::TEXT);
        ~ChronosClient();

        bool Connect(const std::string &ip = "127.0.0.1",
                     int port = net::DEFAULT_PORT,
                     const std::string &username = "",
                     const std::string &password = "",
                     const std::string &database = "");
        
        // Connection string format: chronos://user:pass@host:port/dbname
        // Examples:
        //   chronos://chronos:root@localhost:2501/mydb
        //   chronos://chronos:root@localhost/mydb  (default port 2501)
        //   chronos://chronos:root@localhost      (no database)
        bool ConnectFromString(const std::string &connection_string);
        
        std::string Query(const std::string &sql);
        void Disconnect();
        bool IsConnected() const { return is_connected_; }

        // For binary protocol
        void SendBinary(const std::vector<uint8_t> &data);
        std::vector<uint8_t> ReceiveBinary();
    };
}