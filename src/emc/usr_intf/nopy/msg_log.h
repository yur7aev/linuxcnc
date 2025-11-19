#pragma once

#include <string>
#include <list>

#include <nml.hh>
#include "rcs.hh"
#include "linuxcnc.h"		/* LINELEN */
#include "../shcom.hh"

#include <imgui.h>

// set in main loop
extern time_t current_time;
extern char current_time_string[32];
extern bool messages_visible;

struct Message {
	Message(NMLTYPE t, const char *m) : type(t), msg(m) {
		stamp = std::string(current_time_string);
		time = ImGui::GetTime();
		messages_visible = true;
	}
	NMLTYPE type;
	std::string msg;
	std::string stamp;
	float time;
};

extern std::list<Message> messages;

int get_messages();
