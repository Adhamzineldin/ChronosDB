# FrancoDB Installation Script for Windows (PowerShell)

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  FrancoDB Installation Script" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Check for CMake
Write-Host "Checking for CMake..." -ForegroundColor Yellow
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: CMake is not installed." -ForegroundColor Red
    Write-Host "Please install CMake from: https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}

# Check for compiler
Write-Host "Checking for C++ compiler..." -ForegroundColor Yellow
$compiler = $null
$generator = $null

if (Get-Command g++ -ErrorAction SilentlyContinue) {
    $compiler = "MinGW"
    $generator = "MinGW Makefiles"
} elseif (Get-Command cl -ErrorAction SilentlyContinue) {
    $compiler = "MSVC"
    # Try to detect VS version
    if (Test-Path "C:\Program Files\Microsoft Visual Studio\2022") {
        $generator = "Visual Studio 17 2022"
    } elseif (Test-Path "C:\Program Files (x86)\Microsoft Visual Studio\2019") {
        $generator = "Visual Studio 16 2019"
    } else {
        $generator = "Visual Studio 17 2022"  # Default
    }
} elseif (Get-Command ninja -ErrorAction SilentlyContinue) {
    $compiler = "Ninja"
    $generator = "Ninja"
} else {
    Write-Host "ERROR: No C++ compiler found. Please install one of:" -ForegroundColor Red
    Write-Host "  - MinGW-w64 (with g++), or" -ForegroundColor Yellow
    Write-Host "  - Visual Studio with C++ tools, or" -ForegroundColor Yellow
    Write-Host "  - Ninja build system" -ForegroundColor Yellow
    exit 1
}

Write-Host "Found compiler: $compiler" -ForegroundColor Green

# Create build directory
Write-Host "Creating build directory..." -ForegroundColor Yellow
if (Test-Path "build") {
    Remove-Item -Recurse -Force build
}
New-Item -ItemType Directory -Path build | Out-Null
Set-Location build

# Configure
Write-Host "Configuring build..." -ForegroundColor Yellow
if (-not $generator) {
    Write-Host "ERROR: Could not determine CMake generator" -ForegroundColor Red
    exit 1
}

if ($compiler -eq "MinGW") {
    cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
} elseif ($compiler -eq "MSVC") {
    cmake -G $generator -DCMAKE_BUILD_TYPE=Release ..
} elseif ($compiler -eq "Ninja") {
    cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
} else {
    Write-Host "ERROR: Unknown compiler type" -ForegroundColor Red
    exit 1
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: CMake configuration failed" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "Building FrancoDB..." -ForegroundColor Yellow
cmake --build . --config Release --target francodb_server francodb_shell

# Create data directory
Write-Host "Creating data directory..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path ..\data -Force | Out-Null

Set-Location ..

# Configuration setup
Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  Configuration Setup" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""
$configure = Read-Host "Would you like to configure FrancoDB now? (y/n) [y]"
if ($configure -eq "" -or $configure -eq "y" -or $configure -eq "Y") {
    Write-Host ""
    Write-Host "Running configuration..." -ForegroundColor Yellow
    # Configuration will be handled by the server on first run
    Write-Host "Note: Configuration will be prompted on first server startup." -ForegroundColor Yellow
    Write-Host "You can also edit francodb.conf manually." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "==========================================" -ForegroundColor Green
Write-Host "  Installation Complete!" -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Server executable: .\build\francodb_server.exe" -ForegroundColor Cyan
Write-Host "Shell executable: .\build\francodb_shell.exe" -ForegroundColor Cyan
Write-Host ""
Write-Host "To start the server:" -ForegroundColor Yellow
Write-Host "  .\build\francodb_server.exe" -ForegroundColor White
Write-Host ""
Write-Host "To use the shell:" -ForegroundColor Yellow
Write-Host "  .\build\francodb_shell.exe" -ForegroundColor White
Write-Host ""
Write-Host "Configuration file: francodb.conf" -ForegroundColor Cyan
Write-Host ""
