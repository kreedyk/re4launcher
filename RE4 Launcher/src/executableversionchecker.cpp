#include "ExecutableVersionChecker.h"
#include <sstream>
#include <thread>
#include <chrono>
#include <fstream>
#include "resource.h"
#pragma comment(lib, "Version.lib")

// Helper function to get Bin32 path - Implementation based on the app's context
std::wstring ExecutableVersionChecker::getBin32Path() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(NULL, buffer, MAX_PATH);
    std::wstring exePath(buffer);

    // Find last path separator
    size_t pos = exePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        std::wstring dir = exePath.substr(0, pos);
        // Go up one directory level (Launcher might be in a subfolder)
        pos = dir.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            return dir.substr(0, pos);
        }
        return dir;
    }

    // Fallback to a standard location
    return L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Resident Evil 4\\Bin32";
}

// Helper function to force terminate a process
void ExecutableVersionChecker::ForceTerminateProcess(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe32{};
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                }
            }
        } while (Process32NextW(snapshot, &pe32));
    }
    CloseHandle(snapshot);
}

ExecutableVersionChecker::ExecutableVersionChecker(const std::wstring& gamePath)
    : m_exePath(gamePath + L"\\bio4.exe")
    , m_isValidVersion(false)
{
    checkExecutableVersion();
}

bool ExecutableVersionChecker::isValidVersion() const {
    return m_isValidVersion;
}

const std::wstring& ExecutableVersionChecker::getErrorMessage() const {
    return m_errorMessage;
}

const std::wstring& ExecutableVersionChecker::getVersionInfo() const {
    return m_versionInfo;
}

void ExecutableVersionChecker::checkExecutableVersion() {
    try {
        if (!std::filesystem::exists(m_exePath)) {
            m_errorMessage = L"Executable file not found: " + m_exePath;
            m_versionInfo = L"Unknown version (file not found)";
            return;
        }

        // Check file size
        uintmax_t fileSize = std::filesystem::file_size(m_exePath);
        bool sizeMatches = (fileSize >= EXPECTED_SIZE - SIZE_MARGIN &&
            fileSize <= static_cast<unsigned long long>(EXPECTED_SIZE) + SIZE_MARGIN);

        if (!getFileVersionInfo()) {
            // If version check failed but size matches expected, give benefit of doubt
            if (sizeMatches && fileSize != INVALID_SIZE) {
                m_isValidVersion = true;
                m_versionInfo = L"Version check failed, but file size matches expected (9,126,456 bytes)";
            }
            return;
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        m_errorMessage = L"Error checking executable file: " +
            std::wstring(e.what(), e.what() + strlen(e.what()));
        m_versionInfo = L"Error during version check";
    }
}

bool ExecutableVersionChecker::getFileVersionInfo() {
    DWORD dummy;
    DWORD versionInfoSize = GetFileVersionInfoSizeW(m_exePath.c_str(), &dummy);
    if (versionInfoSize == 0) {
        m_errorMessage = L"Failed to get file version info size";
        m_versionInfo = L"Unknown version (cannot read version info)";
        return false;
    }

    std::vector<BYTE> versionData(versionInfoSize);
    if (!GetFileVersionInfoW(m_exePath.c_str(), 0, versionInfoSize, versionData.data())) {
        m_errorMessage = L"Failed to get file version info";
        m_versionInfo = L"Unknown version (version info read error)";
        return false;
    }

    VS_FIXEDFILEINFO* fixedFileInfo = nullptr;
    UINT fixedInfoLen = 0;
    if (!VerQueryValueW(versionData.data(), L"\\",
        reinterpret_cast<LPVOID*>(&fixedFileInfo), &fixedInfoLen) ||
        fixedInfoLen == 0) {
        m_errorMessage = L"Failed to query version info";
        m_versionInfo = L"Unknown version (version query error)";
        return false;
    }

    // Extract version numbers
    DWORD majorVersion = HIWORD(fixedFileInfo->dwFileVersionMS);
    DWORD minorVersion = LOWORD(fixedFileInfo->dwFileVersionMS);
    DWORD buildNumber = HIWORD(fixedFileInfo->dwFileVersionLS);
    DWORD revisionNumber = LOWORD(fixedFileInfo->dwFileVersionLS);

    // Format version string
    std::wstringstream ss;
    ss << majorVersion << L"." << minorVersion << L"." << buildNumber << L"." << revisionNumber;
    m_versionInfo = ss.str();

    // Check if this is version 1.0.18384.1
    if (majorVersion == 1 && minorVersion == 0 && buildNumber == 18384 && revisionNumber == 1) {
        m_isValidVersion = true;
        return true;
    }

    m_errorMessage = L"Incorrect executable version. Required: 1.0.6, Found: " + m_versionInfo;
    return true;
}

DWORD ExecutableVersionChecker::GetProcessIdFromName(const std::wstring& processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32{};
        pe32.dwSize = sizeof(pe32);

        if (Process32FirstW(snapshot, &pe32)) {
            do {
                if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                    pid = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &pe32));
        }
        CloseHandle(snapshot);
    }

    return pid;
}

std::wstring ExecutableVersionChecker::ExtractExecutableResource() {
    // Find the resource
    HRSRC hResInfo = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_EXECUTABLE), RT_RCDATA);
    if (!hResInfo) {
        MessageBoxW(NULL, L"Failed to find embedded executable resource", L"Error", MB_ICONERROR);
        return L"";
    }

    // Load the resource
    HGLOBAL hResData = LoadResource(NULL, hResInfo);
    if (!hResData) {
        MessageBoxW(NULL, L"Failed to load executable resource", L"Error", MB_ICONERROR);
        return L"";
    }

    // Get pointer to the data
    LPVOID pResource = LockResource(hResData);
    if (!pResource) {
        MessageBoxW(NULL, L"Failed to lock executable resource", L"Error", MB_ICONERROR);
        return L"";
    }

    // Get resource size
    DWORD dwSize = SizeofResource(NULL, hResInfo);
    if (dwSize == 0) {
        MessageBoxW(NULL, L"Resource size is zero", L"Error", MB_ICONERROR);
        return L"";
    }

    // Create a temporary file to save the executable
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    wchar_t tempFileName[MAX_PATH];
    if (!GetTempFileNameW(tempPath, L"re4", 0, tempFileName)) {
        MessageBoxW(NULL, L"Failed to create temporary file", L"Error", MB_ICONERROR);
        return L"";
    }

    // Rename the file to have .exe extension
    std::wstring tempExePath = tempFileName;
    tempExePath += L".exe";
    DeleteFileW(tempExePath.c_str()); // Remove the file if it already exists

    if (!MoveFileW(tempFileName, tempExePath.c_str())) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to rename temporary file. Error: %d", error);
        MessageBoxW(NULL, errorMsg, L"Error", MB_ICONERROR);
        DeleteFileW(tempFileName);
        return L"";
    }

    // Write the resource data to the file
    HANDLE hFile = CreateFileW(tempExePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to create temporary file for writing. Error: %d", error);
        MessageBoxW(NULL, errorMsg, L"Error", MB_ICONERROR);
        return L"";
    }

    DWORD bytesWritten;
    if (!WriteFile(hFile, pResource, dwSize, &bytesWritten, NULL) || bytesWritten != dwSize) {
        CloseHandle(hFile);
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to write resource data. Written: %d, Expected: %d",
            bytesWritten, dwSize);
        MessageBoxW(NULL, errorMsg, L"Error", MB_ICONERROR);
        DeleteFileW(tempExePath.c_str());
        return L"";
    }

    CloseHandle(hFile);
    return tempExePath;
}

bool ExecutableVersionChecker::replaceExecutable() {
    try {
        // Extract the embedded executable resource
        std::wstring correctExePath = ExtractExecutableResource();
        if (correctExePath.empty()) {
            MessageBoxW(NULL, L"Could not extract the embedded executable resource.",
                L"Error", MB_ICONERROR | MB_OK);
            return false;
        }

        // Verify the extracted executable has the correct size
        try {
            uintmax_t fileSize = std::filesystem::file_size(correctExePath);
            if (fileSize != EXPECTED_SIZE) {
                std::wstring errorMsg = L"The extracted executable has an incorrect size.\n"
                    L"Expected: " + std::to_wstring(EXPECTED_SIZE) +
                    L", Actual: " + std::to_wstring(fileSize);
                MessageBoxW(NULL, errorMsg.c_str(), L"Error", MB_ICONERROR | MB_OK);
                std::filesystem::remove(correctExePath);
                return false;
            }
        }
        catch (const std::filesystem::filesystem_error&) {
            MessageBoxW(NULL, L"Failed to verify the extracted executable.",
                L"Error", MB_ICONERROR | MB_OK);
            std::filesystem::remove(correctExePath);
            return false;
        }

        // Create backup of current executable 
        std::wstring backupPath = m_exePath + L".bak";

        // Check if backup already exists and handle it properly
        if (std::filesystem::exists(backupPath)) {
            std::wstring promptMsg = L"A backup file (bio4.exe.bak) already exists.\n\n"
                L"Do you want to overwrite this file?\n\n"
                L"- Click Yes to overwrite\n"
                L"- Click No to create a new numbered backup file.";

            int response = MessageBoxW(NULL, promptMsg.c_str(),
                L"Existing backup file", MB_ICONQUESTION | MB_YESNO);

            if (response == IDYES) {
                // User wants to overwrite - make sure all processes are terminated
                ForceTerminateProcess(L"bio4.exe.bak");

                // Wait longer to ensure file handles are released
                std::this_thread::sleep_for(std::chrono::seconds(1));

                // Try to remove but don't throw if it fails
                try {
                    std::filesystem::remove(backupPath);
                }
                catch (const std::filesystem::filesystem_error&) {
                    // If failed to remove, create a new name
                    backupPath = m_exePath + L".bak.old";
                }
            }
            else {
                // User wants a new backup - create a numbered one
                int backupNumber = 1;
                std::wstring numberedBackup;
                do {
                    numberedBackup = m_exePath + L".bak" + std::to_wstring(backupNumber++);
                } while (std::filesystem::exists(numberedBackup) && backupNumber < 100);

                backupPath = numberedBackup;
            }
        }

        // We're not in the game process, so we can directly replace the file
        ForceTerminateProcess(L"bio4.exe");
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Give the OS time to release the file

        // Try to make backup and replace
        try {
            std::filesystem::copy_file(m_exePath, backupPath,
                std::filesystem::copy_options::overwrite_existing);

            // Replace with correct executable
            std::filesystem::copy_file(correctExePath, m_exePath,
                std::filesystem::copy_options::overwrite_existing);

            // Clean up the temporary file
            std::filesystem::remove(correctExePath);

            // Verify the replacement was successful
            checkExecutableVersion();
            if (!m_isValidVersion) {
                std::wstring warningMsg = L"The executable was replaced, but it still doesn't appear to be the correct version.\n"
                    L"Your original executable has been backed up as '" + backupPath + L"'.\n\n"
                    L"Please try manually installing the correct version.";
                MessageBoxW(NULL, warningMsg.c_str(), L"Warning", MB_ICONWARNING | MB_OK);
                return false;
            }

            std::wstring successMsg = L"The executable has been successfully replaced with the correct version.\n"
                L"Your original executable has been backed up as '" + backupPath + L"'.\n\n"
                L"Please restart the game.";
            MessageBoxW(NULL, successMsg.c_str(), L"Success", MB_ICONINFORMATION | MB_OK);

            return true;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::wstring errorMsg = L"Error replacing executable: " +
                std::wstring(e.what(), e.what() + strlen(e.what()));
            MessageBoxW(NULL, errorMsg.c_str(), L"File Error", MB_ICONERROR | MB_OK);

            // Clean up temporary file in case of error
            try {
                std::filesystem::remove(correctExePath);
            }
            catch (...) {}

            return false;
        }
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL,
            (std::string("Error replacing executable: ") + e.what()).c_str(),
            "Error", MB_ICONERROR | MB_OK);
        return false;
    }
    catch (...) {
        MessageBoxW(NULL,
            L"An unknown error occurred while replacing the executable.",
            L"Error", MB_ICONERROR | MB_OK);
        return false;
    }
}