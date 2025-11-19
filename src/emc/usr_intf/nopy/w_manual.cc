#include <stdarg.h>
#include "nopy.h"
#include "w_manual.h"
#include "widgets.h"
#include "osk.h"
#include "theme.h"
#include "tooldata.hh"
#include "../shcom.hh"             // NML Messaging functions

static float jog_incr = 0.0f;
float jog_speed = 100.0f;

void doJog(int axis, float dir)
{
	if (emcStatus->task.mode != EMC_TASK_MODE_MANUAL) {
		sendManual();
		emcCommandWaitDone();
	}

	int mode = emcStatus->motion.traj.mode == EMC_TRAJ_MODE_FREE ? 1 : 0;

	if (jog_incr == 0.0f)
		sendJogCont(axis, mode, jog_speed * dir);
	else
		sendJogIncr(axis, mode, jog_speed * dir, jog_incr);
}

void w_manual(void)
{
//	SameLine();

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32, 8));
	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

	BeginChild("w_manual", {-FLT_MIN, -FLT_MIN}, ImGuiChildFlags_AlwaysUseWindowPadding, 0);

	ImVec2 size(80, 80);
	ImVec2 s2(80, 55);

	int mode = emcStatus->motion.traj.mode == EMC_TRAJ_MODE_FREE ? 1 : 0;

	MoveCursorY(32);

	float incr = jog_incr;

	PushStyleVar(ImGuiStyleVar_GrabRounding, 20.0f);
	PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
	SetNextItemWidth(-48);
	SliderFloat("##jog_speed", &jog_speed, 1.0f, 3000.0f, "Jog Speed %.1f mm/min", ImGuiSliderFlags_Logarithmic);
	PopStyleVar();
	PopStyleVar();

	SetCursorPosY(GetCursorPosY() + 16);

	if (LedButton("Cont", jog_incr == 0.0f, s2)) incr = 0.0f;
	SameLine();
	if (LedButton("0.001", jog_incr == 0.001f, s2)) incr = 0.001f;
	SameLine();
	if (LedButton("0.01", jog_incr == 0.01f, s2)) incr = 0.01f;
	SameLine();
	if (LedButton("0.1", jog_incr == 0.1f, s2)) incr = 0.1f;
	SameLine();
	if (LedButton("1", jog_incr == 1.0f, s2)) incr = 1.0f;
	jog_incr = incr;

	SetCursorPosY(GetCursorPosY() + 32);

	if (!can_jog) BeginDisabled();
	Dummy(size); SameLine();

	bool down, up;

	if (OSKButton("Y+", size, &down, &up)) {
		if (down) doJog(1, 1);
		else if (jog_incr == 0.0f) sendJogStop(1, mode);
	}
	SameLine();
	Dummy(size); SameLine();
	Dummy(size); SameLine();
	if (OSKButton("Z+", size, &down, &up)) {
		if (down) doJog(2, 1);
		else if (jog_incr == 0.0f) sendJogStop(2, mode);
	}
/*
	if (ButtonEx("Y+", size, ImGuiButtonFlags_PressedOnClick)) doJog(1, 1);
	else if (jog_incr == 0.0f && IsItemDeactivated()) sendJogStop(1, mode);
	SameLine();
	Dummy(size); SameLine();
	Dummy(size); SameLine();
	if (ButtonEx("Z+", size, ImGuiButtonFlags_PressedOnClick)) doJog(2, 1);
	else if (jog_incr == 0.0f && IsItemDeactivated()) sendJogStop(2, mode);
	if (ButtonEx("X-", size, ImGuiButtonFlags_PressedOnClick)) doJog(0, -1);
	else if (jog_incr == 0.0f && IsItemDeactivated()) sendJogStop(0, mode);
	SameLine();
	if (ButtonEx("Y-", size, ImGuiButtonFlags_PressedOnClick)) doJog(1, -1);
	else if (jog_incr == 0.0f && IsItemDeactivated()) sendJogStop(1, mode);
	SameLine();
	if (ButtonEx("X+", size, ImGuiButtonFlags_PressedOnClick)) doJog(0, 1);
	else if (jog_incr == 0.0f && IsItemDeactivated()) sendJogStop(0, mode);
	SameLine();
	Dummy(size); SameLine();
	if (ButtonEx("Z-", size, ImGuiButtonFlags_PressedOnClick)) doJog(2, -1);
	else if (jog_incr == 0.0f && IsItemDeactivated()) sendJogStop(2, mode);
*/
	if (OSKButton("X-", size, &down, &up)) {
		if (down) doJog(0, -1);
		else if (jog_incr == 0.0f) sendJogStop(0, mode);
	}
	SameLine();
	if (OSKButton("Y-", size, &down, &up)) {
		if (down) doJog(1, -1);
		else if (jog_incr == 0.0f) sendJogStop(1, mode);
	}
	SameLine();
	if (OSKButton("X+", size, &down, &up)) {
		if (down) doJog(0, 1);
		else if (jog_incr == 0.0f) sendJogStop(0, mode);
	}
	SameLine();
	Dummy(size); SameLine();
	if (OSKButton("Z-", size, &down, &up)) {
		if (down) doJog(2, -1);
		else if (jog_incr == 0.0f) sendJogStop(2, mode);
	}

	if (!can_jog) EndDisabled();


/*
	SetCursorPosY(GetCursorPosY() + 32);

	int dir = emcStatus->motion.spindle[0].direction;
	int no = 0;

if (!can_spindle) BeginDisabled();
	if (LedButton("Rev", dir == -1, size)) sendSpindleReverse(no);
	SameLine();
	if (Button("Stop", size)) sendSpindleOff(no);
	SameLine();
	if (LedButton("Fwd", dir == 1, size)) sendSpindleForward(no);
	SameLine();
if (!can_spindle) EndDisabled();
if (!can_change_speed) BeginDisabled();
	if (Button("-", size)) sendSpindleDecrease(no);
	SameLine();
	if (Button("+", size)) sendSpindleIncrease(no);
if (!can_change_speed) EndDisabled();

	SetCursorPosY(GetCursorPosY() + 32);

	if (Button("Abort", size)) sendAbort();
*/


	EndChild();
	PopStyleVar();
	PopStyleVar();
}
