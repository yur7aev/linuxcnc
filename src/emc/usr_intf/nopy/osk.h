#ifndef NOPY_OSK_H
#define NOPY_OSK_H

#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include <imgui_internal.h>

struct key {
	ImGuiKey key;
	const char *label;
	const char *label_shift;
	float w, h;
	int code, code_shift;
	ImGuiKey mods;
};

#define KEY_DONE	(ImGuiKey)0
#define KEY_ROW		(ImGuiKey)(ImGuiKey_NamedKey_END+1)
#define KEY_SKIP	(ImGuiKey)(ImGuiKey_NamedKey_END+2)
#define KEY_CLOSE	(ImGuiKey)(ImGuiKey_NamedKey_END+3)

bool osk(struct key *keys, float key_width, float key_height, float key_gap);
void osk_process(void);
void osk_close(void);
void osk_show(ImVec2 pos);
extern struct key numpad[];
extern struct key qwerty[];
extern bool show_osk;

#endif