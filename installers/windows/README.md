# ChronosDB Windows Installer

This folder contains the Windows installer configuration for ChronosDB.

## 📦 Contents

- **installer.iss** - Inno Setup script for creating Windows installer
- **README.md** - This file

## 🔧 Prerequisites

1. **Inno Setup 6.x** - Download from https://jrsoftware.org/isdl.php
2. **Built binaries** - Must compile ChronosDB first in `cmake-build-release/`

## 🏗️ Building the Installer

### Step 1: Build ChronosDB

```powershell
# From project root
mkdir cmake-build-release
cd cmake-build-release
cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
cd ..
```

### Step 2: Verify Binaries

Make sure these files exist in `cmake-build-release/`:
- `chronosdb_server.exe`
- `chronosdb_shell.exe`
- `chronosdb_service.exe`
- Any required `.dll` files

### Step 3: Compile Installer

**Option A: Using Inno Setup GUI**
1. Navigate to `installers/windows/` folder
2. Open `installer.iss` in Inno Setup Compiler
3. Click **Build** > **Compile**
4. Installer will be created in `../../Output/` folder

**Option B: Using Command Line**
```powershell
cd installers\windows
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss
```

Output: `../../Output/ChronosDB_Setup_1.0.0.exe`

## 📋 Installer Features

✅ **Service Installation** - Installs as Windows service
✅ **Configuration Wizard** - Interactive setup for:
  - Server port
  - Root credentials
  - Encryption options
✅ **PATH Integration** - Adds to system PATH
✅ **Protocol Handler** - Registers `chronos://` protocol
✅ **Start Menu Shortcuts** - Desktop and menu items
✅ **Upgrade Support** - Preserves config on upgrade
✅ **Clean Uninstall** - Optional data deletion

## 🎯 Output

After compilation, find the installer at:
```
ChronosDB/Output/ChronosDB_Setup_1.0.0.exe
```

## 🎯 Output

After compilation, find the installer at:
```
ChronosDB/Output/ChronosDB_Setup_1.0.0.exe
```

## 🔧 Post-Installation Management

After installation, you can manage ChronosDB using the provided scripts:

### Batch Scripts (Easiest - Click to Run)
Located in `C:\Program Files\ChronosDB\bin\`:

- **start_server.bat** - Start the ChronosDB service
- **stop_server.bat** - Stop the ChronosDB service gracefully

These scripts are available in the Start Menu under "ChronosDB" folder.

### PowerShell Module (Advanced)

For advanced management, load the PowerShell utility module:

```powershell
# Load the module (run PowerShell as Administrator)
. "C:\Program Files\ChronosDB\bin\ChronosDBUtils.ps1"

# Available commands:
Start-ChronosDBServer            # Start service
Stop-ChronosDBServer             # Stop service gracefully
Stop-ChronosDBServer -Force      # Force stop
Restart-ChronosDBServer          # Restart service
Get-ChronosDBStatus              # Check service status
```

### VBScript Versions (Compatibility)

For older Windows systems or when PowerShell is unavailable:

```cmd
cscript.exe "C:\Program Files\ChronosDB\bin\start_server.vbs"
cscript.exe "C:\Program Files\ChronosDB\bin\stop_server.vbs"
```

### Command Line (sc.exe)

For direct service control:

```cmd
# Start
sc start ChronosDBService

# Stop gracefully
sc stop ChronosDBService

# Force stop (if stuck)
taskkill /F /IM chronosdb_server.exe
```

## ⚙️ Customization

Edit `installer.iss` to customize:

```ini
[Setup]
AppVersion=1.0.0              ; Change version
DefaultDirName={autopf}\ChronosDB  ; Change install path
OutputBaseFilename=ChronosDB_Setup_{#MyAppVersion}  ; Output name
```

## 🐛 Troubleshooting

**Issue**: Build fails with "Source file not found"
- Solution: Ensure binaries are compiled in `cmake-build-release/`
- Check: File paths in `[Files]` section match your build output

**Issue**: Service won't start after install
- Solution: Check `chronosdb.conf` is generated correctly
- Verify: Port 2501 is not in use
- Try: `start_server.bat` script to see detailed error message

**Issue**: Service is stuck and won't stop
- Solution: Use `stop_server.bat` which includes force-kill logic
- Alternative: Use PowerShell with `-Force` flag:
  ```powershell
  . "C:\Program Files\ChronosDB\bin\ChronosDBUtils.ps1"
  Stop-ChronosDBServer -Force
  ```
- Last resort: Manual process kill:
  ```cmd
  taskkill /F /IM chronosdb_server.exe
  taskkill /F /IM chronosdb_service.exe
  ```

**Issue**: Service not found after install
- Solution: Service may not have been created. Run as Administrator and execute:
  ```cmd
  sc create ChronosDBService binPath= "C:\Program Files\ChronosDB\bin\chronosdb_service.exe" start= auto
  ```
- Then: Start using `start_server.bat`

**Issue**: Missing DLL errors
- Solution: Copy all DLLs from build directory to installer
- Check: MinGW DLLs if using MinGW compiler

**Issue**: Permission denied on data or log directories
- Solution: The installer automatically grants permissions, but to fix manually:
  ```cmd
  icacls "C:\Program Files\ChronosDB\data" /grant Users:(OI)(CI)M /T /C /Q
  icacls "C:\Program Files\ChronosDB\log" /grant Users:(OI)(CI)M /T /C /Q
  ```

## 📝 Testing

Test the installer before distribution:

1. **Install test**
   ```powershell
   ChronosDB_Setup_1.0.0.exe /SILENT /LOG="install.log"
   ```

2. **Verify installation**
   ```powershell
   net start ChronosDBService
   chronosdb --version
   ```

3. **Uninstall test**
   ```powershell
   "C:\Program Files\ChronosDB\unins000.exe" /SILENT
   ```

## 🔐 Code Signing (Optional)

For production, sign the installer:

```powershell
signtool sign /f certificate.pfx /p password /t http://timestamp.digicert.com ChronosDB_Setup_1.0.0.exe
```

## 📚 Documentation

- Main installer config: `installer.iss`
- Installation guide: `../../INSTALLATION_GUIDE.md`
- Project README: `../../README.md`

## 🚀 Distribution

Upload the installer to:
- GitHub Releases
- Official website
- Package managers (Chocolatey, WinGet)

---

**Last Updated**: January 19, 2026

