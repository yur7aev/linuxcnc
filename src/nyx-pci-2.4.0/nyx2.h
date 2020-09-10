/*
 *  N Y X 2
 *
 *  YxxxxP servo interface adapter board firmware
 *
 *  DPRAM definition
 *
 *  (c) 2016-2020, http://yurtaev.com
 */

#define RELEASE

#ifndef __KERNEL__
#include <stdint.h>
#endif

#define _P __attribute__((__packed__))

#ifndef NYX_H
#define NYX_H

#define NYX_VER_MAJ 2
#define NYX_VER_MIN 4
#define NYX_VER_REV 3

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
#define YC_VFF		0x00100000      // ----Y M-II velocity feed-forward
#define YC_RST_Z	0x00200000	// z passed flag reset
#define YC_ORIENT	0x00400000	// MDS spindle
//#define YC_		0x00800000

#define Y_TYPE		0xf0000000
#define Y_TYPE_NONE	0x00000000
#define Y_TYPE_PARAM	0x10000000	// number of params transfered per frame sscnet2:10, sscnet1:33, mds:20, sscnet3:16, mds3:16,
#define Y_TYPE_ORIGIN	0x20000000	// absolute encoder position and resolution
#define Y_TYPE_FB	0x30000000	// normal servo feedback
#define Y_TYPE_PLL	0x40000000	// PLL timing debug
#define Y_TYPE_PARAM1	0x50000000	// mechatrolink - 1 param at a time

// per-axis nyx_servo_fb.state

#define YF_VALID	0x00000001	/* reply received in this cycle */
#define YF_DI1		0x00000002	/* J3/J4 digital inputs */
#define YF_DI2		0x00000004
#define YF_DI3		0x00000008
#define YF_ONLINE	0x00000010	/* drive configured/detected */
#define YF_DBG		0x00000020
					/// servo reply-derived flags below:
#define YF_Z_PASSED	0x00000040
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
	uint32_t flags;			// Y_TYPE_PARAM
#ifdef __BIG_ENDIAN__
	uint16_t group, first;
	uint16_t count, mask;
#else
	uint16_t first, group;
	uint16_t mask, count;
#endif
	uint16_t param[10];
} _P nyx_param_req;	// 8 dwords

// ask the nyx driver for up to 6 defined params

typedef struct nyx_param1_req {
	uint32_t flags;			// Y_TYPE_PARAM1
#ifdef __BIG_ENDIAN__
	uint16_t no, first;
	uint16_t count, size;
#else
	uint16_t first, no;
	uint16_t size, count;
#endif
	uint32_t val;
	uint32_t _unused[4];
} _P nyx_param1_req;	// 8 dwords

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
		struct {		// Y_TYPE_ORIGIN
			uint32_t fbres;	// 6
			uint32_t cyc0;	// 7
			uint32_t abs0;	// 8
		};
		uint8_t monb[12];	// 6 7 8
		uint16_t monw[6];
		uint32_t monl[3];
		struct {		// Y_TYPE_FB
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
#define REQ_EXCFG	0x00040000
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
#define FUNC_ALARM	0x0008

#define SERVO_SSCNET	1
#define SERVO_SSCNET2	2
#define SERVO_SSCNET3	3
#define SERVO_MTL2	4
#define SERVO_SSCNET3H	5
#define SERVO_MTL1	6
#define SERVO_MDS	11
#define SERVO_MDS3	13
#define SERVO_MDS3H	15

#define FUNC_RD_PARAM	0x0011
#define FUNC_WR_PARAM	0x0012
#define FUNC_LOAD	0x0015
#define FUNC_SAVE	0x0016
#define FUNC_PMASK_GET	0x0017
#define FUNC_PMASK_SET	0x0018
#define FUNC_ABS_INIT	0x0019
#define FUNC_WRNV_PARAM	0x001a

#define FUNC_READ	0x0021
#define FUNC_ERASE	0x0022
#define FUNC_WRITE	0x0023

// CN2 expansion connector config

#define EX_NONE		0x0000
#define EX_DAC		0x0001	// 0=SCK 1=nCS     3=SDI
#define EX_ENC0		0x0002	//             2:A       4:B 5:Z
#define EX_YIO		0x0004	//                               6:RX 7=TE 8=TX   (ex1/ex2it)

#define EX_DBG		0x0010	// 9:RX 10=TX
#define EX_ENC1		0x0020	//                        13:A1 14:B1 15:Z1
#define EX_1A		0x0040	//            11=1
#define EX_1ASTEP	0x0080	//            11=0  12=EN 13=D0 14=S0 15=D1 16=S1

#define EX_ENC23	0x0100	// 9:A2 10:B2 11:A3 12:B3
#define EX_ENC45	0x0200	//                        13:A4 14:B4 15:A5 16:B5

#define EX_2ENC0	0x1000	// 0:A  1=0   2=0  3:B   4:Z	                  (ex2+it)

#define EX_2YIO		0x0000	//                              14=TX 15:RX 16=TE (ex2 header)
#define EX_2OBA2	0x0000	//                                6:MD 7=MRE 8=MR (with yio_enc)
#define EX_2OBA4	0x0000	//                           5:MD      7=MRE 8=MR
#define EX_2UVW		0x0000	//                           5:U  6:V  7=0   8=0  9=W
#define EX_2SSC		0x0000	//      1=1  2=TX 3:ALM  4:RX          7=1   8=EMG
#define EX_2A		0x0000	//      leds...

#define EX1_A		(EX_DAC | EX_ENC0  | EX_YIO | EX_1A | EX_ENC1)	// 67
#define EX1_B		(EX_DAC | EX_ENC0  | EX_YIO |         EX_ENC1)	// 27
#define EX2_A		(         EX_2ENC0 | EX_YIO | EX_2A)		// 1002
#define EX1_A_PNP	(EX_YIO | EXSTEP1A)				// 84

//

#define ERR_BAD_CODE	1	/* invalid request code */
#define ERR_BAD_FUNC	2	/* invalid function */
#define ERR_BAD_AXIS	3	/* axis no out of range */
#define ERR_BAD_ARG	4	/*  */
#define ERR_NO_AXIS	5	/* axis disconnected */
#define ERR_FAILED	6	/* amplifier error */
#define ERR_UNAVAIL	7	/* unavailable (yet) */
#define ERR_TIMEOUT	8	/* param read timeout */

struct nyx_req {
	volatile uint32_t code;		// 10
	uint32_t arg1;			// 14
	uint32_t arg2;			// 18
	union {
		uint32_t arg3;		// 1c
		uint32_t len;
	};
	union {
		uint8_t byte[4*120];	// 20
		uint16_t word[2*120];
		uint32_t dword[1*120];
	};
} _P;

struct nyx_req_param {
	volatile uint32_t code;
	volatile uint32_t axes;			// mask
	volatile uint32_t rc;
	volatile uint32_t rc2;
	volatile uint32_t pno[16];
	volatile uint32_t pval[16];
	volatile uint32_t pno2[16];
	volatile uint32_t pval2[16];
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

#define MAX_ENC 2
#define YIO_ENC 4
#define MAX_DAC 2
#define MAX_YIO 16

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
	nyx_servo_fb servo_fb[MAX_AXES];	// 8*18 = 144 dw
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

// controller status
#define STATUS_REALTIME		0x01
#define STATUS_READY		0x02
#define STATUS_REQ_COMPLETE	0x04
#define STATUS_REQ_ERROR	0x08
#define STATUS_INSYNC		0x10

typedef struct nyx_dpram {
	union {
		struct {
			uint32_t magic;			// 55c20204
			uint32_t config;		// +4 YIO_AXES:8 NYX_AXES:8
			volatile uint32_t status;	// +8 STATUS_xxx
			uint32_t reserved;		// +c
			union {
				struct nyx_req req;
				struct nyx_req_flash flash;
				struct nyx_req_param req_param;
			};
		};
		uint8_t reqpage[512];
	};
	union {				// host <- board
		uint8_t fbpage[768];	// 192 dwords
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

#endif // NYX_H
