#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>

static TCHAR g_ServiceName[] = TEXT("FrancoDBService");
SERVICE_STATUS g_ServiceStatus;
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;
PROCESS_INFORMATION g_ServerProcess = {0};
bool g_IsStopping = false;

// circuit breaker variables
int g_RestartCount = 0;
const int MAX_RESTARTS = 5;
auto g_LastRestartTime = std::chrono::steady_clock::now();

std::string GetExeDir() {
    wchar_t buffer[32767]; // Large buffer for long paths
    GetModuleFileNameW(NULL, buffer, 32767);
    return std::filesystem::path(buffer).parent_path().string();
}

void LogDebug(const std::string &msg) {
    try {
        std::filesystem::path logDir = std::filesystem::path(GetExeDir()).parent_path() / "log";
        if (!std::filesystem::exists(logDir)) std::filesystem::create_directories(logDir);
        
        std::ofstream log(logDir / "francodb_service.log", std::ios::app);
        if (log.is_open()) {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char timeBuf[26];
            ctime_s(timeBuf, sizeof(timeBuf), &now);
            std::string t(timeBuf);
            if (!t.empty()) t.pop_back();
            log << "[" << t << "] " << msg << std::endl;
        }
    } catch (...) {}
}

void ReportStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) {
    static DWORD checkPoint = 1;
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = currentState;
    g_ServiceStatus.dwWin32ExitCode = win32ExitCode;
    g_ServiceStatus.dwWaitHint = waitHint;
    g_ServiceStatus.dwControlsAccepted = (currentState == SERVICE_START_PENDING) ? 0 : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_ServiceStatus.dwCheckPoint = (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED) ? 0 : checkPoint++;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

bool StartServerProcess() {
    std::filesystem::path binDir = GetExeDir();
    std::filesystem::path serverExe = binDir / "francodb_server.exe";

    if (!std::filesystem::exists(serverExe)) return false;

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; 

    std::wstring cmdLine = L"\"" + serverExe.wstring() + L"\" --service";
    std::vector<wchar_t> cmdLineBuffer(cmdLine.begin(), cmdLine.end());
    cmdLineBuffer.push_back(0);

    return CreateProcessW(NULL, cmdLineBuffer.data(), NULL, NULL, FALSE, 
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, NULL, binDir.wstring().c_str(), &si, &g_ServerProcess);
}

void StopServerProcess() {
    if (g_ServerProcess.hProcess == NULL) return;

    // The Server must have its own Signal Handler for CTRL_BREAK_EVENT
    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, g_ServerProcess.dwProcessId);

    if (WaitForSingleObject(g_ServerProcess.hProcess, 8000) == WAIT_TIMEOUT) {
        LogDebug("Server lazy exit. Killing process.");
        TerminateProcess(g_ServerProcess.hProcess, 1);
    }

    CloseHandle(g_ServerProcess.hProcess);
    CloseHandle(g_ServerProcess.hThread);
    g_ServerProcess.hProcess = NULL;
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    if (CtrlCode == SERVICE_CONTROL_STOP || CtrlCode == SERVICE_CONTROL_SHUTDOWN) {
        g_IsStopping = true;
        ReportStatus(SERVICE_STOP_PENDING, 0, 10000);
        SetEvent(g_ServiceStopEvent);
    }
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
    g_StatusHandle = RegisterServiceCtrlHandler(g_ServiceName, ServiceCtrlHandler);
    ReportStatus(SERVICE_START_PENDING, 0, 3000);
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!StartServerProcess()) {
        ReportStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    // Check for instant crash
    if (WaitForSingleObject(g_ServerProcess.hProcess, 1500) == WAIT_OBJECT_0) {
        ReportStatus(SERVICE_STOPPED, 1067, 0); // Process terminated unexpectedly
        return;
    }

    ReportStatus(SERVICE_RUNNING, 0, 0);

    while (!g_IsStopping) {
        HANDLE waitObjects[2] = { g_ServiceStopEvent, g_ServerProcess.hProcess };
        DWORD result = WaitForMultipleObjects(2, waitObjects, FALSE, INFINITE);
        
        if (result == WAIT_OBJECT_0 || g_IsStopping) break; 
        
        if (result == WAIT_OBJECT_0 + 1) { 
            // Server died - Circuit Breaker Logic
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::minutes>(now - g_LastRestartTime).count() > 10) {
                g_RestartCount = 0; // Reset count if last crash was > 10 mins ago
            }
            
            g_LastRestartTime = now;
            g_RestartCount++;

            if (g_RestartCount > MAX_RESTARTS) {
                LogDebug("Circuit breaker triggered. Too many crashes.");
                break;
            }

            LogDebug("Server crashed. Restarting (" + std::to_string(g_RestartCount) + "/" + std::to_string(MAX_RESTARTS) + ")");
            CloseHandle(g_ServerProcess.hProcess);
            CloseHandle(g_ServerProcess.hThread);
            Sleep(3000);
            if (!StartServerProcess()) break;
        }
    }

    StopServerProcess();
    ReportStatus(SERVICE_STOPPED, 0, 0);
}

int _tmain(int argc, TCHAR *argv[]) {
    SERVICE_TABLE_ENTRY ServiceTable[] = { {g_ServiceName, (LPSERVICE_MAIN_FUNCTION) ServiceMain}, {NULL, NULL} };
    return StartServiceCtrlDispatcher(ServiceTable);
}