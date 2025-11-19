#pragma once

#include <Python.h> // must be first header
#include "linuxcnc.h"              // LINELEN
#include "rs274ngc.hh"
#include "canon.hh"
#include "../../tooldata/tooldata.hh"

#include <vector>

struct RGB {
public:
	RGB(unsigned char rr, unsigned char gg, unsigned char bb, unsigned char aa) :
		r(rr), g(gg), b(bb), a(aa)
	{}
	unsigned char r, g, b, a;
};

extern std::vector<PmCartesian> vtx;
extern std::vector<struct RGB> vtx_clr;
extern EmcPose vtx_min;
extern EmcPose vtx_max;

class Interp {
public:
	Interp();
	~Interp() {
		delete ii;
	}

	void plot(const char *filename);

	InterpBase *ii;
};
