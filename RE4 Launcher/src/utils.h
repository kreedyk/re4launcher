#pragma once
#include <string>
#include <Windows.h>
#include <tlhelp32.h>
#include "AppState.h"

// Structure for monitor thread data
struct GameMonitorData {
    HANDLE hProcess;
    AppStatus* status;
};

// Game Monitor Thread Function
DWORD WINAPI GameMonitorThreadFunc(LPVOID lpParam);

// Launch Timeout Check Function
void CheckLaunchTimeout(AppStatus& status);

namespace Utils {
    // File Utilities
    bool FileExists(const std::string& path);
    std::string GetParentPath(const std::string& path);
    std::string GetFileName(const std::string& path);
    std::string FindGameExe();
    std::string FindDLL(const std::string& gamePath);
    bool IsPatchNeeded(const std::string& exePath);
    bool ApplyPatch(const std::string& exePath, AppStatus& status);
    bool SetWindowedMode();
    std::wstring ExtractExecutableResource();
    void BrowseForExe(AppStatus& status);
    void BrowseForDLL(AppStatus& status);
    
    // Game Process Utilities
    bool IsGameRunning(const std::string& exePath, DWORD* processId = nullptr);
    HANDLE LaunchGame(const std::string& exePath, DWORD* processId = nullptr);
    bool TerminateGameProcess(AppStatus& status);
    void SmartLaunch(AppStatus& status, bool replaceMode);
    bool VerifyExecutableVersion(const std::string& exePath);

    // DLL Injection
    bool InjectDLL(AppStatus& status);
    DWORD WINAPI InjectDLLThreadFunc(LPVOID lpParam);
}