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
if (Get-Command g++ -ErrorAction SilentlyContinue) {
    $compiler = "MinGW"
    $generator = "MinGW Makefiles"
} elseif (Get-Command cl -ErrorAction SilentlyContinue) {
    $compiler = "MSVC"
    $generator = "Visual Studio 17 2022"
} else {
    Write-Host "WARNING: No C++ compiler found. Please install:" -ForegroundColor Yellow
    Write-Host "  - MinGW-w64, or" -ForegroundColor Yellow
    Write-Host "  - Visual Studio with C++ tools" -ForegroundColor Yellow
}

# Create build directory
Write-Host "Creating build directory..." -ForegroundColor Yellow
if (Test-Path "build") {
    Remove-Item -Recurse -Force build
}
New-Item -ItemType Directory -Path build | Out-Null
Set-Location build

# Configure
Write-Host "Configuring build..." -ForegroundColor Yellow
if ($compiler -eq "MinGW") {
    cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
} else {
    cmake -G $generator -DCMAKE_BUILD_TYPE=Release ..
}

# Build
Write-Host "Building FrancoDB..." -ForegroundColor Yellow
cmake --build . --config Release --target francodb_server francodb_shell

# Create data directory
Write-Host "Creating data directory..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path ..\data -Force | Out-Null

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

Set-Location ..
