#include <iostream>

#include "nopy.h"
#include "w_dro.h"
#include "../shcom.hh"             // NML Messaging functions
#include "osk.h"
#include "msg_log.h"

#include "TextEditor.h"

extern bool show_file_browser;
extern bool interpreter_is_running;
void file_browser(void);


void w_gcode(void)
{
	static char *gcode = NULL;
	static size_t gcode_size = 1;
	static char ngc_file[LINELEN] = "";
	static bool show_editor = false;

	ONCE {
		gcode = new char[8000];
		strcpy(gcode, "12345");
		gcode_size = 8000;
		fprintf(stderr, "buf %s\n", gcode);
	}

	if (show_file_browser) {
		file_browser();
		return;
	}

	static std::vector<char *> lines;

	// todo: monitor file changes
	if (strcmp(ngc_file, emcStatus->task.file)) {
		FILE *f = fopen(emcStatus->task.file, "r");
		lines.clear();
		if (f) {
			fseek(f, 0, SEEK_END);
			size_t fsize = ftell(f);
			fseek(f, 0, SEEK_SET);

			delete gcode;		
			gcode = new char [fsize + 1];
			fread(gcode, fsize, 1, f);
			fclose(f);

			gcode[fsize] = 0;
			gcode_size = fsize + 1;
			strncpy(ngc_file, emcStatus->task.file, sizeof(ngc_file));
			fprintf(stderr, "open %s\n", emcStatus->task.file);

			char *s = gcode;
			lines.push_back(s);
			while (*s) {
				if (*s == '\n') {
					*s = 0;
					lines.push_back(s+1);
				}
				++s;
			}
fprintf(stderr, "clear id\n");
			ClearActiveID();
			SetWindowFocus(NULL);

		} else {
			fprintf(stderr, "can't open %s\n", emcStatus->task.file);
		}
	}

	const char *fname = strrchr(emcStatus->task.file, '/');

	PushFont(mono, FS_SMALL);
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 0));
	PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

	static TextEditor editor;

BeginChild("##editor", {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_FrameStyle);
	if (show_editor) {
//	SetNextWindowFocus();
		editor.Render("TextEditor");
	if (IsItemClicked()) show_osk = true;
//		static Scroller scroller;
//		scroller.scroll();
	} else {
//	if (BeginChild("##viewer", {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_FrameStyle)) {
		static int prev_line = -1;

		if (interpreter_is_running)
			SetScrollY((emcStatus->task.motionLine - 5) * FS_SMALL);
/*
		if (Button(" Open ")) {
			show_file_browser = true;
		}
		SameLine();
		Text("File: %s", fname ? fname+1 : emcStatus->task.file);
*/

		ImGuiWindow *w = GetCurrentWindow();
                ImGuiListClipper clipper;
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		unsigned nlines = lines.size();
                clipper.Begin(nlines);
//                if (ms_io->RangeSrcItem != -1)
//                    clipper.IncludeItemByIndex((int)ms_io->RangeSrcItem); // Ensure RangeSrc item is not clipped.
                while (clipper.Step()) {
                    for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; n++) {
			static bool item_is_selected;
			static char lineno[16];
			const char *fmt = nlines < 9 ? "%1d " :
					  nlines < 99 ? "%2d " :
					  nlines < 999 ? "%3d " :
					  nlines < 9999 ? "%4d " :
					  nlines < 99999 ? "%5d " : "%6d ";
			sprintf(lineno, fmt, n+1);
			PushStyleColor(ImGuiCol_Text, color_grey);
			Selectable(lineno, &item_is_selected);
			PopStyleColor();
//			TextColored(ImVec4(1, 1, 1, 0.3), "%3d ", n+1);
			SameLine();
			if (prev_line == n+1) {
				ImVec2 cur = w->DC.CursorPos;
				ImRect r(0, cur.y, 10000, cur.y + FS_SMALL);
				w->DrawList->AddRectFilled(r.Min, r.Max, cell_hl_color);
			}
			Text(lines[n]);
                    }
                }
		prev_line = emcStatus->task.motionLine;
		PopStyleVar();

	}
	static Scroller scroller;
	scroller.scroll();
EndChild();
	PopStyleVar();
	PopFont();

#define DOT "\u00b7"
	ImVec2 p = GetCursorPos();
	ImVec2 q(GetWindowWidth() - 48, GetWindowHeight() - 48);
	ImVec2 s(148+8, 48);
	static bool show_buttons = false;
	if (show_buttons) s = ImVec2(148+8, 48*3 + 8*2);
	q -= s;
	SetCursorPos(q);
	if (BeginChild("##buttons", s, ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY)) {
		if (show_buttons) {
			const char *label = show_editor ? (editor.IsTextChanged() ? "Save" : "View") : "Edit";
			if (Button(label, {100, 48})) {
				if (show_editor) {
					if (editor.IsTextChanged()) {
						std::string text = editor.GetText();
					}
					show_editor = false;
				} else {
					editor.SetTextLines(lines);
					show_editor = true;
	//					show_buttons = false;
					TextEditor::ErrorMarkers err;
					err[10] = "Error 10";
					err[20] = "Error 30";
					err[30] = "Error 30";
					editor.SetErrorMarkers(err);
					TextEditor::Breakpoints brk;
					brk.insert(1);
					brk.insert(2);
					editor.SetBreakpoints(brk);
				}
			}

			if (Button("Save As", {100, 48})) {
				show_file_browser = true;
				show_buttons = false;
			}
//			Dummy({100-48-8, 0}); SameLine();
		} {
			if (Button("Load", {100, 48})) {
				show_file_browser = true;
				show_buttons = false;
			}
		}
		SameLine(); if(Button(DOT " " DOT " " DOT, {48, 48})) show_buttons = !show_buttons;
	}
	EndChild();
	SetCursorPos(p);

	PopStyleVar();
}
