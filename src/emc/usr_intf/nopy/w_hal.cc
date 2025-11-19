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

#include <hal.h>

#include "rtapi.h"		// RTAPI realtime OS API
#include <rtapi_mutex.h>
#include <rtapi_string.h>	// rtapi_strlcpy()
#include "hal.h"		// HAL public API decls
#include "../../../hal/hal_priv.h"	// private HAL decls

#include <vector>

extern bool show_osk;

using namespace ImGui;

struct HalPin {
	HalPin(SHMFIELD(hal_pin_t) p, int i) :
		pin(p), no(i), selected(false), watching(false)
	{
		name = std::string(p->name);
	}
	std::string name;
	SHMFIELD(hal_pin_t) pin;
	int no;
	bool selected;
	bool watching;
};


static const char *data_val2(int type, void *valptr)
{
    const char *value_str;
    static char buf[15];

    switch (type) {
    case HAL_BIT:
	if (*((char *) valptr) == 0)
	    value_str = "FALSE";
	else
	    value_str = "TRUE";
	break;
    case HAL_FLOAT:
	snprintf(buf, 14, "%.7g", (double)*((hal_float_t *) valptr));
	value_str = buf;
	break;
    case HAL_S32:
	snprintf(buf, 14, "%ld", (long)*((hal_s32_t *) valptr));
	value_str = buf;
	break;
    case HAL_U32:
	snprintf(buf, 14, "%ld", (unsigned long)*((hal_u32_t *) valptr));
	value_str = buf;
	break;
    case HAL_PORT:
	snprintf(buf, 14, "%u", hal_port_buffer_size(*((hal_port_t*) valptr)));
	value_str = buf;
	break;

    default:
	/* Shouldn't get here, but just in case... */
	value_str = "unknown_type";
    }
    return value_str;
}

static std::vector<HalPin> pins;
static std::vector<HalPin> watch;

void fill_pins(void)
{
	pins.clear();
	rtapi_mutex_get(&hal_data->mutex);
	int i = 0;
	for (SHMFIELD(hal_pin_t) pin = hal_data->pin_list_ptr; pin != 0; pin = pin->next_ptr) {
		pins.push_back(HalPin(pin, i++));
	}
	rtapi_mutex_give(&hal_data->mutex);
}

static std::vector<std::string> incl, excl;

void make_filters(const char *filter)
{
	incl.clear();
	excl.clear();
	const char *s = filter;
	const char *e;
	while(*s) {
		while(*s && *s == ' ') ++s;
		for (e = s; *e && *e != ' '; ++e);
		bool inv = *s == '!' || *s == '/';
		if (inv) ++s;
		if (*s && s != e) (inv ? excl : incl).push_back(std::string(s, e-s));
		s = e;
	}
}

void w_hal(void)
{
	ONCE {
		fill_pins();
	}

	ImVec2 r = GetContentRegionAvail();

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));	// use full width for the table
	BeginChild("##hal", {-FLT_MIN, -FLT_MIN}, 0, 0);
	PopStyleVar();

	static char filter[80];
	static bool selector = true;
	
	if (!selector) {
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		for (auto i = watch.begin(); i != watch.end(); ++i) {
			Text("%s : ", i->name.c_str());

			SameLine();

			if (i->pin) {
				void *d_ptr;
				if (i->pin->signal != 0) {
					hal_sig_t *sig = SHMPTR(i->pin->signal);
					d_ptr = SHMPTR(sig->data_ptr);
				} else {
					d_ptr = &(i->pin->dummysig);
				}
				Text("%s", data_val2((int)i->pin->type, d_ptr));
			}
		}
		PopStyleVar();
	} else {

	static int selected_right = 0;

	const ImGuiTableFlags table_flags =
		  ImGuiTableFlags_ScrollY 
		| ImGuiTableFlags_BordersV
		| ImGuiTableFlags_Resizable
		| ImGuiTableFlags_NoBordersInBody
		;

	const ImGuiSelectableFlags selectable_flags =
		  ImGuiSelectableFlags_SpanAllColumns
		| ImGuiSelectableFlags_AllowOverlap
		| ImGuiSelectableFlags_NoPadWithHalfSpacing
		;

	static bool show_filter;
	static bool refocus = false;

        if (BeginTable("##table_pins", 3, table_flags, {(r.x-48-16)/2, -9})) {
		GImGui->CurrentTable->DisableDefaultContextMenu = true;
		TableSetupScrollFreeze(0, 1); // Make top row always visible
		TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed);
		TableSetupColumn("Type", ImGuiTableColumnFlags_None);
		TableSetupColumn("Dir", ImGuiTableColumnFlags_None);

		if (!show_filter) {
			TableSetBgColor(ImGuiTableBgTarget_RowBg0, GetColorU32(ImGuiCol_TableHeaderBg));
			if (TableHeadersRow()) refocus = show_filter = true;
		} else {
			PushStyleVar(ImGuiStyleVar_CellPadding, {0, 0});

			const float row_height = TableGetHeaderRowHeight();
			TableNextRow(ImGuiTableRowFlags_Headers, row_height);
			TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32_DISABLE);	// should be here!
			PopStyleVar();	// CellPadding 0,0

			TableSetColumnIndex(0);
			SetNextItemWidth(-48);
			if (refocus) {
				SetKeyboardFocusHere();
				show_osk = true;
			}
			if (InputTextWithHint("##filter", ">>> Filter", filter, IM_ARRAYSIZE(filter)) || refocus) {
				make_filters(filter);
				refocus = false;
			}
			if (IsItemClicked()) show_osk = true;
			SameLine();
			if (Button("\u0378", {48,48})) {
				show_filter = false;
				incl.clear();
				excl.clear();
			}

			ImGuiContext& g = *GImGui;
			TableNextColumn(); 
			TableSetBgColor(ImGuiTableBgTarget_CellBg, GetColorU32(ImGuiCol_TableHeaderBg), -1);
			g.CurrentWindow->DC.CursorPos.y += 12;
			TableHeader("Type");
			TableNextColumn();
			TableSetBgColor(ImGuiTableBgTarget_CellBg, GetColorU32(ImGuiCol_TableHeaderBg), -1);
			g.CurrentWindow->DC.CursorPos.y += 12;
			TableHeader("Dir");
		}
#define FMT "%.3f"
//		PushFont(mono, 0);

		for (unsigned int i = 0; i < pins.size(); i++) {
			if (pins[i].watching) skip: continue;
			for (unsigned f = 0; f < incl.size(); ++f)
				if (pins[i].name.find(incl[f]) == std::string::npos) goto skip;
			for (unsigned f = 0; f < excl.size(); ++f)
				if (pins[i].name.find(excl[f]) != std::string::npos) goto skip;

			TableNextRow();
			TableNextColumn();//SetColumnIndex(0);

			if (Selectable(pins[i].name.c_str(), false, selectable_flags)) {
				if (selected_right <= 0) {
					watch.push_back(pins[i]);
				} else {
					for (auto j = watch.begin(); j != watch.end(); ++j) {
						if (j->selected) {
							pins[j->no].watching = false;
							*j = pins[i];
							--selected_right;
							break;
						}
					}
				}
				pins[i].watching = true;
			}

			const char *haltype[] = { "?", "bit", "float", "s32", "u32", "port" };
			int t = pins[i].pin->type;
			if (t < 0 || t >= IM_ARRAYSIZE(haltype)) t = 0;
			TableNextColumn();
			Text(haltype[t]);

			const int d = pins[i].pin->dir;
			const char *dir =	d == HAL_IN ? "in" :
						d == HAL_OUT ? "out" :
						d == HAL_IO ? "io" :
						"?";
			TableNextColumn();
			Text(dir);
                }
//		PopFont();

		static Scroller scroller;
		scroller.scroll();
		EndTable();
        }

	SameLine();
	BeginGroup();
	bool d;

	d = selected_right <= 0;
	if (d) BeginDisabled();
	if (Button("<<", {48, 48})) {
		for (auto i = watch.begin(); i != watch.end();) {
			if (i->selected) {
				pins[i->no].watching = false;
				i = watch.erase(i);
			} else {
				++i;
			}
		}
		selected_right = 0;
	}
	if (Button("Up", {48, 48})) {
		for (auto i = watch.begin(); i != watch.end(); ++i) {
			if (i->selected) {
				if (i == watch.begin()) break;
				std::swap(i[-1], i[0]);
			}
		}
	}
	if (Button("Dn", {48, 48})) {
		for (auto i = watch.rbegin(); i != watch.rend(); ++i) {
			if (i->selected) {
				if (i == watch.rbegin()) break;
				std::swap(i[-1], i[0]);
			}
		}
	}
	if (d) EndDisabled();

	d = watch.size() <= 0;
	if (d) BeginDisabled();
	if (Button("X", {48, 48})) {
		for (auto i = watch.begin(); i != watch.end();) {
			pins[i->no].watching = false;
			i = watch.erase(i);
		}
		selected_right = 0;
	}
	if (d) EndDisabled();



	if(Button("\u2230", {48,48})) //DOT " " DOT " " DOT))
		if ((show_filter = !show_filter)) refocus = true;



	EndGroup();

	SameLine();
        if (BeginTable("##watch_pins_show", 3, table_flags, {(r.x-48-16)/2, -9})) {
		GImGui->CurrentTable->DisableDefaultContextMenu = true;
		TableSetupScrollFreeze(0, 1); // Make top row always visible
		TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, 300);
		TableSetupColumn("Type", ImGuiTableColumnFlags_None);
		TableSetupColumn("Dir", ImGuiTableColumnFlags_None);
		TableHeadersRow();

		for (unsigned int i = 0; i < watch.size(); i++) {
			TableNextRow();
			TableSetColumnIndex(0);

			if (Selectable(watch[i].name.c_str(), watch[i].selected, selectable_flags)) {
				if ((watch[i].selected = !watch[i].selected))
					++selected_right;
				else
					--selected_right;
			}

/*
		if (IsItemActive() && !IsItemHovered()) {
			unsigned int n = i + (GetMouseDragDelta(0).y < 0.f ? -1 : 1);
			if (n >= 0 && n < watch.size()) {
				std::swap(watch[i], watch[n]);
				ResetMouseDragDelta();
			}
		}
*/

			const char *haltype[] = { "?", "bit", "float", "s32", "u32", "port" };
			int t = watch[i].pin->type;
			if (t < 0 || t >= IM_ARRAYSIZE(haltype)) t = 0;
			TableSetColumnIndex(1);
			Text(haltype[t]);

			const int d = watch[i].pin->dir;
			const char *dir =	d == HAL_IN ? "in" :
						d == HAL_OUT ? "out" :
						d == HAL_IO ? "io" :
						"?";
			TableSetColumnIndex(2);
			Text(dir);
                }


		static Scroller scroller;
		scroller.scroll();
		EndTable();
        }
}

	EndChild();

	ImVec2 p = GetCursorPos();
	ImVec2 q(GetWindowWidth() - 100, GetWindowHeight() - 80);
	ImVec2 s(48, 48);
	SetCursorPos(q);
	if (BeginChild("##buttons", s, 0)) {
		if(Button(DOT " " DOT " " DOT))
			selector = !selector;
	}
	EndChild();
	SetCursorPos(p); Dummy({1,1});


}
