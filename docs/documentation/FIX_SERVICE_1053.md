# Fix for Service Error 1053

## Problem
Windows Service Error 1053: "The service did not respond to the start or control request in a timely fashion."

## Root Cause
The service wrapper was waiting 1 second after starting the server process before reporting SERVICE_RUNNING status. Windows requires services to report RUNNING within 30 seconds, but the delay was causing timeout issues.

## Solution Applied
1. Removed the `Sleep(1000)` delay
2. Service now reports RUNNING immediately after starting the server process
3. Server initialization happens in the background

## Steps to Fix

### 1. Rebuild the Service Executable
```powershell
# Navigate to project root
cd G:\University\Graduation\ChronosDB

# Rebuild the service
cmake --build cmake-build-debug --target chronosdb_service --config Debug
```

### 2. Reinstall the Service
```powershell
# Stop and remove old service
sc stop ChronosDBService
sc delete ChronosDBService

# Copy new service executable
copy cmake-build-debug\chronosdb_service.exe "C:\Program Files\ChronosDB\bin\"

# Recreate service
sc create ChronosDBService binPath= "C:\Program Files\ChronosDB\bin\chronosdb_service.exe" start= auto

# Start service
sc start ChronosDBService
```

### 3. Or Reinstall Using the Installer
1. Uninstall ChronosDB from Control Panel
2. Rebuild the service: `cmake --build cmake-build-debug --target chronosdb_service`
3. Recompile the installer
4. Run the installer again

## Verification

### Check Service Status
```cmd
sc query ChronosDBService
```

Should show: `STATE: 4 RUNNING`

### Check Event Viewer
1. Open Event Viewer: `eventvwr.msc`
2. Navigate to: Windows Logs → Application
3. Filter by Source: "ChronosDBService"
4. Look for: "ChronosDB Service started successfully"

### Test Server Connection
```cmd
chronosdb_shell
```

## If Still Failing

### Check Event Viewer for Errors
```powershell
Get-EventLog -LogName Application -Source "ChronosDBService" -Newest 10 | Format-List *
```

### Verify Files Exist
```cmd
dir "C:\Program Files\ChronosDB\bin\chronosdb_*.exe"
dir "C:\Program Files\ChronosDB\bin\chronosdb.conf"
```

### Manual Test
Try running the server manually to see if it starts:
```cmd
cd "C:\Program Files\ChronosDB\bin"
chronosdb_server.exe
```

If it fails, check the error message - might be a config file issue.
