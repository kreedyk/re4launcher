#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <tchar.h>
#include <Windows.h>
#include "AppState.h"
#include "UI.h"
#include "Style.h"
#include "Utils.h"
#include "resource.h"

// Main code
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Check if the launcher was called in replace mode
    bool replaceMode = (lpCmdLine && strstr(lpCmdLine, "--replace-exe") != NULL);

    // Create application window
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        UI::WndProc,
        0L,
        0L,
        GetModuleHandle(NULL),
        LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1)),
        NULL,
        NULL,
        NULL,
        _T("RE4 Launcher"),
        NULL
    };

    ::RegisterClassEx(&wc);

    // Create window with no maximize button
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("RE4 Launcher"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        100, 100, (int)Style::WINDOW_WIDTH, (int)Style::WINDOW_HEIGHT, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!UI::CreateDeviceD3D(hwnd))
    {
        UI::CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigWindowsMoveFromTitleBarOnly = true;           // Only move from title bar

    // Setup fonts
    UI::SetupFonts();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(UI::g_pd3dDevice, UI::g_pd3dDeviceContext);

    // Setup style
    Style::SetImGuiStyle();

    // Application state
    AppStatus status;

    // Find game executable
    status.exePath = Utils::FindGameExe();
    status.exeFound = !status.exePath.empty();

    // Try to find the DLL
    if (status.exeFound) {
        status.dllPath = Utils::FindDLL(status.exePath);
        status.dllSelected = !status.dllPath.empty();
        if (status.dllSelected) {
            status.updateInjectionStatus("DLL found: " + Utils::GetFileName(status.dllPath));
        }
    }

    // Handle replace mode logic
    if (replaceMode) {
        status.updateStatus("Ready to replace executable");

        // If executable was found, start the replacement process
        if (status.exeFound) {
            status.isPatching = true;

            // Start the replacement process in a separate thread
            CreateThread(NULL, 0, [](LPVOID lpParam) -> DWORD {
                AppStatus* s = (AppStatus*)lpParam;
                Utils::SmartLaunch(*s, true); // true indicates replace mode
                return 0;
                }, &status, 0, NULL);
        }
        else {
            status.updateStatus("ERROR: bio4.exe not found! Cannot replace executable.");
        }
    }
    else {
        // Normal behavior - check if game is already running
        if (status.exeFound) {
            status.gameRunning = Utils::IsGameRunning(status.exePath, &status.gameProcessId);
            if (status.gameRunning) {
                status.updateStatus("Game is already running");
                status.gameProcessHandle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, status.gameProcessId);

                // Update the DLL path automatically when switching to DLL injector tab
                if (status.dllPath.empty()) {
                    status.dllPath = Utils::FindDLL(status.exePath);
                    status.dllSelected = !status.dllPath.empty();
                    if (status.dllSelected) {
                        status.updateInjectionStatus("DLL found: " + Utils::GetFileName(status.dllPath));
                    }
                }
            }
            else {
                status.updateStatus("Ready to launch");
            }
        }
        else {
            status.updateStatus("ERROR: bio4.exe not found! Use the 'Browse...' button");
        }
    }

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // If we're in replace mode and the operation finished, close the launcher
        if (replaceMode && !status.isPatching) {
            // Wait a bit for the user to see the completion message
            static int closeDelay = 0;
            closeDelay++;

            // After approximately 5 seconds (300 frames @ 60fps), close the launcher
            if (closeDelay > 300) {
                done = true;
                continue;
            }
        }

        // Re-check if game is running periodically (only if not in replace mode)
        if (!replaceMode && status.exeFound && !status.gameRunning && !status.isPatching) {
            bool wasRunning = status.gameRunning;
            status.gameRunning = Utils::IsGameRunning(status.exePath, &status.gameProcessId);

            // If the game just started running, update DLL path
            if (!wasRunning && status.gameRunning) {
                status.updateStatus("Game is running");
                status.gameProcessHandle = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, status.gameProcessId);

                // Try to find the DLL if not already selected
                if (status.dllPath.empty()) {
                    status.dllPath = Utils::FindDLL(status.exePath);
                    status.dllSelected = !status.dllPath.empty();
                    if (status.dllSelected) {
                        status.updateInjectionStatus("DLL found: " + Utils::GetFileName(status.dllPath));
                    }
                }

                // Create a monitor thread
                GameMonitorData* monitorData = new GameMonitorData;
                monitorData->hProcess = status.gameProcessHandle;
                monitorData->status = &status;

                CreateThread(NULL, 0, GameMonitorThreadFunc, monitorData, 0, NULL);
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render the main UI window
        UI::RenderMainWindow(status, replaceMode, done);

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 
            Style::COLOR_MAINBACKGROUND.x, 
            Style::COLOR_MAINBACKGROUND.y, 
            Style::COLOR_MAINBACKGROUND.z, 
            1.0f 
        };
        UI::g_pd3dDeviceContext->OMSetRenderTargets(1, &UI::g_mainRenderTargetView, NULL);
        UI::g_pd3dDeviceContext->ClearRenderTargetView(UI::g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        UI::g_pSwapChain->Present(1, 0); // Present with vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    UI::CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}