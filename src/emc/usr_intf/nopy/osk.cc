#include "osk.h"
#include <queue>
#include <map>

using namespace ImGui;

bool show_osk = false;

struct Keypress {
	ImGuiKey key;
	ImGuiKey mods;
	bool down;
	bool up;
	int code;
	float down_duration;
};

std::queue<Keypress> keypresses;
std::map<ImGuiKey, Keypress> down_keys;


#define G 0.5f
struct key qwerty[] = {
//	{ ImGuiKey_GraveAccent, "`",  "~",  1, 1, '`', '~' },
	{ KEY_CLOSE,           "\u0378", "\u0378", 1, 1 },
	{ ImGuiKey_1,           "1",  "!",  1, 1, '1', '!' },
	{ ImGuiKey_2,           "2",  "@",  1, 1, '2', '@' },
	{ ImGuiKey_3,           "3",  "#",  1, 1, '3', '#' },
	{ ImGuiKey_4,           "4",  "$",  1, 1, '4', '$' },
	{ ImGuiKey_5,           "5",  "%",  1, 1, '5', '%' },
	{ ImGuiKey_6,           "6",  "^",  1, 1, '6', '^' },
	{ ImGuiKey_7,           "7",  "&",  1, 1, '7', '&' },
	{ ImGuiKey_8,           "8",  "*",  1, 1, '8', '*' },
	{ ImGuiKey_9,           "9",  "(",  1, 1, '9', '(' },
	{ ImGuiKey_0,           "0",  ")",  1, 1, '0', ')' },
	{ ImGuiKey_Minus,       "-",  "_",  1, 1, '-', '_' },
	{ ImGuiKey_Equal,       "=",  "+",  1, 1, '=', '+' },
	{ ImGuiKey_Backspace,   "BS", "BS", 2, 1 },
	{ ImGuiKey_PageUp,  "PgUp", "PgUp", 1, 1 },
	{ KEY_ROW },

	{ ImGuiKey_Tab,      "Tab", "Tab", 1.5, 1,  },
	{ ImGuiKey_Q,            "q", "Q", 1, 1, 'q', 'Q' },
	{ ImGuiKey_W,            "w", "W", 1, 1, 'w', 'W' },
	{ ImGuiKey_E,            "e", "E", 1, 1, 'e', 'E' },
	{ ImGuiKey_R,            "r", "R", 1, 1, 'r', 'R' },
	{ ImGuiKey_T,            "t", "T", 1, 1, 't', 'T' },
	{ ImGuiKey_Y,            "y", "Y", 1, 1, 'y', 'Y' },
	{ ImGuiKey_U,            "u", "U", 1, 1, 'u', 'U' },
	{ ImGuiKey_I,            "i", "I", 1, 1, 'i', 'I' },
	{ ImGuiKey_O,            "o", "O", 1, 1, 'o', 'O' },
	{ ImGuiKey_P,            "p", "P", 1, 1, 'p', 'P' },
	{ ImGuiKey_LeftBracket,  "[", "{", 1, 1, '[', '{' },
	{ ImGuiKey_RightBracket, "]", "}", 1, 1, ']', '}' },
	{ ImGuiKey_Backslash,   "\\", "|", 1.5, 1, '\\', '|' },
	{ ImGuiKey_PageDown, "PgDn", "PgDn", 1, 1, },
	{ KEY_ROW },

	{ ImGuiKey_CapsLock, "Caps", "Caps", 1.75, 1 },
	{ ImGuiKey_A,          "a", "A", 1, 1, 'a', 'A' },
	{ ImGuiKey_S,          "s", "S", 1, 1, 's', 'S' },
	{ ImGuiKey_D,          "d", "D", 1, 1, 'd', 'D' },
	{ ImGuiKey_F,          "f", "F", 1, 1, 'f', 'F' },
	{ ImGuiKey_G,          "g", "G", 1, 1, 'g', 'G' },
	{ ImGuiKey_H,          "h", "H", 1, 1, 'h', 'H' },
	{ ImGuiKey_J,          "j", "J", 1, 1, 'j', 'J' },
	{ ImGuiKey_K,          "k", "K", 1, 1, 'k', 'K' },
	{ ImGuiKey_L,          "l", "L", 1, 1, 'l', 'L' },
	{ ImGuiKey_Semicolon,  ";", ":", 1, 1, ';', ':'  },
	{ ImGuiKey_Apostrophe, "'", "\"", 1, 1, '\'', '"' },
	{ ImGuiKey_Enter, "Enter", "Enter", 2.25, 1 },
	{ ImGuiKey_Home, "Home", "Home", 1, 1 },
	{ KEY_ROW },

	{ ImGuiKey_LeftShift, "Shift##l", "Shift##l", 2.25, 1, 0, 0, ImGuiMod_Shift},
	{ ImGuiKey_Z,           "z", "Z", 1, 1, 'z', 'Z' },
	{ ImGuiKey_X,           "x", "X", 1, 1, 'x', 'X' },
	{ ImGuiKey_C,           "c", "C", 1, 1, 'c', 'C' },
	{ ImGuiKey_V,           "v", "V", 1, 1, 'v', 'V' },
	{ ImGuiKey_B,           "b", "B", 1, 1, 'b', 'B' },
	{ ImGuiKey_N,           "n", "N", 1, 1, 'n', 'N' },
	{ ImGuiKey_M,           "m", "M", 1, 1, 'm', 'M' },
	{ ImGuiKey_Comma,       ",", "<", 1, 1, ',', '<' },
	{ ImGuiKey_Period,      ".", ">", 1, 1, '.', '>' },
	{ ImGuiKey_Slash,       "/", "?", 1, 1, '/', '?' },
	{ ImGuiKey_RightShift,"Shift##r", "Shift##r", 2.75-1, 1, 0, 0, ImGuiMod_Shift },
	{ ImGuiKey_UpArrow, "\u2191", "\u21e7", 1, 1 },
	{ ImGuiKey_End,     "End", "End", 1, 1  },
	{ KEY_ROW },


	{ ImGuiKey_LeftCtrl, "Ctrl##l", "Ctrl##l", 1.25, 1, 0, 0, ImGuiMod_Ctrl },
	{ KEY_SKIP,           "\u0378", "\u0378", 1, 1 },
	{ ImGuiKey_LeftAlt,    "Alt##l", "Alt##l", 1.25, 1, 0, 0, ImGuiMod_Alt },
	{ ImGuiKey_Space,            " ", " ", 6.25-0.25-1, 1, ' ', ' ',  },
	{ ImGuiKey_RightAlt,   "Alt##r", "Alt##r", 1.25, 1, 0, 0, ImGuiMod_Alt },
	{ KEY_SKIP,           "\u0378", "\u0378", 1, 1 },
	{ ImGuiKey_RightCtrl,"Ctrl##r", "Ctrl##r", 1.25, 1, 0, 0, ImGuiMod_Ctrl },
	{ ImGuiKey_Delete,              "Del", "Del", 1, 1 },
	{ ImGuiKey_LeftArrow,     "\u2190", "\u21e6", 1, 1 },
	{ ImGuiKey_DownArrow,     "\u2193", "\u21e9", 1, 1 },
	{ ImGuiKey_RightArrow,    "\u2192", "\u21e8", 1, 1 },

//	{ ",  "", 0.25, 1, KEY_SKIP },

	{ KEY_DONE },
};

#undef G

struct key numpad[] = {
	{ KEY_CLOSE, "\u0378", "", 1, 1 },
	{ ImGuiKey_KeypadDivide, "/", "", 1, 1, '/' },
	{ ImGuiKey_KeypadMultiply, "*", "", 1, 1, '*' },
	{ ImGuiKey_KeypadSubtract, "-", "", 1, 1, '-' },
	{ KEY_ROW },

	{ ImGuiKey_Keypad7, "7", "", 1, 1, '7' },
	{ ImGuiKey_Keypad8, "8", "", 1, 1, '8' },
	{ ImGuiKey_Keypad9, "9", "", 1, 1, '9' },
	{ ImGuiKey_KeypadAdd, "+", "", 1, 1, '+' },
	{ KEY_ROW },

	{ ImGuiKey_Keypad4, "4", "", 1, 1, '4' },
	{ ImGuiKey_Keypad5, "5", "", 1, 1, '5' },
	{ ImGuiKey_Keypad6, "6", "", 1, 1, '6' },
	{ ImGuiKey_Backspace, "\u2190", "", 1, 1 },
//	{ ImGuiKey_Backspace, "\u232b", "", 1, 1 },"\u2190"
	{ KEY_ROW },

	{ ImGuiKey_Keypad1, "1", "", 1, 1, '1' },
	{ ImGuiKey_Keypad2, "2", "", 1, 1, '2' },
	{ ImGuiKey_Keypad3, "3", "", 1, 1, '3' },
	{ ImGuiKey_KeypadEnter, "\u23ce", "", 1, 2 },
	{ KEY_ROW },

	{ ImGuiKey_Keypad0, "0", "", 2, 1, '0' },
	{ ImGuiKey_Keypad0, ".", "", 1, 1, '.' },
	{ KEY_DONE },
};


bool IsTouchHoveringRect(int i, const ImVec2& r_min, const ImVec2& r_max, bool clip = true)
{
	ImGuiContext& g = *GImGui;
	if (!g.IO.TouchActive[i])
		return false;

	ImRect rect_clipped(r_min, r_max);
	if (clip)
		rect_clipped.ClipWith(g.CurrentWindow->ClipRect);

	if (!rect_clipped.ContainsWithPad(g.IO.TouchPos[i], g.Style.TouchExtraPadding))
		return false;

	return true;
}

ImGuiWindow *saved_nav = 0;
ImGuiID saved_act;

static ImGuiID clicked_id = 0;
static ImGuiID clicked_id2[IMGUI_TOUCH_POINTS];

bool OSKButton(const char* label, const ImVec2 &size, bool *down, bool *up, bool *isheld = nullptr) {
	ImGuiWindow *window = GetCurrentWindow();
	if (window->SkipItems)
		return false;

	const ImGuiID id = window->GetID(label);
	ImVec2 pos = window->DC.CursorPos;
	const ImRect bb(pos, pos + size);
	ItemSize(bb);

	ImGuiContext& g = *GImGui;
	ImGuiStyle& style = g.Style;
	ImDrawList* draw_list = window->DrawList;

	bool hovered = (clicked_id == 0 || clicked_id == id) && IsMouseHoveringRect(bb.Min, bb.Max);
	bool clicked = hovered && IsMouseClicked(0); //, up == NULL);
	bool held = hovered && clicked_id == id && IsMouseDown(0);
	bool released = clicked_id == id && IsMouseReleased(0);

	if (clicked) fprintf(stderr, "OSK in:%x wi:%x\n", g.InputTextState.ID, g.NavWindow);

	if (clicked && g.NavWindow && g.InputTextState.ID) {
		saved_nav = g.NavWindow;
		saved_act = g.InputTextState.ID;//g.LastActiveId;
		//    ImGuiInputTextState     InputTextState;
		//    ImGuiInputTextDeactivatedState InputTextDeactivatedState;
		SetNavWindow(saved_nav);
		SetActiveID(saved_act, saved_nav);
	}

	if (clicked) clicked_id = id;
	if (released) clicked_id = 0;

	for (int i = 1; i < IMGUI_TOUCH_POINTS; ++i) {
		bool hovered2 = (clicked_id2[i] == 0 || clicked_id2[i] == id) && IsTouchHoveringRect(i, bb.Min, bb.Max);
		bool clicked2 = hovered2 && g.IO.TouchDown[i];
		bool released2 = clicked_id2[i] == id && id && g.IO.TouchUp[i];
		bool held2 = hovered2 && clicked_id2[i] == id && g.IO.TouchActive[i] && !released2;

		if (clicked2) clicked_id2[i] = id;
		if (released2) clicked_id2[i] = 0;

		if (held2) held = true;
		if (clicked2) clicked = true;
		if (released2) released = true;
	}


	if (clicked || released) fprintf(stderr, "MMM %d %d %d\n", clicked, released, held);

	*down = (clicked && !held);
	*up = (released && !held);
	if (isheld) *isheld = held;

	// Render
	const ImU32 col = GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
	const ImVec2 label_size = CalcTextSize(label, NULL, true);
	RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
	RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);

	return *up || *down;
}


void osk_close(void)
{
	saved_nav = 0;
	saved_act = 0;
	clicked_id = 0;
	for (int i = 1; i < IMGUI_TOUCH_POINTS; ++i) clicked_id2[i] = 0;
	for (auto it = down_keys.begin(); it != down_keys.end(); ++it) {
		it->second.down = false;
		it->second.up = true;
		it->second.code = 0;
		keypresses.push(it->second);
	}
}


void osk_repeat(void)
{
	ImGuiContext& g = *GImGui;

//	for (auto it = down_keys.begin(); it != down_keys.end(); ++it) {
	for (auto it = down_keys.rbegin(); it != down_keys.rend(); ++it) {
		if (it == down_keys.rbegin() && it->second.code && CalcTypematicRepeatAmount(it->second.down_duration, it->second.down_duration + g.IO.DeltaTime, g.IO.KeyRepeatDelay, g.IO.KeyRepeatRate) > 0) {
			Keypress kp = { ImGuiKey_None, ImGuiKey_None, false, false, it->second.code };
			keypresses.push(kp);
			//fprintf(stderr, "rep %c %f\n", kp.code, it->second.down_duration);
		}
		it->second.down_duration += g.IO.DeltaTime;
	}
}

bool osk(struct key *keys, float w, float h, float gap)
{
	PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

	ImGuiContext& g = *GImGui;
	ImVec2 pos = GetCursorPos();
	ImVec2 p = pos;
	bool rc = false;

//	static ImGuiID input_id = 0;		// input id
//	static ImGuiWindow *input_win;

//	input_id = g.InputTextState.ID;
//	input_win = GetCurrentWindow();

	bool up = false;
	bool down = false;
	const bool shift = g.IO.KeyShift;
	const bool ctrl = g.IO.KeyCtrl;
	const bool alt = g.IO.KeyAlt;

	static int shifts = 0;
	static int ctrls = 0;
	static int alts = 0;

	while (ImGuiKey key = keys->key) {
		switch (key) {
		case KEY_DONE:
			break;
		default:
			SetCursorPos(p);
			if (OSKButton(shift ? keys->label_shift : keys->label, ImVec2(w * keys->w - gap, h * keys->h - gap), &down, &up)) {
				int code = shift ? keys->code_shift : keys->code;
				if (key != KEY_CLOSE) {
					Keypress kp = { key, ImGuiMod_None, down, up, down && !ctrl && !alt ? code : 0 };
//					SetNavWindow(saved_nav);
//					SetActiveID(saved_act, saved_nav);
					if (keys->mods && (
						keys->mods == ImGuiMod_Shift && (down && !shifts++ || up && !--shifts) ||
						keys->mods == ImGuiMod_Ctrl  && (down && !ctrls++ || up && !--ctrls) ||
						keys->mods == ImGuiMod_Alt   && (down && !alts++ || up && !--alts)
					)) kp.mods = keys->mods;
					keypresses.push(kp);
					if (down) down_keys.insert({key, kp});
					if (up) down_keys.erase(key);
				} else {
					osk_close();
					rc = true;
				}
			}
			// fall thru
		case KEY_SKIP:
			p.x += w * keys->w;
			break;
		case KEY_ROW:
			p.x = pos.x;
			p.y += h;
			break;
		}
		keys++;
	}
        PopItemFlag();

	return rc;
}

void osk_process(void)
{
	osk_repeat();
	if (!keypresses.empty()) { // && keyboard_refocus >= 3) {
		ImGuiIO& io = GetIO();
		if (saved_nav && saved_act) {
			SetNavWindow(saved_nav);
//fprintf(stderr, "set id %x\n", saved_act);
			SetActiveID(saved_act, saved_nav);
		}
		Keypress &kp = keypresses.front();
		if (kp.mods != ImGuiMod_None) {
			if (kp.down) io.AddKeyEvent(kp.mods, true);
			if (kp.up) io.AddKeyEvent(kp.mods, false);
		}
		if (kp.down) io.AddKeyEvent(kp.key, true);
		if (kp.up) io.AddKeyEvent(kp.key, false);
		if (kp.code) io.AddInputCharacter(kp.code);
		keypresses.pop();
	}
}

int osk_request = 0;
ImVec2 osk_pos;

void osk_show(int type, ImVec2 pos)
{
	osk_request = type;
	osk_pos = pos;
}

