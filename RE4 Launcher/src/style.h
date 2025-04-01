#pragma once
#include "imgui.h"

// Colors and dimensions for UI styling
namespace Style {
    // Colors matching the Python version
    extern ImVec4 COLOR_MAINBACKGROUND;
    extern ImVec4 COLOR_FRAME_BG;
    extern ImVec4 COLOR_BUTTON_BG;
    extern ImVec4 COLOR_BUTTON_HOVER;
    extern ImVec4 COLOR_TEXT;
    extern ImVec4 COLOR_TITLE;
    extern ImVec4 COLOR_PROGRESS;
    extern ImVec4 COLOR_SEPARATOR;
    extern ImVec4 COLOR_BUTTON_TEXT;
    extern ImVec4 COLOR_2BACKGROUND;
    extern ImVec4 COLOR_CLOSE_BUTTON;
    extern ImVec4 COLOR_COPYRIGHT;

    // Dimensions
    extern float WINDOW_WIDTH;
    extern float WINDOW_HEIGHT;

    // Font sizes
    extern float FONT_SIZE_TITLE;
    extern float FONT_SIZE_SUBTITLE;
    extern float FONT_SIZE_NORMAL;
    extern float FONT_SIZE_SECTION;
    extern float FONT_SIZE_BUTTON;

    // Set ImGui style with all the custom colors and sizes
    void SetImGuiStyle();

    // Section frame with title and separator
    void BeginSection(const char* title, float* yPos, float height = 100.0f);
    void EndSection();
}