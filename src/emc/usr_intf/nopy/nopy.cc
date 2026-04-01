#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <math.h>           // sqrtf, powf, cosf, sinf, floorf, ceilf
#include <dirent.h>
#include <stack>

//// linuxcnc

#include "rcs.hh"
#include "posemath.h"		// PM_POSE, TO_RAD
#include "emc.hh"		// EMC NML
#include "canon.hh"		// CANON_UNITS, CANON_UNITS_INCHES,MM,CM
#include "emcglb.h"		// EMC_NMLFILE, TRAJ_MAX_VELOCITY, etc.
#include "emccfg.h"		// DEFAULT_TRAJ_MAX_VELOCITY
#include "inifile.hh"		// INIFILE
#include "rcs_print.hh"
#include "timer.hh"             // etime()
#include "../shcom.hh"             // NML Messaging functions
#include <rtapi_string.h>	// rtapi_strlcpy()
#include "tooldata.hh"

#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include "nopy.h"
#include "theme.h"
#include "widgets.h"
#include "osk.h"
#include "glfb.h"
#include "w_dro.h"
#include "w_tools.h"
#include "w_offsets.h"
#include "w_3d.h"
#include "w_gcode.h"
#include "w_mdi.h"
#include "w_manual.h"
#include "w_hal.h"
#include "msg_log.h"
#include "interp.h"

extern "C" {
#include "trackball.h"
#include "glutstroke.h"
#include "hal.h"
void glutStrokeCharacter(int c);
}

////

using namespace ImGui;

std::string parameter_file;
std::string tool_table;
std::string ini_dir;
std::string ini_filename;
double min_feed_override;
double max_feed_override;
double min_spindle_override;
double max_spindle_override;
double max_linear_velocity;
bool blink;

int comp_id;

const char *axis_name[AXES+1] = { "X", "Y", "Z", "A", "B", "C", "U", "V", "W", "R" };
const char *offs_name[14] = { "T", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3", "G92", "G28", "G30" };

bool show_file_browser = false;

bool can_jog = false;
bool can_mdi = false;
bool can_spindle = false;
bool can_change_speed = false;

bool can_set_offs = false;
bool can_edit_tooltable = false;
bool can_set_override = true;
bool interpreter_is_running = false;

char current_time_string[32];
time_t current_time;

bool clear_backplot = false;

bool send_sync = false;

ImGuiWindow *main_window;

float tabwindow_size = 100;
float size2 = 2000;
float resize_delta = 0.0f;

struct Hal *hal;

int init_hal(void)
{
	// STEP 1: initialise the hal component
	comp_id = hal_init("nopy");
	if (comp_id < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "HALUI: ERROR: hal_init() failed\n");
		return -1;
	}

	// STEP 2: allocate shared memory for halui data
	hal = (struct Hal*)hal_malloc(sizeof(*hal));
	if (hal == nullptr) {
		rtapi_print_msg(RTAPI_MSG_ERR, "HALUI: ERROR: hal_malloc() failed\n");
fail:		hal_exit(comp_id);
		return -1;
	}

	for (int a = 0; a < 3; ++a) {
		if ((hal_pin_float_newf(HAL_IN, &hal->trq_in[a], comp_id, "nopy.%d.trq-in", a)) < 0) goto fail;
		if ((hal_pin_bit_newf(HAL_IN, &hal->servo_on[a], comp_id, "nopy.%d.servo-on", a)) < 0) goto fail;
		if ((hal_pin_bit_newf(HAL_IN, &hal->servo_alarm[a], comp_id, "nopy.%d.servo-alarm", a)) < 0) goto fail;
		if ((hal_pin_u32_newf(HAL_IN, &hal->servo_alarm_code[a], comp_id, "nopy.%d.servo-alarm-code", a)) < 0) goto fail;
	}

	hal_ready(comp_id);
	return 0;
}

void exit_hal(int)
{
	hal_exit(comp_id);
}


EmcPose relativePosition;
EmcPose calc_relativePosition(void);

struct Tile {
	const char *label;
	float height;
	bool resizeable;
	bool opened;
	bool visible;
	void (*func)(void);
	Tile(const char *l, float h, bool r, void (*f)(void)) :
		label(l), height(h), resizeable(r), func(f), opened(true)
	{
	}
};

class DirEnt {
public:
	DirEnt(const struct dirent *de) :
		name(de->d_name),
		type(de->d_type)
	{
		name_len = name.length();
//		if (is_dir()) name += "[DIR]";
	}
	bool operator <(const DirEnt& other) const {
		bool td = is_dir();
		bool od = other.is_dir();
		if (td && !od) return true;
		if (!td && od) return false;
		return name < other.name;
//		std::cerr << name << (t ? " < " : " >= ") << other.name << std::endl;
//		return t;
	}
	operator const char* ()	{ return name.c_str(); }
	operator const std::string& (){ return name; }
	bool is_dir() const { return type == DT_DIR; }

    	unsigned int type;
	int name_len;
	std::string name;
	std::string mtime;
};

class DirPos {
public:
	DirPos(const std::string &n, float p, int i) :
		name(n), scroll_pos(p), item(i)
	{
	}
	operator const std::string& (){ return name; }
	std::string name;
	float scroll_pos;
	int item;
};

void file_browser(void)
{
	ImGuiStyle& style = GetStyle();

//	PushFont(mono, FS_SMALL);
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

	static bool read_dir = true;
	static bool new_dir = false;
	static std::vector<DirEnt> dir;
	static std::stack<DirPos> dir_stack;
	static char cwd[PATH_MAX];

	if (read_dir) {
		dir.clear();
		if (getcwd(cwd, sizeof(cwd)) != NULL) {
			DIR* dirFile = opendir(cwd);
			struct dirent* hFile;
 			if (dirFile) {
				while ((hFile = readdir(dirFile)) != NULL) {
					if (!strcmp(hFile->d_name, ".")) continue;
					if (hFile->d_name[0] == '.' && strcmp(hFile->d_name, "..")) continue;
				        DirEnt new_entry(hFile);

					std::string path(cwd);
					path += std::string("/") + hFile->d_name;
					struct stat sb;
					if (!stat(path.c_str(), &sb)) {
						char t[21];
						strftime(t, 20, "%Y-%m-%d %H:%M:%S", localtime(&sb.st_mtime));
						new_entry.mtime = std::string(t);
					}

					auto it = std::lower_bound(dir.begin(), dir.end(), new_entry);
					dir.insert(it, new_entry);
				}
				closedir(dirFile);
			}
		}
		read_dir = false;
		new_dir = true;
	}


//	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {15, 15});

	static int sel = -1;
	static float scroll = -1;

	SetNextItemWidth(-FLT_MIN);
	if (InputText("##cwd", cwd, IM_ARRAYSIZE(cwd), ImGuiInputTextFlags_ElideLeft | ImGuiInputTextFlags_EnterReturnsTrue)) {
	}

	static ImGuiTableFlags table_flags =
		  ImGuiTableFlags_ScrollY
		| ImGuiTableFlags_RowBg
		| ImGuiTableFlags_BordersV
		| ImGuiTableFlags_Resizable
		| ImGuiTableFlags_Sortable
		| ImGuiTableFlags_SortTristate
		| ImGuiTableFlags_NoBordersInBody
		| ImGuiTableFlags_Hideable;
	const ImGuiTableColumnFlags column_flags = ImGuiTableColumnFlags_NoSortDescending | ImGuiTableColumnFlags_WidthFixed;

	SetNextItemWidth(-FLT_MIN);
	if (BeginTable("table", 3, table_flags, {-FLT_MIN, -FLT_MIN -48-8})) {
		TableSetupScrollFreeze(0, 1); // Make top row always visible
		TableSetupColumn("Type", column_flags);
		TableSetupColumn("Name", column_flags);
		TableSetupColumn("Date", column_flags);
//		TableSetupColumn("Date", ImGuiTableColumnFlags_WidthFixed, 100);
		TableHeadersRow();

		if (scroll >= 0) {
			SetScrollY(scroll);
			scroll = -1;
		}

		for (int n = 0; n < (int)dir.size(); ++n) {
                PushID(n);
			TableNextRow();
			PushStyleVar(ImGuiStyleVar_ItemSpacing, {style.ItemSpacing.x, style.CellPadding.y * 2}); // Fix

			TableSetColumnIndex(0);
			if (!read_dir && sel == -2) {
				if (dir_stack.top().name == dir[n].name) {
				sel = n;
				dir_stack.pop();
				}
			}

			if (Selectable(dir[n].is_dir() ? "DIR" : "", sel == n, ImGuiSelectableFlags_SpanAllColumns)) { // | ImGuiSelectableFlags_AllowDoubleClick)) {
				sel = n;
				if (dir[n].is_dir()) {
					if (!chdir(dir[n])) {
						read_dir = true;
						sel = -1;
						scroll = 0;
						if (dir[n].name != "..") {
							dir_stack.push(DirPos(dir[n].name, GetScrollY(), n));
						} else if (!dir_stack.empty()) { // parent
							sel = -2; //p.item;
							scroll = dir_stack.top().scroll_pos;
						}
					}
				}
			}
			TableNextColumn();
			Text(dir[n]);
			TableNextColumn();
			Text(dir[n].mtime.c_str());
			PopStyleVar();
                PopID();
		}

		new_dir = false;
		if (!read_dir && sel == -2) {
			sel = -1;
			dir_stack.pop();
		}
		EndTable();
	}
//	PopStyleVar();
//	PopFont();

	static std::string ngc_file;
	static bool load_new_file = false;

	if (sel < 0) {
		PushItemFlag(ImGuiItemFlags_Disabled, true);
		PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.5f);
	}
	if (Button("Open")) {
		ngc_file = dir[sel].name;
		load_new_file = true;
		show_file_browser = false;
		sendProgramClose();
		sendProgramOpen((cwd + std::string("/") + dir[sel].name).c_str());
	}
	if (sel < 0) {
		PopItemFlag();
		PopStyleVar();
	}
	SameLine();
	if (Button("Close")) {
		show_file_browser = false;
		read_dir = true;
	}

	PopStyleVar();
	PopStyleVar();
}


float messages_height = FS_SMALL * 4;

void w_msgs(void)
{
	if (!messages_visible) return;

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 0));
	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

	BeginChild("ChildMsg", {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
/*
	PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 0));
	if (Button("Hide", {100, messages_height})) {
		messages_visible = false;
	}
	SameLine();
	PopStyleVar();
*/
	PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	PushStyleColor(ImGuiCol_FrameBg, 0);//xff0000ff);
	if (BeginListBox("##messages", ImVec2(-FLT_MIN, -FLT_MIN))) {
		double time = GetTime();
		for (auto it = messages.begin(); it != messages.end(); it++) {
			static ImVec4 red = ImVec4(0.66f, 0.25f, 0.25f, 1.0f);
			static ImVec4 yel = ImVec4(0.35,  0.56f, 0.15f, 1.0f);
			static ImVec4 grn = ImVec4(0.051f, 0.914f, 0.663f, 1.0f);

			double dt = (it->time - time + 1) * 2;
			if (dt > 0) {
				ImVec2 cur = GetCurrentWindow()->DC.CursorPos;
				ImU32 clr = ColorConvertFloat4ToU32(ImVec4(0.66f, 0.25f, 0.25f, dt));
				GImGui->CurrentWindow->DrawList->AddRectFilled(cur, cur + ImVec2(GetWindowWidth(), FS_SMALL), clr);
			}
			TextColored(ImVec4(1, 1, 1, 0.5), "%s", it->stamp.c_str());
			SameLine();
			switch (it->type) {
			case EMC_OPERATOR_ERROR_TYPE:	TextColored(red, " E "); break;
			case EMC_OPERATOR_TEXT_TYPE:	TextColored(grn, " T "); break;
			case EMC_OPERATOR_DISPLAY_TYPE:	TextColored(yel, " D "); break;
			case NML_ERROR_TYPE:	TextColored(red, " ! "); break;
			case NML_TEXT_TYPE:	TextColored(grn, " i "); break;
			case NML_DISPLAY_TYPE:	TextColored(yel, " ? "); break;
			}
			SameLine();
			Text("%s", it->msg.c_str());
		}

/*		for (int n = 0; n < 100; n++) {
			const bool is_selected = (pos == n);
			if (Selectable("filename.ngc", is_selected)) {
				pos = n;
			}
			if (is_selected) ScrollToItem();
		}
*/
		EndListBox();
	}
	PopStyleColor();
	PopStyleVar();
	PopStyleVar();
	EndChild();
	PopStyleVar();
	PopStyleVar();
}

void w_op(void)
{
PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 0));
PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
BeginChild("##op_panel", ImVec2(-FLT_MIN, 56 * 3 + 8 * 2), ImGuiChildFlags_AlwaysUseWindowPadding);

	ImVec2 s(120, 56);


	bool estop = emcStatus->io.aux.estop;
	if (LedButton("E-Stop", estop, s, 0xff0000cc)) sendEstop(!estop);
	SameLine();
	if (estop) {
		PushItemFlag(ImGuiItemFlags_Disabled, true);
		PushStyleVar(ImGuiStyleVar_Alpha, GetStyle().Alpha * 0.5f);
	}
	bool machine_on = emcStatus->motion.traj.enabled;
	if (LedButton("Enable", machine_on, s)) sendMachineOn(!machine_on);
	if (estop) {
		PopItemFlag();
		PopStyleVar();
	}

	SameLine();

	bool prog_run = emcStatus->task.interpState == EMC_TASK_INTERP_WAITING || emcStatus->task.interpState == EMC_TASK_INTERP_READING;
	bool prog_paused = emcStatus->task.interpState == EMC_TASK_INTERP_PAUSED;
	bool blk = prog_paused && blink;
	if (LedButton(prog_paused ? "Resume" : prog_run ? "Pause" : "Run", prog_run || blk, s)) {
		if (prog_paused)
			sendProgramResume();
		else if (prog_run)
			sendProgramPause();
		else {
			sendAuto();
			sendProgramRun(0);
			clear_backplot = true;
		}
	}
	SameLine();
	if (LedButton("Stop", !prog_run && !prog_paused, s)) {
		sendProgramAbort();
	}

	ImVec2 cur = GetCursorPos();

	int dir = emcStatus->motion.spindle[0].direction;
	int no = 0;
	SameLine();

	BeginGroup();
//	PushItemFlag(ImGuiItemFlags_Disabled, !can_spindle);
	if (!can_spindle) BeginDisabled();
	if (LedButton("S0 Fwd", dir == 1, s)) sendSpindleForward(no);
	if (Button("S0 Stop", s)) sendSpindleOff(no);
	if (LedButton("S0 Rev", dir == -1, s)) sendSpindleReverse(no);
	if (!can_spindle) EndDisabled();
	EndGroup();

	SameLine();

	BeginGroup();
	if (!can_change_speed) BeginDisabled();
	if (Button("+", s)) sendSpindleIncrease(no);
	if (Button("-", s)) sendSpindleDecrease(no);
	if (!can_change_speed) EndDisabled();
	EndGroup();

	SetCursorPos(cur);

	bool opt_stop = emcStatus->task.optional_stop_state;
	if (LedButton("M1 Break", opt_stop, s)) sendSetOptionalStop(!opt_stop);
	SameLine();
	bool block_delete = emcStatus->task.block_delete_state;
	if (LedButton("Block Del", block_delete, s)) sendSetBlockDelete(!block_delete);
	SameLine();
	bool feed_hold = emcStatus->motion.traj.feed_hold_enabled;
	if (LedButton("Feed Hold", feed_hold, s)) sendSetFeedHoldEnable(!feed_hold);
	SameLine();
	bool s0_oe = emcStatus->motion.spindle[0].spindle_override_enabled;
	if (LedButton("Spindle OE", s0_oe, s)) {
		sendSetSOEnable(0, s0_oe ? 0 : 1);
	}

	bool flood = emcStatus->io.coolant.flood;
	if (LedButton("Flood", flood, s)) {
		if (!flood) sendFloodOn();
		else sendFloodOff();
	}
	SameLine();

	bool mist = emcStatus->io.coolant.mist;
	if (LedButton("Mist", mist, s)) {
		if (!mist) sendMistOn();
		else sendMistOff();
	}
	SameLine();

PopStyleVar();
PopStyleVar();
EndChild();
}

std::list<Tile> tabs;
std::list<Tile> tiles;

void init_tiles(void)
{
	tabs.push_back(Tile("Manual", 100, true, w_manual));
	tabs.push_back(Tile("Auto", 100, true, w_gcode));
	tabs.push_back(Tile("Offsets", 100, true, w_offsets));
	tabs.push_back(Tile("Tools", 100, true, w_tools));
	tabs.push_back(Tile("3D", 700, true, w_3d));
	tabs.back().opened = false;
	tabs.push_back(Tile("OP", 56 * 3 + 8 * 2, false, w_op));
	tabs.back().opened = false;
	tabs.push_back(Tile("Log", 100, true, w_msgs));
	tabs.back().opened = false;
	tabs.push_back(Tile("HAL", 100, true, w_hal));
}


Interp *interp;
GLFB *glfb;

int immain(int argc, char** argv) {
	if (!glfwInit()) return 1;

	GLFWwindow* window = glfwCreateWindow(1080, 1920, "NoPy", NULL, NULL);
	glfwSetWindowSizeLimits(window, 100, 60, 4000, 4000);

	if (!window) {
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	// Initialize ImGui
	IMGUI_CHECKVERSION();
	CreateContext();
	ImGuiIO& io = GetIO(); (void)io;
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	set_theme();
//	io.KeyRepeatDelay = 1;

	glewInit();
	glfb = new GLFB(100,100);

	if (tool_mmap_user()) return -1;

	interp = new Interp();
	init_tiles();

	if (init_hal()) return -1;


	// attach our quit function to SIGINT
	{
		struct sigaction act;
		act.sa_handler = exit_hal;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		sigaction(SIGABRT, &act, NULL);
	}

	// Main loop
	while (!glfwWindowShouldClose(window) && !IsKeyPressed(ImGuiKey_Escape)) {
	        glfwPollEvents();

if (IsKeyPressed(ImGuiKey_GraveAccent)) {
	ImGuiContext& g = *GImGui;
	fprintf(stderr, "WIN:%x INPUT:%x DEAC:%x\n", g.CurrentWindow ? g.CurrentWindow->ID : -1, g.InputTextState.ID, g.InputTextDeactivatedState.ID);
}

	        ImGui_ImplOpenGL3_NewFrame();
	        ImGui_ImplGlfw_NewFrame();
	        NewFrame();

		osk_process();

		struct tm *timeinfo;
		time(&current_time);
		timeinfo = localtime(&current_time);
		snprintf(current_time_string, sizeof(current_time_string), "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

		double temp;
		blink = modf(GetTime() * 1.5, &temp) > 0.5;

		// linuxcnc
		updateStatus();
		get_messages();
		relativePosition = calc_relativePosition();


		int serial_diff = emcStatus->echo_serial_number - emcCommandSerialNumber;
		if (send_sync && emcStatus->task.interpState == EMC_TASK_INTERP_IDLE) {
			if (serial_diff >= 0) {
				fprintf(stderr, "!!! SYNC\n");
				send_sync = false;
				sendSynch();
			} else {
				fprintf(stderr, "%d ",  serial_diff);
			}
		}


		can_jog =  emcStatus->task.state == EMC_TASK_STATE_ON
			&& emcStatus->task.execState == EMC_TASK_EXEC_DONE
			&& emcStatus->task.interpState == EMC_TASK_INTERP_IDLE;
		can_mdi =  can_jog && !emcStatus->motion.jogging_active;
		can_spindle = emcStatus->task.state == EMC_TASK_STATE_ON &&
			 ( emcStatus->task.mode != EMC_TASK_MODE_AUTO
			|| emcStatus->task.interpState == EMC_TASK_INTERP_IDLE
			|| emcStatus->task.interpState == EMC_TASK_INTERP_PAUSED);
		can_change_speed = emcStatus->task.state == EMC_TASK_STATE_ON &&
			 ( emcStatus->motion.spindle[0].state == 0
			&& emcStatus->motion.spindle[0].enabled);
		interpreter_is_running =   (emcStatus->task.motionLine > 0
					&&  emcStatus->task.mode == EMC_TASK_MODE_AUTO
					&& (emcStatus->task.interpState == EMC_TASK_INTERP_WAITING
					||  emcStatus->task.interpState == EMC_TASK_INTERP_READING));

	        // -------------------- content ----------------------

		const ImGuiViewport* viewport = GetMainViewport();
		SetNextWindowPos(viewport->WorkPos);
		SetNextWindowSize(viewport->WorkSize);
		Begin("MainWindow", 0,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoBringToFrontOnFocus);
		GetWindowDrawList()->AddRectFilledMultiColor(ImVec2(0,0), io.DisplaySize, bg_gradient[0], bg_gradient[1], bg_gradient[2], bg_gradient[3]);
		main_window = GetCurrentWindow();

		SetCursorPos(ImVec2(0, 0));

		if (w_dro()) {	// full-screen
			static bool fs = false;
			static int x, y, w, h;
			static int l, t, r, b;
			if (fs = !fs) {
				GLFWmonitor* monitor =  glfwGetPrimaryMonitor();
				const GLFWvidmode* mode = glfwGetVideoMode(monitor);
				glfwGetWindowPos(window, &x, &y);
				glfwGetWindowSize(window, &w, &h);
				glfwGetWindowFrameSize(window, &l, &t, &r, &b);
				glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
			} else {
				glfwSetWindowMonitor(window, NULL, x, y-t, w, h+t, 0);
			}
		}

		w_mdi();

		// ------------------- tab bar -------------------

		float h = 0;
		for (auto i = tabs.begin(); i != tabs.end(); i++) {
			if (!i->opened)
				h += i->height + 24;
		}

		BeginChild("TabWindow", ImVec2(-FLT_MIN, -h - 8), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
		if (BeginBigTabBar("##tabbar")) {
			PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 0));
			PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
			for (auto i = tabs.begin(); i != tabs.end(); i++) {
				if (BeginBigTabItem(i->label, &i->opened, ImGuiTabItemFlags_NoCloseButton)) {
//					BeginChild(i->label, ImVec2(-FLT_MIN, -FLT_MIN), 0, ImGuiWindowFlags_NoScrollbar);
					i->func();
//					EndChild();
					EndTabItem();
				}
			}
			PopStyleVar();
			PopStyleVar();
			EndTabBar();
		}
		EndChild();

		// Tiles

		for (auto i = tabs.begin(); i != tabs.end(); i++)
			if (!i->opened) {
				float size1 = i->height;
				float size2 = 100;
				if (SplitterText(i->label, &size2, i->resizeable ? &i->height : nullptr))
					i->opened = true;
				BeginChild(i->label, ImVec2(-FLT_MIN, size1), 0, ImGuiWindowFlags_NoScrollbar);
				i->func();
				EndChild();
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
			float key_size = 65.0f;
			bool ex = false;
			if (osk(qwerty, key_size, key_size, 8)) {
				ex = true;
			}
			PopStyleVar();
			PopFont();
			End();
			if (ex) show_osk = false;
		}

//		osk_process();

		for (int i = 0; i < IMGUI_TOUCH_POINTS; ++i) {
			if (io.TouchActive[i])
				GetForegroundDrawList()->AddCircleFilled(io.TouchPos[i], 20, 0x8000ffff);
		}

		End();	// -------------------- end of content ----------------------




	        Render();
	        int display_w, display_h;
	        glfwGetFramebufferSize(window, &display_w, &display_h);
	        glViewport(0, 0, display_w, display_h);
	        glClearColor(0.25f, 0.25f, 0.30f, 1.00f);
	        glClear(GL_COLOR_BUFFER_BIT);
	        ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());

	        glfwSwapBuffers(window);
	//	glFinish();
	}

	delete glfb;

	exit_hal(0);

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

static void sigQuit(int sig)
{
}

int main(int argc, char** argv) {
	int opt;

	fprintf(stderr, "tryNml...\n");
	if (tryNml() != 0) {
		rcs_print_error("can't connect to LinuxCNC\n");
		exit(1);
	}

	// get current serial number, and save it for restoring when we quit
	// so as not to interfere with real operator interface
	updateStatus();
	emcCommandSerialNumber = emcStatus->echo_serial_number;

	// attach our quit function to SIGINT
	{
		struct sigaction act;
		act.sa_handler = sigQuit;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		sigaction(SIGINT, &act, NULL);
	}

	// make all threads ignore SIGPIPE
	{
		struct sigaction act;
		act.sa_handler = SIG_IGN;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		sigaction(SIGPIPE, &act, NULL);
	}

	{
		const char *s;
		double d;
		int i;

		std::string t = emcStatus->task.ini_filename;
		size_t pos = t.rfind('/');
		if (pos == std::string::npos) {		// relative
			ini_dir = ".";
			ini_filename = t.substr(1);
		} else {
			ini_dir = t.substr(0, pos);
			ini_filename = t.substr(pos+1);
		}
		std::cerr << "ini dir:" << ini_dir << " name:" << ini_filename << std::endl;

		fprintf(stderr, "parsing ini %s\n", emcStatus->task.ini_filename);

		IniFile	inifile;
		if (inifile.Open(emcStatus->task.ini_filename)) {
			if ((s = inifile.Find("PARAMETER_FILE", "RS274NGC"))) {
				fprintf(stderr, "parameters: %s\n", s);
				parameter_file = std::string(s);
			}
			if ((s = inifile.Find("TOOL_TABLE", "EMCIO"))) {
				fprintf(stderr, "tool table: %s\n", s);
				tool_table = std::string(s);
			}
			if ((s = inifile.Find("MIN_FEED_OVERRIDE", "DISPLAY"))
				&& 1 == sscanf(s, "%lf", &d) && d > 0.0) min_feed_override = d;
			if ((s = inifile.Find("MAX_FEED_OVERRIDE", "DISPLAY"))
				&& 1 == sscanf(s, "%lf", &d) && d > 0.0) max_feed_override = d;

			if ((s = inifile.Find("MIN_SPINDLE_OVERRIDE", "DISPLAY"))
				&& 1 == sscanf(s, "%lf", &d) && d > 0.0) min_spindle_override = d;
			if ((s = inifile.Find("MAX_SPINDLE_OVERRIDE", "DISPLAY"))
				&& 1 == sscanf(s, "%lf", &d) && d > 0.0) max_spindle_override = d;

			if(inifile.Find(&max_linear_velocity, "MAX_LINEAR_VELOCITY", "TRAJ") &&
			   inifile.Find(&max_linear_velocity, "MAX_VELOCITY", "AXIS_X"))
				max_linear_velocity = 1.0;

			inifile.Close();
		}
/*
[TRAJ]
COORDINATES = X Y Z
MAX_LINEAR_VELOCITY = 4
*/

		fprintf(stderr, "ready\n");
	}
	int rc = immain(argc, argv);
	return rc;
}

EmcPose calc_relativePosition(void)
{
	// lathe = not (self.emcstat.axis_mask & 2)
	EmcPose pos = emcStatus->motion.traj.actualPosition;

	pos.coor[0] -= emcStatus->task.toolOffset.coor[0] + emcStatus->task.g5x_offset.coor[0];
	pos.coor[1] -= emcStatus->task.toolOffset.coor[1] + emcStatus->task.g5x_offset.coor[1];
	pos.coor[2] -= emcStatus->task.toolOffset.coor[2] + emcStatus->task.g5x_offset.coor[2];
	pos.coor[3] -= emcStatus->task.toolOffset.coor[3] + emcStatus->task.g5x_offset.coor[3];
	pos.coor[4] -= emcStatus->task.toolOffset.coor[4] + emcStatus->task.g5x_offset.coor[4];
	pos.coor[5] -= emcStatus->task.toolOffset.coor[5] + emcStatus->task.g5x_offset.coor[5];
	pos.coor[6] -= emcStatus->task.toolOffset.coor[6] + emcStatus->task.g5x_offset.coor[6];
	pos.coor[7] -= emcStatus->task.toolOffset.coor[7] + emcStatus->task.g5x_offset.coor[7];
	pos.coor[8] -= emcStatus->task.toolOffset.coor[8] + emcStatus->task.g5x_offset.coor[8];

	if (emcStatus->task.rotation_xy != 0.0) {
		double t = -emcStatus->task.rotation_xy * M_PI / 180;
		double xr = pos.tran.x * cos(t) - pos.tran.y * sin(t);
		double yr = pos.tran.x * sin(t) + pos.tran.y * cos(t);
		pos.tran.x = xr;
		pos.tran.y = yr;
	}

	pos.coor[0] -= emcStatus->task.g92_offset.coor[0];
	pos.coor[1] -= emcStatus->task.g92_offset.coor[1];
	pos.coor[2] -= emcStatus->task.g92_offset.coor[2];
	pos.coor[3] -= emcStatus->task.g92_offset.coor[3];
	pos.coor[4] -= emcStatus->task.g92_offset.coor[4];
	pos.coor[5] -= emcStatus->task.g92_offset.coor[5];
	pos.coor[6] -= emcStatus->task.g92_offset.coor[6];
	pos.coor[7] -= emcStatus->task.g92_offset.coor[7];
	pos.coor[8] -= emcStatus->task.g92_offset.coor[8];
	return pos;
}

