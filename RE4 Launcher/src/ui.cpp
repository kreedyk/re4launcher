#include "UI.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Style.h"
#include "Utils.h"
#include <ctime>
#pragma warning(disable:4996)  // Deprecated function warnings

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace UI {
    // Global DirectX objects
    ID3D11Device* g_pd3dDevice = NULL;
    ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
    IDXGISwapChain* g_pSwapChain = NULL;
    ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

    bool CreateDeviceD3D(HWND hWnd) {
        // Setup swap chain
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 2;
        sd.BufferDesc.Width = 0;
        sd.BufferDesc.Height = 0;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
        if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
            return false;

        CreateRenderTarget();
        return true;
    }

    void CleanupDeviceD3D() {
        CleanupRenderTarget();
        if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
        if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
        if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    }

    void CreateRenderTarget() {
        ID3D11Texture2D* pBackBuffer;
        g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }

    void CleanupRenderTarget() {
        if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
    }

    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        }
        return ::DefWindowProc(hWnd, msg, wParam, lParam);
    }

    void SetupFonts() {
        ImGuiIO& io = ImGui::GetIO();

        // We'll add 5 fonts: Normal, Section, Title, Subtitle, Button
        float fontSizes[] = { 
            Style::FONT_SIZE_NORMAL, 
            Style::FONT_SIZE_SECTION, 
            Style::FONT_SIZE_TITLE, 
            Style::FONT_SIZE_SUBTITLE, 
            Style::FONT_SIZE_BUTTON 
        };

        // Try to load Segoe UI Bold for all sizes
        for (int i = 0; i < 5; i++) {
            ImFontConfig config;
            config.SizePixels = fontSizes[i];
            config.OversampleH = 3;
            config.OversampleV = 1;
            config.PixelSnapH = true;
            config.GlyphExtraSpacing = ImVec2(1.0f, 1.0f);

            // Try to load fonts in this order: Segoe UI Bold, Segoe UI, Arial Bold, Arial, Default
            ImFont* font = NULL;

            // Try Segoe UI Bold
            font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", fontSizes[i], &config);

            // If failed, try Segoe UI
            if (font == NULL) {
                font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", fontSizes[i], &config);
            }

            // If failed, try Arial Bold
            if (font == NULL) {
                font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", fontSizes[i], &config);
            }

            // If failed, try Arial
            if (font == NULL) {
                font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", fontSizes[i], &config);
            }

            // If all failed, add default scaled font
            if (font == NULL) {
                config.SizePixels = fontSizes[i];
                io.Fonts->AddFontDefault(&config);
            }
        }
    }

    void RenderLauncherTab(AppStatus& status, bool replaceMode, bool& done) {
        ImGuiIO& io = ImGui::GetIO();
        
        // Position tracking - adjust for tabs
        float yPos = replaceMode ? 120.0f : 150.0f; // Adjusted for copyright text

        // Game Executable Section
        Style::BeginSection("Game Executable", &yPos, 60);

        // Path display with ellipsis for long paths
        const char* pathText = status.exePath.empty() ? "Not found" : status.exePath.c_str();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120); // Make space for Browse button
        char pathBuffer[256];
        strncpy_s(pathBuffer, pathText, sizeof(pathBuffer) - 1);
        ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_TEXT);
        ImGui::InputText("##path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();

        // Browse button - with centered text
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_BUTTON_TEXT);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 5)); // Adjust vertical centering

        // Show Browse button only if not in replace mode
        if (!replaceMode) {
            if (ImGui::Button("Browse...", ImVec2(100, 30))) {
                Utils::BrowseForExe(status);
            }
        }
        else {
            // Disabled button in replace mode
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::Button("Browse...", ImVec2(100, 30));
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        Style::EndSection();

        // Status Section
        Style::BeginSection("Status", &yPos, 120);

        // Status text with original color
        ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_TEXT);
        ImGui::TextWrapped("%s", status.getStatusText().c_str());  // Thread-safe getter with text wrapping
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Spacing();

        // Progress bar
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Style::COLOR_PROGRESS);
        ImGui::ProgressBar(status.getProgressValue() / 100.0f, ImVec2(-1, 25));  // Thread-safe getter
        ImGui::PopStyleColor();

        Style::EndSection();

        // Action buttons for Game Launcher tab
        ImGui::SetCursorPosY(Style::WINDOW_HEIGHT - 100);
        ImGui::SetCursorPosX((Style::WINDOW_WIDTH - 320) * 0.5f);

        // Play/Close Game button with larger text
        ImGui::PushFont(io.Fonts->Fonts[4]); // Button font
        ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_BUTTON_TEXT);

        // Adapt buttons depending on mode
        if (replaceMode) {
            // In replace mode, show only exit button
            ImGui::SetCursorPosX((Style::WINDOW_WIDTH - 100) * 0.5f);

            if (!status.isPatching) {
                // If the operation is complete, show exit button
                if (ImGui::Button("Exit", ImVec2(100, 50))) {
                    done = true;
                }
            }
            else {
                // Operation in progress, show disabled button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::Button("Exit", ImVec2(100, 50));
                ImGui::PopStyleColor();
            }
        }
        else {
            // Normal behavior - Play/Close Game and Exit buttons
            if (status.exeFound) {
                if (status.gameRunning) {
                    // Close Game button - softer red with proper hover effect
                    ImGui::PushStyleColor(ImGuiCol_Button, Style::COLOR_CLOSE_BUTTON);
                    // For hover, blend between close button color and hover pink
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(
                        Style::COLOR_CLOSE_BUTTON.x * 0.7f + Style::COLOR_BUTTON_HOVER.x * 0.3f,
                        Style::COLOR_CLOSE_BUTTON.y * 0.7f + Style::COLOR_BUTTON_HOVER.y * 0.3f,
                        Style::COLOR_CLOSE_BUTTON.z * 0.7f + Style::COLOR_BUTTON_HOVER.z * 0.3f,
                        1.0f));
                    // For active, darken the color slightly
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(
                        Style::COLOR_CLOSE_BUTTON.x * 0.8f,
                        Style::COLOR_CLOSE_BUTTON.y * 0.8f,
                        Style::COLOR_CLOSE_BUTTON.z * 0.8f,
                        1.0f));

                    if (ImGui::Button("Close Game", ImVec2(200, 50))) {
                        Utils::TerminateGameProcess(status);
                    }

                    ImGui::PopStyleColor(3);
                }
                else if (!status.isPatching) {
                    // Play Game button
                    if (ImGui::Button("Play Game", ImVec2(200, 50))) {
                        status.isPatching = true;
                        status.updateStatus("Preparing to launch game...", 0);

                        // Launch in a separate thread to keep UI responsive
                        CreateThread(NULL, 0, [](LPVOID lpParam) -> DWORD {
                            AppStatus* s = (AppStatus*)lpParam;
                            Utils::SmartLaunch(*s, false); // false indicates normal mode
                            return 0;
                            }, &status, 0, NULL);
                    }
                }
                else {
                    // Disabled button while patching
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    ImGui::Button("Processing...", ImVec2(200, 50));
                    ImGui::PopStyleColor();
                }
            }
            else {
                // Disabled button if game not found
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::Button("Play Game", ImVec2(200, 50));
                ImGui::PopStyleColor();
            }

            ImGui::SameLine();

            // Exit button
            if (ImGui::Button("Exit", ImVec2(100, 50))) {
                if (MessageBoxA(NULL, "Are you sure you want to exit?", "Confirm Exit", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    done = true;
                }
            }
        }

        ImGui::PopStyleColor(); // Pop button text color
        ImGui::PopFont(); // Pop button font
    }

    void RenderDLLInjectorTab(AppStatus& status) {
        ImGuiIO& io = ImGui::GetIO();
        float yPos = 150.0f;

        // DLL Selection Section
        Style::BeginSection("DLL Selection", &yPos, 60);

        // Path display with ellipsis for long paths
        const char* dllPathText = status.dllPath.empty() ? "No DLL selected" : status.dllPath.c_str();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120); // Make space for Browse button
        char dllPathBuffer[256];
        strncpy_s(dllPathBuffer, dllPathText, sizeof(dllPathBuffer) - 1);
        ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_TEXT);
        ImGui::InputText("##dllpath", dllPathBuffer, sizeof(dllPathBuffer), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();

        // Browse button
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_BUTTON_TEXT);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 5));
        if (ImGui::Button("Browse...", ImVec2(100, 30))) {
            Utils::BrowseForDLL(status);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        Style::EndSection();

        // Injection Status Section
        Style::BeginSection("Injection Status", &yPos, 160);

        // Status text
        ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_TEXT);
        ImGui::TextWrapped("%s", status.getInjectionStatus().c_str());
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Spacing();

        // Progress bar
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Style::COLOR_PROGRESS);
        ImGui::ProgressBar(status.getInjectionProgress() / 100.0f, ImVec2(-1, 25));
        ImGui::PopStyleColor();

        // Requirements status text below progress bar
        if (!status.gameRunning || !status.dllSelected) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_TEXT);
            if (!status.gameRunning) {
                ImGui::TextWrapped("Game must be running to inject DLLs.");
            }
            if (!status.dllSelected) {
                ImGui::TextWrapped("Please select a DLL file first.");
            }
            ImGui::PopStyleColor();
        }

        Style::EndSection();

        // Inject button positioned at same place as Play/Close Game button
        ImGui::SetCursorPosY(Style::WINDOW_HEIGHT - 100);
        ImGui::SetCursorPosX((Style::WINDOW_WIDTH - 200) * 0.5f);

        ImGui::PushFont(io.Fonts->Fonts[4]); // Button font
        ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_BUTTON_TEXT);

        if (status.gameRunning && status.dllSelected) {
            if (ImGui::Button("Inject DLL", ImVec2(200, 50))) {
                // Reset injection status first
                status.updateInjectionStatus("Starting DLL injection...", 10);

                // Launch in a separate thread to keep UI responsive
                CreateThread(NULL, 0, Utils::InjectDLLThreadFunc, &status, 0, NULL);
            }
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::Button("Inject DLL", ImVec2(200, 50));
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleColor(); // Pop button text color
        ImGui::PopFont(); // Pop button font
    }

    void RenderMainWindow(AppStatus& status, bool replaceMode, bool& done) {
        ImGuiIO& io = ImGui::GetIO();
        
        // Create main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(Style::WINDOW_WIDTH, Style::WINDOW_HEIGHT));

        // Start main window with flags for no resizing, no scrollbar
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Style::COLOR_2BACKGROUND);
        ImGui::Begin("Resident Evil 4 Patch Tool", NULL,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar);

        // Title - Using large font (index 2)
        ImGui::SetCursorPosY(20);
        ImGui::PushFont(io.Fonts->Fonts[2]);
        ImVec2 titleSize = ImGui::CalcTextSize("Resident Evil 4 Patch Tool");
        ImGui::SetCursorPosX((Style::WINDOW_WIDTH - titleSize.x) * 0.45f);
        ImGui::TextColored(Style::COLOR_TITLE, "Resident Evil 4 Patch Tool");
        ImGui::PopFont();

        // Subtitle - Using subtitle font (index 3)
        ImGui::PushFont(io.Fonts->Fonts[3]);
        ImVec2 subtitleSize = ImGui::CalcTextSize("Developed by kreed");
        ImGui::SetCursorPosX((Style::WINDOW_WIDTH - subtitleSize.x) * 0.5f);
        ImGui::TextColored(Style::COLOR_TEXT, "Developed by kreed");
        ImGui::PopFont();

        // Add copyright as small text under subtitle
        ImGui::PushFont(io.Fonts->Fonts[0]); // Use smaller normal font
        char yearBuffer[5];
        time_t t = time(NULL);
        struct tm* timeinfo = localtime(&t);
        strftime(yearBuffer, sizeof(yearBuffer), "%Y", timeinfo);

        std::string copyrightText = "Copyright " + std::string(yearBuffer) + " All Rights Reserved";
        ImVec2 copyrightSize = ImGui::CalcTextSize(copyrightText.c_str());
        ImGui::SetCursorPosX((Style::WINDOW_WIDTH - copyrightSize.x) * 0.5f);
        ImGui::TextColored(Style::COLOR_COPYRIGHT, "%s", copyrightText.c_str());
        ImGui::PopFont();

        // Tab bar (only show if not in replace mode)
        if (!replaceMode) {
            ImGui::SetCursorPosY(110); // Adjusted for copyright text
            ImGui::PushStyleColor(ImGuiCol_Tab, Style::COLOR_FRAME_BG);
            ImGui::PushStyleColor(ImGuiCol_TabHovered, Style::COLOR_BUTTON_HOVER);
            ImGui::PushStyleColor(ImGuiCol_TabActive, Style::COLOR_BUTTON_BG);
            ImGui::PushStyleColor(ImGuiCol_Text, Style::COLOR_TEXT);
            ImGui::PushFont(io.Fonts->Fonts[3]); // Use subtitle font for tabs

            if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
                if (ImGui::BeginTabItem("Game Launcher")) {
                    status.currentTab = 0;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("DLL Injector")) {
                    status.currentTab = 1;
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::PopFont();
            ImGui::PopStyleColor(4);
        }

        // Render the appropriate tab
        if (status.currentTab == 0 || replaceMode) {
            RenderLauncherTab(status, replaceMode, done);
        }
        else if (status.currentTab == 1 && !replaceMode) {
            RenderDLLInjectorTab(status);
        }

        ImGui::End();
        ImGui::PopStyleColor(); // Pop COLOR_BACKGROUND
    }
}