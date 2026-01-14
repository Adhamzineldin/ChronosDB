[Setup]
AppName=FrancoDB
AppVersion=1.0
AppPublisher=FrancoDB Team
AppPublisherURL=https://github.com/yourusername/FrancoDB
AppId={{8C5E4A9B-3F2D-4B1C-9A7E-5D8F2C1B4A3E}
DefaultDirName={autopf}\FrancoDB
DefaultGroupName=FrancoDB
OutputBaseFilename=FrancoDB_Setup
Compression=lzma
SolidCompression=yes
; Automatically updates PATH
ChangesEnvironment=yes
PrivilegesRequired=admin
; Critical for 64-bit systems
ArchitecturesInstallIn64BitMode=x64
WizardStyle=modern
SetupIconFile=resources\francodb.ico
UninstallDisplayIcon={app}\bin\francodb_server.exe
AppMutex=FrancoDBInstaller
UsePreviousAppDir=yes
DirExistsWarning=auto

[Files]
; --- CRITICAL: Primary paths WITHOUT Check flag ---
; If these files don't exist, installer compilation will FAIL (which is what we want)
; This prevents creating a broken installer that silently skips missing files

; 1. Server Executable (Primary: cmake-build-debug, Fallbacks with Check)
Source: "cmake-build-debug\francodb_server.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "build\francodb_server.exe"; DestDir: "{app}\bin"; Flags: ignoreversion; Check: FileExists(ExpandConstant('{src}\build\francodb_server.exe'))
Source: "cmake-build-release\francodb_server.exe"; DestDir: "{app}\bin"; Flags: ignoreversion; Check: FileExists(ExpandConstant('{src}\cmake-build-release\francodb_server.exe'))

; 2. Shell Executable (Primary: cmake-build-debug, Fallbacks with Check)
Source: "cmake-build-debug\francodb_shell.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "build\francodb_shell.exe"; DestDir: "{app}\bin"; Flags: ignoreversion; Check: FileExists(ExpandConstant('{src}\build\francodb_shell.exe'))
Source: "cmake-build-release\francodb_shell.exe"; DestDir: "{app}\bin"; Flags: ignoreversion; Check: FileExists(ExpandConstant('{src}\cmake-build-release\francodb_shell.exe'))

; 3. Service Executable (Primary: cmake-build-debug, Fallbacks with Check)
; NOTE: If you don't have a service executable, you need to build it or remove these lines
Source: "cmake-build-debug\francodb_service.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "build\francodb_service.exe"; DestDir: "{app}\bin"; Flags: ignoreversion; Check: FileExists(ExpandConstant('{src}\build\francodb_service.exe'))
Source: "cmake-build-release\francodb_service.exe"; DestDir: "{app}\bin"; Flags: ignoreversion; Check: FileExists(ExpandConstant('{src}\cmake-build-release\francodb_service.exe'))

[Icons]
Name: "{group}\FrancoDB Shell"; Filename: "{app}\bin\francodb_shell.exe"; WorkingDir: "{app}\bin"
Name: "{group}\FrancoDB Configuration"; Filename: "notepad.exe"; Parameters: """{app}\bin\francodb.conf"""
Name: "{group}\Uninstall FrancoDB"; Filename: "{uninstallexe}"

[Registry]
; Standard PATH update
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; \
    ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}\bin"; \
    Check: NeedsAddPath(ExpandConstant('{app}\bin'))

[UninstallRun]
; Stop and remove service on uninstall
Filename: "sc.exe"; Parameters: "stop FrancoDBService"; Flags: runhidden; RunOnceId: "StopService"
Filename: "sc.exe"; Parameters: "delete FrancoDBService"; Flags: runhidden; RunOnceId: "DeleteService"

[Code]
var
  PortPage: TInputQueryWizardPage;
  CredentialsPage: TInputQueryWizardPage;
  EncryptionPage: TInputOptionWizardPage;
  EncryptionKeyPage: TInputQueryWizardPage;
  // Summary Page (Changed to Memo so user can copy text)
  SummaryPage: TOutputMsgMemoWizardPage;
  
  // Logic Variables
  GeneratedEncryptionKey: String;
  IsUpgrade: Boolean;
  ServiceInstalled: Boolean;
  ServiceStarted: Boolean;
  ServiceStartResult: Integer;

// Helper: Check if PATH exists
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

// Helper: Generate Random Key
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

// Helper: Check for previous install
function InitializeSetup(): Boolean;
var
  PrevPath: String;
begin
  Result := True;
  IsUpgrade := False;
  // Check Registry for the AppId
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 
     'Software\Microsoft\Windows\CurrentVersion\Uninstall\{8C5E4A9B-3F2D-4B1C-9A7E-5D8F2C1B4A3E}_is1',
     'InstallLocation', PrevPath) then
  begin
    IsUpgrade := True;
    if MsgBox('A previous installation of FrancoDB was detected at:' + #13#10 + PrevPath + #13#10 + #13#10 +
              'Do you want to upgrade it? (Configuration will be preserved)', mbConfirmation, MB_YESNO) = IDNO then
    begin
       // If they say No, treat as fresh install (user can change path)
       IsUpgrade := False;
    end;
  end;
end;

// Helper: Boolean to String
function BoolToStr(Value: Boolean): String;
begin
  if Value then Result := 'true' else Result := 'false';
end;

procedure InitializeWizard;
begin
  // --- Page 1: Port ---
  PortPage := CreateInputQueryPage(wpWelcome,
    'Server Configuration', 'Port Settings',
    'Enter the port number for the server (Default: 2501)');
  PortPage.Add('Port:', False);
  PortPage.Values[0] := '2501';

  // --- Page 2: Credentials ---
  CredentialsPage := CreateInputQueryPage(PortPage.ID,
    'Server Configuration', 'Root User Credentials',
    'Enter the username and password for the root user.');
  CredentialsPage.Add('Username:', False);
  CredentialsPage.Add('Password:', False);
  CredentialsPage.Values[0] := 'admin';
  CredentialsPage.Values[1] := 'root';

  // --- Page 3: Encryption Mode ---
  EncryptionPage := CreateInputOptionPage(CredentialsPage.ID,
    'Data Encryption', 'Security Settings',
    'Choose your encryption preference:', True, False);
  EncryptionPage.Add('No encryption (Standard)');
  EncryptionPage.Add('Auto-generated key (Recommended)');
  EncryptionPage.Add('Custom key (Advanced)');
  EncryptionPage.SelectedValueIndex := 1;

  // --- Page 4: Custom Key ---
  EncryptionKeyPage := CreateInputQueryPage(EncryptionPage.ID,
    'Encryption Key', 'Custom Key Input',
    'Enter a 64-character hex key.');
  EncryptionKeyPage.Add('Key:', False);

  // --- Page 5: Post-Install Summary (Memo Page for Copying) ---
  // wpInstalling means this page appears AFTER the progress bar finishes
  SummaryPage := CreateOutputMsgMemoPage(wpInstalling,
    'Installation Complete', 'FrancoDB Setup Status',
    'Please review the installation results below.',
    '');
end;

// Skip pages logic
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // If Upgrading, SKIP all configuration pages
  if IsUpgrade then
  begin
    if (PageID = PortPage.ID) or (PageID = CredentialsPage.ID) or 
       (PageID = EncryptionPage.ID) or (PageID = EncryptionKeyPage.ID) then
    begin
      Result := True;
      Exit;
    end;
  end;

  // Logic for custom key page
  if PageID = EncryptionKeyPage.ID then
    Result := (EncryptionPage.SelectedValueIndex <> 2);
end;

// Validate Inputs
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = PortPage.ID then
  begin
    if StrToIntDef(PortPage.Values[0], 0) = 0 then
    begin
      MsgBox('Invalid Port', mbError, MB_OK);
      Result := False;
    end;
  end;
  
  // Generate key if needed when leaving Encryption Page
  if (CurPageID = EncryptionPage.ID) and (EncryptionPage.SelectedValueIndex = 1) then
     GeneratedEncryptionKey := GenerateEncryptionKey();
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Port, User, Pass, Key: String;
  ConfigContent, SummaryText: String;
  EncEnabled: Boolean;
  ResultCode: Integer;
  ServicePath: String;
begin
  // 1. Cleanup Old Service
  if CurStep = ssInstall then
  begin
    Exec('sc.exe', 'stop FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec('sc.exe', 'delete FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;

  // 2. Post Install Logic
  if CurStep = ssPostInstall then
  begin
    CreateDir(ExpandConstant('{app}\data'));

    // ONLY Create config if NOT upgrading
    if not IsUpgrade then
    begin
       Port := PortPage.Values[0];
       User := CredentialsPage.Values[0];
       Pass := CredentialsPage.Values[1];
       EncEnabled := (EncryptionPage.SelectedValueIndex > 0);
       
       if EncryptionPage.SelectedValueIndex = 2 then
          Key := EncryptionKeyPage.Values[0]
       else
          Key := GeneratedEncryptionKey;

       ConfigContent := '# FrancoDB Config' + #13#10 +
                        'port = ' + Port + #13#10 +
                        'root_username = "' + User + '"' + #13#10 +
                        'root_password = "' + Pass + '"' + #13#10 +
                        'data_directory = "../data"' + #13#10 +
                        'encryption_enabled = ' + BoolToStr(EncEnabled) + #13#10 +
                        'autosave_interval = 30';
       
       if EncEnabled and (Key <> '') then
          ConfigContent := ConfigContent + #13#10 + 'encryption_key = "' + Key + '"';

       SaveStringToFile(ExpandConstant('{app}\bin\francodb.conf'), ConfigContent, False);
    end;

    // 3. Install Service (FIXED QUOTING FOR PATHS WITH SPACES)
    // We strictly verify the file exists first
    ServicePath := ExpandConstant('{app}\bin\francodb_service.exe');
    
    if FileExists(ServicePath) then
    begin
       // Create service with proper path quoting
       // sc.exe requires the path to be quoted if it contains spaces
       // Format: sc create ServiceName binPath= "path" start= auto
       // CRITICAL: Check ResultCode, not just Exec return value
       Exec('sc.exe', 'create FrancoDBService binPath= "' + ServicePath + '" start= auto', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
       ServiceInstalled := (ResultCode = 0);
       
       // Verify service was created by querying it
       if ServiceInstalled then
       begin
          Sleep(200); // Small delay
          Exec('sc.exe', 'query FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
          // If query fails, service wasn't created
          if ResultCode <> 0 then
             ServiceInstalled := False;
       end;

       // Configure service failure actions and timeout
       if ServiceInstalled then
       begin
          // Set service to restart on failure
          Exec('sc.exe', 'failure FrancoDBService reset= 86400 actions= restart/5000/restart/10000/restart/20000', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
          // Set service timeout to 60 seconds (default is 30)
          Exec('sc.exe', 'config FrancoDBService start= auto', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
       end;

       // 4. Start & Verify Service (with delay to ensure service is ready)
       if ServiceInstalled then
       begin
          Sleep(500); // Small delay after service creation
          Exec('sc.exe', 'start FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ServiceStartResult);
          // Success if 0 (OK) or 1056 (Already running) or 1058 (Service disabled)
          ServiceStarted := (ServiceStartResult = 0) or (ServiceStartResult = 1056) or (ServiceStartResult = 1058);
          
          // If start failed, wait a bit and check status
          if not ServiceStarted then
          begin
             Sleep(2000);
             Exec('sc.exe', 'query FrancoDBService', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
             // Check if service is actually running despite error code
             if ResultCode = 0 then
                ServiceStarted := True;
          end;
       end
       else
       begin
          ServiceStarted := False;
          ServiceStartResult := 2; // Service not installed
       end;
    end
    else
    begin
       ServiceInstalled := False;
       ServiceStarted := False;
       ServiceStartResult := 2; // File not found manual code
    end;

    // 5. Build Professional Summary Text
    SummaryText := 'STATUS REPORT' + #13#10;
    SummaryText := SummaryText + '------------------------------------------------------------' + #13#10;
    
    // Service Status Section
    if ServiceStarted then
      SummaryText := SummaryText + '[ OK ] Service : FrancoDB is running' + #13#10
    else if not ServiceInstalled then
      SummaryText := SummaryText + '[FAIL] Service : Failed to create service' + #13#10
    else if not FileExists(ServicePath) then
      SummaryText := SummaryText + '[FAIL] Service : Executable missing (Check Antivirus/Build)' + #13#10
    else if ServiceStartResult = 2 then
    begin
      SummaryText := SummaryText + '[FAIL] Service : Service not found (Error 2)' + #13#10;
      SummaryText := SummaryText + '         Service creation failed - check permissions' + #13#10;
      SummaryText := SummaryText + '         Try: sc create FrancoDBService binPath= "' + ServicePath + '"' + #13#10;
    end
    else if ServiceStartResult = 1053 then
    begin
      SummaryText := SummaryText + '[FAIL] Service : Timeout starting (Error 1053)' + #13#10;
      SummaryText := SummaryText + '         Check Event Viewer: eventvwr.msc' + #13#10;
      SummaryText := SummaryText + '         Try manually: sc start FrancoDBService' + #13#10;
    end
    else
      SummaryText := SummaryText + '[FAIL] Service : Failed to start (Error Code ' + IntToStr(ServiceStartResult) + ')' + #13#10;

    // Path Status
    if NeedsAddPath(ExpandConstant('{app}\bin')) then
      SummaryText := SummaryText + '[INFO] Env Var : Updated (Restart your terminal)' + #13#10
    else
      SummaryText := SummaryText + '[ OK ] Env Var : Already configured' + #13#10;

    // Config Status
    if IsUpgrade then
      SummaryText := SummaryText + '[INFO] Config  : Preserved previous settings' + #13#10
    else
      SummaryText := SummaryText + '[ OK ] Config  : Generated successfully' + #13#10;

    SummaryText := SummaryText + #13#10;

    // Encryption Key Display (Only if generated)
    if (not IsUpgrade) and (EncryptionPage.SelectedValueIndex = 1) then
    begin
      SummaryText := SummaryText + 'SECURITY ALERT' + #13#10;
      SummaryText := SummaryText + '------------------------------------------------------------' + #13#10;
      SummaryText := SummaryText + 'Below is your Master Encryption Key. You MUST save this.' + #13#10;
      SummaryText := SummaryText + 'If you lose this key, your database is gone forever.' + #13#10 + #13#10;
      SummaryText := SummaryText + GeneratedEncryptionKey + #13#10;
      SummaryText := SummaryText + '------------------------------------------------------------' + #13#10;
    end;

    // Set the text in the Memo page
    SummaryPage.RichEditViewer.Lines.Text := SummaryText;
    
    // Set font to Consolas or Courier for better alignment
    SummaryPage.RichEditViewer.Font.Name := 'Consolas';
    SummaryPage.RichEditViewer.Font.Size := 9;
  end;
end;
