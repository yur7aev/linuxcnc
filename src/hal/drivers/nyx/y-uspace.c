/*
 * N Y X
 *
 * (c) 2016, dmitry@yurtaev.com
 */

//#include <stdlib.h>
//#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <rtapi_slab.h>
#include <rtapi_io.h>
//u//#include <linux/byteorder/generic.h>
//u//#include <linux/string.h>
//i//#include <linux/delay.h>

//#define be32toh be32_to_cpu

#include "y.h"

static YSSC2 yssc2_brd[YSSC2_MAX_BOARDS];
static unsigned int num_boards = 0;

//int yssc2_board_count()
int get_count(void)
{
	return num_boards;
}

YSSC2 *yssc2_board(int i)
{
	if(i >= num_boards) return NULL;
	return &yssc2_brd[i];
}

void freq_init(YSSC2 *y)
{
	y->freq.i =
	y->freq.j =
	y->freq.bucket =
	y->freq.sum = 0;
}

/*
{
		uint32_t magic = y->iomem->dpram.magic;
		int rev_min = magic & 0xff;
		int rev_maj = (magic & 0xff00) >> 8;

		if (magic != (0x55c20000 | (NYX_VER_MAJ<<8) | NYX_VER_MIN)) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx: this driver v%d.%d is not compatible with YSSCxP (%x) v%d.%d at %s\n",
					NYX_VER_MAJ, NYX_VER_MIN, magic, rev_maj, rev_min, rtapi_pci_name(dev));
			r = -ENODEV;
			goto fail1;
		}

	y->initial_delay = 1125 * 3;	// time to wait for sync
	y->errors_shown = 0;
	freq_init(y);
	y->was_ready = 0;
	memset(y->index_req, 0, sizeof(y->index_req));

	y->dev = dev;
	rtapi_pci_set_drvdata(dev, y);

	y->dpram->magic = y->iomem->dpram.magic;
	y->dpram->config = y->iomem->dpram.config;

	++num_boards;
	return 0;
}
*/

int load_params(struct servo_params *p, const char *name, int axes);
void free_params(struct servo_params *p, int axes);

int yssc2_init()
{
	YSSC2 *y = &yssc2_brd[0];

	y->fd = open("/dev/nyx0", O_RDWR);
	if (y->fd < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: can't open /dev/nyx0");
		return -1;
	}

	num_boards = 1;
	y->dpram = calloc(sizeof(nyx_dpram), 1);
	read(y->fd, y->dpram, 16);

	y->axes = y->dpram->config & 0xff;
	if (y->axes > NYX_AXES) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: card has %d axes, limited to %d", y->axes, NYX_AXES);
		y->axes = NYX_AXES;
	}

//	rtapi_print_msg(RTAPI_MSG_ERR, "nyx: magic: %x, %d axes", y->dpram->magic, y->axes );

	y->initial_delay = 1125 * 3;	// time to wait for sync
	y->errors_shown = 0;
	freq_init(y);
	y->was_ready = 0;
	memset(y->index_req, 0, sizeof(y->index_req));

	return 0;
}

void yssc2_cleanup()
{
	int i;

	for (i = 0; i < num_boards; i++) {
		YSSC2 *y = &yssc2_brd[i];
		free_params(y->par, 16);
		close(y->fd);
		free(y->dpram);
	}

}

int yssc2_start(int no, int maxdrives)
{
	int i;
	YSSC2 *y = &yssc2_brd[no];

	memset(y->par, 0, sizeof(*(y->par)));

	for (i = 0; i <= no; i++) {
		if(param_file[i] == NULL) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx.%d: no params", no);
			return -1;
		}
	}

	if (y->axes > maxdrives) y->axes = maxdrives;
	load_params(y->par, param_file[no], y->axes);

	return 0;
}

/*
#include <linux/fs.h>      // Needed by filp
#include <asm/uaccess.h>   // Needed by segment descriptors
*/

int load_params(struct servo_params *p, const char *name, int axes)
{
//	struct file *f;
	int fd;

//	f = filp_open(name, O_RDONLY, 0);
	fd = open(name, O_RDONLY);
//	if (!IS_ERR(f)) {
	if (fd >= 0) {
		int a, i, l;
		ssize_t s;
//		mm_segment_t fs;

//		fs = get_fs();		// Get current segment descriptor
//		set_fs(get_ds());	// Set segment descriptor associated to kernel space

		for (a = 0; a < axes; a++, p++) {
			memset(p, 0, sizeof(*p));

//			s = f->f_op->read(f, (char *)&p->magic, 6, &f->f_pos);	// read the header
			s = read(fd, (char *)&p->magic, 6);	// read the header
			if (s != 6) {
				if (s) rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params short read 1, axis=%d", a);
				goto skip;
			} else if (p->magic != ' RAP') {
				rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params no magic");
				goto skip;
			} else if (p->ng < 1) {
				rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params ng < 1");
				goto skip;
			} else if (p->ng > 16) {
				rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params ng > 16");
				goto skip;
			}

//			s = f->f_op->read(f, (char *)&p->np, 2*p->ng, &f->f_pos);	// read number of params in each of ng group
			s = read(fd, (char *)&p->np, 2*p->ng);	// read number of params in each of ng group
			if (s != 2*p->ng) {
				rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params short read 2");
				goto skip;
			}

			for (i = l = 0; i < p->ng; i++) {		// calc space needed for params+masks
				if (p->np[i] > 1024) {
					rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params too many params");
					goto skip;
				}
				l += (p->np[i] + 15) / 16 + p->np[i];	// mask data + param data
			}

			l *= 2;					// shorts

			//p->buf = rtapi_kmalloc(l, GFP_KERNEL);
			p->buf = malloc(l);
			if (!p->buf) {
				rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params kalloc");
				goto skip;
			}

//			s = f->f_op->read(f, (char *)p->buf, l, &f->f_pos);		// read param data
			s = read(fd, (char *)p->buf, l);		// read param data
			if (s != l) {
				rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params short read 3");
				goto skip;
			}

			for (i = l = 0; i < p->ng; i++) {		// fill pointer arrays
				p->mask[i] = &p->buf[l];
				l += (p->np[i] + 15) / 16;
				p->par[i] = &p->buf[l];
				l += p->np[i];
			}

			continue;
skip:			memset(p, 0, sizeof(*p));
		}

//		set_fs(fs);	        // Restore segment descriptor
//		filp_close(f, NULL);
		close(fd);
	} else {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params can't open %s", name);
	}
	return 0;
}

void free_params(struct servo_params *p, int axes) {
	int a;

	for (a = 0; a < axes; a++, p++) {
		if (p) {
			//if (p->buf) rtapi_kfree(p->buf);
			if (p->buf) free(p->buf);
			memset(p, 0, sizeof(*p));
		}
	}
}

int get_params(struct servo_params *p, int group, int first, int count, uint16_t *dm, uint16_t *dp)
{
	int i;

	memset(dp, 0, count*2);
	if (dm) memset(dm, 0, 2*(count/16+1));

	if (group > 15) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:get_params group > 15");
		return 0;
	}

	if (first + count > p->np[group])
		count = p->np[group] - first;

	for (i = 0; i < count; i++) {
		dp[i] = p->par[group][first + i];	// endianness...
	}

	if (dm && count > 0) {
		int n;
		uint16_t m;
		for (i = 0; i <= (count - 1)/16; i++) {
			m = ((p->mask[group][i+first/16+1] << 16) | p->mask[group][i+first/16]) >> (first % 16);
			n = count - i*16;
			if (n < 16) m &= (1<<n)-1;
			*dm++ = m;
		}
	}

	return count;
}

#include <stddef.h>

void yssc2_receive(YSSC2 *y)
{
	int rc;
	if (y == NULL) return;

	lseek(y->fd, offsetof(struct nyx_dpram, fb), SEEK_SET);
	//read(y->fd, &y->dpram->fb, offsetof(struct nyx_dp_fb, servo_fb) + sizeof(nyx_servo_fb) * y->axes);
	rc = read(y->fd, &y->dpram->fb, offsetof(struct nyx_dp_fb, servo_fb) + sizeof(nyx_servo_fb) * y->axes);
	if (rc <= 0)
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:read err %d", rc);
	y->_status_falling = y->dpram->cmd.seq & ~y->dpram->fb.seq;
	y->dpram->cmd.seq = y->dpram->fb.seq;
}

void yssc2_process(YSSC2 *y)
{
	int a;
	for (a = 0; a < y->axes; a++) {
//		if (yssc2_online(y, a)) {
			nyx_servo_fb *fb = &y->dpram->fb.servo_fb[a];
			nyx_servo_cmd *cmd = &y->dpram->cmd.servo_cmd[a];

			switch (fb->state & Y_TYPE) {
			case Y_TYPE_ORIGIN:
				y->amp[a].fbres = fb->fbres;
				y->amp[a].cyc0 = fb->cyc0;
				y->amp[a].abs0 = fb->abs0;
				//rtapi_print_msg(RTAPI_MSG_ERR, "nyx: #%d fbres=%d", a, y->amp[a].fbres);
				break;
			case Y_TYPE_PARAM: {
				nyx_param_req *pq = (nyx_param_req*)fb;
				nyx_param_req *pr = (nyx_param_req*)cmd;
				pr->flags = Y_TYPE_PARAM;
				get_params(&y->par[a], pq->first >> 10, pq->first & 0x3ff, 10, &pr->mask, pr->param);
				pr->first = pq->first;
				pr->count = 10;
				//rtapi_print_msg(RTAPI_MSG_ERR, "nyx: #%d param req %x\n", a, pq->first);
				}
				break;
			}

				if (yssc2_valid(y, a)) {
					if (y->amp[a].fbres == 0) {	// cant start until feedback resolution and origin is known
						cmd->flags = (cmd->flags & ~Y_TYPE) | Y_TYPE_ORIGIN;
						fb->state &= ~YF_VALID;
					} else {
						cmd->flags = (cmd->flags & ~Y_TYPE) | Y_TYPE_FB;
					}
				}
//		}
	}
}

int yssc2_transmit(YSSC2 *y)
{
	int rc;
	if (y == NULL) return -1;

	lseek(y->fd, offsetof(struct nyx_dpram, cmd), SEEK_SET);
	//write(y->fd, &y->dpram->fb, offsetof(struct nyx_dp_cmd, servo_cmd) + sizeof(nyx_servo_cmd) * y->axes);
	rc = write(y->fd, &y->dpram->cmd, offsetof(struct nyx_dp_cmd, servo_cmd) + sizeof(nyx_servo_cmd) * y->axes);
	if (rc <= 0)
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:write %d\n", rc);

	return 0;
}
