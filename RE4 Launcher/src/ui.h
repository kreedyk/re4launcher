#pragma once
#include "AppState.h"
#include <d3d11.h>
#include <Windows.h>

namespace UI {
    // Helper function to create the main window
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    
    // Win32 window procedure
    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Font setup
    void SetupFonts();
    
    // Render the main window UI
    void RenderMainWindow(AppStatus& status, bool replaceMode, bool& done);
    
    // Tab rendering
    void RenderLauncherTab(AppStatus& status, bool replaceMode, bool& done);
    void RenderDLLInjectorTab(AppStatus& status);
    
    // Global directX objects
    extern ID3D11Device* g_pd3dDevice;
    extern ID3D11DeviceContext* g_pd3dDeviceContext;
    extern IDXGISwapChain* g_pSwapChain;
    extern ID3D11RenderTargetView* g_mainRenderTargetView;
}