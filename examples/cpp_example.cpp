// Example: Using FrancoDB Client in C++
#include <iostream>
#include "francodb_client.h"

using namespace francodb;

int main() {
    FrancoClient client;
    
    // Connect
    if (!client.ConnectFromString("maayn://maayn:root@localhost:2501/mydb")) {
        std::cerr << "Failed to connect to FrancoDB." << std::endl;
        return 1;
    }
    
    std::cout << "Connected to FrancoDB!" << std::endl;
    
    // 1. Create & Insert (Text Mode is default)
    client.Query("2e3mel gadwal users (id rakam, name gomla);");
    client.Query("emla gowa users elkeyam (1, 'Alice');");
    
    // 2. Query in JSON Mode
    std::cout << "\n--- Fetching JSON ---" << std::endl;
    
    // Switch client protocol to JSON
    client.SetProtocol(ProtocolType::JSON);
    
    // Send query (Client handles the [J][Len][SQL] header automatically)
    std::string json_result = client.Query("2e5tar * men users;");
    
    std::cout << json_result << std::endl;
    
    client.Disconnect();
    return 0;
}