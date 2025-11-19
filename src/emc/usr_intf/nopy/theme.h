#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

using namespace ImGui;

void set_theme();
ImU32 HSV(float h, float s, float v, float a = 1.0f);

// font sizes (should be even)
#define FS_SMALL 24
#define FS_MEDIUM 32
#define FS_BIG 60
extern ImFont *font;
extern ImFont *mono;

extern ImU32 bg_gradient[4];

extern ImVec4 color_green;
extern ImVec4 color_grey;
extern ImVec4 color_white;
extern ImVec4 color_yellow;
extern ImVec4 color_red;
extern ImU32 cell_hl_color;
extern ImU32 cell_err_color;

#define DOT "\u00b7"
