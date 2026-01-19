; FrancoDB Installer for Windows (Inno Setup)
; Version: 2.0 - FIXED paths for new installer location

#define MyAppName "FrancoDB"
#define MyAppVersion "1.0.0"
#define MyAppExeName "francodb_server.exe"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=FrancoDB Team
AppPublisherURL=https://github.com/yourusername/FrancoDB
AppId={{8C5E4A9B-3F2D-4B1C-9A7E-5D8F2C1B4A3E}
DefaultDirName={autopf}\FrancoDB
DefaultGroupName=FrancoDB
OutputDir=..\..\Output
OutputBaseFilename=FrancoDB_Setup_{#MyAppVersion}
Compression=lzma
SolidCompression=yes
ChangesEnvironment=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
WizardStyle=modern
SetupIconFile=..\..\resources\francodb.ico
UninstallDisplayIcon={app}\bin\{#MyAppExeName}
AppMutex=FrancoDBInstaller
UsePreviousAppDir=yes
DirExistsWarning=auto
AllowNoIcons=yes
ShowLanguageDialog=no

[Dirs]
Name: "{app}\data"; Permissions: users-modify
Name: "{app}\log"; Permissions: users-modify
Name: "{app}\etc"; Permissions: users-modify

[Files]
; ================= BINARIES =================
; Server
Source: "..\..\cmake-build-release\francodb_server.exe"; DestDir: "{app}\bin"; Flags: ignoreversion; Check: FileExists(ExpandConstant('..\..\cmake-build-release\francodb_server.exe'))

; Shell
Source: "..\..\cmake-build-release\francodb_shell.exe"; DestDir: "{app}\bin"; Flags: ignoreversion; Check: FileExists(ExpandConstant('..\..\cmake-build-release\francodb_shell.exe'))
Source: "..\..\cmake-build-release\francodb_shell.exe"; DestDir: "{app}\bin"; DestName: "francodb.exe"; Flags: ignoreversion; Check: FileExists(ExpandConstant('..\..\cmake-build-release\francodb_shell.exe'))

; Service (Windows Service Runner)
Source: "..\..\cmake-build-release\francodb_service.exe"; DestDir: "{app}\bin"; Flags: ignoreversion; Check: FileExists(ExpandConstant('..\..\cmake-build-release\francodb_service.exe'))

; ================= DLLs =================
Source: "..\..\cmake-build-release\*.dll"; DestDir: "{app}\bin"; Flags: ignoreversion skipifsourcedoesntexist

; ================= DOCUMENTATION =================
Source: "..\..\README.md"; DestDir: "{app}"; Flags: isreadme skipifsourcedoesntexist
Source: "..\..\INSTALLATION_GUIDE.md"; DestDir: "{app}"; Flags: skipifsourcedoesntexist
Source: "..\..\QUICK_START_S_PLUS.md"; DestDir: "{app}"; Flags: skipifsourcedoesntexist

[UninstallDelete]
Type: files; Name: "{app}\bin\francodb.conf"
Type: filesandordirs; Name: "{app}\data"
Type: filesandordirs; Name: "{app}\log"
Type: dirifempty; Name: "{app}"

[Icons]
Name: "{group}\{#MyAppName} Shell"; Filename: "{app}\bin\francodb.exe"; WorkingDir: "{app}\bin"
Name: "{group}\Configuration"; Filename: "notepad.exe"; Parameters: """{app}\bin\francodb.conf"""
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\bin\francodb.exe"; WorkingDir: "{app}\bin"

[Registry]
; Add to System PATH
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; \
    ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}\bin"; \
    Check: NeedsAddPath(ExpandConstant('{app}\bin'))

; URL Protocol Handler (maayn://)
Root: HKCR; Subkey: "maayn"; ValueType: string; ValueName: ""; ValueData: "URL:FrancoDB Protocol"; Flags: uninsdeletekey
Root: HKCR; Subkey: "maayn"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""; Flags: uninsdeletekey
Root: HKCR; Subkey: "maayn\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\francodb_shell.exe,0"; Flags: uninsdeletekey
Root: HKCR; Subkey: "maayn\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\francodb_shell.exe"" ""%1"""; Flags: uninsdeletekey

[UninstallRun]
Filename: "sc.exe"; Parameters: "stop FrancoDBService"; Flags: runhidden; RunOnceId: "StopService"
Filename: "sc.exe"; Parameters: "delete FrancoDBService"; Flags: runhidden; RunOnceId: "DeleteService"

[Run]
; Start service after fresh install
Filename: "sc.exe"; Parameters: "start FrancoDBService"; Flags: runhidden; RunOnceId: "StartService"; Check: (not IsUpgrade) and ServiceInstalled

[Code]
var
  PortPage: TInputQueryWizardPage;
  CredentialsPage: TInputQueryWizardPage;
  EncryptionPage: TInputOptionWizardPage;
  EncryptionKeyPage: TInputQueryWizardPage;
  SummaryPage: TOutputMsgMemoWizardPage;
  
  GeneratedEncryptionKey: String;
  IsUpgrade: Boolean;
  ServiceInstalled: Boolean;
  ServiceStarted: Boolean;
  ServiceRebootRequired: Boolean;
  ServiceStartResult: Integer;

// ============================================================================
// Helper: Check if path needs to be added to environment
// ============================================================================
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
    'Path', OrigPath)
  then begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + Param + ';', ';' + OrigPath + ';') = 0;
end;

// ============================================================================
// Helper: Generate random 64-char hex encryption key
// ============================================================================
function GenerateEncryptionKey(): String;
var
  I: Integer;
  Bytes: Array[0..31] of Byte;
begin
  Result := '';
  for I := 0 to 31 do
  begin
    Bytes[I] := Random(256);
    Result := Result + Format('%.2x', [Bytes[I]]);
  end;
end;

// ============================================================================
// Initialization: Check for upgrades
// ============================================================================
function InitializeSetup(): Boolean;
var
  PrevPath: String;
begin
  Result := True;
  IsUpgrade := False;
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 
     'Software\Microsoft\Windows\CurrentVersion\Uninstall\{8C5E4A9B-3F2D-4B1C-9A7E-5D8F2C1B4A3E}_is1',
     'InstallLocation', PrevPath) then
  begin
    IsUpgrade := True;
    if MsgBox('Upgrade detected. Preserve configuration?' + #13#10 + #13#10 +
              'Click Yes to keep existing settings, No for fresh install', 
              mbConfirmation, MB_YESNO) = IDNO then
       IsUpgrade := False;
  end;
end;

function BoolToStr(Value: Boolean): String;
begin
  if Value then Result := 'true' else Result := 'false';
end;

function NeedRestart(): Boolean;
begin
  Result := ServiceRebootRequired;
end;

// ============================================================================
// Wizard Pages Setup
// ============================================================================
procedure InitializeWizard;
begin
  PortPage := CreateInputQueryPage(wpWelcome, 'Server Configuration', 'Network Port', 
                                    'Enter the port to run FrancoDB server on:');
  PortPage.Add('Port (1024-65535):', False);
  PortPage.Values[0] := '2501';

  CredentialsPage := CreateInputQueryPage(PortPage.ID, 'Server Configuration', 'Root User Credentials', 
                                           'Set root database user credentials:');
  CredentialsPage.Add('Username:', False);
  CredentialsPage.Add('Password:', True);
  CredentialsPage.Add('Confirm Password:', True);
  CredentialsPage.Values[0] := 'maayn';

  EncryptionPage := CreateInputOptionPage(CredentialsPage.ID, 'Security Settings', 'Encryption', 
                                          'Choose encryption preference:', True, False);
  EncryptionPage.Add('No encryption');
  EncryptionPage.Add('Auto-generated key');
  EncryptionPage.Add('Custom 64-char hex key');
  EncryptionPage.SelectedValueIndex := 1;

  EncryptionKeyPage := CreateInputQueryPage(EncryptionPage.ID, 'Custom Encryption Key', 'Security', 
                                            'Enter your 64-character hexadecimal encryption key:');
  EncryptionKeyPage.Add('Encryption Key:', False);

  SummaryPage := CreateOutputMsgMemoPage(wpInstalling, 'Installation Summary', 'Installation Result', 
                                         'Review the installation status below:', '');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // Skip config pages on upgrade
  if IsUpgrade and ((PageID = PortPage.ID) or (PageID = CredentialsPage.ID) or 
                    (PageID = EncryptionPage.ID) or (PageID = EncryptionKeyPage.ID)) then
  begin
    Result := True;
    Exit;
  end;
  // Skip custom key page if not selected
  if PageID = EncryptionKeyPage.ID then 
    Result := (EncryptionPage.SelectedValueIndex <> 2);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  PortNum: Integer;
begin
  Result := True;
  
  if CurPageID = PortPage.ID then
  begin
    PortNum := StrToIntDef(PortPage.Values[0], 0);
    if (PortNum < 1024) or (PortNum > 65535) then
    begin
      MsgBox('Port must be between 1024 and 65535', mbError, MB_OK);
      Result := False;
    end;
  end;
  
  if CurPageID = CredentialsPage.ID then
  begin
    if CredentialsPage.Values[1] <> CredentialsPage.Values[2] then
    begin
      MsgBox('Passwords do not match', mbError, MB_OK);
      Result := False;
    end;
    if Length(CredentialsPage.Values[0]) < 3 then
    begin
      MsgBox('Username must be at least 3 characters', mbError, MB_OK);
      Result := False;
    end;
  end;
  
  if CurPageID = EncryptionPage.ID then
  begin
    if EncryptionPage.SelectedValueIndex = 1 then
      GeneratedEncryptionKey := GenerateEncryptionKey();
  end;
end;

// ============================================================================
// Helper: Check if process is running
// ============================================================================
function IsAppRunning(const FileName: string): Boolean;
var
  ResultCode: Integer;
begin
  Exec('tasklist.exe', '/FI "IMAGENAME eq ' + FileName + '"', '', SW_HIDE, 
       ewWaitUntilTerminated, ResultCode);
  Result := (ResultCode = 0);
end;

// ============================================================================
// Installation Steps
// ============================================================================
procedure CurStepChanged(CurStep: TSetupStep);
var
  Port, User, Pass, Key: String;
  ConfigContent, SummaryText: String;
  EncEnabled: Boolean;
  ResultCode, I: Integer;
  ServicePath, DataDir, LogDir: String;
begin
  // ========== PRE-INSTALL: Stop existing service ==========
  if CurStep = ssInstall then
  begin
    WizardForm.StatusLabel.Caption := 'Stopping existing FrancoDB service...';
    Exec('sc.exe', 'stop FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    
    // Wait for graceful shutdown (max 30 seconds)
    WizardForm.StatusLabel.Caption := 'Waiting for database to shut down safely (max 30s)...';
    for I := 1 to 30 do
    begin
      if not IsAppRunning('francodb_server.exe') then Break;
      Sleep(1000);
    end;
    
    // If still running, ask user to stop it
    while IsAppRunning('francodb_server.exe') do
    begin
      if MsgBox('Database server still running.' + #13#10 + 
                'Please close it using Task Manager, then click OK.', 
                mbError, MB_OKCANCEL) = IDCANCEL then
        Abort;
    end;
    
    // Delete old service entry
    Exec('sc.exe', 'delete FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(1000);
  end;

  // ========== POST-INSTALL: Configure and start ==========
  if CurStep = ssPostInstall then
  begin
    DataDir := ExpandConstant('{app}\data');
    LogDir  := ExpandConstant('{app}\log');
    
    // Set permissions
    WizardForm.StatusLabel.Caption := 'Setting permissions...';
    Exec('icacls', '"' + DataDir + '" /grant Users:(OI)(CI)M /T /C /Q', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec('icacls', '"' + LogDir + '" /grant Users:(OI)(CI)M /T /C /Q', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    
    // Generate config for new installs
    if not IsUpgrade then
    begin
      WizardForm.StatusLabel.Caption := 'Generating configuration...';
      Port := PortPage.Values[0];
      User := CredentialsPage.Values[0];
      Pass := CredentialsPage.Values[1];
      EncEnabled := (EncryptionPage.SelectedValueIndex > 0);
      
      if EncryptionPage.SelectedValueIndex = 2 then 
        Key := EncryptionKeyPage.Values[0] 
      else 
        Key := GeneratedEncryptionKey;
      
      ConfigContent := '# FrancoDB Configuration' + #13#10 + #13#10 +
                       'port = ' + Port + #13#10 +
                       'bind_address = "0.0.0.0"' + #13#10 +
                       'root_username = "' + User + '"' + #13#10 +
                       'root_password = "' + Pass + '"' + #13#10 +
                       'data_directory = "' + DataDir + '"' + #13#10 +
                       'log_directory = "' + LogDir + '"' + #13#10 +
                       'encryption_enabled = ' + BoolToStr(EncEnabled) + #13#10 +
                       'autosave_interval = 30' + #13#10;
      
      if EncEnabled and (Key <> '') then 
        ConfigContent := ConfigContent + 'encryption_key = "' + Key + '"' + #13#10;
      
      SaveStringToFile(ExpandConstant('{app}\bin\francodb.conf'), ConfigContent, False);
    end;
    
    // Create Windows Service
    WizardForm.StatusLabel.Caption := 'Creating Windows service...';
    ServicePath := ExpandConstant('{app}\bin\francodb_service.exe');
    ServiceInstalled := False;
    ServiceRebootRequired := False;
    
    if FileExists(ServicePath) then
    begin
      for I := 1 to 3 do
      begin
        Exec('sc.exe', 'create FrancoDBService binPath= "' + ServicePath + '" start= auto', 
             '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
        
        if ResultCode = 0 then
        begin
          ServiceInstalled := True;
          Break;
        end
        else if ResultCode = 1073 then  // Service already exists
        begin
          Exec('sc.exe', 'delete FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
          Sleep(2000);
        end
        else if ResultCode = 1072 then  // Mark for deletion
        begin
          Sleep(2000);
          ServiceRebootRequired := True;
        end;
      end;
      
      // Set service recovery options
      if ServiceInstalled then
      begin
        Exec('sc.exe', 'failure FrancoDBService reset= 86400 actions= restart/5000', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
        Sleep(500);
      end;
    end;
    
    // Show summary
    WizardForm.StatusLabel.Caption := 'Installation complete. Preparing summary...';
    SummaryText := 'FrancoDB Installation Summary' + #13#10 + 
                   '========================================' + #13#10 + #13#10;
    
    if ServiceInstalled then
      SummaryText := SummaryText + '[OK] Windows Service: Created successfully' + #13#10
    else
      SummaryText := SummaryText + '[ERROR] Windows Service: Failed to create' + #13#10;
    
    if IsUpgrade then
      SummaryText := SummaryText + '[OK] Configuration: Preserved from previous version' + #13#10
    else
      SummaryText := SummaryText + '[OK] Configuration: Generated successfully' + #13#10;
    
    if NeedsAddPath(ExpandConstant('{app}\bin')) then
      SummaryText := SummaryText + '[OK] System PATH: Updated (restart terminal to apply)' + #13#10
    else
      SummaryText := SummaryText + '[OK] System PATH: Already configured' + #13#10;
    
    SummaryText := SummaryText + #13#10 + 'Next Steps:' + #13#10 +
                   '1. Edit config: ' + ExpandConstant('{app}\bin\francodb.conf') + #13#10 +
                   '2. Start service: net start FrancoDBService' + #13#10 +
                   '3. Connect: francodb' + #13#10 + #13#10;
    
    if not IsUpgrade and (EncryptionPage.SelectedValueIndex = 1) then
    begin
      SummaryText := SummaryText + 'IMPORTANT: Your encryption key' + #13#10 +
                     '========================================' + #13#10 +
                     'Save this key - data is unrecoverable without it:' + #13#10 + #13#10 +
                     GeneratedEncryptionKey + #13#10;
    end;
    
    SummaryPage.RichEditViewer.Lines.Text := SummaryText;
    SummaryPage.RichEditViewer.Font.Name := 'Consolas';
    SummaryPage.RichEditViewer.Font.Size := 9;
  end;
end;

// ============================================================================
// Uninstallation
// ============================================================================
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec('sc.exe', 'stop FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec('sc.exe', 'delete FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(1000);
  end;

  if CurUninstallStep = usPostUninstall then
  begin
    if DirExists(ExpandConstant('{app}\data')) or DirExists(ExpandConstant('{app}\log')) then
    begin
      if MsgBox('Delete database files and logs?' + #13#10 +
                'This action cannot be undone!', 
                mbConfirmation, MB_YESNO) = IDYES then
      begin
        DelTree(ExpandConstant('{app}\data'), True, True, True);
        DelTree(ExpandConstant('{app}\log'), True, True, True);
        DeleteFile(ExpandConstant('{app}\bin\francodb.conf'));
        RemoveDir(ExpandConstant('{app}\bin'));
        RemoveDir(ExpandConstant('{app}'));
      end;
    end;
  end;
end;

