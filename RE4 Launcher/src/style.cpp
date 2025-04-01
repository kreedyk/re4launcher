#include "Style.h"

namespace Style {
    // Colors
    ImVec4 COLOR_MAINBACKGROUND = ImVec4(1.0f, 0.94f, 0.97f, 1.0f); // #FFF0F5 LavenderBlush
    ImVec4 COLOR_FRAME_BG = ImVec4(1.0f, 0.92f, 0.95f, 1.0f); // #FFEBF3 Light pink
    ImVec4 COLOR_BUTTON_BG = ImVec4(1.0f, 0.41f, 0.71f, 1.0f); // #FF69B4 HotPink
    ImVec4 COLOR_BUTTON_HOVER = ImVec4(1.0f, 0.08f, 0.58f, 1.0f); // #FF1493 DeepPink
    ImVec4 COLOR_TEXT = ImVec4(0.43f, 0.13f, 0.31f, 1.0f); // #6D214F Dark pink text
    ImVec4 COLOR_TITLE = ImVec4(0.43f, 0.13f, 0.31f, 1.0f); // #6D214F Dark pink title
    ImVec4 COLOR_PROGRESS = ImVec4(1.0f, 0.41f, 0.71f, 1.0f); // #FF69B4 Pink progress bar
    ImVec4 COLOR_SEPARATOR = ImVec4(1.0f, 0.41f, 0.71f, 1.0f); // #FF69B4 Pink separator
    ImVec4 COLOR_BUTTON_TEXT = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White text for buttons
    ImVec4 COLOR_2BACKGROUND = ImVec4(1.0f, 0.94f, 0.97f, 1.0f); // Same as MAINBACKGROUND
    ImVec4 COLOR_CLOSE_BUTTON = ImVec4(0.7f, 0.2f, 0.3f, 1.0f); // Softer red for Close button
    ImVec4 COLOR_COPYRIGHT = ImVec4(0.7f, 0.3f, 0.5f, 1.0f); // Pink for copyright text

    // Sizes
    float WINDOW_WIDTH = 500.0f;
    float WINDOW_HEIGHT = 600.0f;

    // Font sizes
    float FONT_SIZE_TITLE = 32.0f;
    float FONT_SIZE_SUBTITLE = 18.0f;
    float FONT_SIZE_NORMAL = 15.0f;
    float FONT_SIZE_SECTION = 18.0f;
    float FONT_SIZE_BUTTON = 20.0f;

    void SetImGuiStyle() {
        ImGuiStyle& style = ImGui::GetStyle();

        // Colors
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = COLOR_MAINBACKGROUND;
        colors[ImGuiCol_FrameBg] = COLOR_FRAME_BG;
        colors[ImGuiCol_Button] = COLOR_BUTTON_BG;
        colors[ImGuiCol_ButtonHovered] = COLOR_BUTTON_HOVER;
        colors[ImGuiCol_ButtonActive] = ImVec4(COLOR_BUTTON_HOVER.x * 0.8f, COLOR_BUTTON_HOVER.y * 0.8f, COLOR_BUTTON_HOVER.z * 0.8f, 1.0f);
        colors[ImGuiCol_Text] = COLOR_TEXT;
        colors[ImGuiCol_PlotHistogram] = COLOR_PROGRESS;
        colors[ImGuiCol_Border] = COLOR_BUTTON_BG;
        colors[ImGuiCol_Separator] = COLOR_SEPARATOR;

        // Styling
        style.WindowRounding = 10.0f;
        style.FrameRounding = 5.0f;
        style.ChildRounding = 5.0f;
        style.GrabRounding = 5.0f;
        style.PopupRounding = 5.0f;
        style.ScrollbarRounding = 5.0f;
        style.TabRounding = 5.0f;

        style.FramePadding = ImVec2(10, 8);
        style.ItemSpacing = ImVec2(10, 10);
        style.ItemInnerSpacing = ImVec2(10, 6);
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

        // Button padding
        style.FramePadding = ImVec2(15, 10);

        // Increase sizes for better visibility
        style.ScrollbarSize = 16.0f;
        style.FrameBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;

        // No window resize option
        style.WindowMinSize = ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    void BeginSection(const char* title, float* yPos, float height) {
        // Set the cursor position
        ImGui::SetCursorPosY(*yPos);

        // Draw section title
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]); // Use the section title font
        ImGui::Text("%s", title);
        ImGui::PopFont();

        // Draw pink separator
        ImGui::PushStyleColor(ImGuiCol_Separator, COLOR_SEPARATOR);
        ImGui::Separator();
        ImGui::PopStyleColor();

        // Start child frame
        ImGui::PushStyleColor(ImGuiCol_ChildBg, COLOR_FRAME_BG);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);

        ImGui::BeginChild(title, ImVec2(WINDOW_WIDTH - 40, height), true);
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);

        // Update cursor position for next section
        *yPos += height + 50;
    }

    void EndSection() {
        ImGui::PopFont(); // Pop normal font
        ImGui::EndChild();
        ImGui::PopStyleVar(); // Pop ChildRounding
        ImGui::PopStyleColor(); // Pop ChildBg
    }
}