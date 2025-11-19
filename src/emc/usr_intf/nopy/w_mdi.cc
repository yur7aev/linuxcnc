#include <stdarg.h>
#include "widgets.h"
#include "nopy.h"
#include "w_mdi.h"
#include "osk.h"
#include "theme.h"
#include "tooldata.hh"
#include "../shcom.hh"             // NML Messaging functions

extern bool show_osk;

// ---------------------------------------------------------------------------------------

void w_mdi(void)
{
	static ImVector<char*> History;
	static int HistoryPos = -1;    // -1: new line, 0..History.Size-1 browsing history.

	struct Funcs2 {
	static int MyCallback(ImGuiInputTextCallbackData* data) {
		if (History.Size <= 0) return 0;
		if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
			if (data->EventKey == ImGuiKey_UpArrow) {
				if (--HistoryPos < 0) HistoryPos = History.Size - 1;
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, History[HistoryPos]);
				data->SelectAll();
			} else if (data->EventKey == ImGuiKey_DownArrow) {
				if (++HistoryPos >= History.Size) HistoryPos = 0;
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, History[HistoryPos]);
				data->SelectAll();
			}
		}
		return 0;
	}

	// Insert into history. First find match and delete it so it can be pushed to the back.
	// This isn't trying to be smart or optimal.
	static void SaveToHistory(const char* command_line) {
		HistoryPos = -1;
		for (int i = History.Size - 1; i >= 0; i--)
			if (strcmp(History[i], command_line) == 0) {
				free(History[i]);
				History.erase(History.begin() + i);
				break;
		}
		History.push_back(strdup(command_line));
	}
	};

	ONCE {
		Funcs2::SaveToHistory("G10 L20 P1 X10");
		Funcs2::SaveToHistory("G10 L20 P1 X20");
	};

	static char buf2[64];
	static bool do_execute = false;
	static bool reclaim_focus = false;

	PushFont(mono, FS_SMALL);
	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
	Indent(8);
	SetNextItemWidth(GetContentRegionAvail().x-108-108-8);
	if (InputTextWithHint("##mdi2_command", ">>> MDI >>>", buf2, IM_ARRAYSIZE(buf2), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, Funcs2::MyCallback) || do_execute) {
		sendMdiAndSynch(buf2);
		Funcs2::SaveToHistory(buf2);
		strcpy(buf2, "");

		do_execute = false;
        	reclaim_focus = true;
		send_sync = true;
	}
	if (IsItemClicked()) show_osk = true;
//	bool input_focused = IsItemFocused();
	SetItemDefaultFocus();
	if (reclaim_focus) { reclaim_focus = false; SetKeyboardFocusHere(-1); }

	if (!can_mdi || !buf2[0]) BeginDisabled();
	SameLine();
	if (Button("Execute", ImVec2(100, 0))) do_execute = true;
	if (!can_mdi || !buf2[0]) EndDisabled();
	SameLine();
	if (Button("Abort", ImVec2(100, 0))) sendAbort();

/*
	{

		if (input_active && !show_osk) { //IsPopupOpen("##osk")) {
			show_osk = true;
//			reclaim_focus = true;
		}

		if (show_osk) {
			ImVec2 ws = {65*16+8, 65*5+8};
			SetNextWindowSize(ws);
//			SetNextWindowPos(GetCursorScreenPos());

			Begin("##osk2", &show_osk, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus);

//			ImVec2 wp = GetWindowPos();
//			if (wp.x < 0) wp.x = 0;
//			if (wp.y < 0) wp.y = 0;
//			SetWindowPos(wp);
//			SetWindowSize(ws);

			PushFont(font, FS_SMALL);
			PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
			SetCursorPos({8,8});
//			float key_size = (ws.x - 8) / 16.0f;
			float key_size = 65.0f;
			bool ex = false;
			if (osk(qwerty, key_size, key_size, 8)) {
fprintf(stderr, "CLOSE\n");
				ex = true;
			}
			PopStyleVar();
			PopFont();
			End();
			if (ex) show_osk = false;
		}
	}
*/
	PopStyleVar();
	PopFont();
}
