#include "widgets.h"
#include "theme.h"
#include "osk.h"

bool LedButton(const char* label, bool v, const ImVec2& size, ImU32 color)
{
	bool pressed = Button(label, size);

	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;

	ImDrawList* draw_list = window->DrawList;
	ImRect r = g.LastItemData.Rect;
	ImGuiStyle& style = GetStyle();
	///.Max - ImVec2(10,10), 4, v ? color : (color >> 2) & 0x3c3c3c3c | 0xff000000);
//	draw_list->AddCircleFilled(ImVec2(r.Min.x + FS_SMALL/2, r.Min.y + FS_SMALL/2), 4, v ? color : (color >> 2) & 0x3c3c3c3c | 0xff000000);
	draw_list->AddCircleFilled(ImVec2(r.Min.x + style.FrameRounding, r.Min.y + style.FrameRounding), 4, v ? color : (color >> 2) & 0x3c3c3c3c | 0xff000000);

	return pressed;
}


bool Gage(const char* str_id, float diam, float *v, const ImVec2 &range, float thickness, const ImVec4 &color) {
	ImGuiWindow *window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	const ImGuiID id = window->GetID(str_id);
	ImVec2 pos = window->DC.CursorPos;
	ImVec2 size(diam, diam);
	const ImRect bb(pos, pos + size);
	ItemSize(bb);
	if (!ItemAdd(bb, id))
		return false;

	ImGuiContext& g = *GImGui;
	bool value_changed = false;

	const bool hovered = ItemHoverable(bb, id, g.LastItemData.ItemFlags);
	const bool clicked = hovered && IsMouseClicked(0, ImGuiInputFlags_None, id);
	if (clicked) {
	//	SetKeyOwner(ImGuiKey_MouseLeft, id);
fprintf(stderr, "set id\n");
		SetActiveID(id, window);
	//	SetFocusID(id, window);
	//	FocusWindow(window);
	//	g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
	}

	float vu = *v;
	vu -= range.x;
	vu /= range.y - range.x;
	vu = ImSaturate(vu);

	if (g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Mouse) {
		if (g.IO.MouseDown[0]) {
			const float mouse_abs_pos = g.IO.MousePos[ImGuiAxis_Y];
			if (g.ActiveIdIsJustActivated) {
				g.SliderGrabClickOffset = mouse_abs_pos;
				g.SliderCurrentAccum = vu;
			} else {
				float v_new = ImSaturate(g.SliderCurrentAccum + (g.SliderGrabClickOffset - mouse_abs_pos) / 400.0);
				if (vu != v_new) {
					vu = v_new;
					v_new *= range.y - range.x;
					v_new += range.x;
					*v = v_new;
					value_changed = true;
				}
			}
		} else {
fprintf(stderr, "clear id\n");
			ClearActiveID();
		}
	}

	ImGuiStyle& style = g.Style;
	ImDrawList* draw_list = window->DrawList;
	//draw_list->AddRect(pos, pos+size, 0xff0000ff);

	ImU32 c2 = GetColorU32((g.ActiveId == id || hovered) ? ImGuiCol_ScrollbarGrabHovered : ImGuiCol_ScrollbarGrab);
	ImU32 c1 = GetColorU32(ImGuiCol_Text);

	float radius = diam/2;
	ImVec2 centre = pos + ImVec2(radius, radius);

	if (g.ActiveId == id) {
		radius += 2;
		thickness += 2;
	}

	const float gap = 1.0/3;
	const float angle = -IM_PI / 3;
	float a_min = -IM_PI * (1.5 - gap) + angle;
	float a_max = IM_PI * (vu * (2 - gap * 2) - 1.5 + gap) + angle;

	static char t[32];
	snprintf(t, 32, "%.0f %%", *v * 100);
	ImVec2 ts1 = CalcTextSize(str_id) / 2;
	ImVec2 ts2 = CalcTextSize(t) / 2;
	ts1.y *= 2;
	ts2.y = 0;

	draw_list->AddText(centre - ts1, GetColorU32(ImGuiCol_Text), str_id);
	draw_list->AddText(centre - ts2, GetColorU32(ImGuiCol_Text), t);

	draw_list->PathArcTo(centre, radius-thickness, a_min, a_max);
	draw_list->PathStroke(c1, false, thickness);

	a_min = a_max;
	a_max = IM_PI * (0.5 - gap) + angle;

	draw_list->PathArcTo(centre, radius-thickness, a_min, a_max);
	draw_list->PathStroke(c2, false, thickness);

//	bool pressed = ButtonBehavior(bb, id, NULL, NULL, ImGuiButtonFlags_PressedOnClick);

	return value_changed;
}

bool BeginBigTabBar(const char* str_id, ImGuiTabBarFlags flags)
{
	PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 16));
	bool rc = BeginTabBar(str_id, flags);
	PopStyleVar();
	return rc;
}

bool BeginBigTabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags)
{
	PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 16));
	SetNextItemWidth(110);
	bool rc = BeginTabItem(label, p_open, flags);
	PopStyleVar();
	return rc;
}

void MoveCursorY(float d) {
    ImGuiWindow* window = GetCurrentWindow();
    window->DC.CursorPos.y += d;
    window->DC.IsSetPos = true;
}

void MoveCursorX(float d) {
    ImGuiWindow* window = GetCurrentWindow();
    window->DC.CursorPos.x += d;
    window->DC.IsSetPos = true;
}

void Sep(const char *text)
{
	PushStyleColor(ImGuiCol_Text, color_grey);
	SeparatorText(text);
	PopStyleColor();
}


bool SplitterTextEx(ImGuiID id, const char* label, const char* label_end, float extra_w, float *size1, float *size2)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiStyle& style = g.Style;

    const ImVec2 label_size = CalcTextSize(label, label_end, false);
    const ImVec2 pos = window->DC.CursorPos;
    const ImVec2 padding = style.SeparatorTextPadding;

    const float separator_thickness = style.SeparatorTextBorderSize;
    const ImVec2 min_size(label_size.x + extra_w + padding.x * 2.0f, ImMax(label_size.y + padding.y * 2.0f, separator_thickness));
    ImRect bb(pos, ImVec2(window->WorkRect.Max.x, pos.y + min_size.y));
    const float text_baseline_y = ImTrunc((bb.GetHeight() - label_size.y) * style.SeparatorTextAlign.y + 0.99999f); //ImMax(padding.y, ImFloor((style.SeparatorTextSize - label_size.y) * 0.5f));

	const char *x = " \u0378 ";
	const ImVec2 x_size = CalcTextSize(x, x+5, false);
	const ImVec2 x_pos(bb.Max.x - x_size.x, pos.y + text_baseline_y); // FIXME-ALIGN
	const ImRect bbb(x_pos, x_pos + x_size); // FIXME-ALIGN

	PushID(id);
	ImGuiID bid = window->GetID("X");
	PopID();

        if (!ItemAdd(bbb, bid)) return false;

	bool hovered, held;
	bool pressed = ButtonBehavior(ImRect(x_pos, x_pos+x_size), bid, &hovered, &held, ImGuiButtonFlags_FlattenChildren | ImGuiButtonFlags_AllowOverlap);

	const ImU32 col = GetColorU32(held ? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
        if (held || hovered)
		window->DrawList->AddRectFilled(x_pos, x_pos+x_size, col, 0.0f);

	RenderText(x_pos, x, x+5, false);

	bb.Max.x -= x_size.x;

    ItemSize(min_size, text_baseline_y);
    if (!ItemAdd(bb, id)) return false;

	const float min_size1 = 24;
	const float min_size2 = 24;;	
	if (size2 != nullptr) SplitterBehavior(bb, id, ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0, 0, 0x00000000);

    const float sep1_x1 = pos.x;
    const float sep2_x2 = bb.Max.x;// - x_size.x;
    const float seps_y = ImTrunc((bb.Min.y + bb.Max.y) * 0.5f + 0.99999f);

    const float label_avail_w = ImMax(0.0f, sep2_x2 - sep1_x1 - padding.x * 2.0f);
    const ImVec2 label_pos(pos.x + padding.x + ImMax(0.0f, (label_avail_w - label_size.x - extra_w) * style.SeparatorTextAlign.x), pos.y + text_baseline_y); // FIXME-ALIGN

    // This allows using SameLine() to position something in the 'extra_w'
    window->DC.CursorPosPrevLine.x = label_pos.x + label_size.x;

    const ImU32 separator_col = GetColorU32(ImGuiCol_Separator);
    if (label_size.x > 0.0f)
    {
        const float sep1_x2 = label_pos.x - style.ItemSpacing.x - padding.x;
        const float sep2_x1 = label_pos.x + label_size.x + extra_w + style.ItemSpacing.x + padding.x;
        if (sep1_x2 > sep1_x1 && separator_thickness > 0.0f)
            window->DrawList->AddLine(ImVec2(sep1_x1, seps_y), ImVec2(sep1_x2, seps_y), separator_col, separator_thickness);
        if (sep2_x2 > sep2_x1 && separator_thickness > 0.0f)
            window->DrawList->AddLine(ImVec2(sep2_x1, seps_y), ImVec2(sep2_x2, seps_y), separator_col, separator_thickness);
        if (g.LogEnabled)
            LogSetNextTextDecoration("---", NULL);
        RenderTextEllipsis(window->DrawList, label_pos, ImVec2(bb.Max.x, bb.Max.y + style.ItemSpacing.y), bb.Max.x, label, label_end, &label_size);
    }
    else
    {
        if (g.LogEnabled)
            LogText("---");
        if (separator_thickness > 0.0f)
            window->DrawList->AddLine(ImVec2(sep1_x1, seps_y), ImVec2(sep2_x2, seps_y), separator_col, separator_thickness);
    }
	return pressed;
}

bool SplitterText(const char* label, float *size1, float *size2)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // The SeparatorText() vs SeparatorTextEx() distinction is designed to be considerate that we may want:
    // - allow separator-text to be draggable items (would require a stable ID + a noticeable highlight)
    // - this high-level entry point to allow formatting? (which in turns may require ID separate from formatted string)
    // - because of this we probably can't turn 'const char* label' into 'const char* fmt, ...'
    // Otherwise, we can decide that users wanting to drag this would layout a dedicated drag-item,
    // and then we can turn this into a format function.
    return SplitterTextEx(window->GetID(label), label, FindRenderedTextEnd(label), 0.0f, size1, size2);
}


bool CellEdit(void (*save_func)(int, int, const char *), const char *fmt, va_list args)
{
	bool rc = false;
	static char osk_buf[81];

	ImVec2 tl = GetCursorScreenPos() - ImVec2(12, 12);
	ImRect r(tl, tl + ImVec2(GetColumnWidth() + 24, 48));

	bool numeric = show_osk || strcmp(fmt, "%s");
	struct key *layout = numeric ? numpad : qwerty;
	float key_count = numeric ? 4 : 16;

	const float button_size = 65;

	ImVec2 ws(button_size*key_count + 8, button_size*5 + 48 + 8*2);
	tl -= ImVec2(8, 8);

	ImVec2 avail = GetWindowSize();
	float dx = avail.x - tl.x - ws.x;
	if (dx > 0) {
		dx = 0;
	} else {
		tl.x += dx;
	}

	int row = TableGetRowIndex() - 1;	// skip header
	int col = TableGetColumnIndex();

	ImGuiID id = GetCurrentWindow()->GetID(row*256 + col);

	if (!ItemAdd(r, id)) ;
//		return false;

	bool pressed = ButtonBehavior(r, id, NULL, NULL, ImGuiButtonFlags_PressedOnClickRelease);

	PushID(id);
	if (pressed) {
		vsnprintf(osk_buf, sizeof(osk_buf)-1, fmt, args);
	        OpenPopup("##osk");
		TextUnformatted(osk_buf);
//fprintf(stderr, "%d,%d %x\n", col, row, id);

	}// else {
	TextV(fmt, args);
	//}

	////////////////

	if (show_osk) ws.y = 48 + 16;

	SetNextWindowSize(ws);
	SetNextWindowPos(tl);

	SetStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	SetStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
	SetStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));

	if (BeginPopupModal("##osk", NULL, ImGuiWindowFlags_NoDecoration)) {
		SetNextItemWidth(-FLT_MIN);
		if (pressed) SetKeyboardFocusHere();
		MoveCursorX(-dx);
		if (InputText("##ose", osk_buf, IM_ARRAYSIZE(osk_buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
			rc = true;
			osk_close();
			CloseCurrentPopup();
//fprintf(stderr, "clear id\n");
			ClearActiveID();
			save_func(row, col, osk_buf);
		}
//		if (numeric)
//			PushFont(mono, FS_MEDIUM);
//		else
			PushFont(font, FS_SMALL);
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		if (!show_osk && osk(layout, button_size, button_size, 8)) CloseCurrentPopup();
		PopStyleVar();
		PopFont();
		EndPopup();
	}
	PopID();

	return rc;
}
