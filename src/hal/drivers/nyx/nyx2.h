/*
 *  N Y X 2
 *
 *  YSSC2P/YMDS2 servo interface adapter board firmware
 *
 *  (c) 2016-2018, dmitry@yurtaev.com
 */

#define RELEASE

#ifndef __KERNEL__
#include <stdint.h>
#endif

#define _P __attribute__((__packed__))

// DPRAM description

#ifndef NYX_H
#define NYX_H

#define NYX_VER_MAJ 2
#define NYX_VER_MIN 2
#define NYX_VER_REV 0

#ifndef NYX_AXES
#define NYX_AXES 10
#endif

#define MAX_AXES 18

// per-axis nyx_servo_cmd.flags

#define YC_VALID	0x00000001
//#define YC_		0x00000002
//#define YC_		0x00000004
//#define YC_		0x00000008
//#define YC_		0x00000010
//#define YC_		0x00000020
#define YC_POWER	0x00000040
#define YC_ENABLE	0x00000080
#define YC_FWD		0x00000100	// MDS spindle
#define YC_REV		0x00000200	// MDS spindle
#define YC_LIM_TORQUE	0x00000400
#define YC_RST_ALARM	0x00000800
#define YC_RAPID	0x00001000	// set for MDS rapid moves?
#define YC_OD3		0x00002000	// MDS
#define YC_SPEC		0x00004000	// MDS
#define YC_REQ_HOME	0x00008000
#define YC_WR_PARAM	0x00010000
#define YC_PARAM_ACK	0x00020000	// param change notofy acknowledge
#define YC_RD_PARAM	0x00040000	// ??
#define YC_VEL_CTL	0x00080000
//#define YC_		0x00100000
//#define YC_		0x00200000
#define YC_ORIENT	0x00400000	// MDS spindle
//#define YC_		0x00800000

#define Y_TYPE		0xf0000000
#define Y_TYPE_NONE	0x00000000
#define Y_TYPE_PARAM	0x10000000	// number of params transfered per frame sscnet2:10, sscnet1:33, mds:20, sscnet3:16
#define Y_TYPE_ORIGIN	0x20000000	// absolute encoder position and resolution
#define Y_TYPE_FB	0x30000000	// normal servo feedback
#define Y_TYPE_PLL	0x40000000	// PLL timing debug

// per-axis nyx_servo_fb.state

#define YF_VALID	0x00000001	/* reply received in this cycle */
#define YF_DI1		0x00000002	/* J3/J4 digital inputs */
#define YF_DI2		0x00000004
#define YF_DI3		0x00000008
#define YF_ONLINE	0x00000010	/* drive configured/detected */
//#define YF_		0x00000020
//#define YF_		0x00000040
					/// servo reply-derived flags below:
#define YF_READY	0x00000080	/* power relay on */
#define YF_ENABLED	0x00000100	/* servo is on */
#define YF_IN_POSITION	0x00000200
#define YF_AT_SPEED	0x00000400
#define YF_ZERO_SPEED	0x00000800
#define YF_TORQUE_LIM	0x00001000
#define YF_ALARM	0x00002000
#define YF_WARNING	0x00004000
#define YF_ABS		0x00008000	/* absolute positioning mode */
#define YF_ABS_LOST	0x00010000	/* amp lost absolute position */
#define YF_HOME_ACK	0x00020000
#define YF_HOME_NACK	0x00040000
#define YF_PARAM_ACK	0x00080000
#define YF_PARAM_NFY	0x00100000	/* amplifier parameter w[10] changed to w[11] */
#define YF_VEL_CTL	0x00200000
#define YF_ORIENTED	0x00400000	/* spindle */
#define YF_ORIENTING	0x00800000
#define YF_FWD		0x01000000
#define YF_REV		0x02000000
#define YF_OD3		0x04000000
#define YF_SPEC		0x08000000

// global relpy status nyx_dpram.fb.seq

#define YS_SEQ		0x000000ff	/* sequence no */
#define YS_SYNCING	0x00000100	/* 0.88ms servo cycle synchronized */
#define YS_INSYNC	0x00000200	/* 0.88ms servo cycle synchronized */
#define YS_INSEQ	0x00000400	/* previous request was processed */
#define YS_FB		0x00000800	// there's a feedback update in this cycle

// those also go to nyx_dpram.cmd.seq
#define YS_INDEX0	0x00001000	/* spindle encoder index request */
#define YS_INDEX1	0x00002000
#define YS_INDEX2	0x00004000
#define YS_INDEX3	0x00008000
#define YS_INDEXI	0x0000F000

//////////////////////////////////

typedef struct nyx_servo_cmd {
	uint32_t flags;			// 1 YC_something

	int32_t pos;			// 2
	int32_t vel;			// 3
#ifdef __BIG_ENDIAN__
	uint16_t fwd_trq, rev_trq;	// 4
	uint16_t pno1, pval1;		// 5
	uint16_t pno2, pval2;		// 6
#else
	uint16_t rev_trq, fwd_trq;
	uint16_t pval1, pno1;
	uint16_t pval2, pno2;
#endif
	union {
//		uint8_t zb[16];
//		uint16_t zw[8];
		uint32_t zl[2];		// 7,8
	};
} _P nyx_servo_cmd;	// 8 dwords

//
// goes into nyx_servo_cmd and nyx_servo_fb when Y_TYPE_PARAM
//

typedef struct nyx_param_req {
	uint32_t flags;			// 1 YC_something
#ifdef __BIG_ENDIAN__
	uint16_t group, first;
	uint16_t count, mask;
#else
	uint16_t first, group;
	uint16_t mask, count;
#endif
	uint16_t param[10];
} _P nyx_param_req;	// 8 dwords

//
//
//

typedef struct nyx_servo_fb {
	uint32_t state;		// 1 4 low bits - counter
	int32_t pos;		// 2
	int32_t vel;		// 3
#ifdef __BIG_ENDIAN__
	int16_t trq;
	uint16_t alarm;		// 4
	uint16_t pno, pval;	// 6
#else
	uint16_t alarm;
	int16_t trq;
	uint16_t pval, pno;
#endif
	union {
		struct {		// Y_TYPE_FB
			uint32_t fbres;	// 6
			uint32_t cyc0;	// 7
			uint32_t abs0;	// 8
		};
		uint8_t monb[12];	// 6 7 8
		uint16_t monw[6];
		uint32_t monl[3];
		struct {		// Y_TYPE_ORIGIN
			int32_t droop;	// 6
			int32_t smth2;	// 7
			int32_t rxtime;	// 8 !!!DEBUG!!!
		};
	};
} _P nyx_servo_fb;	// 8 dwords 32 bytes

//////////////////////////////////

#define REQ_NOP		0x00000000
#define REQ_INFO	0x00010000
#define REQ_PARAM	0x00020000
#define REQ_SERVO	0x00030000
#define REQ_EXP		0x00040000
#define REQ_FLASH	0x00050000
#define REQ_REBOOT	0x00060000
#define REQ_SNOOP	0x00070000
#define REQ_DNA		0x00080000
#define REQ_PLL		0x00090000

#define REQ_FUNC	0x0000ffff
#define REQ_CODE	0xffff0000

#define FUNC_INFO	0x0001
#define FUNC_ENABLE	0x0002	/* init */
#define FUNC_DISABLE	0x0003	/* cleanup */
#define FUNC_START	0x0004	/* start discovery */
#define FUNC_STOP	0x0005
#define FUNC_RUN	0x0006	/* init servos, go to position control */
#define FUNC_FW		0x0007

#define SERVO_SSCNET	1
#define SERVO_SSCNET2	2
#define SERVO_SSCNET3	3
#define SERVO_MLINK2	4
#define SERVO_MDS	11

#define FUNC_SV_GET	0x0011
#define FUNC_SV_SET	0x0012
#define FUNC_SP_GET	0x0013
#define FUNC_SP_SET	0x0014
#define FUNC_LOAD	0x0015
#define FUNC_SAVE	0x0016
#define FUNC_PMASK_GET	0x0017
#define FUNC_PMASK_SET	0x0018

#define FUNC_READ	0x0021
#define FUNC_ERASE	0x0022
#define FUNC_WRITE	0x0023

struct nyx_req {
	volatile uint32_t code;
	uint32_t arg1;
	uint32_t arg2;
	union {
		uint32_t arg3;
		uint32_t len;
	};
	union {
		uint8_t byte[4*120];
		uint16_t word[2*120];
		uint32_t dword[1*120];
	};
} _P;

struct nyx_req_param {			// unused
	volatile uint32_t code;
	uint32_t axis;
	uint32_t first;
	uint32_t count;
	uint16_t param[240];
} _P;

struct nyx_req_flash {
	volatile uint32_t code;
	uint32_t password;		// not used
	uint32_t addr;
	uint32_t len;
	uint8_t buf[480];
} _P;

typedef struct nyx_snoop {
	uint32_t _empty[3];
	volatile uint32_t seq;
	uint32_t len;
	uint8_t buf[768-5*4];
} _P nyx_snoop;

//
//
//

/*
servo:	16 ax * 32 bytes = 512 bytes
yio:	16 ax * 4 bytes = 64 bytes

*/
typedef struct nyx_dp_fb {
	// realtime controller status
	uint32_t seq;		// YS_ goes here
	int32_t irq_time;
	uint32_t valid;		// servo_fb valid bitmap
	//
	uint32_t gpi;		// base card inputs 0-12, YSSC3: 31..24 outputs open load, 23..16: outputs overload
	uint32_t enc[2];
	uint32_t yi[16];	// YIO inputs
	// 22 dwords
	nyx_servo_fb servo_fb[MAX_AXES];	// *18 = 144 dw
	// 26 dwords free
} _P nyx_dp_fb;	// 192 dwords max


typedef struct nyx_dp_cmd {
	uint32_t seq;
	uint32_t gpo;		// base card outputs 0-7
	uint16_t dac[2];
	uint32_t yo[16];	// YIO outputs
	// 19 dwords
	nyx_servo_cmd servo_cmd[MAX_AXES];
} _P nyx_dp_cmd;

//

typedef struct nyx_dp_debug {
	uint32_t magic;
	uint32_t t_recv_start;
	uint32_t t_recv_end;
	uint32_t t_sync_start;
	uint32_t t_sync_end;
	uint32_t t_proc_start;
	uint32_t t_proc_end;
	uint32_t t_send_start;
	uint32_t t_send_end;
	uint32_t dpll_shift;
	uint32_t cmd_seq_miss;	// number of out-of-seq cmd's
} _P nyx_dp_debug;

// controller status
#define STATUS_REALTIME		0x01
#define STATUS_READY		0x02
#define STATUS_REQ_COMPLETE	0x04
#define STATUS_REQ_ERROR	0x08
#define STATUS_INSYNC		0x10

typedef struct nyx_dpram {
	union {
		struct {
			uint32_t magic;		// 55c20201
			uint32_t config;	// YIO_AXES:8 NYX_AXES:8
			volatile uint32_t status;	// STATUS_xxx
			uint32_t reserved;
			union {
				struct nyx_req req;
				struct nyx_req_flash flash;
				struct nyx_req_param param;
			};
		};
		uint8_t reqpage[512];
	};
	union {				// host <- board
		uint8_t fbpage[768];
		nyx_dp_fb fb;
		nyx_snoop snoop;
	};
	union {				// host -> board
		uint8_t cmdpage[768];
		nyx_dp_cmd cmd;
	};
} _P nyx_dpram;

//
// PCI I/O memory
//

typedef struct nyx_iomem {
	union {
		struct {
			volatile uint32_t jtag;
			uint32_t dma_dst;       // 4
			uint32_t dma_src;       // 8
			uint32_t dma_len;       // c
		};
	        uint32_t page0[512];	// align @2K
	};
	struct nyx_dpram dpram;
} _P nyx_iomem;

#endif
