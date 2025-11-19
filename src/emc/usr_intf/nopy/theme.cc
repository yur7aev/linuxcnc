#include "theme.h"

ImFont *font;
ImFont *mono;
ImU32 bg_gradient[4];

ImVec4 color_green = ImVec4(0.051f, 0.914f, 0.663f, 0.7f);	// 179 169 233 13 #b3a9e909
ImVec4 color_white = ImVec4(1, 1, 1, 1);
ImVec4 color_yellow = ImVec4(1.0f, 0.863f, 0.1f, 1.0f);
ImVec4 color_red = ImVec4(1.0f, 0.2f, 0.1f, 0.7f);
ImVec4 color_grey = ImVec4(1.0f, 1.0f, 1.0f, 0.3f);
ImU32 cell_hl_color;
ImU32 cell_err_color;

void set_theme()
{
	ImFontConfig fcfg;
	fcfg.GlyphOffset = ImVec2(0.0f, -1.0f);
	fcfg.EllipsisChar = (ImWchar)0x2026;

	ImGuiIO& io = GetIO();
	font = io.Fonts->AddFontFromFileTTF("/home/dmitry/git/imgui/YNotoSans-Regular.ttf", 24.0f, &fcfg);
	mono = io.Fonts->AddFontFromFileTTF("/home/dmitry/git/imgui/YNotoSansMono-Regular.ttf", 24.0f, &fcfg);
	font->EllipsisChar = (ImWchar)0x2026;
	mono->EllipsisChar = (ImWchar)0x2026;

	StyleColorsDark();

	ImGuiStyle& style = GetStyle();

	style.FramePadding =
	style.CellPadding = { FS_SMALL/2, FS_SMALL/2 };

	style.FrameRounding = FS_SMALL/3;
	style.TabRounding =
	style.PopupRounding =
	style.WindowRounding = FS_SMALL/2;	// modal popups use this

	style.ScrollbarSize = FS_SMALL*3/2;
	style.ScrollbarRounding =
	style.GrabRounding = style.ScrollbarSize / 2;
	style.GrabMinSize = style.ScrollbarSize * 2;

	style.TouchExtraPadding = ImVec2(4.0f, 4.0f);	// half of item spacing

	style.ItemSpacing = ImVec2(0, 0);
	style.WindowPadding = ImVec2(0, 0);

	style.SeparatorTextBorderSize = 1;
	style.SeparatorTextPadding = { 8, 0 };

	style.TabBarBorderSize = 0;
	style.PopupBorderSize = 0;
	style.WindowBorderSize = 0;	// modal popups use this
	style.ChildBorderSize = 0;

	// colors

	style.Colors[ImGuiCol_Button] = 
	style.Colors[ImGuiCol_Tab] = 
	style.Colors[ImGuiCol_ScrollbarGrab] = 
	style.Colors[ImGuiCol_FrameBg] = 
	ImVec4(0.0f, 0.0f, 0.0f, 0.2f);

	style.Colors[ImGuiCol_ButtonHovered] = 
	style.Colors[ImGuiCol_TabHovered] = 
	style.Colors[ImGuiCol_HeaderHovered] = 		// in listboxes
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = 
	style.Colors[ImGuiCol_FrameBgHovered] = 
	style.Colors[ImGuiCol_FrameBgActive] = 
	style.Colors[ImGuiCol_SeparatorHovered] =
	ImVec4(0.051f, 0.914f, 0.663f, 0.15f);

	style.Colors[ImGuiCol_Header] = 
	style.Colors[ImGuiCol_HeaderActive] = 
	style.Colors[ImGuiCol_ButtonActive] = 
	style.Colors[ImGuiCol_TabSelected] = 
	style.Colors[ImGuiCol_TabHoveredSelected] = 
	style.Colors[ImGuiCol_ScrollbarGrabActive] = 
	style.Colors[ImGuiCol_SliderGrab] = 
	style.Colors[ImGuiCol_SliderGrabActive] = 
	style.Colors[ImGuiCol_SeparatorActive] =
	ImVec4(0.051f, 0.914f, 0.663f, 0.5f);

	style.Colors[ImGuiCol_TextSelectedBg] = 
	ImVec4(0.051f, 0.914f, 0.663f, 0.15f);

	style.Colors[ImGuiCol_ModalWindowDimBg] = 
	ImVec4(0.0f, 0.0f, 0.0f, 0.3f);

	style.Colors[ImGuiCol_WindowBg] =
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.26f, 0.32f, 0.35f, 1.0f);

	style.Colors[ImGuiCol_FrameBg] = ImVec4(0, 0, 0, 0.2);
	style.Colors[ImGuiCol_Border] =
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);


	style.Colors[ImGuiCol_TableRowBg] =
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0, 0, 0, 0);

	bg_gradient[0] = HSV(195.8f, 19.8f, 37.6f);
	bg_gradient[1] = HSV(202.9f, 25.9f, 31.8f);
	bg_gradient[2] = HSV(201.4f, 34.1f, 16.1f);
	bg_gradient[3] = HSV(208.0f, 26.8f, 22.0f);

	style.SeparatorTextAlign = ImVec2(0.5f, 0.5f);
//	style.Colors[ImGuiCol_Separator] = ImVec4(0.0f, 0.0f, 0.0f, 0.2f);
	style.Colors[ImGuiCol_TextDisabled] =   //???
	style.Colors[ImGuiCol_Separator] = ImVec4(1.0f, 1.0f, 1.0f, 0.3f);

/*
	style.Colors[ImGuiCol_TableHeaderBg]       // Table header background
	style.Colors[ImGuiCol_TableBorderStrong]   // Table outer and header borders
	style.Colors[ImGuiCol_TableBorderLight]    // Table inner borders
	style.Colors[ImGuiCol_TableRowBg]          // Table row background when ImGuiTableFlags_RowBg is enabled (even rows)
	style.Colors[ImGuiCol_TableRowBgAlt]       // Table row background when ImGuiTableFlags_RowBg is enabled (odds rows)
*/

	cell_hl_color = GetColorU32(style.Colors[ImGuiCol_HeaderHovered]);
	cell_err_color = GetColorU32(ImVec4(0.914f, 0.251f, 0.163f, 0.15f));
}

ImU32 HSV(float h, float s, float v, float a)
{
	ImVec4 rgba;
	ColorConvertHSVtoRGB(h/360.0f, s/100.0f, v/100.0, rgba.x, rgba.y, rgba.z);
	rgba.w = a;
	return ColorConvertFloat4ToU32(rgba);
}
