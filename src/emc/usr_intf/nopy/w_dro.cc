#include "nopy.h"
#include "w_dro.h"
#include "../shcom.hh"             // NML Messaging functions
#include "osk.h"
#include "msg_log.h"

ImVec2 dro_window_size;
extern char current_time_string[32];
extern EmcPose relativePosition;


#ifdef TOOL_NML
#error TOOL_NML not supported
#endif

const char *joints[9] = { "0", "1", "2", "3", "4", "5", "6", "7", "8" };

//
// single axis/joint position
//
void dro(int j,
	float load
)
{
	static char gbuf[64];

	const char *pfx = emcStatus->motion.traj.mode == EMC_TRAJ_MODE_FREE ? joints[j] : axis_name[j];
	bool homed = emcStatus->motion.joint[j].homed;
	double position = relativePosition.coor[j];

	PushID(pfx);

	PushStyleColor(ImGuiCol_Text, emcStatus->motion.joint[j].enabled ? ImVec4(1, 1, 1, 1) : ImVec4(1, 1, 1, 0.5));
	PushFont(mono, 48);
	// DebugDrawLineExtents(0xFF0000FF);
	Text("%s", pfx);	// joint no / axis name
	SameLine();
	Text("% 9.3f", position);
	PopStyleColor();
	SameLine();

	// set offset popup
	ImGuiID id = GetID("pos");
	ImRect r(GetItemRectMin(), GetItemRectMax());

//	ItemSize(r);
	if (!ItemAdd(r, id))
		return;

	bool pressed = ButtonBehavior(r, id, NULL, NULL, ImGuiButtonFlags_PressedOnClickRelease);
	if (pressed) {
		if (!homed) {
			messages.push_front(Message(EMC_OPERATOR_ERROR_TYPE, "Not homed"));
		} else if (emcStatus->task.interpState != EMC_TASK_INTERP_IDLE) {
			messages.push_front(Message(EMC_OPERATOR_ERROR_TYPE, "Interpreter busy"));
		} else {
			snprintf(gbuf, sizeof(gbuf), "% 9.3f", position);
		        OpenPopup("kp");
			SetNextWindowPos(r.Min - ImVec2(8,8));
		}
	}

	const float button_size = 65.0;

	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	SetNextWindowSize(ImVec2(button_size*4 + 8, button_size*5 + 48 + 8*2));
	if (BeginPopupModal("kp", NULL, ImGuiWindowFlags_NoDecoration)) {
		if (pressed) SetKeyboardFocusHere();
		SetNextItemWidth(button_size*4 - 8);
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		if (InputText("##ti", gbuf, IM_ARRAYSIZE(gbuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
fprintf(stderr, "CL\n");
			static char cbuf[80];
			snprintf(cbuf, sizeof(cbuf), "G10 L20 P0 %s[%s]\n", pfx, gbuf);
			osk_close();
			CloseCurrentPopup();
			sendMdiAndSynch(cbuf);
			PopStyleVar();
		} else {
			PopStyleVar();
			PushFont(font, FS_SMALL);
			if (osk(numpad, button_size, button_size, 8)) CloseCurrentPopup();
			PopFont();
		}
		EndPopup();
	}
	PopStyleVar();
	PopStyleVar();
	PopFont();	// 48

	// DTG
	SameLine(0, 8);
	BeginGroup();
	if (homed) {
		if (!IsPopupOpen(pfx)) {
			ImVec2 p = GetCursorPos();
//			PushFont(mono, FS_SMALL);
//			Text("% 9.3f\n[       ]", emcStatus->motion.traj.dtg.coor[j]);
			Text("% 9.3f", emcStatus->motion.traj.actualPosition.coor[j]);

			if (*(hal->servo_alarm[j])) {
				if (blink)
					Text("[   A.%02X]", *(hal->servo_alarm_code[j]));
				else
					Text("[       ]");
			} else if (*(hal->servo_on[j])) {
				Text("[       ]");

				p += ImVec2(7, FS_SMALL+11);

				float trq = *(hal->trq_in[j]);
				unsigned int clr = trq < 100 ? 0xffffffff : trq < 150 ? 0xff00ffff : 0xff0000ff;
				if (trq > 150) trq = 150;

				GetCurrentWindow()->DrawList->AddRectFilled(p, p + ImVec2(81 / 150.0 * trq, 4), clr);
			} else {
				Text("[   %4x]", *(hal->servo_alarm_code[j]));
			}

//			PopFont();
		}
	} else {
		if (SmallButton("Home")) {
			sendHome(j);
		}
	}
	EndGroup();

	PopID();
	//DebugDrawLineExtents(0xff00FF00);
	//DebugDrawCursorPos(0xFF0080FF);
}

////////////////////////////////////////////////////////////////////////////////////
//
// DRO window
//
bool w_dro(void)
{
	ImGuiStyle& style = GetStyle();

	PushFont(mono, FS_SMALL);
	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 0));

//		BeginChild("##dro", {-FLT_MIN, 60*3}, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_ResizeY | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
	BeginChild("##dro", {-FLT_MIN, FS_SMALL*7}, 
//		ImGuiChildFlags_AutoResizeX
//		| ImGuiChildFlags_ResizeY 
		ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);

	static const char *modeName[4] = { "-", "MAN", "AUT", "MDI" };
	static const char *stateName[] = { "-", "EMG", "RDY", "OFF", "ON " };
	static const char *execName[] = { "-", "ERR ", "DONE", "WM  ", "WMQ ", "WIO ", "WMIO", "WD  ", "WSYS", "WSP " };
	static const char *interpName[] = { "-", "IDLE", "READ", "PAUS", "WAIT" };
	static const char *tcName[] = { "-", "FREE", "COOR", "TELE" };

	int mode = emcStatus->task.mode;
	int g5x = emcStatus->task.g5x_index;
	int state = emcStatus->task.state;
	int execState = emcStatus->task.execState;
	int interpState = emcStatus->task.interpState;
	int tc = emcStatus->motion.traj.mode;
	int queued = emcStatus->motion.traj.queue;
	const char *in_position = emcStatus->motion.traj.inpos ? "INP" : "MOV";
	int sp = emcStatus->motion.spindle[0].state;
	int spen = emcStatus->motion.spindle[0].enabled;
	int hb = emcStatus->task.heartbeat;

	// top status line
	Text("\u0383 %s %s %s %s %s %s %s",
		modeName[mode], offs_name[g5x], stateName[state],
		execName[execState], interpName[interpState], tcName[tc], in_position
	);

	// clock
	const ImGuiViewport* viewport = GetMainViewport();
	ImGuiIO& io = GetIO();
	char t[100];
	snprintf(t, 100, "%f %dx%d %04d fps %s", io.MouseClickedTime[0], (int)viewport->WorkSize.x, (int)viewport->WorkSize.y, (int)io.Framerate, current_time_string);
	ImVec2 ts = CalcTextSize(t);

	// fullscreen button
	bool rc = false;
	SetCursorPos(ImVec2(GetWindowWidth() - ts.x-8, 0)); //(56-ts.y)/2));
	if (Button("##Full-screen", ts)) rc = true;
	SetCursorPos(ImVec2(GetWindowWidth() - ts.x-8, 0)); //(56-ts.y)/2));
	Text(t);
         
	// ------------------ DRO ------------------

	BeginGroup();
	int ax = emcStatus->motion.traj.axis_mask;
	for (int a = 0; a < 9; ++a)
		if (ax & 1<<a) dro(a, 0);
	EndGroup();

#define SUP_MINUS "\u00af"
#define SUP_ONE  "\u00b9"
#define DIAM "\u2205"
#define DOT "\u00b7"

	SameLine(0, 32);
	BeginGroup();

	// ------------------ T --------------------

	int tool = emcStatus->io.tool.toolInSpindle;
	int prep = emcStatus->io.tool.pocketPrepped;
	float diam = 0;
	float len = emcStatus->task.toolOffset.tran.z;
	//int pock = emcStatus->io.tool.toolFromPocket;
	if (tool > 0) {
		diam = emcStatus->io.tool.toolTableCurrent.diameter;
		len = emcStatus->io.tool.toolTableCurrent.offset.tran.z;
	}

	PushFont(mono, 48);
	Text(tool ? "T %5d" : "T     -", tool); SameLine(0, 8);
	PopFont();
	Text(DIAM "%.3f L%.3f\nT%d", diam, len, prep);

	// ------------------ S --------------------

	float speed = emcStatus->task.activeSettings[2];
	float spd = emcStatus->motion.spindle[0].speed;
	float sfm = diam > 0.001 ? speed * diam * 3.1415926 / 1000: 0;

	PushFont(mono, 48);
	Text("S %5.0f", speed); SameLine(0, 8);
	PopFont();
	Text("%.0f min" SUP_MINUS SUP_ONE "\n%.1f m/min", spd, sfm);

	// ------------------ F --------------------

	float vel  = emcStatus->motion.traj.current_vel * 60;
	float feed = emcStatus->task.activeSettings[1];

	PushFont(mono, 48);
	Text("F %5.0f", feed);  SameLine(0, 8);
	PopFont();
	Text("%.1f mm/min\n", vel);

	EndGroup();

	// ------------------ G --------------------

	SameLine(0, 32);
	BeginGroup();
	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	static int defaultGCodes[ACTIVE_G_CODES] = { 0,
		 -2,  -1, 170, 400,
		210, 900, 940, 540,
		430, 990, 640,  -1,
		970, 911,  80,  -1
	};
	for (int g = 1; g < ACTIVE_G_CODES; g++) {
		int j = emcStatus->task.activeGCodes[g];
		if (j >= 0) {
			int f = j % 10;
			int i = j / 10;
//			TextColored(color_green, "G"); SameLine();
			TextColored(j == defaultGCodes[g] || defaultGCodes[g] == -2 ? color_green : color_yellow, "G");
//			TextColored(j == defaultGCodes[g] || defaultGCodes[g] == -2 ? color_green : color_yellow, "%d%s%c%c%s",
			SameLine();
			Text("%d%s%c%c%s",
				i, i < 10 ? " " : "",
				f ? '.' : ' ',
				f ? f + '0' : ' ',
				g&7 ? " " : "");
		} else {
			Text(" -%s", g & 7 ? "    " : "");
		}
		if (g % 4) SameLine();
	}

	for (int g = 1; g < ACTIVE_M_CODES - 1; g++) {
		int i = emcStatus->task.activeMCodes[g];
		if (i >= 0) {
			PushStyleColor(ImGuiCol_Text, color_green);
			Text("M"); SameLine();
			PopStyleColor();
			Text("%d%s%s", i, i < 10 ? " " : "", g!=8?"   ":"");
		} else {
			Text(" -    ");
		}
		if (g % 4) SameLine();
	}
	PopStyleVar();
	EndGroup();
	// ------------------ overrides --------------------
	SameLine(0, 16);
	BeginGroup();
	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	float o_feed = emcStatus->motion.traj.scale;
	float o_spindle = emcStatus->motion.spindle[0].spindle_scale;
	float o_rapid = emcStatus->motion.traj.rapid_scale;
	float max_vel = emcStatus->motion.traj.maxVelocity * 60 / 1000;
	extern float jog_speed;
	Text("F %.1f", o_feed * 100);
	Text("S %.1f", o_spindle * 100);
	Text("R %.1f", o_rapid * 100 );
	Text("Max %.3f", max_vel);
	Text("Jog %.3f", jog_speed / 1000);
	PopStyleVar();
	EndGroup();

/*
MAX_FEED_OVERRIDE = 1.2
MAX_SPINDLE_OVERRIDE = 1.0

 SameLine();
	PushStyleColor(ImGuiCol_Text, color_grey);
	MoveCursorY(-8);
	Sep("Override");
	PopStyleColor();

//	MoveCursorY(2);

	style.ItemSpacing = ImVec2(8, 8);

	float d = (GetWindowWidth() - 40) / 5;

//	static float o_jog = 0.5f;
	float o_spindle = emcStatus->motion.spindle[0].spindle_scale;
	float o_feed = emcStatus->motion.traj.scale;
	float o_rapid = emcStatus->motion.traj.rapid_scale;
	float o_max_vel = emcStatus->motion.traj.maxVelocity * 60 / 100;

	if (Gage("Feed", d, &o_feed, ImVec2(0.1f, 1.5f))) {
		sendFeedOverride(o_feed);
	}
	SameLine();

	if (Gage("Rapid", d, &o_rapid)) {
		sendRapidOverride(o_rapid);
	}
	SameLine();

	if (Gage("S 0", d, &o_spindle)) {
		sendSpindleOverride(0, o_spindle);
	}
	SameLine();

	if (Gage("Max V", d, &o_max_vel, ImVec2(0, 32))) {
		sendMaxVelocity(o_max_vel * 100 / 60);
	}
*/

//	style.ItemSpacing = ImVec2(8, 8);
	dro_window_size = GetWindowSize();

	EndChild();
	PopStyleVar();
	PopFont();

	return rc;
}
