#pragma once

#include <string>
#include "theme.h"
#include <hal.h>

#define AXES 9
extern const char *axis_name[AXES+1];
extern const char *offs_name[14];

extern std::string parameter_file;
extern std::string tool_table;
extern std::string ini_dir;
extern std::string ini_filename;

extern bool blink;

struct Hal {
	hal_float_t *trq_in[16];
	hal_bit_t *servo_on[16];
	hal_bit_t *servo_alarm[16];
	hal_u32_t *servo_alarm_code[16];
};

extern struct Hal *hal;
extern bool can_mdi;
extern bool send_sync;
extern bool can_jog;

