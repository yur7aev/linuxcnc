/*
 *  N Y X
 *
 *  (c) 2016, dmitry@yurtaev.com
 *
 *  YSSC2P-A SSCNET2 interface board firmware
 *
 *  *** P R E L I M I N A R Y ***
 */

// DPRAM description

#ifndef NYX_H
#define NYX_H

#define NYX_VER_MAJ 1
#define NYX_VER_MIN 3

#define NYX_AXES 6

// command

#define YC_U1		0x00000010
#define YC_U2		0x00000020
#define YC_POWER	0x00000040
#define YC_ENABLE	0x00000080
#define YC_U5		0x00000100
#define YC_U6		0x00000200
#define YC_LIM_TORQUE	0x00000400
#define YC_RST_ALARM	0x00000800
#define YC_U9		0x00001000
#define YC_U10		0x00002000
#define YC_U11		0x00004000
#define YC_REQ_HOME	0x00008000
#define YC_WR_PARAM	0x00010000
#define YC_PARAM_ACK	0x00020000
#define YC_U15		0x00040000
#define YC_VEL_CTL	0x00080000
#define YC_REQ_INDEX0	0x00100000
#define YC_REQ_INDEX1	0x00200000

// reply

#define YR_PRESENT	0x00000010	/* drive detected */
#define YR_INSYNC	0x00000020	/* 0.88ms servo cycle synchronized */
#define YR_READY	0x00000040	/* power relay on */
#define YR_ENABLED	0x00000080	/* servo is on */
#define YR_IN_POSITION	0x00000100	/* in position/at speed */
#define YR_AT_SPEED	0x00000100
#define YR_ZERO_SPEED	0x00000200	/* zero speed */
#define YR_TORQUE_LIM	0x00000400
#define YR_ALARM	0x00000800
#define YR_WARNING	0x00001000
#define YR_ABS		0x00002000	/* absolute positioning mode */
#define YR_ABS_LOST	0x00004000
#define YR_HOME_ACK	0x00008000
#define YR_HOME_NACK	0x00010000
#define YR_PARAM_ACK	0x00020000
#define YR_PARAM_NFY	0x00040000	/* amplifier parameter w[10] changed to w[11] */
#define YR_VEL_CTL	0x00080000
#define YR_INDEX0	0x00100000	/* spindle encoder index request */
#define YR_INDEX1	0x00200000
#define YR_U18		0x00400000
#define YR_U19		0x00800000
#define YR_U20		0x01000000
#define YR_U21		0x02000000
#define YR_U22		0x04000000
#define YR_U23		0x08000000
#define YR_VALID	0x10000000

#define Y_SEQ		0x0000000F

#define YS_SYNCING	0x00000010	/* 0.88ms servo cycle synchronized */
#define YS_INSYNC	0x00000020	/* 0.88ms servo cycle synchronized */
#define YS_INSEQ	0x00000040	/* previous request was processed */

#define YS_INDEX0	0x00000100	/* spindle encoder index request */
#define YS_INDEX1	0x00000200
#define YS_INDEX2	0x00000400
#define YS_INDEX3	0x00000800
#define YS_INDEXI	0x00000F00

typedef struct nyx_cmd {	// sscnet command
	uint32_t flags;	// YC_something
	int32_t cmd;	// context-specific, depends on monitor values and parameter notifications
	uint16_t fwd_trq, rev_trq;
	union {
		uint8_t zb[20];
		uint16_t zw[10];
		uint32_t zl[5];
	};
} __attribute__((__packed__)) nyx_cmd;	// 8 dwords

typedef struct nyx_fb {
	uint32_t state;		// 4 low bits - counter
	int32_t pos;
	int32_t vel;
	int32_t trq;
	int32_t alarm;

	union {
		uint8_t b[16];
		uint16_t w[8];
		uint32_t l[4];
		int32_t droop;
	};
} __attribute__((__packed__)) nyx_fb;	// 9 dwors

typedef struct nyx_dpram {
	uint32_t magic;		// 55c2000c
	uint32_t seq;		// 4
	uint32_t status;	// 8
	uint32_t req;		// c

	nyx_cmd cmd[NYX_AXES];	// 4*8*6 = 192 bytes
	nyx_fb fb[NYX_AXES];	// offs:D0 4*9*6 = 216(d8) bytes

	int32_t irq_time;	// 1a8=424
	uint32_t gpo;		// 1ac
	uint32_t gpi;		// 1b0
	uint32_t enc[1];	// 1b4
	uint16_t dac[2];	// 1b8

	struct nyx_amp {
		uint32_t fb_res;
		uint32_t enc_res;
		uint32_t cyc0;
		int32_t abs0;	// 16 bits really
		uint32_t pol;
		char fw[16];	// = 216(d8) bytes
	} amp[NYX_AXES];	// ???28c=652

	int32_t dbg[4];		// controller -> cpu
} __attribute__((__packed__)) nyx_dpram;

typedef struct nyx_dpram_mon {
	uint32_t magic;		// 55c2ffff
	uint32_t reply;		// seq
	uint32_t cmd;		// status
	uint32_t addr;		// req

	uint32_t len;
	uint32_t status;

	uint32_t _unused[256-6];
	uint8_t data[256];	// align @1K offset
	uint8_t data2[256];
} __attribute__((__packed__)) nyx_dpram_mon;

typedef struct nyx_iomem {
	uint32_t jtag;
        uint32_t dma_dst;       // 4
        uint32_t dma_src;       // 8
        uint32_t dma_len;       // c
        uint32_t _unused_10[512-4];	// align @2K
	union {
		nyx_dpram dpram;
		nyx_dpram_mon dpram_mon;
	};
} __attribute__((__packed__)) nyx_iomem;

typedef struct nyx_ioport {
	uint32_t magic;
	uint32_t nyxctl;
/*	struct epp_regs {
		uint8_t data;
		uint8_t status;
		uint8_t ctl;
		uint8_t epp_addr;
		uint8_t epp_data;
		uint8_t _pad[3];
	} lpt[8];
*/
} nyx_ioport;

#endif
