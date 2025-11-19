#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "nopy.h"
#include "widgets.h"
#include "glfb.h"
#include "interp.h"
#include "../shcom.hh"             // NML Messaging functions

extern Interp *interp;
extern bool clear_backplot;

extern "C" {
#include "trackball.h"
#include "glutstroke.h"
#include "emcpose.h"
void glutStrokeCharacter(int c);
}

using namespace ImGui;

extern float tabwindow_size;
extern float size2;
extern bool messages_visible;
extern float messages_height;
extern GLFB *glfb;

bool InvertMatrix(const float *m, float *invOut)
{
    float inv[16], det;
    int i;

    inv[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] + 
              m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - 
              m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + 
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] - 
             m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + 
             m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - 
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] + 
              m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - 
              m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + 
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] - 
              m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + 
              m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - 
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] - 
             m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + 
             m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - 
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] + 
              m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - 
              m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + 
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] - 
              m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + 
              m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - 
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] + 
               m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - 
               m[4]  * m[2] * m[13] - 
               m[12] * m[1] * m[6] + 
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + 
              m[1] * m[7] * m[10] + 
              m[5] * m[2] * m[11] - 
              m[5] * m[3] * m[10] - 
              m[9] * m[2] * m[7] + 
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - 
             m[0] * m[7] * m[10] - 
             m[4] * m[2] * m[11] + 
             m[4] * m[3] * m[10] + 
             m[8] * m[2] * m[7] - 
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + 
               m[0] * m[7] * m[9] + 
               m[4] * m[1] * m[11] - 
               m[4] * m[3] * m[9] - 
               m[8] * m[1] * m[7] + 
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - 
              m[0] * m[6] * m[9] - 
              m[4] * m[1] * m[10] + 
              m[4] * m[2] * m[9] + 
              m[8] * m[1] * m[6] - 
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0)
        return false;

    det = 1.0 / det;

    for (i = 0; i < 16; i++)
        invOut[i] = inv[i] * det;

    return true;
}

GLfloat iMV[4][4];

void update_iMV()
{
	GLfloat m[4][4];
	glGetFloatv(GL_MODELVIEW_MATRIX, &m[0][0]);
	InvertMatrix(&m[0][0], &iMV[0][0]);
}

ImVec4 window_to_model(ImVec2 v)
{
	v -= GetWindowPos();
	v *= 2;

	ImVec2 s = GetWindowSize();
	if (s.x > s.y) {
		v /= s.x;
		v -= ImVec2(1, s.y/s.x);
	} else {
		v /= s.y;
		v -= ImVec2(1, s.x/s.y);
	}
//	v /= s;
//	v -= ImVec2(1, 1);
	v.y = -v.y;

	ImVec4 q;
	q.x = iMV[0][0] * v.x + iMV[1][0] * v.y + iMV[3][0];
	q.y = iMV[0][1] * v.x + iMV[1][1] * v.y + iMV[3][1];
	q.z = iMV[0][2] * v.x + iMV[1][2] * v.y + iMV[3][2];
	q.w = iMV[0][3] * v.x + iMV[1][3] * v.y + iMV[3][3];
	q.x /= q.w;
	q.y /= q.w;
	q.z /= q.w;
	q.w = 1;

	return q;
}

ImVec4 model_to_view(GLfloat MV[4][4], const PmCartesian &v)
{
	ImVec4 q;
	q.x = MV[0][0] * v.x + MV[1][0] * v.y + MV[2][0] * v.z + iMV[3][0];
	q.y = MV[0][1] * v.x + MV[1][1] * v.y + MV[2][1] * v.z + iMV[3][1];
	q.z = MV[0][2] * v.x + MV[1][2] * v.y + MV[2][2] * v.z + iMV[3][2];
	q.w = MV[0][3] * v.x + MV[1][3] * v.y + MV[2][3] * v.z + iMV[3][3];
	q.x /= q.w;
	q.y /= q.w;
	q.z /= q.w;
	q.w = 1;

	return q;
}


ImVec4 z1_model(void)
{
	ImVec4 q;
	q.x = iMV[2][0] + iMV[3][0];
	q.y = iMV[2][1] + iMV[3][1];
	q.z = iMV[2][2] + iMV[3][2];
	q.w = iMV[2][3] + iMV[3][3];
	q.x /= q.w;
	q.y /= q.w;
	q.z /= q.w;
	q.w = 1.0f;

	return q;
}

ImVec4 origin_model(void)
{
	ImVec4 q;
	q.x = iMV[3][0];
	q.y = iMV[3][1];
	q.z = iMV[3][2];
	q.w = iMV[3][3];
	q.x /= q.w;
	q.y /= q.w;
	q.z /= q.w;
	q.w = 1.0f;

	return q;
}

float dot_product(const ImVec4 &v1, const ImVec4 &v2) {
    return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

ImVec4 cross_product(const ImVec4 &a, const ImVec4 &b) {
	ImVec4 result;
	result.x = a.y * b.z - a.z * b.y;
	result.y = a.z * b.x - a.x * b.z;
	result.z = a.x * b.y - a.y * b.x;
	return result;
}

void normalize(ImVec4 &v) {
	if (v.w != 0);
	v.x /= v.w;
	v.y /= v.w;
	v.z /= v.w;
}

float magnitude(const ImVec4 &v) {
    return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

float xangle_between_vectors(const ImVec4 &v1, const ImVec4 &v2) {
    // Calculate dot product
    float dot = dot_product(v1, v2);

    // Calculate magnitudes
    float mag1 = magnitude(v1);
    float mag2 = magnitude(v2);

    // Avoid division by zero if a vector has zero magnitude
    if (mag1 == 0 || mag2 == 0) {
        return 0.0f; // Or handle as an error
    }

fprintf(stderr, "vec %f,%f,%f -> %f,%f,%f\n", v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);

    // Calculate the cosine of the angle
    float cos_theta = dot / (mag1 * mag2);

    // Clamp the value to the range [-1, 1] to avoid NaN due to floating-point inaccuracies
    if (cos_theta > 1.0) cos_theta = 1.0;
    else if (cos_theta < -1.0) cos_theta = -1.0;

    // Calculate the angle in radians
    float angle_rad = acos(cos_theta);

    // Convert to degrees
    return angle_rad * (180.0f / M_PI);
}

float angle_between_vectors(const ImVec2 &a, const ImVec2 &b) {
    // Calculate the "perp dot product" or the z-component of the cross product
    double cross_product_z = a.x * b.y - a.y * b.x;

    // Calculate the dot product
    double dot_product = a.x * b.x + a.y * b.y;

    // Use atan2 to get the signed angle
    return atan2(cross_product_z, dot_product) * (180.0f / M_PI);
}

float len2(const ImVec2 &v) {
	return v.x*v.x + v.y*v.y;
}
float len2(const ImVec4 &v) {
	return v.x*v.x + v.y*v.y + v.z*v.z;
}

static float zzz = 1.0;

void box(double x0, double y0, double z0, double x1, double y1, double z1, bool draw_dims = false)
{
	// the box

	glBegin(GL_LINE_LOOP);
	glVertex3d(x0, y0, z0);
	glVertex3d(x1, y0, z0);
	glVertex3d(x1, y1, z0);
	glVertex3d(x0, y1, z0);
	glEnd();
	glBegin(GL_LINE_LOOP);
	glVertex3d(x0, y0, z1);
	glVertex3d(x1, y0, z1);
	glVertex3d(x1, y1, z1);
	glVertex3d(x0, y1, z1);
	glEnd();
	glBegin(GL_LINES);
	glVertex3d(x0, y0, z0);
	glVertex3d(x0, y0, z1);
	glVertex3d(x1, y0, z0);
	glVertex3d(x1, y0, z1);
	glVertex3d(x1, y1, z0);
	glVertex3d(x1, y1, z1);
	glVertex3d(x0, y1, z0);
	glVertex3d(x0, y1, z1);
	glEnd();

	if (!draw_dims) return;

	// dimensions

	double d = 0.03/zzz;

	glColor3f(1, 0.5, 0.4);
	glBegin(GL_LINES);

	glVertex3d(x0, y0-1*d, z0); glVertex3d(x0, y0-3*d, z0);
	glVertex3d(x0, y0-2*d, z0); glVertex3d(x1, y0-2*d, z0);
	glVertex3d(x1, y0-1*d, z0); glVertex3d(x1, y0-3*d, z0);

	glVertex3d(x0-1*d, y0, z0); glVertex3d(x0-3*d, y0, z0);
	glVertex3d(x0-2*d, y0, z0); glVertex3d(x0-2*d, y1, z0);
	glVertex3d(x0-1*d, y1, z0); glVertex3d(x0-3*d, y1, z0);

	glVertex3d(x0-1*d, y0-2*d, z0); glVertex3d(x0-3*d, y0-2*d, z0);
	glVertex3d(x0-2*d, y0-2*d, z0); glVertex3d(x0-2*d, y0-2*d, z1);
	glVertex3d(x0-1*d, y0-2*d, z1); glVertex3d(x0-3*d, y0-2*d, z1);

	glEnd();

	// x

	float f = 0.0003/zzz;
	glPushMatrix();
	glTranslatef((x0+x1)/2, y0-2.5*d, z0);
	glScalef(f, f, f);
	glutAPrintf(ALIGN_CENTER | ALIGN_TOP, "%.2f", x1-x0);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(x0, y0-3.5*d, z0);
	glRotatef(90, 0, 0, 1);
	glScalef(f, f, f);
	glutAPrintf(ALIGN_RIGHT | ALIGN_MIDDLE, "%.2f", x0);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(x1, y0-3.5*d, z0);
	glRotatef(90, 0, 0, 1);
	glScalef(f, f, f);
	glutAPrintf(ALIGN_RIGHT | ALIGN_MIDDLE, "%.2f", x1);
	glPopMatrix();

	// y

	glPushMatrix();
	glTranslatef(x0-2.5*d, (y1+y0)/2, z0);
	glRotatef(90, 0, 0, 1);
	glScalef(f, f, f);
	glutAPrintf(ALIGN_CENTER | ALIGN_BOTTOM, "%.2f", y1-y0);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(x0-3.5*d, y0, z0);
	glScalef(f, f, f);
	glutAPrintf(ALIGN_RIGHT | ALIGN_MIDDLE, "%.2f", y0);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(x0-3.5*d, y1, z0);
	glScalef(f, f, f);
	glutAPrintf(ALIGN_RIGHT | ALIGN_MIDDLE, "%.2f", y1);
	glPopMatrix();

	// z

	glPushMatrix();
	glTranslatef(x0-2.5*d, y0-2*d, (z0+z1)/2);
	glRotatef(90, 0, 0, 1);
	glRotatef(-90, 0, 1, 0);
	glScalef(f, f, f);
	glutAPrintf(ALIGN_CENTER | ALIGN_BOTTOM, "%.2f", z1-z0);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(x0-3.5*d, y0-2*d, z0);
	glRotatef(90, 1, 0, 0);
	glScalef(f, f, f);
	glutAPrintf(ALIGN_RIGHT | ALIGN_MIDDLE, "%.2f", z0);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(x0-3.5*d, y0-2*d, z1);
	glRotatef(90, 1, 0, 0);
	glScalef(f, f, f);
	glutAPrintf(ALIGN_RIGHT | ALIGN_MIDDLE, "%.2f", z1);
	glPopMatrix();
}

void w_3d(void)
{
	ImGuiIO& io = GetIO(); (void)io;

	// 3D -----------------------------------------------------------------------------------
//	if (SplitterText("3D", &tabwindow_size, &size2)) {
//		fprintf(stderr, "Close 3D\n");
//	}

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

//if (resize_delta != 0.0f) {
//	SetWindowSize(GetWindowSize() + ImVec2(0, -resize_delta));
//}

	BeginChild("3D View", ImVec2(-FLT_MIN, -FLT_MIN), ImGuiChildFlags_AlwaysUseWindowPadding, 0);

	ImDrawList* drawList = GetWindowDrawList();
	ImVec2 pos = GetCursorScreenPos();
	ImVec2 size = GetContentRegionAvail();

	glfb->Resize(size.x, size.y);
	glfb->Bind();

	glClearColor(0, 0, 0, 0);
	glClearDepth(1);
	glShadeModel(GL_FLAT); //SMOOTH);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//	glShadeModel(GL_FLAT);

	glEnable(GL_LINE_SMOOTH);
//	glEnable(GL_POINT_SMOOTH);
//	glEnable(GL_POLYGON_SMOOTH);

	glEnable(GL_BLEND);
//	glEnable(GL_MULTISAMPLE);
	glDisable(GL_STENCIL_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
//glfwWindowHint(GLFW_SAMPLES, 4);

	glViewport(0, 0, size.x, size.y);

/*
	if (size.x > size.y)
		glScalef(1.0f, size.x / size.y, 1.0f);
	else
		glScalef(size.y / size.x, 1.0f, 1.0f);
*/
	static int mouse_dragging = 0;

//	static float curquat[4];
	static float lastquat[4];

	static bool init = false;
	static float zoom = 1.0f;

	static ImVec2 scale = {1.0f, 1.0f};

	if (!init) {
		init = true;
//		trackball(curquat, 0.0, 0.0, 0.0, 0.0);

		glMatrixMode(GL_PROJECTION);  
		glLoadIdentity();
/*
		const double f = 0.2;
		glFrustum(-f,f, -f,f, 1,10);
		glTranslatef(0, 0, -3);
*/
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glScalef(0.25, 0.25, 0.25);
//		glRotatef(-90, 1, 0, 0);
		update_iMV();
	}

	{
		glMatrixMode(GL_PROJECTION);  
		glLoadIdentity();
//		const double f = 0.2;
//		glFrustum(-f,f, -f,f, 1,10);
//		glTranslatef(0, 0, -3);
		if (size.x > size.y) {
			scale = ImVec2(1.0f, size.x / size.y);
		} else {
			scale = ImVec2(size.y / size.x, 1.0);
		}
		glScalef(scale.x, scale.y, 0.01f);
		glMatrixMode(GL_MODELVIEW);  
	}

//	if (mouse_dragging == 1 && IsMouseDragging(1)) {	// translate
	if ((mouse_dragging & 2) && IsMouseDown(1)) {
		ImVec2 d = io.MouseDelta * 2  / size.x;
		GLfloat m[4][4];
		glGetFloatv(GL_MODELVIEW_MATRIX, &m[0][0]);
		glLoadIdentity();
		glTranslatef(d.x, -d.y, 0.0f);
		glMultMatrixf(&m[0][0]);
		update_iMV();
	}


	if (zoom != 1.0f) {
		ImVec4 q = window_to_model(GetMousePos());
		glTranslatef(q.x, q.y, q.z);
		glScalef(zoom, zoom, zoom);
		glTranslatef(-q.x, -q.y, -q.z);
		zzz *= zoom;
		zoom = 1.0f;
		update_iMV();
	}


	bool dorot = false;

	if ((mouse_dragging & 1) && IsMouseDown(0)) {
		if (
			io.TouchActive[0] && io.TouchWasActive[0] 
			&& io.TouchActive[1] && io.TouchWasActive[1] 
			&& io.TouchDelta[0] != ImVec2(0.0f,0.0f) || io.TouchDelta[1] != ImVec2(0.0f,0.0f)
		) {
			ImVec4 a0 = window_to_model(io.TouchPosPrev[0]);
			ImVec4 a1 = window_to_model(io.TouchPos[0]);

			dorot = (len2(io.TouchPos[1] - io.TouchPos[0]) > 100*100 && len2(io.TouchPosPrev[1] - io.TouchPosPrev[0]) > 100*100);
			if (dorot) {
				ImVec4 b0 = window_to_model(io.TouchPosPrev[1]);
				ImVec4 b1 = window_to_model(io.TouchPos[1]);

				float l0 = sqrt((a0.x-b0.x)*(a0.x-b0.x) + (a0.y-b0.y)*(a0.y-b0.y) + (a0.z-b0.z)*(a0.z-b0.z));
				float l1 = sqrt((a1.x-b1.x)*(a1.x-b1.x) + (a1.y-b1.y)*(a1.y-b1.y) + (a1.z-b1.z)*(a1.z-b1.z));

				float angle = angle_between_vectors(io.TouchPos[1]-io.TouchPos[0], io.TouchPosPrev[1]-io.TouchPosPrev[0]);

				 {
					glTranslatef(a1.x, a1.y, a1.z);
					if (l0 > 0) {
						glScalef(l1/l0, l1/l0, l1/l0);
						zzz *= l1/l0;
					}
					if (angle != 0.0f) {
						ImVec4  z = z1_model();
						glRotatef(angle, z.x, z.y, z.z);
					}
					glTranslatef(-a1.x, -a1.y, -a1.z);
				}
			}

			ImVec4 da = a1 - a0;
			if (len2(da) < 100*100)
				glTranslatef(da.x, da.y, da.z);

			update_iMV();

		} else if (!io.TouchActive[1]) {
			ImVec2 d = io.MouseDelta * 2  / size.x;
			if (d.x != 0.0f || d.y != 0.0f) {
				ImVec2 cur = (GetMousePos() - pos - size/2) * 2  / size.x;
				ImVec2 prev = cur - d;
				trackball(lastquat, prev.x, -prev.y, cur.x, -cur.y);

				GLfloat p[4][4];
				glGetFloatv(GL_MODELVIEW_MATRIX, &p[0][0]);
				glLoadIdentity();

				GLfloat m[4][4];
				build_rotmatrix(m, lastquat);
				glMultMatrixf(&m[0][0]);

				glMultMatrixf(&p[0][0]);
				update_iMV();
			}
		}
	}

	struct v3 {
		double x;
		double y;
		double z;
	};

	PmCartesian spos;
	spos.x = emcStatus->motion.traj.actualPosition.coor[0] - emcStatus->task.toolOffset.coor[0];
	spos.y = emcStatus->motion.traj.actualPosition.coor[1] - emcStatus->task.toolOffset.coor[1];
	spos.z = emcStatus->motion.traj.actualPosition.coor[2] - emcStatus->task.toolOffset.coor[2];

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_DOUBLE, sizeof(PmCartesian), vtx.data());
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_UNSIGNED_BYTE, 4, vtx_clr.data());
	glLineWidth(2.0f);
	glDrawArrays(GL_LINE_STRIP, 0, vtx.size());
//	glPointSize(1.0);
//	glDrawArrays(GL_POINTS, 0, vtx.size());

	static std::vector<PmCartesian> backplot;

	if (clear_backplot) {
		clear_backplot = false;
		backplot.clear();
	}

	if (backplot.empty()
		|| fabs(backplot.back().x - spos.x) > 0.005
		|| fabs(backplot.back().y - spos.y) > 0.005
		|| fabs(backplot.back().z - spos.z) > 0.005
	) {
		backplot.push_back(spos);
	}


	if (!backplot.empty()) {
		glDisableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_DOUBLE, sizeof(spos), backplot.data());
		glColor3f(1.0, 0.3, 0.3);
		glLineWidth(2.0f);
		glDrawArrays(GL_LINE_STRIP, 0, backplot.size());
	}

	{
		//  --------------- limits ---------------
		glPushMatrix();
		glTranslated(-emcStatus->task.toolOffset.coor[0], -emcStatus->task.toolOffset.coor[1], -emcStatus->task.toolOffset.coor[2]);
		glLineWidth(1.0f);
		EMC_AXIS_STAT *axis = emcStatus->motion.axis;
		glColor3f(0.5, 0, 0);
		box(axis[0].minPositionLimit, axis[1].minPositionLimit, axis[2].minPositionLimit,
		    axis[0].maxPositionLimit, axis[1].maxPositionLimit, axis[2].maxPositionLimit);
		glColor3f(0, 0.5, 0);
		glPopMatrix();

		box(vtx_min.coor[0], vtx_min.coor[1], vtx_min.coor[2], 
		    vtx_max.coor[0], vtx_max.coor[1], vtx_max.coor[2], true);

		float f = 0.0003/zzz;
		float l = 0.1/zzz;

		// --------------- G5x offset ---------------
		if (
			fabs(emcStatus->task.g5x_offset.tran.x) > 0.0001 ||
			fabs(emcStatus->task.g5x_offset.tran.y) > 0.0001 ||
			fabs(emcStatus->task.g5x_offset.tran.z) > 0.0001
		) {
			glColor3f(0.0, 0.8, 0.8);
			glBegin(GL_LINES);
			glVertex3d(0, 0, 0);
			glVertex3d(emcStatus->task.g5x_offset.tran.x, emcStatus->task.g5x_offset.tran.y, emcStatus->task.g5x_offset.tran.z);
			glEnd();

			glPushMatrix();
				glScalef(f, f, f);
				int g5x = emcStatus->task.g5x_index;
				glTranslatef(0, 50, 0);
				glutAPrintf(ALIGN_BOTTOM | ALIGN_CENTER, offs_name[g5x]);
			glPopMatrix();
		}

		// --------------- G92 offset ---------------
		if (
			fabs(emcStatus->task.g92_offset.tran.x) > 0.0001 ||
			fabs(emcStatus->task.g92_offset.tran.y) > 0.0001 ||
			fabs(emcStatus->task.g92_offset.tran.z) > 0.0001
		) {
			glPushMatrix();
				glTranslated(emcStatus->task.g5x_offset.tran.x, emcStatus->task.g5x_offset.tran.y, emcStatus->task.g5x_offset.tran.z);
				glRotated(emcStatus->task.rotation_xy, 0, 0, 1);
				glBegin(GL_LINES);
					glVertex3d(0, 0, 0);
					glVertex3d(emcStatus->task.g92_offset.tran.x, emcStatus->task.g92_offset.tran.y, emcStatus->task.g92_offset.tran.z);
				glEnd();
				glScalef(f, f, f);
				glTranslatef(0, 50, 0);
				glutAPrintf(ALIGN_BOTTOM | ALIGN_CENTER, "G92");
			glPopMatrix();
		}

		// --------------- working coorditate system ---------------
	glPushMatrix();

		glTranslated(emcStatus->task.g5x_offset.tran.x, emcStatus->task.g5x_offset.tran.y, emcStatus->task.g5x_offset.tran.z);
		glRotated(emcStatus->task.rotation_xy, 0, 0, 1);
		glTranslated(emcStatus->task.g92_offset.tran.x, emcStatus->task.g92_offset.tran.y, emcStatus->task.g92_offset.tran.z);
		glBegin(GL_LINES);
		glColor3f(0.3, 1, 0.3);	glVertex3d(0, 0, 0); glVertex3d(l, 0, 0);
		glColor3f(1, 0.3, 0.3);	glVertex3d(0, 0, 0); glVertex3d(0, l, 0);
		glColor3f(0.2, 0.6, 1);	glVertex3d(0, 0, 0); glVertex3d(0, 0, l);
		glEnd();

		if (0) {
			ImVec4  o = origin_model();
			ImVec4  z = z1_model();
			
			glBegin(GL_LINES);
			glColor4f(1, 0.2, 1, 0.3);
			glVertex3f(0, 0, 0); glVertex3f(z.x, z.y, z.z);
			glVertex3f(0, 0, 0); glVertex3f(o.x, o.y, o.z);
			glEnd();
		}

		glPushMatrix();

		glTranslatef(l, 0, 0);
		glScalef(f, f, f);
		glColor3f(0.3, 1, 0.3);
		glutStrokeCharacter('X');
		glPopMatrix();

		glPushMatrix();
		glTranslatef(0, l, 0);
		glScalef(f, f, f);
	//	glRotatef(90.0, 1, 0, 0);
		glColor3f(1, 0.3, 0.3);
		glutStrokeCharacter('Y');
		glPopMatrix();

		glPushMatrix();
		glRotatef(90.0, 1, 0, 0);
		glTranslatef(0, l, 0);
		glScalef(f, f, f);
		glColor3f(0.2, 0.6, 1);
		glutStrokeCharacter('Z');
		glPopMatrix();

	glPopMatrix();


		// --------------- spindle tool ---------------

	glPushMatrix();
		glLoadIdentity();
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		GLfloat lightPosition[]  = { -10.0,  10.0,  20.0 , 1.0 };	// directional
		GLfloat colorWhite[]     = { 1.00, 1.00, 1.00, 1.0 };
		GLfloat colorDarkGray[]  = { 0.10, 0.10, 0.10, 1.0 };
		GLfloat colorLightGray[] = { 0.75, 0.75, 0.75, 1.0 };
		glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
//		glLightfv(GL_LIGHT0, GL_AMBIENT, colorDarkGray);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, colorLightGray);
//		glLightfv(GL_LIGHT0, GL_SPECULAR, colorWhite);
	glPopMatrix();
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colorLightGray);
//		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, colorLightGray);
//		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, colorLightGray);
		glMaterialf(GL_FRONT, GL_SHININESS, 0.0);

		glPushMatrix();
		glTranslated(emcStatus->motion.traj.actualPosition.coor[0], emcStatus->motion.traj.actualPosition.coor[1], emcStatus->motion.traj.actualPosition.coor[2]);
		glTranslated(-emcStatus->task.toolOffset.coor[0], -emcStatus->task.toolOffset.coor[1], -emcStatus->task.toolOffset.coor[2]);
		glColor4f(1, 1, 1, 0.6);
		glBegin(GL_TRIANGLE_FAN);
		glVertex3f(0, 0, 0);
		float x1, y1;
		glVertex3f(x1 = 2, y1 = 0, 10.0f);
		ImVec4 pv(2, 0, 10, 1);
		for (int a = 0; a <= 32; ++a) {
			float x = 2*cos(2 * M_PI * a / 32);
			float y = 2*sin(2 * M_PI * a / 32);
			ImVec4 n = cross_product(pv, ImVec4(x, y, 10, 1));
			n.w = magnitude(n);
			normalize(n);
			glNormal3f(n.x, n.y, n.z);
			glVertex3f(x, y, 10.0f);
		}
		glEnd();
		glPopMatrix();
		glDisable(GL_LIGHTING);
	}

//    glFlush();
	glfb->Unbind();

	uint f_tex = glfb->getFrameTexture();
	drawList->AddImage((void*)f_tex, pos, ImVec2(pos + size), ImVec2(0, 1), ImVec2(1, 0));

	Indent(8); 

	static float q[4] = {-0.36, 1, 0.3, 0.56};
	bool redraw = false;
/**
	PushItemWidth(-100);
	redraw = DragFloat4("X0 Y0 X1 Y1", q, 0.01f, -1.0f, 1.0f);
	PopItemWidth();
*/
	if (Button("Z", {48, 48})) {
		interp->plot(emcStatus->task.file);
		EmcPose center, siz;
		emcPoseAdd(&vtx_max, &vtx_min, &center);
		emcPoseSub(&vtx_max, &vtx_min, &siz);
		if (size.x > size.y) {
			zzz = std::min(1.9 / siz.tran.x, 1.9 * size.y / size.x / siz.tran.y);
		} else {
			zzz = std::min(1.9 * size.x / size.y / siz.tran.x, 1.9 / siz.tran.y);
		}

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glScalef(zzz, zzz, zzz);
		glTranslated(-center.coor[0]/2, -center.coor[1]/2, -center.coor[2]/2);
		update_iMV();
	}
	SameLine();

	if (Button("X", {48, 48})) {
		interp->plot(emcStatus->task.file);
		EmcPose center, siz;
		emcPoseAdd(&vtx_max, &vtx_min, &center);
		emcPoseSub(&vtx_max, &vtx_min, &siz);
		if (size.x > size.y) {
			zzz = std::min(1.9 / siz.tran.x, 1.9 * size.y / size.x / siz.tran.y);
		} else {
			zzz = std::min(1.9 * size.x / size.y / siz.tran.x, 1.9 / siz.tran.y);
		}

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(-90, 0, 0, 1);
		glRotatef(-90, 0, 1, 0);
		glScalef(zzz, zzz, zzz);
		glTranslated(-center.coor[0]/2, -center.coor[1]/2, -center.coor[2]/2);
		update_iMV();
	}
	SameLine();

	if (Button("Y", {48, 48})) {
		interp->plot(emcStatus->task.file);
		EmcPose center, siz;
		emcPoseAdd(&vtx_max, &vtx_min, &center);
		emcPoseSub(&vtx_max, &vtx_min, &siz);
		if (size.x > size.y) {
			zzz = std::min(1.9 / siz.tran.x, 1.9 * size.y / size.x / siz.tran.y);
		} else {
			zzz = std::min(1.9 * size.x / size.y / siz.tran.x, 1.9 / siz.tran.y);
		}

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(-90, 1, 0, 0);
		glScalef(zzz, zzz, zzz);
		glTranslated(-center.coor[0]/2, -center.coor[1]/2, -center.coor[2]/2);
		update_iMV();
	}
	SameLine();
	
	if (Button("P", {48, 48}) || redraw) {
		interp->plot(emcStatus->task.file);
		EmcPose center;
		emcPoseAdd(&vtx_max, &vtx_min, &center);
//		emcPoseSub(&vtx_max, &vtx_min, &size);
//		zzz = size.coor[0] > size.coor[1] ? 1.9 / size.coor[0] : 1.9 / size.coor[1];
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		zzz = 1.0;
//		glScalef(zzz, zzz, zzz);
		glRotatef(q[0]*180, q[1], q[2], q[3]);
		glTranslated(-center.coor[0]/2, -center.coor[1]/2, -center.coor[2]/2);

		{
			GLfloat p[4][4];
			// get bounds
			glGetFloatv(GL_MODELVIEW_MATRIX, &p[0][0]);
			ImVec2 wmin = {FLT_MAX, FLT_MAX};
			ImVec2 wmax = {-FLT_MAX, -FLT_MAX};
			for (auto i : vtx) {
				auto v = model_to_view(p, i);
				if (v.x < wmin.x) wmin.x = v.x;
				if (v.x > wmax.x) wmax.x = v.x;
				if (v.y < wmin.y) wmin.y = v.y;
				if (v.y > wmax.y) wmax.y = v.y;
				//fprintf(stderr, "%f,%f,%f >>> %f,%f,%f\n", i.tran.x, i.tran.y, i.tran.z, v.x, v.y, v.z);
			}
			ImVec2 cen = (wmax + wmin) / 2;
			ImVec2 siz = wmax - wmin;

			if (size.x > size.y) {
				zzz = std::min(1.9 / siz.x, 1.9 * size.y / size.x / siz.y);
			} else {
				zzz = std::min(1.9 * size.x / size.y / siz.x, 1.9 / siz.y);
			}
			glGetFloatv(GL_MODELVIEW_MATRIX, &p[0][0]);
			glLoadIdentity();
//			glTranslatef(-cen.x/2*zzz, -cen.y/2*zzz, 0);
//			glTranslatef(-cen.x, -cen.y, 0);
			glScalef(zzz, zzz, zzz);
			glMultMatrixf(&p[0][0]);
		}
		update_iMV();


	}
	SameLine();
	if (Button("\u0382", {48, 48})) clear_backplot = true;

//	static int cnt = 0;
//	PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
//	if (Button("INC")) ++cnt;
//	PopItemFlag();
//	SameLine();
/*	SameLine();
	ImVec4 q = window_to_model(GetMousePos());
	Text("%.0f x %.0f %.3f,%.3f,%.3f,%.3f", GetWindowWidth(), GetWindowHeight(), q.x, q.y, q.z, q.w);

	if(0){
		ImVec2 p = (GetMousePos() - GetWindowPos());
		ImVec4 q = window_to_model(GetMousePos());
		Text("%d x %d %f %.3f,%.3f -> %.3f,%.3f,%.3f", (int)size.x, (int)size.y, zoom, p.x, p.y, q.x, q.y, q.z);
	}
*/
	EndChild();
	PopStyleVar();
	PopStyleVar();

	if (IsItemClicked(0)) mouse_dragging |= 1;
	if (IsItemClicked(1)) mouse_dragging |= 2;
	if (!IsMouseDown(0)) mouse_dragging &= ~1;
	if (!IsMouseDown(1)) mouse_dragging &= ~2;
//	if (mouse_dragging == 0 && !IsMouseDown(0)) mouse_dragging = -1;
//	if (mouse_dragging == 1 && !IsMouseDown(1)) mouse_dragging = -1;

	if (IsItemHovered()) {
		if (io.MouseWheel < 0) zoom = 1 / (1.2 * -io.MouseWheel);
		if (io.MouseWheel > 0) zoom = 1.2f * io.MouseWheel;
	}

	// ------------------------------------------------------------------------------------------

 	//ImGui::BeginChild("Test");
//	GetWindowDrawList()->AddLine(ImVec2(0,0), io.MousePos, io.MouseDown[0] ? IM_COL32(200, 50, 50, 255) : IM_COL32(50, 200, 50, 255), 1.0f);
//if (io.TouchActive[1]) 
//	GetWindowDrawList()->AddLine(io.TouchPos[0], io.TouchPos[1], dorot ? IM_COL32(250, 50, 50, 155) : IM_COL32(50, 250, 50, 155));
	//ImGui::EndChild();

}

