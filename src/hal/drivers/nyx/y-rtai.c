/*
 * N Y X
 *
 * (c) 2016-2019, dmitry@yurtaev.com
 */

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
	{
		.vendor = YMTL2_VENDOR_ID,
		.device = YMTL2P_A_DEVICE_ID,
		.subvendor = YMTL2_VENDOR_ID,
		.subdevice = YMTL2P_A_DEVICE_ID,
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

		if ((0xffffff00 & magic) != (0x55c20000 | (NYX_VER_MAJ<<8))) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx: this driver v%d.%d is not compatible with YxxxnP (%x) v%d.%d at %s\n",
					NYX_VER_MAJ, NYX_VER_MIN, magic, rev_maj, rev_min, rtapi_pci_name(dev));
			r = -ENODEV;
			goto fail1;
		}


		rtapi_print_msg(RTAPI_MSG_INFO, "nyx: YxxxnP v%d.%d #%d at %s, mapped to %p..%p, y=%p\n", rev_maj, rev_min,
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
	y->yios = (y->dpram->config >> 8) & 0xff;

	if (y->axes > NYX_AXES) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: card has %d axes, limited to %d", y->axes, NYX_AXES);
		y->axes = NYX_AXES;
	}

	if (y->yios > 16) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: yio nodes number %d, limited to 16", y->yios);
		y->yios = 16;
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

int yssc2_init()
{
	int rc;

	if ((rc = rtapi_pci_register_driver(&yssc2_pci_driver))) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: can't registering PCI driver\n");
	}

	return rc;
}

int params_init(servo_params *params);
int params_load (servo_params *p, const char *name);
void params_free(servo_params *p);

void yssc2_cleanup()
{
	int i;
	rtapi_pci_unregister_driver(&yssc2_pci_driver);

	for (i = 0; i < num_boards; i++) {
		YSSC2 *y = &yssc2_brd[i];
		params_free(y->par);
	}
}

int yssc2_start(int no, int maxdrives)
{
	YSSC2 *y = &yssc2_brd[no];

	params_init(y->par);

	if(param_file[no] == NULL) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx.%d: no param file specified", no);
		return -1;
	}

	if (y->axes > maxdrives) y->axes = maxdrives;
	params_load(y->par, param_file[no]);

	return 0;
}

//
// params
//

int params_init(servo_params *params)
{
	int a;

	for (a = 0; a < NYX_AXES; a++) {
		params[a].pa = NULL;
		params[a].size = 0;
		params[a].count = 0;
		params[a].load_axis = 0;
	}

	for (a = 0; a < NYX_AXES; a++) {
		params[a].pa = kmalloc(sizeof(struct servo_param), GFP_KERNEL);
		if (params[a].pa == NULL) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx:params_init can't allocate params buffer");
			return -1;
		}
		params[a].size = 1;
		params[a].count = 0;
	}
	return 0;
}

void params_free(servo_params *params)
{
	int a;

	for (a = 0; a < NYX_AXES; a++) {
		if (params[a].pa) {
			kfree(params[a].pa);
			params[a].pa = NULL;
		}
		params[a].size = 0;
		params[a].count = 0;
	}
}

servo_param *params_bfind(uint16_t key, servo_param *base, size_t num)
{
	servo_param *pivot = 0;

	while (num > 0) {
		pivot = base + (num >> 1);

		if (key == pivot->no)
			return pivot;

		if (key > pivot->no) {
			base = pivot + 1;
			num--;
		}
		num >>= 1;
	}

	return pivot;
}

int params_add(servo_params *params, uint16_t no, uint16_t size, uint32_t val)
{
	servo_param *p = params->pa;

	if (params->count >= params->size) {
		p = krealloc(params->pa, params->size * 2 * sizeof(servo_param), GFP_KERNEL);
		if (p == NULL) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx:params_add realloc failed");
			return -1;
		}
		params->pa = p;
		params->size <<= 1;
	}
	if (params->count > 0) {
		p = params_bfind(no, params->pa, params->count);
		if (p->no == no) {
			return -1;
		}
		if (p->no < no) ++p;
		memmove(p+1, p, (params->count - (p - params->pa)) * sizeof(servo_param));
	}
	p->no = no;
	p->size = size;
	p->val = val;
	++params->count;

	return p - params->pa;
}

int strtol(char *s, char **endptr, int base)
{
	long l;
	if (kstrtol(s, base, &l)) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:params bad number '%s'", s);
		return 0xffff;	// error
	}
	return l;
}

int params_parse_no(char *s, servo_param *p)
{
	int l;

	l = strlen(s);

	if (l < 3) return -1;

	p->size = 2;	// 16-bit param by default
	if (s[0] == 'P' && s[1] == 'n' && s[2] >= '0' && s[2] <= '9') {
		if (s[l-1] == 'L') {
			s[l-1] = 0;
			p->size = 4;
		}
		p->no = strtol(s+3, NULL, 16) | ((s[2] - '0') << 8);

	} else if (s[0] == 'P' && s[1] >= '0' && s[1] <= '9') {
		p->no = strtol(s+1, NULL, 10);

	} else if (s[0] == 'S' && s[1] == 'V') {
		p->no = strtol(s+2, NULL, 10);

	} else if (s[0] == 'S' && s[1] == 'P') {
		p->no = strtol(s+2, NULL, 10) | (1 << 10);

	} else if (s[0] == 'P' && s[2] >= '0' && s[2] <= '9') {
		uint16_t grp = 0;
		switch(s[1]) {
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H': grp = s[1] - 'A'; break;
		case 'J': grp = 8; break;
		case 'O': grp = 9; break;
		case 'S': grp = 10; break;
		case 'L': grp = 11; break;
		case 'T': grp = 12; break;
		case 'M': grp = 13; break;
		case 'N': grp = 14; break;
		default:  return -1;
		}
		p->no = strtol(s+2, NULL, 10) | (grp << 10);
	} else
		return -1;

	return p->no == 0xffff ? -1 : 0;
}


void params_parse_line(servo_params *params, char *str, int ln)
{
	servo_param par;
	char *s = str, *t, *p;
	int a;

	while (*s == ' ' || *s == '\t') s++;
	if ((t = strchr(s, ';'))) *t = 0;
	p = strsep(&s, " \t\r\n");	// chop leading whitespaces
	if (*p) {
		if (!strcmp(p, "AXIS")) {	// set base axis number
			while (s) {
				t = strsep(&s, " \t\r\n");
				if (*t) {
					params->load_axis = strtol(t, NULL, 0);
					break;
				}
			}
			return;
		}

		if (params_parse_no(p, &par) < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx:params_parse_line bad param '%s' at line %d", p, ln);
			return;
		}

		for (a = params->load_axis; par.no && s && a < NYX_AXES;) {
			t = strsep(&s, " \t\r\n");
			if (*t)	{
				if (t[0] != '-' || t[1] != 0)  {
					char *d;
					if ((d = strchr(t, '.'))) {	// remove decimal point
						do {
							*d = d[1];
						} while (*d++);
					}
					params_add(params + a, par.no, par.size, strtol(t, NULL, 0));
				}
				a++;
			}
		}
	}
}

#include <linux/fs.h>      // Needed by filp
#include <asm/uaccess.h>   // Needed by segment descriptors

int params_load(servo_params *params, const char *filename)
{
	struct file *f;

	f = filp_open(filename, O_RDONLY, 0);
	if (!IS_ERR(f)) {
		if (f->f_op && f->f_op->read && f->f_op->llseek) {
			mm_segment_t fs;
			loff_t size, n;
			char *buf, *s, *l;

			fs = get_fs();		// Get current segment descriptor
			set_fs(get_ds());	// Set segment descriptor associated to kernel space

			size = f->f_op->llseek(f, 0, SEEK_END);
///			rtapi_print_msg(RTAPI_MSG_ERR, "nyx:par file size %d", (int)size);
			f->f_op->llseek(f, 0, SEEK_SET);

			buf = kmalloc(size + 1, GFP_KERNEL);
			if (buf) {
				int ln = 1;
				n = f->f_op->read(f, buf, size, &f->f_pos);	// gulp the file
///        			rtapi_print_msg(RTAPI_MSG_ERR, "nyx:par file read %d", (int)n);
				buf[size] = 0;

				s = buf;
				while (s) {
					l = strsep(&s, "\n");
					if (l && *l)
						params_parse_line(params, l, ln);
					ln++;
				}

				kfree(buf);
			} else {
				rtapi_print_msg(RTAPI_MSG_ERR, "nyx:params_load malloc of %d failed", (int)size+1);
			}

			set_fs(fs);	        // Restore segment descriptor
		} else {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx:params_load can't read par file %s : no read, llseek f_ops", filename);
		}
		filp_close(f, NULL);
	} else {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:params_load can't open %s", filename);
	}

	if(debug) rtapi_print_msg(RTAPI_MSG_ERR, "nyx:params_load %d", params->count);

	return 0;
}

void params_get_span(servo_params *params, uint16_t first, int count, uint16_t *dm, uint16_t *dp)
{
	servo_param *p;
	uint32_t mask = 0;
	int i;

	p = params_bfind(first, params->pa, params->count);
	for (i = 0; i < count; i++) {
		if (p && p - params->pa < params->count && p->no == first + i) {
			mask |= 1<<i;
			dp[i] = p->val;
			p++;
		} else {
			dp[i] = 0;
		}
	}
	*dm = mask;
}

void params_get_next(servo_params *params, uint16_t *no, uint16_t *size, uint32_t *val)
{
	servo_param *p;

	p = params_bfind(*no, params->pa, params->count);
	if (p && p->no >= *no) {
		*no = p->no;
		*size = p->size;
		*val = p->val;
	} else if (p && p - params->pa < params->count - 1) {	// the last item
		p++;
		*no = p->no;
		*size = p->size;
		*val = p->val;
	} else {	// the last item
		*no = 0xffff;
		*size = 0;
		*val = 0;
	}


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
				params_get_span(&y->par[a], pq->first, 10, &pr->mask, pr->param);
				pr->first = pq->first;
				pr->count = 10;	// max number if fb struct
				//rtapi_print_msg(RTAPI_MSG_ERR, "nyx: #%d param req %x\n", a, pq->first);
				}
				break;
			case Y_TYPE_PARAM1: {
				nyx_param1_req *pq = (nyx_param1_req*)fb;
				nyx_param1_req *pr = (nyx_param1_req*)cmd;

				pr->flags = Y_TYPE_PARAM1;
				pr->no = pr->first = pq->first;
				params_get_next(&y->par[a], &pr->no, &pr->size, &pr->val);
//				if (pr->no > 0 && pr->no != 0xffff)
//				rtapi_print_msg(RTAPI_MSG_ERR, "nyx: #%d param req %x -> %x (%x:%x)\n", a, pr->first, pr->no, pr->size, pr->val);
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
