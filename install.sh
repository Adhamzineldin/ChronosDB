#!/bin/bash
# FrancoDB Installation Script for Linux/macOS

set -e

echo "=========================================="
echo "  FrancoDB Installation Script"
echo "=========================================="

# Check for dependencies
echo "Checking dependencies..."

if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake is not installed. Please install it first:"
    echo "  Ubuntu/Debian: sudo apt-get install cmake"
    echo "  macOS: brew install cmake"
    exit 1
fi

if ! command -v ninja &> /dev/null; then
    echo "WARNING: Ninja not found. Installing..."
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        sudo apt-get install -y ninja-build
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        brew install ninja
    fi
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure
echo "Configuring build..."
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..

# Build
echo "Building FrancoDB..."
cmake --build . --target francodb_server francodb_shell

# Create data directory
echo "Creating data directory..."
mkdir -p ../data

echo ""
echo "=========================================="
echo "  Installation Complete!"
echo "=========================================="
echo ""
echo "Server executable: ./build/francodb_server"
echo "Shell executable: ./build/francodb_shell"
echo ""
echo "To start the server:"
echo "  ./build/francodb_server"
echo ""
echo "To use the shell:"
echo "  ./build/francodb_shell"
echo ""
