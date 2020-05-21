/*
 * N Y X
 *
 * (c) 2016-2019, dmitry@yurtaev.com
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <rtapi_slab.h>
#include <rtapi_io.h>

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

	if (ioctl(y->fd, 1, nodma))
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:init dma ioctl failed - old driver?");

	num_boards = 1;
	y->dpram = calloc(sizeof(nyx_dpram), 1);
	read(y->fd, y->dpram, 16);	// includes "magic" and "config" field

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

//	rtapi_print_msg(RTAPI_MSG_ERR, "nyx: magic: %x, %d axes", y->dpram->magic, y->axes );

	y->initial_delay = 0;
	y->errors_shown = 0;
	freq_init(y);
	y->was_ready = 0;
	y->prev_fb_seq = 0;

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
		params[a].pa = malloc(sizeof(struct servo_param));
		if (params[a].pa == NULL) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx: can't allocate params buffer");
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
			free(params[a].pa);
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
		p = realloc(params->pa, params->size * 2 * sizeof(servo_param));
		if (p == NULL) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx: param realloc failed");
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

/*
int strtol(char *s, char **endptr, int base)
{
	long l;
	if (kstrtol(s, base, &l)) {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx: bad number '%s'", s);
		return 0xffff;	// error
	}
	return l;
}
*/

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
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx: bad param '%s' at line %d", p, ln);
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

//#include <linux/fs.h>      // Needed by filp
//#include <asm/uaccess.h>   // Needed by segment descriptors

int params_load(servo_params *params, const char *filename)
{
	int f;

	f = open(filename, O_RDONLY);
	if (f >= 0) {
		struct stat st;
		char *buf, *s, *l;

		if (fstat(f, &st) < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx:can't stat %s", filename);
			return -1;
		}

		buf = malloc(st.st_size + 1);
		if (buf) {
			int ln = 1;
			read(f, buf, st.st_size);	// gulp the file
			buf[st.st_size] = 0;

			s = buf;
			while (s) {
				l = strsep(&s, "\n");
				if (l && *l)
					params_parse_line(params, l, ln);
				ln++;
			}

			free(buf);
		} else {
			rtapi_print_msg(RTAPI_MSG_ERR, "nyx:malloc of %d failed", (int)st.st_size+1);
		}

		close(f);
	} else {
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:load_params can't open %s", filename);
	}

	rtapi_print_msg(RTAPI_MSG_ERR, "nyx:params %d", (int)params->count);

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
	int rc;
	if (y == NULL) return;

	lseek(y->fd, offsetof(struct nyx_dpram, fb), SEEK_SET);
	rc = read(y->fd, &y->dpram->fb, offsetof(struct nyx_dp_fb, servo_fb) + sizeof(nyx_servo_fb) * y->axes);
	if (rc <= 0)
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:read err %d", rc);

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
					//rtapi_print_msg(RTAPI_MSG_ERR, "nyx: #%d fbres=%d cyc=%d abs=%d", a, y->amp[a].fbres, y->amp[a].cyc0, y->amp[a].abs0);
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
	int rc;
	if (y == NULL) return -1;

	y->prev_fb_seq = y->dpram->fb.seq;

	lseek(y->fd, offsetof(struct nyx_dpram, cmd), SEEK_SET);
	rc = write(y->fd, &y->dpram->cmd, offsetof(struct nyx_dp_cmd, servo_cmd) + sizeof(nyx_servo_cmd) * y->axes);
	if (rc <= 0)
		rtapi_print_msg(RTAPI_MSG_ERR, "nyx:write %d\n", rc);

	return 0;
}
