// // examples/json_client.cpp
// #include "network/chronos_client.h"
// #include <iostream>
// #include <nlohmann/json.hpp> // Using JSON library
//
// int main() {
//     chronosdb::ChronosClient client(chronosdb::ProtocolType::JSON);
//     
//     if (!client.Connect("localhost", chronosdb::net::DEFAULT_PORT)) {
//         std::cerr << "Failed to connect" << std::endl;
//         return 1;
//     }
//     
//     // Create JSON request
//     nlohmann::json request;
//     request["query"] = "SELECT * FROM users";
//     request["format"] = "json";
//     
//     std::string response = client.Query(request.dump());
//     
//     auto json_response = nlohmann::json::parse(response);
//     std::cout << "Got " << json_response["row_count"] << " rows" << std::endl;
//     
//     return 0;
// }