#pragma once
#include <string>
#include <mutex>
#include <Windows.h>

// Status struct with thread safety to maintain application state
struct AppStatus {
    std::string exePath;
    std::string statusText = "Searching for game executable...";
    float progressValue = 0.0f;
    bool isPatching = false;
    bool exeFound = false;
    bool gameRunning = false;
    HANDLE gameProcessHandle = NULL;
    DWORD gameProcessId = 0;

    // Added for DLL injection
    std::string dllPath;
    bool dllSelected = false;
    std::string injectionStatus = "No DLL selected";
    float injectionProgress = 0.0f;

    // Tab system
    int currentTab = 0; // 0 = Launcher, 1 = DLL Injector

    // Mutex for thread synchronization
    std::mutex statusMutex;

    // Thread-safe methods to update status
    void updateStatus(const std::string& newStatus, float newProgress = -1.0f) {
        std::lock_guard<std::mutex> lock(statusMutex);
        statusText = newStatus;
        if (newProgress >= 0.0f) {
            progressValue = newProgress;
        }
    }

    // Thread-safe methods to update injection status
    void updateInjectionStatus(const std::string& newStatus, float newProgress = -1.0f) {
        std::lock_guard<std::mutex> lock(statusMutex);
        injectionStatus = newStatus;
        if (newProgress >= 0.0f) {
            injectionProgress = newProgress;
        }
    }

    // Thread-safe methods to get status
    std::string getStatusText() {
        std::lock_guard<std::mutex> lock(statusMutex);
        return statusText;
    }

    float getProgressValue() {
        std::lock_guard<std::mutex> lock(statusMutex);
        return progressValue;
    }

    std::string getInjectionStatus() {
        std::lock_guard<std::mutex> lock(statusMutex);
        return injectionStatus;
    }

    float getInjectionProgress() {
        std::lock_guard<std::mutex> lock(statusMutex);
        return injectionProgress;
    }
};