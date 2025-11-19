#include <stdarg.h>
#include "nopy.h"
#include "w_tools.h"
#include "osk.h"
#include "theme.h"
#include "tooldata.hh"
#include "../shcom.hh"             // NML Messaging functions
#include <algorithm>
#include <map>

/*  TODO
+fetch all tools, check for duplicates
+sort

search?
load, save
*/

using namespace ImGui;


void TTEdit(const char *fmt, ...);
bool was_dragging = false;
static char osk_buf[81];

std::vector<struct CANON_TOOL_TABLE> tt;
std::vector<int> ti;
std::map<int, int> tdup;
int tt_change = -1;
int idx_last = -1;
bool is_random_tc = false;
bool resort = false;


void fill_tt()
{	
	int change = tooldata_change_get();
	if (change == tt_change) return;
	idx_last = tooldata_last_index_get();
	tt.clear();
	ti.clear();
	tdup.clear();
	for (int i = 0; i < idx_last; ++i) {
		static struct CANON_TOOL_TABLE tdata;
		if (tooldata_get(&tdata, i+1) != IDX_OK) continue;
		tt.push_back(tdata);
		ti.push_back(i);

		int no = tdata.toolno;
		if (no <= 0) {
			tdup[no] = -1;
		} else {
			auto e = tdup.find(tdata.toolno);
			if (e != tdup.end())
				++e->second;
			else
				tdup[tdata.toolno] = 0;
		}
	}
	is_random_tc = tool_mmap_is_random_toolchanger();
	tt_change = change;
	resort = true;
	// fprintf(stderr, "tt fill\n");
}

static const ImGuiTableSortSpecs* s_current_sort_specs;
static bool CompareWithSortSpecs(int a, int b)
{
	for (int n = 0; n < s_current_sort_specs->SpecsCount; n++) {
		const ImGuiTableColumnSortSpecs* sort_spec = &s_current_sort_specs->Specs[n];
		double delta = 0;
		switch (sort_spec->ColumnIndex)
		{
		case  0: delta = tt[a].toolno - tt[b].toolno; break;
		case  1: delta = tt[a].pocketno - tt[b].pocketno; break;
		case  2: delta = tt[a].offset.coor[0] - tt[b].offset.coor[0]; break;
		case  3: delta = tt[a].offset.coor[1] - tt[b].offset.coor[1]; break;
		case  4: delta = tt[a].offset.coor[2] - tt[b].offset.coor[2]; break;
		case  5: delta = tt[a].offset.coor[3] - tt[b].offset.coor[3]; break;
		case  6: delta = tt[a].offset.coor[4] - tt[b].offset.coor[4]; break;
		case  7: delta = tt[a].offset.coor[5] - tt[b].offset.coor[5]; break;
		case  8: delta = tt[a].offset.coor[6] - tt[b].offset.coor[6]; break;
		case  9: delta = tt[a].offset.coor[7] - tt[b].offset.coor[7]; break;
		case 10: delta = tt[a].offset.coor[8] - tt[b].offset.coor[8]; break;
		case 11: delta = tt[a].diameter - tt[b].diameter; break;
		case 12: //delta = tt[a].frontangle - tt[b].frontangle; break;
		case 13: //delta = tt[a].backangle - tt[b].backangle; break;
		case 14: //delta = tt[a].orientation - tt[b].orientation; break;
		case 15: delta = strcmp(tt[a].comment, tt[b].comment); break;
		default: IM_ASSERT(0); break;
		}
		if (delta < 0) return (sort_spec->SortDirection == ImGuiSortDirection_Ascending);
		if (delta > 0) return !(sort_spec->SortDirection == ImGuiSortDirection_Ascending);
	}
	return a < b;
}

static void SortWithSortSpecs(ImGuiTableSortSpecs* sort_specs)
{
	s_current_sort_specs = sort_specs; // Store in variable accessible by the sort function.
	if (ti.size() > 1)
		std::sort(ti.begin(), ti.end(), CompareWithSortSpecs);
}


void w_tools(void)
{
	const char *id = "##tools";
	static bool init = false;
	static float column_width = 0.0f;

	ONCE {
//		if (!tool_mmap_user()) 
		init = true;
		PushFont(mono, 0);
		column_width = CalcTextSize("-9999.999").x + 0;
		PopFont();
	}

	if (!init) {
		Text("No tool data");
		return;
	}

	fill_tt();


	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	BeginChild(id, {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_AlwaysUseWindowPadding, 0);
	PopStyleVar();

	static ImGuiTableFlags flags =
		  ImGuiTableFlags_ScrollY 
		| ImGuiTableFlags_RowBg
		| ImGuiTableFlags_BordersV
		| ImGuiTableFlags_Resizable
		| ImGuiTableFlags_Sortable
		| ImGuiTableFlags_SortTristate
		| ImGuiTableFlags_NoBordersInBody
		| ImGuiTableFlags_Hideable;
	const ImGuiTableColumnFlags cf = ImGuiTableColumnFlags_NoSortDescending | ImGuiTableColumnFlags_WidthFixed;

	if (BeginTable("table_tools", AXES + 4, flags, {-FLT_MIN, -FLT_MIN})) {
		TableSetupScrollFreeze(0, 1); // Make top row always visible
		TableSetupColumn("Tool", cf);
		TableSetupColumn("Pocket", cf);
		for (int a = 0; a < AXES; ++a) {
			TableSetupColumn(axis_name[a], cf, column_width);
			if (a >= 0 && a < AXES && !(emcStatus->motion.traj.axis_mask & 1<<a))
				TableSetColumnEnabled(a+2, false);
		}
		TableSetupColumn("d", cf, column_width);
		TableSetupColumn("Comment", cf & ~ImGuiTableColumnFlags_WidthFixed);
		TableHeadersRow();


            // Sort our data if sort specs have been changed!
		if (ImGuiTableSortSpecs* sort_specs = TableGetSortSpecs())
			if (sort_specs->SpecsDirty || resort) {
				SortWithSortSpecs(sort_specs);
				sort_specs->SpecsDirty = resort = false;
			}

		PushFont(mono, 0);

		ImGuiListClipper clipper;
		clipper.Begin(tt.size());
		while (clipper.Step()) {
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
				const struct CANON_TOOL_TABLE &tdata = tt[ti[row]];
				int tool = emcStatus->io.tool.toolInSpindle;
				int prep = emcStatus->io.tool.pocketPrepped;	// actually tt slot, not a pocket

				TableNextRow();
				TableSetColumnIndex(0);

				bool dup = tdup[tdata.toolno] != 0;
				if (dup) PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3, 0.2, 1));
//				MoveCursorX(8); 
				TTEdit("%5.0f", (double)tdata.toolno);
				if (dup) PopStyleColor();
				if (tool == tdata.toolno) TableSetBgColor(ImGuiTableBgTarget_CellBg, cell_hl_color);
//				if (tdup[tdata.toolno] > 0) TableSetBgColor(ImGuiTableBgTarget_CellBg, cell_err_color);

				TableNextColumn();
				TTEdit("%5.0f", (double)tdata.pocketno);
				if (prep == row + 1) TableSetBgColor(ImGuiTableBgTarget_CellBg, cell_hl_color);

				for (int a = 0; a < AXES; ++a) {
					TableNextColumn();
					double v = ((double*)&tdata.offset.tran.x)[a];
					if (v == 0.0) PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.3));
					TTEdit("%.3f", v);
					if (v == 0.0) PopStyleColor();
				}

				TableNextColumn();
				TTEdit("%.3f", tdata.diameter);

				TableNextColumn();
				TTEdit("%s", tdata.comment);
			}
		}

		// footer

		TableNextRow();
		TableSetColumnIndex(2);
		if (Button("New", {column_width,48})) {
			if (tooldata_put(tooldata_entry_init(), tooldata_last_index_get() + 1) != IDX_OK) {
				fprintf(stderr, "can't put tdata\n");
			}
			TableNextRow();
			TableSetColumnIndex(0);
			Text("");
			SetScrollY(GetScrollY() + 48);
		}
		TableSetColumnIndex(3);
		SetNextItemWidth(100);
		if (Button("Purge", {column_width,48}))
			sendLoadToolTable(tool_table.c_str());
		TableSetColumnIndex(4);
		SetNextItemWidth(100);
		if (Button("Load", {column_width,48}))
			sendLoadToolTable(tool_table.c_str());
		TableSetColumnIndex(AXES+2);
		SetNextItemWidth(100);
		if (Button("Save", {column_width,48}))
			sendLoadToolTable(tool_table.c_str());

		PopFont();

		static Scroller scroller;
		scroller.scroll();

		EndTable();
	}
	EndChild();
}

void tt_save(int row, int col, const char *osk_buf)
{
	if (row < 0 || row >= ti.size()) return;
	static struct CANON_TOOL_TABLE tdata;
	if (tooldata_get(&tdata, ti[row]+1) == IDX_OK) {
		const double v = atof(osk_buf);
		switch (col) {
		case  0: tdata.toolno = v; break;
		case  1: tdata.pocketno = v; break;
		case  2: tdata.offset.tran.x = v; break;
		case  3: tdata.offset.tran.y = v; break;
		case  4: tdata.offset.tran.z = v; break;
		case  5: tdata.offset.a = v; break;
		case  6: tdata.offset.b = v; break;
		case  7: tdata.offset.c = v; break;
		case  8: tdata.offset.u = v; break;
		case  9: tdata.offset.v = v; break;
		case 10: tdata.offset.w = v; break;
		case 11: tdata.diameter = v; break;
		//case 12: tdata.frontangle = v; break;
		//case 13: tdata.backangle = v; break;
		//case 14: tdata.orientation = v; break;
		case 12: strncpy(tdata.comment, osk_buf, sizeof(tdata.comment) - 1); break;
		}
		if (tooldata_put(tdata, ti[row]+1) != IDX_OK)
			fprintf(stderr, "can't put tdata\n");
		sendToolSetOffset(1, 1, 1);	// signal update, doesn't work with shared mem
	} else {
		fprintf(stderr, "can't get tdata\n");
	}
}

void TTEdit(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	CellEdit(tt_save, fmt, args);
//	TextV(fmt, args);
	va_end(args);
}
