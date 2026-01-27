# Rebuild Service to Fix Error 1053

## Problem
Service shows as running in installer but fails with Error 1053 when started manually. This means the service executable needs to be rebuilt with the latest fixes.

## Solution

### Step 1: Rebuild the Service Executable
```powershell
cd G:\University\Graduation\ChronosDB
cmake --build cmake-build-debug --target chronosdb_service --config Debug
```

### Step 2: Stop and Remove Old Service
```cmd
sc stop ChronosDBService
sc delete ChronosDBService
```

### Step 3: Copy New Service Executable
```cmd
copy cmake-build-debug\chronosdb_service.exe "C:\Program Files\ChronosDB\bin\"
```

### Step 4: Recreate and Start Service
```cmd
sc create ChronosDBService binPath= "C:\Program Files\ChronosDB\bin\chronosdb_service.exe" start= auto
sc start ChronosDBService
```

### Step 5: Verify
```cmd
sc query ChronosDBService
```

Should show: `STATE: 4 RUNNING`

## What Was Fixed

1. **Process Verification**: Service now verifies the server process actually started before reporting RUNNING
2. **Quick Crash Detection**: Detects if server process exits immediately (within 100ms)
3. **Better Error Handling**: Reports specific error codes if process fails to start
4. **Proper Status Reporting**: Reports RUNNING only after confirming process is alive

## Alternative: Reinstall Using Installer

1. Uninstall ChronosDB from Control Panel
2. Rebuild service: `cmake --build cmake-build-debug --target chronosdb_service`
3. Recompile installer
4. Run installer again

## Check Event Viewer

If service still fails, check Event Viewer:
```powershell
Get-EventLog -LogName Application -Source "ChronosDBService" -Newest 10
```

Look for error messages about:
- Server process not found
- Process exited immediately
- Invalid process handle
