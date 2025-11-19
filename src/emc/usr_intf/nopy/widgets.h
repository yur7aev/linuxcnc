#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <stdarg.h>

using namespace ImGui;

bool LedButton(const char* label, bool v, const ImVec2& size = ImVec2(0, 0), ImU32 color = 0xff00aa00);
bool Gage(const char* str_id, float diam, float *v, const ImVec2 &range = ImVec2(0.0f, 1.0f), float thickness = 4, const ImVec4 &color = ImVec4(1, 1, 1, 1));
bool BeginBigTabBar(const char* str_id, ImGuiTabBarFlags flags = 0);
bool BeginBigTabItem(const char* label, bool* p_open = NULL, ImGuiTabItemFlags flags = 0);

class VarPopper {
public:
	VarPopper() {}
	~VarPopper() { PopStyleVar(); }
};

class FontPopper {
public:
	FontPopper() {}
	~FontPopper() { PopFont(); }
};

class ColorPopper {
public:
	ColorPopper() {}
	~ColorPopper() { PopStyleColor(); }
};

#define CAT_(x,y) x##y
#define CAT(x,y) CAT_(x,y)

#define SetStyleVar(a,b) VarPopper CAT(varpopper, __COUNTER__); PushStyleVar(a,b)
#define SetFont(a,b) FontPopper CAT(fontpopper, __COUNTER__); PushFont(a,b)
#define SetStyleColor(a,b) ColorPopper CAT(colorpopper, __COUNTER__); PushStyleColor(a,b)

//#define ONCE static bool __once__ = false; if (!__once__ && (__once__ = true)) 
#define ONCE static bool CAT_(once, __COUNTER__) = false; if (!CAT_(once, __COUNTER__) && (CAT_(once, __COUNTER__) = true)) 

void MoveCursorY(float d);
void MoveCursorX(float d);
void Sep(const char *text);
bool SplitterText(const char* label, float *size1, float *size2);

class Scroller {
	bool scrolling;
	bool owning;
	float delta;
public:
	Scroller() : scrolling(false), owning(false), delta(0.0f) {}

	void scroll(void) {	// flick scroller
		if (IsMouseClicked(1) && IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem| ImGuiHoveredFlags_ChildWindows)) {//ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_ChildWindows)) {
			ImGuiContext& g = *GImGui;
			ImRect sbr = GetWindowScrollbarRect(GetCurrentWindow(), ImGuiAxis_Y);
			bool not_scrollbar = !sbr.ContainsWithPad(g.IO.MousePos, g.Style.TouchExtraPadding);
//			bool inner = !g.CurrentTable || g.CurrentTable->InnerClipRect.ContainsWithPad(g.IO.MousePos, g.Style.TouchExtraPadding);
  			bool not_header = true;
			if (g.CurrentTable) {
				ImRect r = g.CurrentTable->InnerClipRect;
				r.Min.y += TableGetHeaderRowHeight();
				not_header = r.ContainsWithPad(g.IO.MousePos, g.Style.TouchExtraPadding);
				//fprintf(stderr, "T %f %f\n", g.IO.MousePos.y, r.Min.y);
			}
			if (!scrolling && not_scrollbar && not_header) {
fprintf(stderr, "%f,%f -> %f,%f-%f,%f\n", GetMousePos().x, GetMousePos().y, sbr.Min.x, sbr.Min.y, sbr.Max.x, sbr.Max.y);
//				SetKeyOwner(ImGuiKey_MouseLeft, GetCurrentWindow()->ID);	// steal mouse button so clicked widgets won't activate
				scrolling = true;
			}
		}

		if (scrolling && IsMouseDragging(1)) {
			delta = GetMouseDragDelta(1).y;
			ResetMouseDragDelta(1);
			if (!owning) {
//fprintf(stderr, "SCROLLING %d\n", hvr);
				SetKeyOwner(1 ? ImGuiKey_MouseRight : ImGuiKey_MouseLeft, GetCurrentWindow()->ID);	// steal mouse button so clicked widgets won't activate
				owning = true;
			}
		}
		if (scrolling && IsMouseReleased(1)) {
			if (owning) SetKeyOwner(1 ? ImGuiKey_MouseRight : ImGuiKey_MouseLeft, ImGuiKeyOwner_NoOwner);
			owning = false;
			scrolling = false;
			delta *= 2;
fprintf(stderr, "END SCROLL\n");
		}
		if (fabs(delta) > 0.1f) {
			SetScrollY(GetScrollY() - delta);
			delta /= 1.1f;
		}
	}
};


bool CellEdit(void (*save_func)(int, int, const char *), const char *fmt, va_list args);
bool OSKButton(const char* label, const ImVec2 &size, bool *down, bool *up, bool *isheld = nullptr);
