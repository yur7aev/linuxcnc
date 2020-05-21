/*
 * N Y X
 *
 * (c) 2016, dmitry@yurtaev.com
 */

#ifndef Y_H
#define Y_H

typedef struct yssc2_amp {	// data collected during initialization
	char fw[16];		// firmware version: 'B26W200B3   '
	uint16_t param[256];	// parameters value, live
	uint32_t fbres;		// feedback pulse number, as set in param No.6
	uint32_t cyc0;		// 1-rev motor position, in fb_res units
	uint32_t abs0;		// absolute rev counter
	uint32_t type;
} YSSC2_amp;

struct freq_data {
	int b[16];
	unsigned i, j;
	int bucket;
	int sum;
};

typedef struct servo_param {
	uint16_t no;
	uint16_t size;		// number of bytes: 2 or 4
	uint32_t val;
} servo_param;

// params for single axis
typedef struct servo_params {
	servo_param *pa;	// an array of params for a single axis, sorted by no
	size_t count;		// number of items in the array
	size_t size;		// size of allocated array
	long load_axis;
} servo_params;

typedef struct yssc2 {
	nyx_dpram *dpram;	// local copy

	// uspace
	int fd;			// nyx device file descriptor
	int axes;
	int yios;

	// RTAI
#ifdef __KERNEL__
	volatile nyx_iomem *iomem;
	uint32_t iolen;
	void *dev;
	dma_addr_t dpram_bus_addr;
#endif

	YSSC2_amp amp[NYX_AXES];

	uint32_t prev_fb_seq;	// to detect index req change

	struct io_pins *io;
	int initial_delay;
	int errors_shown;
	struct freq_data freq;
	int was_ready;

	servo_params par[NYX_AXES];
} YSSC2;

uint32_t yssc2_magic(YSSC2 *y)		{ return y->dpram->magic; }
int yssc2_axes(YSSC2 *y)		{ return y->axes; }
int yssc2_fb_seq(YSSC2 *y)		{ return y->dpram->fb.seq; }
double yssc2_irq_time_us(YSSC2 *y)	{ return y->dpram->fb.irq_time / 45.0; }
///double yssc2_debug(YSSC2 *y, int n)	{ return 0; } ///y->dpram->dbg[n]; }
double   yssc2_state(YSSC2 *y, int n)	{ return y->dpram->fb.servo_fb[n].state; }
unsigned yssc2_debug(YSSC2 *y, int n)	{ return y->dpram->fb.servo_fb[n].smth2; }
double yssc2_rx_time(YSSC2 *y, int n)	{ return y->dpram->fb.servo_fb[n].rxtime; }

int yssc2_has_fb(YSSC2 *y)		{ return (y->dpram->fb.seq & YS_FB) && 1; }
int yssc2_valid(YSSC2 *y, int a)	{ return (y->dpram->fb.valid & (1<<a)) && (y->dpram->fb.servo_fb[a].state & YF_VALID); }
int yssc2_feedback_res(YSSC2 *y, int a)	{ return  y->amp[a].fbres; }
int yssc2_cyc0(YSSC2 *y, int a)		{ return  y->amp[a].cyc0; }
int yssc2_abs0(YSSC2 *y, int a)		{ return  y->amp[a].abs0; }

int yssc2_online(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_ONLINE) && 1; }
int yssc2_ready(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_READY) && 1; }
int yssc2_enabled(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_ENABLED) && 1; }
int yssc2_in_position(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_IN_POSITION) && 1; }
int yssc2_zero_speed(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_ZERO_SPEED) && 1; }
int yssc2_at_speed(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_AT_SPEED) && 1; }
int yssc2_torque_clamped(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_TORQUE_LIM) && 1; }
int yssc2_alarm(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_ALARM) && 1; }
int yssc2_warning(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_WARNING) && 1; }
int yssc2_in_vel_ctl(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_VEL_CTL) && 1; }
int yssc2_absolute(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_ABS) && 1; }
int yssc2_abs_lost(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_ABS_LOST) && 1; }
int yssc2_oriented(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_ORIENTED) && 1; }
int yssc2_orienting(YSSC2 *y, int a)	{ return (y->dpram->fb.servo_fb[a].state & YF_ORIENTING) && 1; }
int yssc2_di(YSSC2 *y, int a, int n)	{ return (y->dpram->fb.servo_fb[a].state & (YF_DI1 << n)) && 1; }

int yssc2_droop(YSSC2 *y, int a)	{ return y->dpram->fb.servo_fb[a].droop; }
int yssc2_pos_fb(YSSC2 *y, int a)	{ return y->dpram->fb.servo_fb[a].pos; }
int yssc2_vel_fb(YSSC2 *y, int a)	{ return y->dpram->fb.servo_fb[a].vel; }
int yssc2_trq_fb(YSSC2 *y, int a)	{ return y->dpram->fb.servo_fb[a].trq; }
int yssc2_alarm_code(YSSC2 *y, int a)	{ return y->dpram->fb.servo_fb[a].alarm; }
void yssc2_pos_cmd(YSSC2 *y, int a, int p) { y->dpram->cmd.servo_cmd[a].pos = p; }
void yssc2_vel_cmd(YSSC2 *y, int a, int p) { y->dpram->cmd.servo_cmd[a].vel = p; }
void yssc2_vel_ctl(YSSC2 *y, int a, int e) { if (e) y->dpram->cmd.servo_cmd[a].flags |= YC_VEL_CTL; else y->dpram->cmd.servo_cmd[a].flags &= ~YC_VEL_CTL; }
void yssc2_power(YSSC2 *y, int a, int e)  { if (e) y->dpram->cmd.servo_cmd[a].flags |= YC_POWER; else y->dpram->cmd.servo_cmd[a].flags &= ~YC_POWER; }
void yssc2_enable(YSSC2 *y, int a, int e) { if (e) y->dpram->cmd.servo_cmd[a].flags |= YC_ENABLE; else y->dpram->cmd.servo_cmd[a].flags &= ~YC_ENABLE; }
void yssc2_reset_alarm(YSSC2 *y, int a, int e) { if (e) y->dpram->cmd.servo_cmd[a].flags |= YC_RST_ALARM; else y->dpram->cmd.servo_cmd[a].flags &= ~YC_RST_ALARM; }
void yssc2_limit_torque(YSSC2 *y, int a, int e) { if (e) y->dpram->cmd.servo_cmd[a].flags |= YC_LIM_TORQUE; else y->dpram->cmd.servo_cmd[a].flags &= ~YC_LIM_TORQUE; }
void yssc2_forward_torque(YSSC2 *y, int a, short t) { if (t < 1) t = 1; if (t > 0x4000) t = 0x4000; y->dpram->cmd.servo_cmd[a].fwd_trq = t; }
void yssc2_reverse_torque(YSSC2 *y, int a, short t) { if (t < 1) t = 1; if (t > 0x4000) t = 0x4000; y->dpram->cmd.servo_cmd[a].rev_trq = t; }
unsigned int yssc2_flags(YSSC2 *y, int a) { return y->dpram->fb.servo_fb[a].state; }

void yssc2_fwd(YSSC2 *y, int a, int e) { if (e) y->dpram->cmd.servo_cmd[a].flags |= YC_FWD; else y->dpram->cmd.servo_cmd[a].flags &= ~YC_FWD; }
void yssc2_rev(YSSC2 *y, int a, int e) { if (e) y->dpram->cmd.servo_cmd[a].flags |= YC_REV; else y->dpram->cmd.servo_cmd[a].flags &= ~YC_REV; }

void yssc2_orient(YSSC2 *y, int a, int e) { if (e) y->dpram->cmd.servo_cmd[a].flags |= YC_ORIENT; else y->dpram->cmd.servo_cmd[a].flags &= ~YC_ORIENT; }


#define YSSC2_VENDOR_ID 0x1067
#define YSSC2P_A_DEVICE_ID 0x55c2
#define YSSC3_VENDOR_ID 0x1067
#define YSSC3P_A_DEVICE_ID 0x55c3
#define YMTL2_VENDOR_ID 0x1313
#define YMTL2P_A_DEVICE_ID 0x0712
#define YSSC2_MAX_BOARDS 8

/*
int yssc2_homed(YSSC2 *y, int a);
int yssc2_not_homed(YSSC2 *y, int a);

int yssc2_param1_set(YSSC2 *y, int a);
int yssc2_param2_not_set(YSSC2 *y, int a);
int yssc2_param2_set(YSSC2 *y, int a);
int yssc2_param2_not_set(YSSC2 *y, int a);
*/

/*
void yssc2_torque_fwd(YSSC2 *y, int a, int e);
void yssc2_torque_rev(YSSC2 *y, int a, int e);
*/

int  yssc2_init(void);
int  yssc2_start(int instance, int maxdrives);
void yssc2_cleanup(void);
void yssc2_receive(YSSC2 *y);
void yssc2_process(YSSC2 *y);
int  yssc2_transmit(YSSC2 *y);
int  get_count(void);
YSSC2 *yssc2_board(int i);

uint32_t yssc2_gpi(YSSC2 *y) { return y->dpram->fb.gpi; }
void yssc2_gpo(YSSC2 *y, int a) { y->dpram->cmd.gpo = a; }

void yssc2_index_req(YSSC2 *y, int a, int n) {
	if (n) {
		y->dpram->cmd.seq |= YS_INDEX0 << a;
	} else {
		y->dpram->cmd.seq &= ~(YS_INDEX0 << a);
	}
}
int  yssc2_index_falling(YSSC2 *y, int a) {
	return (y->prev_fb_seq & (YS_INDEX0<<a)) && !(y->dpram->fb.seq & (YS_INDEX0<<a));
}
int  yssc2_enc(YSSC2 *y, int a) { return (y->dpram->fb.enc[a]); }

void yssc2_dac(YSSC2 *y, int a, uint16_t v) { y->dpram->cmd.dac[a] = v; }

uint32_t yssc2_yi(YSSC2 *y, int a) { return y->dpram->fb.yi[a]; }
void yssc2_yo(YSSC2 *y, int a, uint32_t v) { y->dpram->cmd.yo[a] = v; }

#endif
