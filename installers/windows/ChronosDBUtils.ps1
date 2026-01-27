# ==============================================================================
# ChronosDB Server Management Scripts
# ==============================================================================
# PowerShell functions to manage ChronosDB service

# Requires Admin privileges
#Requires -RunAsAdministrator

# ==============================================================================
# START FUNCTION
# ==============================================================================
function Start-ChronosDBServer {
    <#
    .SYNOPSIS
    Starts the ChronosDB service.
    
    .DESCRIPTION
    Attempts to start the ChronosDB Windows service. If the service is not found,
    it tries to start the executable directly.
    
    .EXAMPLE
    Start-ChronosDBServer
    #>
    
    Write-Host "ChronosDB Service Startup" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    $serviceName = "ChronosDBService"
    $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
    
    if ($service) {
        Write-Host "[INFO] Service found: $serviceName" -ForegroundColor Yellow
        
        if ($service.Status -eq 'Running') {
            Write-Host "[OK] ChronosDB is already running!" -ForegroundColor Green
            return $true
        }
        
        Write-Host "[INFO] Starting service..." -ForegroundColor Yellow
        Start-Service -Name $serviceName -ErrorAction SilentlyContinue
        
        # Wait for service to start (max 30 seconds)
        $waited = 0
        while ($waited -lt 30) {
            Start-Sleep -Seconds 1
            $service = Get-Service -Name $serviceName
            if ($service.Status -eq 'Running') {
                Write-Host "[OK] ChronosDB started successfully!" -ForegroundColor Green
                return $true
            }
            $waited++
        }
        
        Write-Host "[WARN] Service took too long to start. Check chronosdb.conf" -ForegroundColor Yellow
        return $false
    }
    else {
        Write-Host "[WARN] Service not found. Attempting direct startup..." -ForegroundColor Yellow
        
        $serverExe = Join-Path -Path $PSScriptRoot -ChildPath "chronosdb_server.exe"
        
        if (Test-Path $serverExe) {
            Write-Host "[INFO] Starting: $serverExe" -ForegroundColor Yellow
            & $serverExe
            Write-Host "[OK] ChronosDB started" -ForegroundColor Green
            return $true
        }
        else {
            Write-Host "[ERROR] Server executable not found: $serverExe" -ForegroundColor Red
            return $false
        }
    }
}

# ==============================================================================
# STOP FUNCTION
# ==============================================================================
function Stop-ChronosDBServer {
    <#
    .SYNOPSIS
    Stops the ChronosDB service.
    
    .DESCRIPTION
    Gracefully stops the ChronosDB service. If it doesn't stop within 60 seconds,
    the process is forcefully terminated.
    
    .PARAMETER Force
    Force terminate the process without waiting for graceful shutdown
    
    .EXAMPLE
    Stop-ChronosDBServer
    Stop-ChronosDBServer -Force
    #>
    
    param(
        [switch]$Force
    )
    
    Write-Host "ChronosDB Service Stop" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    $serviceName = "ChronosDBService"
    $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
    
    if ($service) {
        Write-Host "[INFO] Service found. Stopping..." -ForegroundColor Yellow
        Stop-Service -Name $serviceName -Force:$Force -ErrorAction SilentlyContinue
        
        if (-not $Force) {
            # Wait for graceful shutdown
            $waited = 0
            while ($waited -lt 60) {
                Start-Sleep -Seconds 1
                $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
                if ($service.Status -eq 'Stopped') {
                    Write-Host "[OK] Service stopped" -ForegroundColor Green
                    return $true
                }
                if ($waited % 10 -eq 0) {
                    Write-Host "[INFO] Waiting for shutdown... ($waited`s)" -ForegroundColor Yellow
                }
                $waited++
            }
            Write-Host "[WARN] Graceful stop timeout. Force terminating..." -ForegroundColor Yellow
        }
    }
    
    # Kill any remaining processes
    $procs = Get-Process -Name "chronosdb_server", "chronosdb_service" -ErrorAction SilentlyContinue
    if ($procs) {
        Write-Host "[INFO] Terminating processes..." -ForegroundColor Yellow
        $procs | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 1
    }
    
    # Verify
    $remaining = Get-Process -Name "chronosdb_server", "chronosdb_service" -ErrorAction SilentlyContinue
    if ($remaining) {
        Write-Host "[ERROR] Could not terminate all processes" -ForegroundColor Red
        return $false
    }
    else {
        Write-Host "[OK] All ChronosDB processes stopped" -ForegroundColor Green
        return $true
    }
}

# ==============================================================================
# STATUS FUNCTION
# ==============================================================================
function Get-ChronosDBStatus {
    <#
    .SYNOPSIS
    Gets the status of ChronosDB service.
    
    .EXAMPLE
    Get-ChronosDBStatus
    #>
    
    $serviceName = "ChronosDBService"
    $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
    
    Write-Host "ChronosDB Service Status" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    if ($service) {
        $statusColor = if ($service.Status -eq 'Running') { 'Green' } else { 'Red' }
        Write-Host "Service Status: $($service.Status)" -ForegroundColor $statusColor
        Write-Host "Service Name: $($service.Name)" -ForegroundColor White
        Write-Host "Display Name: $($service.DisplayName)" -ForegroundColor White
        Write-Host "Start Type: $($service.StartType)" -ForegroundColor White
    }
    else {
        Write-Host "Service Status: NOT FOUND" -ForegroundColor Red
    }
    
    # Check processes
    Write-Host ""
    $procs = Get-Process -Name "chronosdb_server", "chronosdb_service" -ErrorAction SilentlyContinue
    if ($procs) {
        Write-Host "Running Processes:" -ForegroundColor Yellow
        $procs | Select-Object -Property Name, Id, Handles, WorkingSet | Format-Table
    }
    else {
        Write-Host "No ChronosDB processes running" -ForegroundColor Yellow
    }
}

# ==============================================================================
# RESTART FUNCTION
# ==============================================================================
function Restart-ChronosDBServer {
    <#
    .SYNOPSIS
    Restarts the ChronosDB service.
    
    .EXAMPLE
    Restart-ChronosDBServer
    #>
    
    Write-Host "ChronosDB Service Restart" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "[INFO] Stopping ChronosDB..." -ForegroundColor Yellow
    Stop-ChronosDBServer
    
    Write-Host ""
    Start-Sleep -Seconds 2
    
    Write-Host "[INFO] Starting ChronosDB..." -ForegroundColor Yellow
    Start-ChronosDBServer
}

# ==============================================================================
# EXPORT FUNCTIONS
# ==============================================================================
Export-ModuleMember -Function @(
    'Start-ChronosDBServer',
    'Stop-ChronosDBServer',
    'Get-ChronosDBStatus',
    'Restart-ChronosDBServer'
)

# ==============================================================================
# QUICK REFERENCE
# ==============================================================================
# Usage in PowerShell:
#
#   . .\ChronosDBUtils.ps1              # Load the script
#   Start-ChronosDBServer                # Start service
#   Stop-ChronosDBServer                 # Stop service
#   Restart-ChronosDBServer              # Restart service
#   Get-ChronosDBStatus                  # Check status
#
# ==============================================================================

