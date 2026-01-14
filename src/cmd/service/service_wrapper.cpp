// service_wrapper.cpp - Windows Service Wrapper for FrancoDB Server
#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>

// Use non-const strings for service name
static TCHAR g_ServiceName[] = TEXT("FrancoDBService");

// Global variables
SERVICE_STATUS g_ServiceStatus;
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;
PROCESS_INFORMATION g_ServerProcess;
std::atomic<bool> g_Running(false);

// Log to Windows Event Log
void LogEvent(LPCTSTR message, WORD type = EVENTLOG_INFORMATION_TYPE) {
    HANDLE hEventLog = RegisterEventSource(NULL, g_ServiceName);
    if (hEventLog) {
        ReportEvent(hEventLog, type, 0, 0, NULL, 1, 0, &message, NULL);
        DeregisterEventSource(hEventLog);
    }
}

// Start the FrancoDB server process
bool StartServerProcess() {
    // Get the service executable directory
    TCHAR servicePath[MAX_PATH];
    GetModuleFileName(NULL, servicePath, MAX_PATH);

    std::filesystem::path exePath(servicePath);
    std::filesystem::path binDir = exePath.parent_path();
    std::filesystem::path serverExe = binDir / "francodb_server.exe";

    if (!std::filesystem::exists(serverExe)) {
        LogEvent(TEXT("ERROR: francodb_server.exe not found"), EVENTLOG_ERROR_TYPE);
        return false;
    }

    // Setup process creation (use Unicode version)
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Run hidden

    ZeroMemory(&g_ServerProcess, sizeof(g_ServerProcess));

    // Create command line (needs to be writable)
    std::wstring cmdLine = L"\"" + serverExe.wstring() + L"\"";
    std::vector<wchar_t> cmdLineBuffer(cmdLine.begin(), cmdLine.end());
    cmdLineBuffer.push_back(0);

    // Set working directory to bin folder
    std::wstring workingDir = binDir.wstring();
    std::wstring appName = serverExe.wstring();

    // Create the process
    BOOL result = CreateProcessW(
        appName.c_str(),               // Application name (Unicode)
        cmdLineBuffer.data(),          // Command line (Unicode)
        NULL,                          // Process security attributes
        NULL,                          // Thread security attributes
        FALSE,                         // Inherit handles
        CREATE_NO_WINDOW,              // Creation flags
        NULL,                          // Environment
        workingDir.c_str(),           // Current directory (Unicode)
        &si,                           // Startup info
        &g_ServerProcess               // Process information
    );

    if (!result) {
        DWORD error = GetLastError();
        TCHAR errorMsg[256];
        _stprintf_s(errorMsg, TEXT("Failed to start server process. Error: %d"), error);
        LogEvent(errorMsg, EVENTLOG_ERROR_TYPE);
        return false;
    }

    g_Running = true;
    LogEvent(TEXT("FrancoDB server process started successfully"));
    return true;
}

// Stop the FrancoDB server process
void StopServerProcess() {
    if (g_ServerProcess.hProcess == NULL) {
        return;
    }

    g_Running = false;

    // First, try graceful shutdown with Ctrl+C
    if (AttachConsole(g_ServerProcess.dwProcessId)) {
        SetConsoleCtrlHandler(NULL, TRUE);
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        FreeConsole();
        SetConsoleCtrlHandler(NULL, FALSE);

        // Wait up to 10 seconds for graceful shutdown
        DWORD waitResult = WaitForSingleObject(g_ServerProcess.hProcess, 10000);
        if (waitResult == WAIT_OBJECT_0) {
            LogEvent(TEXT("Server shutdown gracefully"));
            CloseHandle(g_ServerProcess.hProcess);
            CloseHandle(g_ServerProcess.hThread);
            g_ServerProcess.hProcess = NULL;
            return;
        }
    }

    // If graceful shutdown failed, terminate the process
    LogEvent(TEXT("Graceful shutdown failed, terminating process..."), EVENTLOG_WARNING_TYPE);
    TerminateProcess(g_ServerProcess.hProcess, 1);
    WaitForSingleObject(g_ServerProcess.hProcess, 5000);

    CloseHandle(g_ServerProcess.hProcess);
    CloseHandle(g_ServerProcess.hThread);
    g_ServerProcess.hProcess = NULL;

    LogEvent(TEXT("Server process terminated"));
}

// Monitor the server process and restart if it crashes
void MonitorServerProcess() {
    while (g_Running) {
        if (g_ServerProcess.hProcess != NULL) {
            DWORD waitResult = WaitForSingleObject(g_ServerProcess.hProcess, 1000);

            if (waitResult == WAIT_OBJECT_0) {
                // Process exited
                DWORD exitCode;
                GetExitCodeProcess(g_ServerProcess.hProcess, &exitCode);

                CloseHandle(g_ServerProcess.hProcess);
                CloseHandle(g_ServerProcess.hThread);
                g_ServerProcess.hProcess = NULL;

                if (g_Running) {
                    // Unexpected exit - try to restart
                    TCHAR msg[256];
                    _stprintf_s(msg, TEXT("Server process exited unexpectedly (code %d). Restarting..."), exitCode);
                    LogEvent(msg, EVENTLOG_WARNING_TYPE);

                    Sleep(2000); // Wait 2 seconds before restart

                    if (!StartServerProcess()) {
                        LogEvent(TEXT("Failed to restart server process"), EVENTLOG_ERROR_TYPE);
                        g_Running = false;
                    }
                }
            }
        } else if (g_Running) {
            // Process handle is NULL but we should be running - restart
            Sleep(2000);
            StartServerProcess();
        }
    }
}

// Service control handler
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
                break;

            LogEvent(TEXT("Service stop requested"));

            g_ServiceStatus.dwControlsAccepted = 0;
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            g_ServiceStatus.dwWin32ExitCode = 0;
            g_ServiceStatus.dwCheckPoint = 4;

            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

            // Signal the service to stop
            SetEvent(g_ServiceStopEvent);
            break;

        case SERVICE_CONTROL_INTERROGATE:
            break;

        default:
            break;
    }
}

// Service main function
VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    // Register control handler
    g_StatusHandle = RegisterServiceCtrlHandler(g_ServiceName, ServiceCtrlHandler);

    if (g_StatusHandle == NULL) {
        LogEvent(TEXT("Failed to register service control handler"), EVENTLOG_ERROR_TYPE);
        return;
    }

    // Initialize service status
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Create stop event
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) {
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    // Service is starting - report progress with checkpoints
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwCheckPoint = 1;
    g_ServiceStatus.dwWaitHint = 30000; // 30 seconds timeout
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Start the server process
    g_ServiceStatus.dwCheckPoint = 2;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    
    // CRITICAL: Report RUNNING immediately after starting process
    // Don't wait for server to fully initialize - that can cause timeout
    if (!StartServerProcess()) {
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        g_ServiceStatus.dwServiceSpecificExitCode = 1;
        g_ServiceStatus.dwCheckPoint = 0;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        CloseHandle(g_ServiceStopEvent);
        LogEvent(TEXT("FrancoDB Service failed to start server process"), EVENTLOG_ERROR_TYPE);
        return;
    }

    // Start monitoring thread BEFORE reporting RUNNING
    std::thread monitorThread(MonitorServerProcess);
    monitorThread.detach();

    // CRITICAL: Report RUNNING immediately - Windows requires this within 30 seconds
    // The server process initialization happens in background
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    LogEvent(TEXT("FrancoDB Service started successfully - server process launched"));

    // Wait for stop signal
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);

    // Service is stopping
    g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
    g_ServiceStatus.dwCheckPoint = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Stop the server process
    StopServerProcess();

    // Cleanup
    CloseHandle(g_ServiceStopEvent);

    // Service has stopped
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    LogEvent(TEXT("FrancoDB Service stopped"));
}

// Entry point
int _tmain(int argc, TCHAR* argv[]) {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        {g_ServiceName, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE) {
        DWORD error = GetLastError();
        if (error == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            // Running as console app for testing
            std::wcout << L"FrancoDB Service Wrapper" << std::endl;
            std::wcout << L"This program is meant to run as a Windows Service." << std::endl;
            std::wcout << L"Install it using: sc create FrancoDBService binPath= <path>" << std::endl;
            return 0;
        }
        return error;
    }

    return 0;
}
