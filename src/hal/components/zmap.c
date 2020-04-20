/*
    zmap.c

    height map z axis correction for a vibrating knife cutting machine

    Copyright (C) 2019 Dmitry Yurtaev <dmitry@yurtaev.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation, version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301-1307 USA.
*/

#include "rtapi_math.h"
#include "posemath.h"
#include "zmap.h"

#ifdef RTAPI

#include "rtapi.h"      /* RTAPI realtime OS API */
#include "rtapi_app.h"      /* RTAPI realtime module decls */
#include "hal.h"

struct haldata {
    hal_bit_t *enable;
    hal_bit_t *disable;
    hal_float_t *x;
    hal_float_t *y;
    hal_float_t *z;
    hal_float_t *zout;
    hal_float_t *dz;
    hal_float_t *fb;
    hal_float_t *fbout;
} *haldata;

static zmap_shm *mp;
static void zmap_update(void *h, long l);
static int comp_id;
static int shm_id;
static int key = SHMEM_KEY;

static int size = 40*8*32*32;
RTAPI_MP_INT(size, "grid buffer size 40+8*nx*ny")
MODULE_LICENSE("GPL");

inline double lerp(double t, double a, double b)
{
	return (1 - t) * a + t * b;
}

static void zmap_update(void *arg, long l)
{
	int nx = mp->nx;
	int ny = mp->ny;

	if (haldata && mp && nx > 0 && ny > 0) {
		double x = *haldata->x;
		double y = *haldata->y;
		double dx = mp->dx;
		double dy = mp->dy;
		double x0 = mp->x0;
		double x1 = mp->x0 + dx * (nx-1);
		double y0 = mp->y0;
		double y1 = mp->y0 + dy * (ny-1);
		double tx;
		double ty;
		double z0, z1;
		int i0, j0, i1, j1;

		// find the grid cell x,y falls into
		if (x >= x0 && x < x1) { i0 = (x - x0) / dx; i1 = i0 + 1; tx = (x - (x0 + i0*dx)) / dx; }
		else if (x < x0) { i0 = i1 = 0; tx = 0; }
		else { i0 = i1 = nx-1; tx = 0; }

		if (y >= y0 && y < y1) { j0 = (y - y0) / dy; j1 = j0 + 1; ty = (y - (y0 + j0*dy)) / dy; }
		else if (y <= y0) { j0 = j1 = 0; ty = 0; }
		else { j0 = j1 = ny-1; ty = 0; }

		// printf("%d/%g/%d %d/%g/%d\n", i0,tx,i1, j0,ty,j1);

		z0 = lerp(tx, mp->z[j0*nx+i0], mp->z[j0*nx+i1]);
		z1 = lerp(tx, mp->z[j1*nx+i0], mp->z[j1*nx+i1]);
		double dz = lerp(ty, z0, z1);
		*haldata->dz = dz;
		if (!*haldata->enable || *haldata->disable) dz = 0;
		*haldata->zout = *haldata->z + dz;
		*haldata->fbout = *haldata->fb - dz;
	} else {
		*haldata->zout = *haldata->z;
		*haldata->dz = 0;
		*haldata->fbout = *haldata->fb;
	}
}

int rtapi_app_main(void) {

	int retval;
	int res;
	int level = rtapi_get_msg_level();

	//rtapi_set_msg_level(RTAPI_MSG_DBG);

	comp_id = hal_init("zmap");
	if (comp_id < 0)
		return comp_id;
	rtapi_print_msg(RTAPI_MSG_DBG, "zmap shmsize=%d comp_id=%d\n", size, comp_id );

	shm_id = rtapi_shmem_new(key, comp_id, size);
	if (shm_id < 0) {
		rtapi_exit(comp_id);
		rtapi_print_msg(RTAPI_MSG_ERR, "can't get shared memory!\n");
		return -1;
	}

	retval = rtapi_shmem_getptr(shm_id, (void **)&mp);
	if (retval < 0) {
		rtapi_exit(comp_id);
		rtapi_print_msg(RTAPI_MSG_ERR, "can't get shared memory pointer!\n");
		return -1;
	}

	do {
		haldata = hal_malloc(sizeof(struct haldata));

		if (!haldata) {
			rtapi_print_msg(RTAPI_MSG_ERR, "can't malloc haldata!\n");
			break;
		}

		res = hal_pin_bit_new("zmap.enable", HAL_IN, &(haldata->enable), comp_id);
		if (res < 0) break;

		res = hal_pin_bit_new("zmap.disable", HAL_IN, &(haldata->disable), comp_id);
		if (res < 0) break;

		res = hal_pin_float_new("zmap.x", HAL_IN, &(haldata->x), comp_id);
		if (res < 0) break;

		res = hal_pin_float_new("zmap.y", HAL_IN, &(haldata->y), comp_id);
		if (res < 0) break;

		res = hal_pin_float_new("zmap.z", HAL_IN, &(haldata->z), comp_id);
		if (res < 0) break;

		res = hal_pin_float_new("zmap.zout", HAL_OUT, &(haldata->zout), comp_id);
		if (res < 0) break;

		res = hal_pin_float_new("zmap.dz", HAL_OUT, &(haldata->dz), comp_id);
		if (res < 0) break;

		res = hal_pin_float_new("zmap.fb", HAL_IN, &(haldata->fb), comp_id);
		if (res < 0) break;

		res = hal_pin_float_new("zmap.fbout", HAL_OUT, &(haldata->fbout), comp_id);
		if (res < 0) break;

		res =  hal_export_funct("zmap.update", zmap_update,  NULL, 1, 0, comp_id);
		if (res < 0) break;

		hal_ready(comp_id);
		rtapi_set_msg_level(level);
		return 0;
	} while (0);

	rtapi_print_msg(RTAPI_MSG_ERR, "can't create pins!\n");
	hal_exit(comp_id);
	return comp_id;
}

void rtapi_app_exit(void)
{
	//rtapi_set_msg_level(RTAPI_MSG_DBG);
	rtapi_shmem_delete(shm_id, comp_id);
	hal_exit(comp_id);
}
#endif
