#include "Utils.h"
#include "ExecutableVersionChecker.h"
#include <fstream>
#include <vector>
#include <Shlobj.h>
#include "resource.h"

// Game Monitor Thread Function 
DWORD WINAPI GameMonitorThreadFunc(LPVOID lpParam) {
    GameMonitorData* data = (GameMonitorData*)lpParam;

    // Wait for the game process to exit
    if (data->hProcess != NULL) {
        // Wait with a timeout (check every 2 seconds)
        while (WaitForSingleObject(data->hProcess, 2000) == WAIT_TIMEOUT) {
            // Periodically check if the game is still running (in case our handle is invalid)
            if (!Utils::IsGameRunning(data->status->exePath, NULL)) {
                data->status->updateStatus("Game no longer running", 0);
                data->status->gameRunning = false;
                data->status->isPatching = false;
                break;
            }
        }

        // Update status when game closes (using thread-safe method)
        data->status->updateStatus("Game closed, ready to start again", 0);
        data->status->gameRunning = false;
        data->status->isPatching = false;

        // Cleanup
        CloseHandle(data->hProcess);
        data->status->gameProcessHandle = NULL;
    }
    else {
        // No valid handle, use timeout and process name check
        CheckLaunchTimeout(*data->status);
    }

    delete data;
    return 0;
}

// Function to check if launch timeout occurred
void CheckLaunchTimeout(AppStatus& status) {
    // Wait for 10 seconds
    Sleep(10000);

    // If the game isn't running but we're still in patching mode, something went wrong
    if (!status.gameRunning && status.isPatching) {
        status.updateStatus("Launch timeout - game may have been patched by another tool", 0);
        status.isPatching = false;
    }
}

namespace Utils {
    // File Utilities Implementation
    bool FileExists(const std::string& path) {
        DWORD fileAttributes = GetFileAttributesA(path.c_str());
        return (fileAttributes != INVALID_FILE_ATTRIBUTES &&
            !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY));
    }

    std::string GetParentPath(const std::string& path) {
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            return path.substr(0, pos);
        }
        return "";
    }

    std::string GetFileName(const std::string& path) {
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            return path.substr(pos + 1);
        }
        return path;
    }

    std::string FindGameExe() {
        // Check standard Steam paths
        std::vector<std::string> steamPaths = {
            "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Resident Evil 4\\Bin32\\bio4.exe",
            "C:\\Program Files\\Steam\\steamapps\\common\\Resident Evil 4\\Bin32\\bio4.exe"
        };

        for (const auto& path : steamPaths) {
            if (FileExists(path)) {
                return path;
            }
        }

        // Check relative path (two levels up)
        try {
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            std::string exePath(buffer);

            std::string dir = GetParentPath(exePath);
            std::string parentDir = GetParentPath(dir);
            std::string twoLevelsUp = GetParentPath(parentDir);
            std::string targetPath = twoLevelsUp + "\\bio4.exe";

            if (FileExists(targetPath)) {
                return targetPath;
            }
        }
        catch (...) {
            // Silently fail and continue
        }

        return "";
    }

    std::string FindDLL(const std::string& gamePath) {
        if (gamePath.empty()) {
            return "";
        }

        // Get game directory from game executable path
        std::string gameDir = GetParentPath(gamePath);

        // Try the specified path - Bin32\kreed\nikaelly_t.dll
        std::string targetPath = gameDir + "\\kreed\\nikaelly_t.dll";
        if (FileExists(targetPath)) {
            return targetPath;
        }

        // Try parent directory + kreed\nikaelly_t.dll
        std::string parentDir = GetParentPath(gameDir);
        targetPath = parentDir + "\\kreed\\nikaelly_t.dll";
        if (FileExists(targetPath)) {
            return targetPath;
        }

        // Try Bin32\kreed directory
        targetPath = gameDir + "\\kreed";
        // If directory exists but file doesn't
        DWORD dirAttrs = GetFileAttributesA(targetPath.c_str());
        if (dirAttrs != INVALID_FILE_ATTRIBUTES && (dirAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Create directory automatically
            CreateDirectoryA(targetPath.c_str(), NULL);
        }

        return "";
    }

    bool IsPatchNeeded(const std::string& exePath) {
        try {
            std::ifstream file(exePath, std::ios::binary);
            if (!file) return true;

            // Go to offset 342 (343-1)
            file.seekg(342);

            // Read 1 byte
            char byte;
            file.read(&byte, 1);

            // If already patched, this value would be 35
            return (byte != 35);
        }
        catch (...) {
            // If error occurs, assume patch is needed
            return true;
        }
    }

    bool ApplyPatch(const std::string& exePath, AppStatus& status) {
        try {
            // Skip if already patched
            if (!IsPatchNeeded(exePath)) {
                status.updateStatus("Patch already applied, skipping...", 50);
                return true;
            }

            // Read the file
            status.updateStatus("Reading executable...", 10);

            std::ifstream input(exePath, std::ios::binary | std::ios::ate);
            if (!input) {
                status.updateStatus("ERROR: Could not open file!", 0);
                return false;
            }

            // Get file size and read the data
            std::streamsize size = input.tellg();
            input.seekg(0, std::ios::beg);

            std::vector<char> buffer(size);
            if (!input.read(buffer.data(), size)) {
                status.updateStatus("ERROR: Could not read file!", 0);
                return false;
            }

            input.close();

            // Apply patches
            status.updateStatus("Applying patches...", 30);

            // Patch 1: Offset 343 (adjusted to 342), 1 byte with 35
            if (342 < buffer.size()) {
                buffer[342] = 35;
            }

            // Patch 2: Offset 409 (adjusted to 408), 2 bytes with 193, 77
            if (408 + 1 < buffer.size()) {
                buffer[408] = (char)193;
                buffer[409] = (char)77;
            }

            // Patch 3: Memory patches at multiple offsets
            std::vector<int> offsets = { 605, 645, 685, 725, 765, 805, 845 };
            char memoryPatch[4] = { 96, 0, 0, (char)240 };

            for (size_t i = 0; i < offsets.size(); i++) {
                int offset = offsets[i] - 1; // Adjust to 0-based
                if (offset + 3 < buffer.size()) {
                    buffer[offset] = memoryPatch[0];
                    buffer[offset + 1] = memoryPatch[1];
                    buffer[offset + 2] = memoryPatch[2];
                    buffer[offset + 3] = memoryPatch[3];
                }

                status.updateStatus("Applying patch at offset " + std::to_string(offsets[i]) + "...", 40 + (float)(i * 5));
            }

            // Save the patched file
            status.updateStatus("Saving modified executable...", 80);

            std::ofstream output(exePath, std::ios::binary);
            if (!output) {
                status.updateStatus("ERROR: Could not write file!", 0);
                return false;
            }

            output.write(buffer.data(), size);
            output.close();

            status.updateStatus("Patch applied successfully!", 100);

            return true;
        }
        catch (std::exception& e) {
            status.updateStatus(std::string("ERROR: ") + e.what(), 0);
            return false;
        }
        catch (...) {
            status.updateStatus("ERROR: Unknown error during patching", 0);
            return false;
        }
    }

    bool SetWindowedMode() {
        try {
            // Get Documents folder
            char documentsPath[MAX_PATH];
            HRESULT result = SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, documentsPath);
            if (FAILED(result)) {
                return false;
            }

            // Build path to config.ini
            std::string configPath = std::string(documentsPath) + "\\My Games\\Capcom\\RE4\\config.ini";

            // Check if directory exists, create if not
            std::string configDir = GetParentPath(configPath);
            std::string current;
            std::string delimiter = "\\";
            size_t start = 0;
            size_t end = configDir.find(delimiter);

            while (end != std::string::npos) {
                current += configDir.substr(start, end - start + 1);
                CreateDirectoryA(current.c_str(), NULL);
                start = end + 1;
                end = configDir.find(delimiter, start);
            }

            // Read existing file or create new
            std::vector<std::string> lines;
            bool fullscreenModified = false;

            if (FileExists(configPath)) {
                // Read existing file
                std::ifstream inFile(configPath);
                if (inFile.is_open()) {
                    std::string line;
                    while (std::getline(inFile, line)) {
                        // Check if this is the fullscreen line
                        if (line.substr(0, 10) == "fullscreen") {
                            lines.push_back("fullscreen 0");
                            fullscreenModified = true;
                        }
                        else {
                            lines.push_back(line);
                        }
                    }
                }
            }

            // If we didn't find and replace the fullscreen setting, add it
            if (!fullscreenModified) {
                lines.push_back("fullscreen 0");
            }

            // Write the file
            std::ofstream outFile(configPath);
            if (!outFile.is_open()) {
                return false;
            }

            for (const auto& line : lines) {
                outFile << line << std::endl;
            }

            return true;
        }
        catch (...) {
            return false;
        }
    }

    std::wstring ExtractExecutableResource() {
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

    void BrowseForExe(AppStatus& status) {
        char filename[MAX_PATH] = "";

        OPENFILENAMEA ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = "Executable Files\0*.exe\0All Files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrTitle = "Select bio4.exe executable";

        // Initial directory
        std::string initialDir = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Resident Evil 4\\Bin32";
        if (!FileExists(initialDir)) {
            initialDir = "C:\\";
        }
        ofn.lpstrInitialDir = initialDir.c_str();

        if (GetOpenFileNameA(&ofn)) {
            status.exePath = filename;
            status.exeFound = true;
            status.updateStatus("Ready to launch");

            // Check if game is already running
            if (IsGameRunning(status.exePath, &status.gameProcessId)) {
                status.gameRunning = true;
                status.updateStatus("Game is already running");
                status.gameProcessHandle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, status.gameProcessId);
            }

            // Try to find the DLL
            status.dllPath = FindDLL(status.exePath);
            status.dllSelected = !status.dllPath.empty();
            if (status.dllSelected) {
                status.updateInjectionStatus("DLL found: " + GetFileName(status.dllPath));
            }
        }
    }

    void BrowseForDLL(AppStatus& status) {
        char filename[MAX_PATH] = "";

        OPENFILENAMEA ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = "DLL Files\0*.dll\0All Files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrTitle = "Select DLL to inject";

        // Initial directory - use game directory if available
        std::string initialDir = status.exePath.empty() ? "C:\\" : GetParentPath(status.exePath);
        ofn.lpstrInitialDir = initialDir.c_str();

        if (GetOpenFileNameA(&ofn)) {
            status.dllPath = filename;
            status.dllSelected = true;
            status.updateInjectionStatus("DLL selected: " + GetFileName(status.dllPath));
        }
    }

    // Game Process Utilities Implementation
    bool IsGameRunning(const std::string& exePath, DWORD* processId) {
        std::string exeName = GetFileName(exePath);
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return false;
        }

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(snapshot, &pe32)) {
            CloseHandle(snapshot);
            return false;
        }

        bool found = false;
        do {
            std::wstring wProcessName(pe32.szExeFile);
            std::string processName(wProcessName.begin(), wProcessName.end());

            if (_stricmp(processName.c_str(), exeName.c_str()) == 0) {
                if (processId) {
                    *processId = pe32.th32ProcessID;
                }
                found = true;
                break;
            }
        } while (Process32Next(snapshot, &pe32));

        CloseHandle(snapshot);
        return found;
    }

    HANDLE LaunchGame(const std::string& exePath, DWORD* processId) {
        try {
            // Get the directory
            std::string directory = GetParentPath(exePath);

            // Launch the process
            STARTUPINFOA si = { 0 };
            PROCESS_INFORMATION pi = { 0 };
            si.cb = sizeof(si);

            // Create the process
            if (CreateProcessA(
                exePath.c_str(),   // Application name
                NULL,              // Command line
                NULL,              // Process attributes
                NULL,              // Thread attributes
                FALSE,             // Inherit handles
                0,                 // Creation flags
                NULL,              // Environment
                directory.c_str(), // Current directory
                &si,               // Startup info
                &pi                // Process info
            )) {
                // Save the process ID if requested
                if (processId) {
                    *processId = pi.dwProcessId;
                }

                // Close only the thread handle, keep the process handle
                CloseHandle(pi.hThread);
                return pi.hProcess;
            }

            return NULL;
        }
        catch (...) {
            return NULL;
        }
    }

    bool TerminateGameProcess(AppStatus& status) {
        if (status.gameProcessHandle != NULL) {
            // Try to terminate the process using the existing handle
            if (TerminateProcess(status.gameProcessHandle, 0)) {
                CloseHandle(status.gameProcessHandle);
                status.gameProcessHandle = NULL;
                status.gameRunning = false;
                status.gameProcessId = 0;
                status.updateStatus("Game closed forcefully", 0);
                return true;
            }
        }

        // If we don't have a handle or termination failed, try to find the process by ID
        if (status.gameProcessId != 0) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, status.gameProcessId);
            if (hProcess != NULL) {
                if (TerminateProcess(hProcess, 0)) {
                    CloseHandle(hProcess);
                    status.gameProcessHandle = NULL;
                    status.gameRunning = false;
                    status.gameProcessId = 0;
                    status.updateStatus("Game closed forcefully", 0);
                    return true;
                }
                CloseHandle(hProcess);
            }
        }

        // Try to find the process by name as a last resort
        std::string exeName = GetFileName(status.exePath);
        DWORD processId = 0;
        if (IsGameRunning(exeName, &processId)) {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
            if (hProcess != NULL) {
                if (TerminateProcess(hProcess, 0)) {
                    CloseHandle(hProcess);
                    status.gameProcessHandle = NULL;
                    status.gameRunning = false;
                    status.gameProcessId = 0;
                    status.updateStatus("Game closed forcefully", 0);
                    return true;
                }
                CloseHandle(hProcess);
            }
        }

        status.updateStatus("Failed to close game", 0);
        return false;
    }

    bool VerifyExecutableVersion(const std::string& exePath) {
        // Convert std::string to std::wstring
        std::wstring wExePath(exePath.begin(), exePath.end());
        std::wstring gamePath = wExePath.substr(0, wExePath.find_last_of(L"\\/"));

        ExecutableVersionChecker versionChecker(gamePath);

        if (!versionChecker.isValidVersion()) {
            std::wstring errorMsg = L"This launcher only works with Resident Evil 4 version 1.0.6.\n\n";
            errorMsg += L"Detected version: " + versionChecker.getVersionInfo() + L"\n\n";
            errorMsg += L"Would you like to replace your executable with the correct version?\n"
                L"(Your current executable will be backed up as 'bio4.exe.bak')";

            int response = MessageBoxW(NULL, errorMsg.c_str(),
                L"Incorrect Game Version", MB_ICONQUESTION | MB_YESNO);

            if (response == IDYES) {
                return versionChecker.replaceExecutable();
            }

            // User declined replacement
            MessageBoxW(NULL,
                L"Please ensure you're using the correct game version.",
                L"Launcher Error", MB_ICONERROR | MB_OK);
            return false;
        }

        return true;
    }

    void SmartLaunch(AppStatus& status, bool replaceMode) {
        if (status.exePath.empty()) {
            status.updateStatus("ERROR: Game executable not found!", 0);
            status.isPatching = false;
            return;
        }

        // If we're in replace mode, go directly to version check and replacement
        if (replaceMode) {
            try {
                // Step 1: Verify and replace executable
                status.updateStatus("Preparing to replace executable...", 10);

                // Use ExecutableVersionChecker to replace the executable
                std::wstring wExePath(status.exePath.begin(), status.exePath.end());
                std::wstring gamePath = wExePath.substr(0, wExePath.find_last_of(L"\\/"));
                ExecutableVersionChecker versionChecker(gamePath);

                // Force replacement regardless of current version
                if (versionChecker.replaceExecutable()) {
                    status.updateStatus("Executable replaced successfully! Game is ready to launch.", 100);
                }
                else {
                    status.updateStatus("ERROR: Failed to replace executable.", 0);
                }

                status.isPatching = false;
                return;
            }
            catch (std::exception& e) {
                status.updateStatus(std::string("ERROR: ") + e.what(), 0);
                status.isPatching = false;
                return;
            }
            catch (...) {
                status.updateStatus("ERROR: Unknown error during replacement", 0);
                status.isPatching = false;
                return;
            }
        }

        // Normal behavior - check if game is already running, etc.
        // Check if the game is already running
        if (IsGameRunning(status.exePath, &status.gameProcessId)) {
            status.gameRunning = true;
            status.gameProcessHandle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, status.gameProcessId);
            status.updateStatus("Game is already running", 100);

            // Create a monitor thread to watch the existing process
            GameMonitorData* monitorData = new GameMonitorData;
            monitorData->hProcess = status.gameProcessHandle;
            monitorData->status = &status;

            CreateThread(NULL, 0, GameMonitorThreadFunc, monitorData, 0, NULL);
            return;
        }

        try {
            // Step 1: Verify Executable Version FIRST
            status.updateStatus("Checking executable version...", 10);
            if (!VerifyExecutableVersion(status.exePath)) {
                status.updateStatus("ERROR: Incorrect game version. Launch aborted.", 0);
                status.isPatching = false;
                return;
            }

            // Step 2: Set windowed mode
            status.updateStatus("Setting windowed mode...", 20);
            SetWindowedMode();

            // Step 3: Check if patch is needed
            bool needPatch = IsPatchNeeded(status.exePath);

            if (needPatch) {
                status.updateStatus("Patch needed, applying...", 30);

                // Create backup
                std::string backupPath = status.exePath + ".bak";
                if (!FileExists(backupPath)) {
                    status.updateStatus("Creating backup...", 35);

                    std::ifstream src(status.exePath, std::ios::binary);
                    std::ofstream dst(backupPath, std::ios::binary);
                    if (!src || !dst) {
                        status.updateStatus("ERROR: Could not create backup", 0);
                        status.isPatching = false;
                        return;
                    }
                    dst << src.rdbuf();
                }

                // Apply patch
                if (!ApplyPatch(status.exePath, status)) {
                    status.updateStatus("ERROR: Failed to apply patch!", 0);
                    status.isPatching = false;
                    return;
                }
            }
            else {
                status.updateStatus("Patch already applied, proceeding to launch...", 50);
            }

            // Step 4: Launch the game
            status.updateStatus("Launching game...", 100);

            HANDLE hProcess = LaunchGame(status.exePath, &status.gameProcessId);
            if (hProcess != NULL) {
                status.updateStatus("Game is running...");
                status.gameRunning = true;
                status.gameProcessHandle = hProcess;

                // Create data for the monitor thread
                GameMonitorData* monitorData = new GameMonitorData;
                monitorData->hProcess = hProcess;
                monitorData->status = &status;

                // Create a thread to monitor the game process
                CreateThread(NULL, 0, GameMonitorThreadFunc, monitorData, 0, NULL);
            }
            else {
                // Check if the game started despite us not getting a handle (might be launched by DLL)
                Sleep(2000); // Wait a bit
                if (IsGameRunning(status.exePath, &status.gameProcessId)) {
                    status.gameRunning = true;
                    status.gameProcessHandle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, status.gameProcessId);
                    status.updateStatus("Game is running (started externally)", 100);

                    // Create a monitor thread to watch the game
                    GameMonitorData* monitorData = new GameMonitorData;
                    monitorData->hProcess = status.gameProcessHandle;
                    monitorData->status = &status;

                    CreateThread(NULL, 0, GameMonitorThreadFunc, monitorData, 0, NULL);
                }
                else {
                    // Launch failed
                    status.updateStatus("ERROR: Failed to launch game!", 0);
                    status.isPatching = false;

                    // Create a monitor thread with timeout
                    GameMonitorData* monitorData = new GameMonitorData;
                    monitorData->hProcess = NULL;
                    monitorData->status = &status;

                    CreateThread(NULL, 0, GameMonitorThreadFunc, monitorData, 0, NULL);
                }
            }
        }
        catch (std::exception& e) {
            status.updateStatus(std::string("ERROR: ") + e.what(), 0);
            status.isPatching = false;
        }
        catch (...) {
            status.updateStatus("ERROR: Unknown error occurred", 0);
            status.isPatching = false;
        }
    }

    // DLL Injection Thread Function
    DWORD WINAPI InjectDLLThreadFunc(LPVOID lpParam) {
        AppStatus* status = (AppStatus*)lpParam;
        InjectDLL(*status);
        return 0;
    }

    // DLL Injection Implementation
    bool InjectDLL(AppStatus& status) {
        if (!status.gameRunning || status.gameProcessId == 0) {
            status.updateInjectionStatus("ERROR: Game not running, cannot inject DLL", 0);
            return false;
        }

        if (status.dllPath.empty() || !status.dllSelected) {
            status.updateInjectionStatus("ERROR: No DLL selected", 0);
            return false;
        }

        try {
            // Get full path of the DLL
            char lpFullDLLPath[MAX_PATH];
            const DWORD dwFullPathResult = GetFullPathNameA(status.dllPath.c_str(), MAX_PATH, lpFullDLLPath, nullptr);
            if (dwFullPathResult == 0) {
                status.updateInjectionStatus("ERROR: Could not get full path of DLL", 0);
                return false;
            }

            // Open the target process
            const HANDLE hTargetProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, status.gameProcessId);
            if (hTargetProcess == INVALID_HANDLE_VALUE || hTargetProcess == NULL) {
                status.updateInjectionStatus("ERROR: Could not open game process", 0);
                return false;
            }

            status.updateInjectionStatus("Injecting DLL: Allocating memory...", 25);
            Sleep(100); // Small delay to ensure UI updates

            // Allocate memory in the target process
            const LPVOID lpPathAddress = VirtualAllocEx(hTargetProcess, nullptr, lstrlenA(lpFullDLLPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (lpPathAddress == nullptr) {
                CloseHandle(hTargetProcess);
                status.updateInjectionStatus("ERROR: Could not allocate memory in game process", 0);
                return false;
            }

            status.updateInjectionStatus("Injecting DLL: Writing path...", 50);
            Sleep(100); // Small delay to ensure UI updates

            // Write the DLL path to the allocated memory
            const DWORD dwWriteResult = WriteProcessMemory(hTargetProcess, lpPathAddress, lpFullDLLPath, lstrlenA(lpFullDLLPath) + 1, nullptr);
            if (dwWriteResult == 0) {
                VirtualFreeEx(hTargetProcess, lpPathAddress, 0, MEM_RELEASE);
                CloseHandle(hTargetProcess);
                status.updateInjectionStatus("ERROR: Could not write DLL path to game process", 0);
                return false;
            }

            status.updateInjectionStatus("Injecting DLL: Getting LoadLibraryA address...", 75);
            Sleep(100); // Small delay to ensure UI updates

            // Get the address of LoadLibraryA
            const HMODULE hModule = GetModuleHandleA("kernel32.dll");
            if (hModule == INVALID_HANDLE_VALUE || hModule == nullptr) {
                VirtualFreeEx(hTargetProcess, lpPathAddress, 0, MEM_RELEASE);
                CloseHandle(hTargetProcess);
                status.updateInjectionStatus("ERROR: Could not get kernel32.dll handle", 0);
                return false;
            }

            const FARPROC lpFunctionAddress = GetProcAddress(hModule, "LoadLibraryA");
            if (lpFunctionAddress == nullptr) {
                VirtualFreeEx(hTargetProcess, lpPathAddress, 0, MEM_RELEASE);
                CloseHandle(hTargetProcess);
                status.updateInjectionStatus("ERROR: Could not get LoadLibraryA address", 0);
                return false;
            }

            status.updateInjectionStatus("Injecting DLL: Creating remote thread...", 90);
            Sleep(100); // Small delay to ensure UI updates

            // Create a remote thread to load the DLL
            const HANDLE hThreadCreationResult = CreateRemoteThread(hTargetProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)lpFunctionAddress, lpPathAddress, 0, nullptr);
            if (hThreadCreationResult == INVALID_HANDLE_VALUE || hThreadCreationResult == NULL) {
                VirtualFreeEx(hTargetProcess, lpPathAddress, 0, MEM_RELEASE);
                CloseHandle(hTargetProcess);
                status.updateInjectionStatus("ERROR: Could not create thread in game process", 0);
                return false;
            }

            // Wait for the thread to finish
            WaitForSingleObject(hThreadCreationResult, 5000); // 5 second timeout

            // Cleanup
            CloseHandle(hThreadCreationResult);
            VirtualFreeEx(hTargetProcess, lpPathAddress, 0, MEM_RELEASE);
            CloseHandle(hTargetProcess);

            status.updateInjectionStatus("DLL injected successfully!", 100);
            Sleep(100); // Ensure UI updates
            return true;
        }
        catch (std::exception& e) {
            status.updateInjectionStatus(std::string("ERROR: ") + e.what(), 0);
            return false;
        }
        catch (...) {
            status.updateInjectionStatus("ERROR: Unknown error during DLL injection", 0);
            return false;
        }
    }
}