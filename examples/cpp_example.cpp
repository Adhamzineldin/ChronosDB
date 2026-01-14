// Example: Using FrancoDB Client in C++
#include <iostream>
#include "francodb_client.h"  // Include the client header

using namespace francodb;

int main() {
    // Create client instance
    FrancoClient client;
    
    // Connect using connection string
    std::string conn_str = "maayn://maayn:root@localhost:2501/mydb";
    if (!client.ConnectFromString(conn_str)) {
        std::cerr << "Failed to connect to FrancoDB server" << std::endl;
        return 1;
    }
    
    std::cout << "Connected to FrancoDB!" << std::endl;
    
    // Create a table
    std::string create_table = "CREATE TABLE users (id INT PRIMARY KEY, name VARCHAR(50), age INT);\n";
    std::string result = client.Query(create_table);
    std::cout << "Create table: " << result << std::endl;
    
    // Insert data
    std::string insert = "INSERT INTO users VALUES (1, 'Alice', 25);\n";
    result = client.Query(insert);
    std::cout << "Insert: " << result << std::endl;
    
    // Query data
    std::string select = "SELECT * FROM users WHERE age > 20;\n";
    result = client.Query(select);
    std::cout << "Query result:\n" << result << std::endl;
    
    // Disconnect
    client.Disconnect();
    
    return 0;
}
