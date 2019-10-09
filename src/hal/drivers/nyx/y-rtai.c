/*
 * N Y X
 *
 * (c) 2016, dmitry@yurtaev.com
 */

//#include <stdlib.h>
//#include <string.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <sys/mman.h>
//#include <fcntl.h>
#include <rtapi_pci.h>
#include <linux/byteorder/generic.h>
#include <linux/string.h>
#include <linux/delay.h>

#define be32toh be32_to_cpu

#include "y.h"

static YSSC2 yssc2_brd[YSSC2_MAX_BOARDS];
static unsigned int num_boards = 0;

static struct rtapi_pci_device_id yssc2_pci_tbl[] = {
	{
		.vendor = YSSC3_VENDOR_ID,
		.device = YSSC3P_A_DEVICE_ID,
		.subvendor = YSSC3_VENDOR_ID,
		.subdevice = YSSC3P_A_DEVICE_ID,
	},
	{
		.vendor = YSSC2_VENDOR_ID,
		.device = YSSC2P_A_DEVICE_ID,
		.subvendor = YSSC2_VENDOR_ID,
		.subdevice = YSSC2P_A_DEVICE_ID,
	},
	{0,},
};

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

static int yssc2_pci_probe(struct rtapi_pci_dev *dev, const struct rtapi_pci_device_id *id) {
	int r;
	YSSC2 *y;

	if (num_boards >= YSSC2_MAX_BOARDS) {
		rtapi_print_msg(RTAPI_MSG_WARN, "nyx: skipping board %d\n", num_boards);
		return -EINVAL;
	}

	if (rtapi_pci_enable_device(dev)) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: failed to enable PCI device %s\n", rtapi_pci_name(dev));
		return -ENODEV;
	}

	y = &yssc2_brd[num_boards];
	memset(y, 0, sizeof(YSSC2));
	y->iomem = rtapi_pci_ioremap_bar(dev, 0);
	y->iolen = rtapi_pci_resource_len(dev, 0);


        if (y->iomem == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: could not map %s PCI address space\n", rtapi_pci_name(dev));
		r = -ENODEV;
		goto fail0;
	} else {
		uint32_t magic = y->iomem->dpram.magic;
		int rev_min = magic & 0xff;
		int rev_maj = (magic & 0xff00) >> 8;

		if (magic != (0x55c20000 | (NYX_VER_MAJ<<8) | NYX_VER_MIN)) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx: this driver v%d.%d is not compatible with YSSCxP (%x) v%d.%d at %s\n",
					NYX_VER_MAJ, NYX_VER_MIN, magic, rev_maj, rev_min, rtapi_pci_name(dev));
			r = -ENODEV;
			goto fail1;
		}


		rtapi_print_msg(RTAPI_MSG_INFO, "nyx: YSSC3P-A v%d.%d #%d at %s, mapped to %p..%p, y=%p\n", rev_maj, rev_min,
			num_boards, rtapi_pci_name(dev), y->iomem, y->iomem - y->iolen, y);
	}

	y->dpram = (nyx_dpram *)dma_alloc_coherent(&dev->dev, sizeof(nyx_dpram), &y->dpram_bus_addr, 0);
	{
		size_t offs = offsetof(struct nyx_dpram, fb);	// should be 512
		rtapi_print_msg(RTAPI_MSG_INFO, "nyx: DMA mem: vm:%lx bus:%x offs:%x\n", (uintptr_t)y->dpram, y->dpram_bus_addr, offs);
	}

	if (y->dpram == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: can't allocate DMA memory: vm:%lx bus:%x\n", (uintptr_t)y->dpram, y->dpram_bus_addr);
		goto fail1;
	}

	// period - in ns
	y->initial_delay = 0;
	y->errors_shown = 0;
	freq_init(y);
	y->was_ready = 0;
	y->prev_fb_seq = 0;

	y->dev = dev;
	rtapi_pci_set_drvdata(dev, y);

	y->dpram->magic = y->iomem->dpram.magic;
	y->dpram->config = y->iomem->dpram.config;

	y->axes = y->dpram->config & 0xff;
	if (y->axes > NYX_AXES) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: card has %d axes, limited to %d", y->axes, NYX_AXES);
		y->axes = NYX_AXES;
	}

	++num_boards;
	return 0;

fail1:	rtapi_pci_set_drvdata(dev, NULL);
	rtapi_iounmap(y->iomem);
	y->iomem = NULL;

fail0:  rtapi_pci_disable_device(dev);
	return r;
}

static void yssc2_pci_remove(struct rtapi_pci_dev *dev) {
	YSSC2 *y = pci_get_drvdata(dev); //dev->driver_data;

	if (y != NULL) {
		if (y->dpram != NULL) {
			dma_free_coherent(&dev->dev, sizeof(nyx_dpram), y->dpram, y->dpram_bus_addr);
			y->dpram = NULL;
		}
		if (y->iomem != NULL) {
			rtapi_iounmap(y->iomem);
			y->iomem = NULL;
		}
		y->dev = NULL;
	}

	rtapi_pci_disable_device(dev);
	rtapi_pci_set_drvdata(dev, NULL);
}

static struct rtapi_pci_driver yssc2_pci_driver = {
	.name = "nyx",
	.id_table = yssc2_pci_tbl,
	.probe = yssc2_pci_probe,
	.remove = yssc2_pci_remove,
};

int load_params(struct servo_params *p, const char *name, int axes);
void free_params(struct servo_params *p, int axes);

int yssc2_init()
{
	int rc;

	if ((rc = rtapi_pci_register_driver(&yssc2_pci_driver))) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: can't registering PCI driver\n");
	}

	return rc;
}

void yssc2_cleanup()
{
	int i;
	rtapi_pci_unregister_driver(&yssc2_pci_driver);

	for (i = 0; i < num_boards; i++) {
		YSSC2 *y = &yssc2_brd[i];
		free_params(y->par, 16);
	}
}

int yssc2_start(int instance, int maxdrives)
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

#include <linux/fs.h>      // Needed by filp
#include <asm/uaccess.h>   // Needed by segment descriptors

int load_params(struct servo_params *p, const char *name, int axes)
{
	struct file *f;

	f = filp_open(name, O_RDONLY, 0);
	if (!IS_ERR(f)) {
		int a, i, l;
		ssize_t s;
		mm_segment_t fs;

		fs = get_fs();		// Get current segment descriptor
		set_fs(get_ds());	// Set segment descriptor associated to kernel space

		for (a = 0; a < axes; a++, p++) {
			memset(p, 0, sizeof(*p));

			s = f->f_op->read(f, (char *)&p->magic, 6, &f->f_pos);	// read the header
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

			s = f->f_op->read(f, (char *)&p->np, 2*p->ng, &f->f_pos);	// read number of params in each of ng group
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

			p->buf = kmalloc(l, GFP_KERNEL);
			if (!p->buf) {
				rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params kalloc");
				goto skip;
			}

			s = f->f_op->read(f, (char *)p->buf, l, &f->f_pos);		// read param data
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

		set_fs(fs);	        // Restore segment descriptor
		filp_close(f, NULL);
	} else {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params can't open %s", name);
	}
	return 0;
}

void free_params(struct servo_params *p, int axes) {
	int a;

	for (a = 0; a < axes; a++, p++) {
		if (p) {
			if (p->buf) kfree(p->buf);
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
		// rtapi_print_msg(RTAPI_MSG_ERR, "nyx:get_params P%d.%d..%d group > 15", group, first, first+count-1);
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
	if (y == NULL) return;
	if (y->iomem == NULL) return;

	y->iomem->jtag = 0x80;	// sync pulse
	y->iomem->jtag = 0;

	if (nodma & 2) {
		memcpy(&y->dpram->fb, (void*)&y->iomem->dpram.fb, offsetof(struct nyx_dp_fb, servo_fb) + sizeof(nyx_servo_fb) * y->axes);
	} else {
		size_t offs = offsetof(struct nyx_dpram, fb);	// should be 512

		y->iomem->dma_dst = y->dpram_bus_addr + offs;
		y->iomem->dma_src = offs;	// start of fb in dpram
		y->iomem->dma_len = offsetof(struct nyx_dp_fb, servo_fb) + sizeof(nyx_servo_fb) * y->axes;
		y->iomem->jtag = 0x40;    // start DMA transfer ram write <- pci

		do { udelay(5); } while (y->iomem->jtag & 0x40);
		if (y->iomem->jtag & 8) {	// DMA error
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx: DMA mem write error\n");
		}
	}

	y->dpram->cmd.seq =
		(y->dpram->cmd.seq & ~YS_SEQ) |
		(y->dpram->fb.seq  &  YS_SEQ);
}

void yssc2_process(YSSC2 *y)
{
	int a;

	if (!(y->dpram->fb.seq & YS_INSYNC)) return;	// do nothing until insync

	for (a = 0; a < y->axes; a++) {
//		if (yssc2_online(y, a)) {
			nyx_servo_fb *fb = &y->dpram->fb.servo_fb[a];
			nyx_servo_cmd *cmd = &y->dpram->cmd.servo_cmd[a];

			switch (fb->state & Y_TYPE) {
			case Y_TYPE_ORIGIN:
				if (y->amp[a].fbres == 0) {
					y->amp[a].fbres = fb->fbres;
					y->amp[a].cyc0 = fb->cyc0;
					y->amp[a].abs0 = fb->abs0;
					//rtapi_print_msg(RTAPI_MSG_ERR, "nyx: #%d fbres=%d", a, y->amp[a].fbres);
				}
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
	if (y == NULL) return -1;
	if (y->iomem == NULL) return -1;

	y->prev_fb_seq = y->dpram->fb.seq;
	if (nodma & 1) {
		memcpy((void*)&y->iomem->dpram.cmd, &y->dpram->cmd, offsetof(struct nyx_dp_cmd, servo_cmd) + sizeof(nyx_servo_cmd) * y->axes);
	} else {
		size_t offs = offsetof(struct nyx_dpram, cmd);	// should be 256

		y->iomem->dma_dst = y->dpram_bus_addr + offs;
		y->iomem->dma_src = offs;	// start of cmd in dpram
		///y->iomem->dma_len = sizeof(nyx_dp_cmd);
		y->iomem->dma_len = offsetof(struct nyx_dp_cmd, servo_cmd) + sizeof(nyx_servo_cmd) * y->axes;
		// do {
			y->iomem->jtag = 0x60;	// start DMA transfer ram read -> pci
			do { udelay(5); } while (y->iomem->jtag & 0x40);
		// } while (y->iomem->jtag & 8);	// DMA error

		if (y->iomem->jtag & 8) {	// DMA error
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx: DMA mem read error\n");
		}
	}

	return 0;
}
