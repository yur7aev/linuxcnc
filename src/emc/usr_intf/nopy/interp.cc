//    Copyright © 2016 Jeff Epler <jepler@unpythonic.net>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <interp.h> // must be first header
#include <float.h>

std::vector<PmCartesian> vtx;
std::vector<struct RGB> vtx_clr;
EmcPose vtx_min;
EmcPose vtx_max;

extern std::string ini_dir;
extern std::string parameter_file;

// this is junk that you have to define in exactly this way because of how mah
// implemented the python "remap" functionality of the interpreter
// (and it needs Python.h for the definition of struct inttab)
int _task = 0;
char _parameter_file_name[LINELEN];

Interp::Interp() {
	ii = makeInterp();
	ii->init();
//	tool_mmap_user();
	vtx.push_back({-1, 0, 0});
	vtx.push_back({0, 1, 0});
	vtx.push_back({1, 0, 0});

	vtx_clr.push_back({255, 0, 0, 255});
	vtx_clr.push_back({0, 255, 0, 255});
	vtx_clr.push_back({0, 0, 255, 255});
}

void Interp::plot(const char *filename) {
	std::string copy_command = "cp " + ini_dir + "/" + parameter_file + " " + ini_dir + "/" + parameter_file + ".plot";

	system(copy_command.c_str());

	vtx.clear();
	vtx_clr.clear();
	ii->init();
	ii->open(filename);
	while(ii->read() == 0) {
		ii->execute();
	}

	for (int a = 0; a < 9; ++a) {
		vtx_min.coor[a] = DBL_MAX;
		vtx_max.coor[a] = -DBL_MAX;
	}

	auto i = vtx.begin();
	for (++i; i != vtx.end(); ++i) {
		for (int a = 0; a < 9; ++a) {
			if (i->coor[a] < vtx_min.coor[a]) vtx_min.coor[a] = i->coor[a];
			if (i->coor[a] > vtx_max.coor[a]) vtx_max.coor[a] = i->coor[a];
		}
	}

	ii->close();
}

extern "C" PyObject* PyInit_emctask(void);
extern "C" PyObject* PyInit_interpreter(void);
extern "C" PyObject* PyInit_emccanon(void);
extern "C" struct _inittab builtin_modules[];
struct _inittab builtin_modules[] = {
	{ "interpreter", PyInit_interpreter },
	{ "emccanon", PyInit_emccanon },
	{ NULL, NULL }
};

CANON_PLANE plane = CANON_PLANE_XY;
EmcPose pos;
double g5xoffset[9];
double g92offset[9];
//double tooloffset[9];
double rotation_xy;
bool rotation_active = false;
double rotation_sin;
double rotation_cos;

void arc_to_segments(
	int lineno,
	double x1, double y1,
	double cx, double cy, int rot,
	double z1,
	double a, double b, double c,
	double u, double v, double w
);


void rotate_and_translate(
                           double x, double y, double z,
                           double a, double b, double c,
                           double u, double v, double w)
{
        x += g92offset[0];
        y += g92offset[1];
        z += g92offset[2];
        a += g92offset[3];
        b += g92offset[4];
        c += g92offset[5];
        u += g92offset[6];
        v += g92offset[7];
        w += g92offset[8];

        if (rotation_active) {
		double rotx = x * rotation_cos - y * rotation_sin;
		y = x * rotation_sin + y * rotation_cos;
		x = rotx;
	}

        x += g5xoffset[0];
        y += g5xoffset[1];
        z += g5xoffset[2];
        a += g5xoffset[3];
        b += g5xoffset[4];
        c += g5xoffset[5];
        u += g5xoffset[6];
        v += g5xoffset[7];
        w += g5xoffset[8];

	pos.coor[0] = x;
	pos.coor[1] = y;
	pos.coor[2] = z;
	pos.coor[3] = a;
	pos.coor[4] = b;
	pos.coor[5] = c;
	pos.coor[6] = u;
	pos.coor[7] = v;
	pos.coor[8] = w;
}

// everything below here is stuff that needs a real implementation, not a dummy
// one
void INIT_CANON() {}
void SET_G5X_OFFSET(int origin,
                           double x, double y, double z,
                           double a, double b, double c,
                           double u, double v, double w)
{
	g5xoffset[0] = x;
	g5xoffset[1] = y;
	g5xoffset[2] = z;
	g5xoffset[3] = a;
	g5xoffset[4] = b;
	g5xoffset[5] = c;
	g5xoffset[6] = u;
	g5xoffset[7] = v;
	g5xoffset[8] = w;
	fprintf(stderr, "set G5x %d %.3f,%.3f,%.3f\n", origin, x, y, z);
}
void SET_G92_OFFSET(double x, double y, double z,
                           double a, double b, double c,
                           double u, double v, double w)
{
	g92offset[0] = x;
	g92offset[1] = y;
	g92offset[2] = z;
	g92offset[3] = a;
	g92offset[4] = b;
	g92offset[5] = c;
	g92offset[6] = u;
	g92offset[7] = v;
	g92offset[8] = w;
	fprintf(stderr, "set G92 %.3f,%.3f,%.3f\n", x, y, z);
}
void SET_XY_ROTATION(double t) {
	rotation_xy = t * M_PI / 180;
        rotation_sin = sin(rotation_xy);
        rotation_cos = cos(rotation_xy);
	rotation_active = (t != 0.0);
	fprintf(stderr, "set rot %f\n", t);
}
void CANON_UPDATE_END_POINT(double x, double y, double z,
				   double a, double b, double c,
				   double u, double v, double w)
{
	fprintf(stderr, "update end %.3f,%.3f,%.3f\n", x, y, z);
}
void USE_LENGTH_UNITS(CANON_UNITS u) {
	fprintf(stderr, "use len %d\n", u);
}
void SELECT_PLANE(CANON_PLANE pl) {
	switch (pl) {
	case CANON_PLANE_XY: // G17
	case CANON_PLANE_YZ: // G18
	case CANON_PLANE_XZ: // G19
		fprintf(stderr, "plane: %d\n", pl);
		plane = pl;
		break;
	default:
		fprintf(stderr, "bad plane: %d\n", pl);
	}
}
void SET_TRAVERSE_RATE(double rate) {}
void STRAIGHT_TRAVERSE(int lineno,
                              double x, double y, double z,
			      double a, double b, double c,
                              double u, double v, double w
) {
	rotate_and_translate(x, y, z, a, b, c, u, v, w);
		vtx.push_back(pos.tran);
	vtx_clr.push_back({255,155,155,155});
//printf("RAPID -> %.3f,%.3f,%.3f\n", pos.tran.x, pos.tran.y, pos.tran.z);
}
void SET_FEED_RATE(double rate) {}
void SET_FEED_REFERENCE(CANON_FEED_REFERENCE reference) {}
void SET_FEED_MODE(int spindle, int mode) {}
void SET_MOTION_CONTROL_MODE(CANON_MOTION_MODE mode, double tolerance) {}
void SET_NAIVECAM_TOLERANCE(double tolerance) {}
void SET_CUTTER_RADIUS_COMPENSATION(double radius) {}
void START_CUTTER_RADIUS_COMPENSATION(int direction) {}
void STOP_CUTTER_RADIUS_COMPENSATION() {}
void START_SPEED_FEED_SYNCH(int spindle, double feed_per_revolution, bool velocity_mode) {}
void STOP_SPEED_FEED_SYNCH() {}
void ARC_FEED(int lineno,
                     double first_end, double second_end,
		     double first_axis, double second_axis, int rotation,
		     double axis_end_point,
                     double a, double b, double c,
                     double u, double v, double w)
{
/*
    printf("arc -> %f %f %f %f %d %f\n",
                     first_end, second_end,
		     first_axis, second_axis, rotation,
		     axis_end_point);
*/
	arc_to_segments(lineno,
                     first_end,  second_end,
		     first_axis, second_axis, rotation,
		     axis_end_point,
                     a, b, c,
                     u, v, w);
}
void STRAIGHT_FEED(int lineno,
                          double x, double y, double z,
                          double a, double b, double c,
                          double u, double v, double w) {
	rotate_and_translate(x, y, z, a, b, c, u, v, w);
	vtx.push_back(pos.tran);
	vtx_clr.push_back({255,255,255,255});
//printf("feed -> %.3f,%.3f,%.3f\n", pos.tran.x, pos.tran.y, pos.tran.z);
}
void NURBS_FEED(int lineno, std::vector<CONTROL_POINT> nurbs_control_points, unsigned int k) {
	double u = 0.0;
	unsigned int n = nurbs_control_points.size() - 1;
	double umax = n - k + 2;
	unsigned int div = nurbs_control_points.size()*3;
	std::vector<unsigned int> knot_vector = knot_vector_creator(n, k);
	PLANE_POINT P1;
	while (u+umax/div < umax) {
		PLANE_POINT P1 = nurbs_point(u+umax/div,k,nurbs_control_points,knot_vector);
		STRAIGHT_FEED(lineno, P1.X,P1.Y, 0., 0.,0.,0.,  0.,0.,0.);
		u = u + umax/div;
	}
	P1.X = nurbs_control_points[n].X;
	P1.Y = nurbs_control_points[n].Y;
	STRAIGHT_FEED(lineno, P1.X,P1.Y, 0., 0.,0.,0.,  0.,0.,0.);
	knot_vector.clear();
}
void RIGID_TAP(int lineno, double x, double y, double z, double scale) {
// TODO
}

void STRAIGHT_PROBE(int lineno,
			double x, double y, double z,
			double a, double b, double c,
			double u, double v, double w, unsigned char probe_type) {
// TODO
}
void STOP() {}
void DWELL(double seconds) {}
void SET_SPINDLE_MODE(int spindle, double r) {}
void SPINDLE_RETRACT_TRAVERSE() {}
void START_SPINDLE_CLOCKWISE(int spindle, int dir) {}
void START_SPINDLE_COUNTERCLOCKWISE(int spindle, int dir) {}
void SET_SPINDLE_SPEED(int spindle, double r) {}
void STOP_SPINDLE_TURNING(int spindle) {}
void SPINDLE_RETRACT() {}
void ORIENT_SPINDLE(int spindle, double orientation, int mode) {}
void WAIT_SPINDLE_ORIENT_COMPLETE(int spindle, double timeout) {}
void LOCK_SPINDLE_Z() {}
void USE_SPINDLE_FORCE() {}
void USE_NO_SPINDLE_FORCE() {}
void SET_TOOL_TABLE_ENTRY(int pocket, int toolno, EmcPose offset, double diameter,
				double frontangle, double backangle, int orientation) {
	fprintf(stderr, "set tool table %d %d\n", pocket, toolno);
}
void USE_TOOL_LENGTH_OFFSET(EmcPose offset) {
	fprintf(stderr, "TOOL OFFSET\n");
//	tooloffset = offset;
}
void CHANGE_TOOL(int slot) {
	fprintf(stderr, "CHANGE TOOL %d\n", slot);
}
void SELECT_TOOL(int tool) {
	fprintf(stderr, "SELECT TOOL %d\n", tool);
}
void CHANGE_TOOL_NUMBER(int number) {
	fprintf(stderr, "change TOOL %d\n", number);
}
void RELOAD_TOOLDATA() {}
void START_CHANGE(void) {}
void CLAMP_AXIS(CANON_AXIS axis) {}
void COMMENT(const char *s) { puts(s); }
void DISABLE_ADAPTIVE_FEED() {}
void ENABLE_ADAPTIVE_FEED() {}
void DISABLE_FEED_OVERRIDE() {}
void ENABLE_FEED_OVERRIDE() {}
void DISABLE_SPEED_OVERRIDE(int spindle) {}
void ENABLE_SPEED_OVERRIDE(int spindle) {}
void DISABLE_FEED_HOLD() {}
void ENABLE_FEED_HOLD() {}
void FLOOD_OFF() {}
void FLOOD_ON() {}
void MESSAGE(char *s) { puts(s); }
void LOG(char *s) {}
void LOGOPEN(char *s) {}
void LOGAPPEND(char *s) {}
void LOGCLOSE() {}
void MIST_OFF() {}
void MIST_ON() {}
void PALLET_SHUTTLE() {}
void TURN_PROBE_OFF() {}
void TURN_PROBE_ON() {}
void UNCLAMP_AXIS(CANON_AXIS axis) {}
void NURB_KNOT_VECTOR() {}
void NURB_CONTROL_POINT(int i, double x, double y, double z,
			       double w) {}
void NURB_FEED(double sStart, double sEnd) {}

void SET_BLOCK_DELETE(bool enabled) {}
bool GET_BLOCK_DELETE(void) { return false; }
void OPTIONAL_PROGRAM_STOP() {}
void SET_OPTIONAL_PROGRAM_STOP(bool state) {}
bool GET_OPTIONAL_PROGRAM_STOP() { return false; }
void PROGRAM_END() {}
void PROGRAM_STOP() {}
void SET_MOTION_OUTPUT_BIT(int index) {}
void CLEAR_MOTION_OUTPUT_BIT(int index) {}
void SET_AUX_OUTPUT_BIT(int index) {}
void CLEAR_AUX_OUTPUT_BIT(int index) {}
void SET_MOTION_OUTPUT_VALUE(int index, double value) {}
void SET_AUX_OUTPUT_VALUE(int index, double value) {}
int WAIT(int index, int input_type, int wait_type, double timeout) { return 0; }
int UNLOCK_ROTARY(int line_no, int axis) { return 0; }
int LOCK_ROTARY(int line_no, int axis) { return 0; }
double GET_EXTERNAL_FEED_RATE() { return 0.0; }
int GET_EXTERNAL_FLOOD() { return 0; }
CANON_UNITS GET_EXTERNAL_LENGTH_UNIT_TYPE() { return CANON_UNITS_MM; }
double GET_EXTERNAL_LENGTH_UNITS() { return 1.0; }
double GET_EXTERNAL_ANGLE_UNITS() { return 1.0; }
int GET_EXTERNAL_MIST() { return 0; }
CANON_MOTION_MODE GET_EXTERNAL_MOTION_CONTROL_MODE() { return CANON_EXACT_STOP; }
double GET_EXTERNAL_MOTION_CONTROL_TOLERANCE() { return 0.0; }

extern void SET_PARAMETER_FILE_NAME(const char *name) {
	fprintf(stderr, "SET PARAMETER_FILE %s\n", name);
}
double GET_EXTERNAL_MOTION_CONTROL_NAIVECAM_TOLERANCE() { return 0.0; }
void GET_EXTERNAL_PARAMETER_FILE_NAME(char *filename, int max_size) {
//	filename[0] = 0;
//    snprintf(filename, max_size, "%s", "/home/dmitry/sim_mm1.var");
	std::string params = ini_dir + "/" + parameter_file + ".plot";
	snprintf(filename, max_size, "%s", params.c_str());
	fprintf(stderr, "GET PARAMETER_FILE:%s\n", filename);
}
CANON_PLANE GET_EXTERNAL_PLANE() { return CANON_PLANE_XY; }
double GET_EXTERNAL_POSITION_A() { return 0.0; }
double GET_EXTERNAL_POSITION_B() { return 0.0; }
double GET_EXTERNAL_POSITION_C() { return 0.0; }
double GET_EXTERNAL_POSITION_X() { return 0.0; }
double GET_EXTERNAL_POSITION_Y() { return 0.0; }
double GET_EXTERNAL_POSITION_Z() { return 0.0; }
double GET_EXTERNAL_POSITION_U() { return 0.0; }
double GET_EXTERNAL_POSITION_V() { return 0.0; }
double GET_EXTERNAL_POSITION_W() { return 0.0; }
double GET_EXTERNAL_PROBE_POSITION_A() { return 0.0; }
double GET_EXTERNAL_PROBE_POSITION_B() { return 0.0; }
double GET_EXTERNAL_PROBE_POSITION_C() { return 0.0; }
double GET_EXTERNAL_PROBE_POSITION_X() { return 0.0; }
double GET_EXTERNAL_PROBE_POSITION_Y() { return 0.0; }
double GET_EXTERNAL_PROBE_POSITION_Z() { return 0.0; }
double GET_EXTERNAL_PROBE_POSITION_U() { return 0.0; }
double GET_EXTERNAL_PROBE_POSITION_V() { return 0.0; }
double GET_EXTERNAL_PROBE_POSITION_W() { return 0.0; }
double GET_EXTERNAL_PROBE_VALUE() { return 0.0; }
int GET_EXTERNAL_PROBE_TRIPPED_VALUE() { return 0; }
int GET_EXTERNAL_QUEUE_EMPTY() { return 1; }
double GET_EXTERNAL_SPEED(int spindle) { return 0.0; }
CANON_DIRECTION GET_EXTERNAL_SPINDLE(int spindle) { return CANON_STOPPED; }
double GET_EXTERNAL_TOOL_LENGTH_XOFFSET() { return 0.0; }
double GET_EXTERNAL_TOOL_LENGTH_YOFFSET() { return 0.0; }
double GET_EXTERNAL_TOOL_LENGTH_ZOFFSET() { return 0.0; }
double GET_EXTERNAL_TOOL_LENGTH_AOFFSET() { return 0.0; }
double GET_EXTERNAL_TOOL_LENGTH_BOFFSET() { return 0.0; }
double GET_EXTERNAL_TOOL_LENGTH_COFFSET() { return 0.0; }
double GET_EXTERNAL_TOOL_LENGTH_UOFFSET() { return 0.0; }
double GET_EXTERNAL_TOOL_LENGTH_VOFFSET() { return 0.0; }
double GET_EXTERNAL_TOOL_LENGTH_WOFFSET() { return 0.0; }
int GET_EXTERNAL_TOOL_SLOT() {
	fprintf(stderr, "TOOL SLOT?\n");
	return 1;
}
int GET_EXTERNAL_SELECTED_TOOL_SLOT() {
	fprintf(stderr, "SELECTED SLOT?\n");
	return 2;
}
CANON_TOOL_TABLE GET_EXTERNAL_TOOL_TABLE(int pocket) {
//fprintf(stderr, ">>>>>> POCKET %d\n", pocket);
//	CANON_TOOL_TABLE retval = tooldata_entry_init();
	CANON_TOOL_TABLE retval;
	retval.toolno = retval.pocketno = -1;
	return retval;
}
int GET_EXTERNAL_TC_FAULT() { return 0; }
int GET_EXTERNAL_TC_REASON() { return 0; }
double GET_EXTERNAL_TRAVERSE_RATE() { return 0.0; }
int GET_EXTERNAL_FEED_OVERRIDE_ENABLE() { return 0; }
int GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE(int spindle) { return 0; }
int GET_EXTERNAL_ADAPTIVE_FEED_ENABLE() { return 0; }
int GET_EXTERNAL_FEED_HOLD_ENABLE() { return 0; }
int GET_EXTERNAL_DIGITAL_INPUT(int index, int def) { return 0; }
double GET_EXTERNAL_ANALOG_INPUT(int index, double def) { return 0.0; }
int GET_EXTERNAL_AXIS_MASK() { return 7; }
void FINISH(void) { fprintf(stderr, "FINISH\n"); }
void ON_RESET(void) {}

void CANON_ERROR(const char *fmt, ...) {
	va_list args;
	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);
}

void PLUGIN_CALL(int len, const char *call) {}
void IO_PLUGIN_CALL(int len, const char *call) {}
void UPDATE_TAG(StateTag tag) {}

USER_DEFINED_FUNCTION_TYPE USER_DEFINED_FUNCTION[USER_DEFINED_FUNCTION_NUM];

int GET_EXTERNAL_OFFSET_APPLIED() { return 0; }

EmcPose GET_EXTERNAL_OFFSETS() {
	EmcPose retval;
	ZERO_EMC_POSE(retval);
	return retval;
}

static void unrotate(double &x, double &y, double c, double s) {
	double tx = x * c + y * s;
	y = -x * s + y * c;
	x = tx;
}

static void rotate(double &x, double &y, double c, double s) {
	double tx = x * c - y * s;
	y = x * s + y * c;
	x = tx;
}

#ifndef hypot
#define hypot(a,b) (sqrt((a)*(a)+(b)*(b)))
#endif

void arc_to_segments(int lineno,
                     double x1, double y1,
		     double cx, double cy, int rot,
		     double z1,
                     double a, double b, double c,
                     double u, double v, double w
) {
	double o[9], n[9];
	int X, Y, Z;
//	double rotation_cos, rotation_sin;
//	int max_segments = 128;
	int max_segments = 32;

	for (int i = 0; i < 9; ++i) {
		o[i] = pos.coor[i];
//		g5xoffset[i] = g92offset[i] = 0.0;
	}

//	rotation_cos = 1.0;
//	rotation_sin = 0.0;

    if(plane == CANON_PLANE_XY) {
        X=0; Y=1; Z=2;
    } else if(plane == CANON_PLANE_XZ) {
        X=2; Y=0; Z=1;
    } else {
        X=1; Y=2; Z=0;
    }
    n[X] = x1;
    n[Y] = y1;
    n[Z] = z1;
    n[3] = a;
    n[4] = b;
    n[5] = c;
    n[6] = u;
    n[7] = v;
    n[8] = w;
    for(int ax=0; ax<9; ax++) o[ax] -= g5xoffset[ax];
    unrotate(o[0], o[1], rotation_cos, rotation_sin);
    for(int ax=0; ax<9; ax++) o[ax] -= g92offset[ax];

    double theta1 = atan2(o[Y]-cy, o[X]-cx);
    double theta2 = atan2(n[Y]-cy, n[X]-cx);

    /* Issue #1528 1/2/22 andypugh */
    /*_posemath checks for small arcs too, but uses config units */
    double len = hypot(o[X]-n[X], o[Y]-n[Y]); // * (25.4 * GET_EXTERNAL_LENGTH_UNITS());
    /* If the signs of the angles differ, make them the same to allow monotonic progress through the arc */
    /* If start and end points are nearly identical, then interpret as a full turn */
    if(rot < 0) { // CW G2
        if (theta1 < theta2) theta2 -= 2*M_PI;
        if (len < CART_FUZZ) theta2 -= 2*M_PI;
    } else { // CCW G3
        if (theta1 > theta2) theta2 += 2*M_PI;
        if (len < CART_FUZZ) theta2 += 2*M_PI;
    }

    // if multi-turn, add the right number of full circles
    if(rot < -1) theta2 += 2*M_PI*(rot+1);
    if(rot > 1) theta2 += 2*M_PI*(rot-1);

    int steps = std::max(3, int(max_segments * fabs(theta1 - theta2) / M_PI));
    double rsteps = 1. / steps;

    double dtheta = theta2 - theta1;
    double d[9] = {0, 0, 0, n[3]-o[3], n[4]-o[4], n[5]-o[5], n[6]-o[6], n[7]-o[7], n[8]-o[8]};
    d[Z] = n[Z] - o[Z];

    double tx = o[X] - cx, ty = o[Y] - cy, dc = cos(dtheta*rsteps), ds = sin(dtheta*rsteps);
    for(int i=0; i<steps-1; i++) {
        double f = (i+1) * rsteps;
        double p[9];
        rotate(tx, ty, dc, ds);
        p[X] = tx + cx;
        p[Y] = ty + cy;
        p[Z] = o[Z] + d[Z] * f;
        p[3] = o[3] + d[3] * f;
        p[4] = o[4] + d[4] * f;
        p[5] = o[5] + d[5] * f;
        p[6] = o[6] + d[6] * f;
        p[7] = o[7] + d[7] * f;
        p[8] = o[8] + d[8] * f;

//        for(int ax=0; ax<9; ax++) p[ax] += g92offset[ax];
//        rotate(p[0], p[1], rotation_cos, rotation_sin);
//        for(int ax=0; ax<9; ax++) p[ax] += g5xoffset[ax];

	STRAIGHT_FEED(lineno, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8]);
    }
//    for(int ax=0; ax<9; ax++) n[ax] += g92offset[ax];
//    rotate(n[0], n[1], rotation_cos, rotation_sin);
//    for(int ax=0; ax<9; ax++) n[ax] += g5xoffset[ax];
	STRAIGHT_FEED(lineno, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8]);
}
