#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include <tlhelp32.h>
#include <filesystem>

class ExecutableVersionChecker {
public:
    ExecutableVersionChecker(const std::wstring& gamePath);

    bool isValidVersion() const;
    const std::wstring& getErrorMessage() const;
    const std::wstring& getVersionInfo() const;
    
    bool replaceExecutable();
    
private:
    static constexpr size_t EXPECTED_SIZE = 9126456;
    static constexpr size_t SIZE_MARGIN = 1000;
    static constexpr size_t INVALID_SIZE = 10000000;
    
    // Helper function to get Bin32 path
    std::wstring getBin32Path();
    
    // Terminate a process by name
    void ForceTerminateProcess(const std::wstring& processName);
    
    // Get process ID from name
    DWORD GetProcessIdFromName(const std::wstring& processName);
    
    // Check the executable version
    void checkExecutableVersion();
    
    // Get file version info
    bool getFileVersionInfo();
    
    // Extract the embedded executable resource
    std::wstring ExtractExecutableResource();
    
    std::wstring m_exePath;
    std::wstring m_errorMessage;
    std::wstring m_versionInfo;
    bool m_isValidVersion;
};