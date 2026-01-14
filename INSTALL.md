# FrancoDB Installation Guide

## Quick Start with Docker (Recommended)

### Prerequisites
- Docker and Docker Compose installed

### Steps

1. **Clone the repository:**
```bash
git clone <your-repo-url>
cd FrancoDB
```

2. **Start the server:**
```bash
docker-compose up -d
```

3. **Check if server is running:**
```bash
docker-compose ps
```

4. **View logs:**
```bash
docker-compose logs -f francodb
```

5. **Stop the server:**
```bash
docker-compose down
```

The server will be available at `localhost:2501`

## Manual Installation

### Prerequisites
- CMake 3.10 or higher
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Ninja build system (optional, but recommended)

### Linux/Unix

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build

# Build
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Run server
./francodb_server
```

### Windows

```powershell
# Install dependencies using vcpkg or download manually
# CMake and Visual Studio 2019+ or MinGW

# Build
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Run server
.\francodb_server.exe
```

### macOS

```bash
# Install dependencies
brew install cmake ninja

# Build
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Run server
./francodb_server
```

## Using FrancoDB in Your Code

### C++ Client

1. **Include the client header:**
```cpp
#include "network/franco_client.h"
```

2. **Link against the library:**
Add `francodb_lib` to your `target_link_libraries` in CMakeLists.txt

3. **Example usage:**
See `examples/cpp_example.cpp`

### Python Client

1. **Install the client:**
```bash
# Copy the Python client from examples/python_example.py
# Or install via pip (if package created)
```

2. **Example usage:**
See `examples/python_example.py`

### JavaScript/Node.js Client

1. **Install dependencies:**
```bash
npm install  # (if package.json exists)
```

2. **Example usage:**
See `examples/javascript_example.js`

## Connection String Format

FrancoDB uses a connection string format similar to MySQL:

```
maayn://username:password@host:port/database
```

Examples:
- `maayn://maayn:root@localhost:2501/mydb`
- `maayn://maayn:root@localhost/mydb` (default port 2501)
- `maayn://maayn:root@localhost` (no database)

## Default Credentials

- **Username:** `maayn`
- **Password:** `root`
- **Port:** `2501`

## Data Persistence

Data is stored in the `data/` directory:
- `data/system.francodb` - System database (users, permissions)
- `data/francodb.db.francodb` - Default database
- `data/<dbname>.francodb` - User-created databases

## Troubleshooting

### Port already in use
```bash
# Change port in docker-compose.yml or use different port
docker-compose up -d
```

### Permission denied
```bash
# On Linux, ensure user has write permissions to data/ directory
chmod -R 755 data/
```

### Connection refused
- Ensure server is running
- Check firewall settings
- Verify port 2501 is accessible

## Next Steps

- See `docs/test_commands_comprehensive.txt` for example queries
- Check `README.md` for more information
- Review `examples/` directory for client usage examples
