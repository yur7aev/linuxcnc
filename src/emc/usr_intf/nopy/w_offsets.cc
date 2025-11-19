#include "nopy.h"
#include "w_offsets.h"
#include "theme.h"
#include "osk.h"
#include <string>
#include <filesystem>
#include <iostream>
#include <sys/inotify.h>
#include <unistd.h>

#include "nopy.h"             // NML Messaging functions
#include "../shcom.hh"             // NML Messaging functions

using namespace ImGui;

extern std::string parameter_file;
extern std::string ini_dir;
extern bool can_mdi;

#define AXES 9

double g5x_offs[14][10];

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 8 * ( EVENT_SIZE + 16 ) )

void OffsEdit(const char *fmt, ...);

class FileWatch {
	std::string dir;
	std::string file;
	bool changed;
	bool watching;
	int fd, wd;
public:
	FileWatch(const std::string &s) {
		std::filesystem::path p = s;
		file = p.filename();
		p.remove_filename();
		dir = p;
		watching = false;
		changed = false;
	}

	~FileWatch() {
		if (wd >= 0) close(wd);
		if (fd >= 0) close(fd);
	}

	bool init(void) {
		fd = inotify_init1(IN_NONBLOCK);
		wd = inotify_add_watch(fd, dir.c_str(), IN_MODIFY | IN_MOVED_TO);
		watching = changed = fd >= 0 && wd >= 0;
		if (!watching) {
			if (wd >= 0) close(wd);
			if (fd >= 0) close(fd);
		}
		return changed;
	}

	bool poll(void) {
		char buf[BUF_LEN];
		int l;

		if (!watching) init();

		if (watching)
		do {
			l = read(fd, buf, BUF_LEN);

			for(int i = 0; i < l; ) {
				struct inotify_event *event = (struct inotify_event *)&buf[i];

				// fprintf(stderr, "NOTIFY %d %s\n", event->len, event->len ? event->name : "");

				if (~event->mask & IN_ISDIR && event->len > 0) {
					if (file == std::string(event->name))
						changed = true;
				}

				if (event->mask & IN_IGNORED) {
					printf( "The watch dir was moved/deleted\n");
					close(wd);
					close(fd);
					watching = false;
				}
				i += EVENT_SIZE + event->len;
			}
		} while (l > 0);

		return changed;
	}

	bool is_changed(void) {
		bool c = changed;
		changed = false;
		return c;
	}
};

void TextNZ(const char *fmt, double v)
{

	ImVec2 tl = GetCursorScreenPos() - ImVec2(12, 12);
	ImRect r(tl, tl + ImVec2(GetColumnWidth() + 24, 48));

	int row = TableGetRowIndex();
	int col = TableGetColumnIndex();

	ImGuiID id = GetCurrentWindow()->GetID(row*256 + col);
	PushID(id);

	static char ebuf[64];
	bool pressed = ButtonBehavior(r, id, NULL, NULL, ImGuiButtonFlags_PressedOnRelease);
	if (pressed) {
		//printf("pressed %d %d @ %d,%d\n", row, col, (int)tl.x, (int)tl.y);
		snprintf(ebuf, sizeof(ebuf), fmt, v);
	        OpenPopup("##qqq");
	}

	static ImVec4 clr = { 1.0f, 1.0f, 1.0f, 1.0f };		// TODO: move to theme
	static ImVec4 zer = { 1.0f, 1.0f, 1.0f, 0.3f };

	TextColored(v == 0.0f ? zer : clr, fmt, v);

	////////////////

	const float button_size = 64;
	SetNextWindowSize(ImVec2(button_size*4 + 8, button_size*5 + 48 + 8*2));
	SetNextWindowPos(tl - ImVec2(8, 8));
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	if (BeginPopupModal("##qqq", NULL, ImGuiWindowFlags_NoDecoration)) {
		if (pressed) SetKeyboardFocusHere();
		SetNextItemWidth(button_size*4 - 8);
		if (InputText("##ti", ebuf, IM_ARRAYSIZE(ebuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
			static char cbuf[80];
			cbuf[0] = 0;
			// printf("edited %d %d @ %d,%d\n", row, col, (int)tl.x, (int)tl.y);
			if (row >= 1 && row <= 9 && col >= 1 && col <= AXES+1) {
				snprintf(cbuf, sizeof(cbuf), "G10 L2 P%d %s[%s]", row, axis_name[col-1], ebuf);
			} else if (row == 10) {
				if (col >= 1 && col <= AXES) {	// G92
					snprintf(cbuf, sizeof(cbuf), "G92 %s[%s]", axis_name[col-1], ebuf);
				} else if (col == AXES+1) {
					int val = atoi(ebuf);
					snprintf(cbuf, sizeof(cbuf), "G92.%d", val ? 3 : 2);
				}
			}
			if (cbuf[0]) {
				sendMdiAndSynch(cbuf);
				send_sync = true;
			}

			CloseCurrentPopup();
		}
		PushFont(mono, FS_MEDIUM);
		if (osk(numpad, button_size, button_size, 8)) CloseCurrentPopup();
		PopFont();
		EndPopup();
	}
	PopStyleVar();
	PopID();
}


void w_offsets(void)
{
	const char *id = "##offsets";

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));	// use full width for the table
	BeginChild(id, {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_AlwaysUseWindowPadding, 0);
	PopStyleVar();
	
	static bool init = false;
	static FileWatch *watch;

	if (!init) {
		watch = new FileWatch(ini_dir + "/" + parameter_file);
		// std::cerr << p << ": " << "d:" << dir << " f:" << file << std::endl;
		init = true;

	}

	watch->poll();

	if (watch->is_changed()) {	// re-read the interpreter parameters
		
		fprintf(stderr, "reread...\n");

		FILE *f = fopen((ini_dir + "/" + parameter_file).c_str(), "r");
		if (f) {
			while (!feof(f)) {
				int no;
				double val;
				if (fscanf(f, "%d %lf", &no, &val) == 2) {
					// 5221-5230	Coordinate System 2, G54 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
					// 5241-5250	Coordinate System 2, G55 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
					// 5261-5270	Coordinate System 3, G56 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
					// 5281-5290	Coordinate System 4, G57 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
					// 5301-5310	Coordinate System 5, G58 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
					// 5321-5330	Coordinate System 6, G59 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
					// 5341-5350	Coordinate System 7, G59.1 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
					// 5361-5370	Coordinate System 8, G59.2 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
					// 5381-5390	Coordinate System 9, G59.3 for X, Y, Z, A, B, C, U, V, W & R. Persistent.
					if (no >= 5221 && no <= 5390) {
						int n = (no - 5221) / 20 + 1;
						int i = (no - 5221) % 20;
						if (i < 10) g5x_offs[n][i] = val;
					}
					// 5211-5219	"G92" offset for X, Y, Z, A, B, C, U, V & W. Persistent.
					if (no >= 5211 && no <= 5219) { // g92
						int n = 10;
						int i = no - 5211;
						g5x_offs[n][i] = val;
					}
					// 5210	1 if "G92" offset is currently applied, 0 otherwise. Persistent.
					if (no == 5210) g5x_offs[10][9] = val;

					// 5161-5169 - "G28" Home for X, Y, Z, A, B, C, U, V & W. Persistent.
//					if (no >= 5161 && no <= 5169) {
					if (no >= 5161 && no <= 5169) {
						int n = 11;
						int i = no - 5161;
						g5x_offs[n][i] = val;
					}
					// 5181-5189 - "G30" Home for X, Y, Z, A, B, C, U, V & W. Persistent.
					if (no >= 5181 && no <= 5189) {
						int n = 12;
						int i = no - 5181;
						g5x_offs[n][i] = val;
					}
				}
			}
			fclose(f);
		} else {
			fprintf(stderr, "can't open %s\n", parameter_file.c_str());
		}
	}

	static float column_width = 0.0f;
	ONCE {
		PushFont(mono, 0);
		column_width = CalcTextSize("-9999.999").x + 16;
		PopFont();
	}

	int i =  emcStatus->task.g5x_index;
	for (int j = 0; j < 9; ++j) {
		g5x_offs[0][j] = emcStatus->task.toolOffset.coor[j];
		g5x_offs[i][j] = emcStatus->task.g5x_offset.coor[j];
//		g5x_offs[10][j] = emcStatus->task.g92_offset.coor[j];
	}

	if (!init) {
		Text("No offset data");
		return;
	}

	ImGuiStyle &style = GetStyle();

	static ImGuiTableFlags flags =
		  ImGuiTableFlags_ScrollY 
		| ImGuiTableFlags_RowBg
		| ImGuiTableFlags_BordersV
		| ImGuiTableFlags_Resizable
		| ImGuiTableFlags_Sortable
		| ImGuiTableFlags_SortTristate
		| ImGuiTableFlags_NoBordersInBody
		| ImGuiTableFlags_Hideable;

        if (BeginTable("##table_offsets", AXES+3, flags, {-FLT_MIN, -FLT_MIN})) {
		// PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(12, 8));
		GImGui->CurrentTable->DisableDefaultContextMenu = true;
		TableSetupScrollFreeze(0, 1); // Make top row always visible
		TableSetupColumn("CS", ImGuiTableColumnFlags_WidthFixed);
		for (int a = 0; a < IM_ARRAYSIZE(axis_name); ++a) {
			TableSetupColumn(axis_name[a], ImGuiTableColumnFlags_WidthFixed, column_width);
			if (a >= 0 && a < 9 && !(emcStatus->motion.traj.axis_mask & 1<<a))
				TableSetColumnEnabled(a+1, false);
		}
		TableSetupColumn("Comment", ImGuiTableColumnFlags_None);
		TableHeadersRow();
#define FMT "%.3f"
		PushFont(mono, 0);

		for (int i = 0; i < 13; i++) {
			TableNextRow();
			bool is_current_wcs = (i == emcStatus->task.g5x_index);
			if (i == 10 && g5x_offs[10][9] > 0) is_current_wcs = true;	// G92?
			TableSetColumnIndex(0);
			Text(offs_name[i]);
			if (is_current_wcs) TableSetBgColor(ImGuiTableBgTarget_CellBg, cell_hl_color);
			for (int a = 0; a < IM_ARRAYSIZE(axis_name); ++a) {
				TableSetColumnIndex(a + 1);
				if (g5x_offs[i][a] == 0.0f) PushStyleColor(ImGuiCol_Text, color_grey);
				if (i >= 1 && i <= 10 || a < IM_ARRAYSIZE(axis_name)-1)
					OffsEdit(FMT, g5x_offs[i][a]);
//				} else if (a < IM_ARRAYSIZE(axis_name) - 1) {
//					Text(FMT, g5x_offs[i][a]);
//				}
				if (g5x_offs[i][a] == 0.0f) PopStyleColor();
			}
                }
		PopFont();

		static Scroller scroller;
		scroller.scroll();

		EndTable();
        }

	EndChild();
}


void offs_save(int row, int col, const char *osk_buf)
{
	static char cbuf[81];
fprintf(stderr, "offs %d,%d %s\n", row, col, osk_buf);
	cbuf[0] = 0;
	if (row >= 1 && row <= 9 && col >= 1 && col <= AXES+1) {
		snprintf(cbuf, sizeof(cbuf), "G10 L2 P%d %s[%s]", row, axis_name[col-1], osk_buf);
	} else if (row == 10) {	// G92
		if (col >= 1 && col <= AXES) {
			snprintf(cbuf, sizeof(cbuf), "#%d=%s", 5210+col, osk_buf);
		} else if (col == AXES+1) {
			int val = atoi(osk_buf);
			snprintf(cbuf, sizeof(cbuf), "G92.%d", val ? 3 : 2);
		}
	} else if (row == 11) {	// G28
		if (col >= 1 && col <= AXES) {
			snprintf(cbuf, sizeof(cbuf), "#%d=%s", 5160+col, osk_buf);
		}
	} else if (row == 12) {	// G30
		if (col >= 1 && col <= AXES) {
			snprintf(cbuf, sizeof(cbuf), "#%d=%s", 5180+col, osk_buf);
		}
	} else if (row == 0) {	// G30
		if (col >= 1 && col <= AXES) {
			snprintf(cbuf, sizeof(cbuf), "G43.1 %s[%s]", axis_name[col-1], osk_buf);
		}
	}

	if (cbuf[0]) {
		sendMdiAndSynch(cbuf);
		send_sync = true;
	}
}

void OffsEdit(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if (can_mdi) {
		CellEdit(offs_save, fmt, args);
	} else {
		TextV(fmt, args);
	}
	va_end(args);
}
