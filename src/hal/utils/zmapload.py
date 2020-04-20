#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# zmapload reads x,y,z points from stdin, constructs a regular z height grid
# and loads it into a shared memory segment
#
# zmapload <probe.txt

import os,sys,math,struct,ctypes
from optparse import Option, OptionParser
import hal

options = [Option( '-v','--view', action='store_true', dest='view', help="view active z map"),
           Option( '-r','--reset', action='store_true', dest='reset', help="reset z map"),
           Option( '-z','--zlimit', type='float', default=3, dest='zlimit', help="max acceptable zmap height")
          ]

key = 4712

def get_buffer(nx, ny):
    c = hal.component("notused")
    sm = hal.shm(c, key, 40+nx*ny*8)
    return sm.getbuffer()

class Point(object):
    def __init__(self,x,y,z):
        self.x = x
        self.y = y
        self.z = z
    def __add__(self, p):
        return Point(self.x + p.x, self.y + p.y, self.z + p.z)
    def __sub__(self, p):
        return Point(self.x - p.x, self.y - p.y, self.z - p.z)
    def __mul__(self, v):
	return Point(self.y*v.z-self.z*v.y, self.x*v.z-self.z*v.x, self.x*v.y-self.y*v.x)
    def __str__(self):
        return "<%g,%g,%g>" % (self.x, self.y, self.z)
    def dot(self, v):
	return self.x*v.x + self.y*v.y + self.z*v.z
    def len(self):
	return math.sqrt(self.dot(self))
    def norm(self):
	l = self.len()
	if l > 0: return Point(self.x/l, self.y/l, self.z/l)
	return self
    def angle(self, v):
        return math.acos(self.dot(v)/self.len()/v.len())

def add_grid(g, new):
    for x in g.keys():
        if abs(x - new) < 2:
            g[x][0] += new
            g[x][1] += 1
            return
    g[new] = [ new, 1 ]

def index(a, first, step):
    return int(round((a - first) / step))

def average_grid(g):
    x = g.keys()
    x.sort()
    f = x[0]
    l = x[-1]
    first = g[f][0] / g[f][1]
    last  = g[l][0] / g[l][1]
    n = len(x)

    return (n, first, (last - first) / (n-1))

def readpoints(filename, zlimit):

    points = []
    zmin = 1e10
    zmax = -1e10

    f = open(filename)
    for l in f:
        l.strip()
        p = map(float,l.split()[0:3])
        if len(p) == 3:
            points.append(Point(p[0],p[1],p[2]))
        zmax = max(zmax, p[2])
    f.close()

    for p in points:
        p.z -= zmax
        zmin = min(zmin, p.z)

    if abs(zmin) > zlimit:
        print >>sys.stderr, "z map height=%g > %g limit exceeded, abort!" % (zmin, zlimit)
	return None

    print >>sys.stderr, "z map height=%g" % (zmin)

    return points

def make_grid(points):
    gx = {}
    gy = {}
    for p in points:
        add_grid(gx, p.x)
        add_grid(gy, p.y)
    (nx, x0, dx) = average_grid(gx)
    (ny, y0, dy) = average_grid(gy)

    if nx <= 0 or ny <= 0:
	print >>stderr, "need at lead one point!"
	return False
    if nx > 32 or ny > 32:
	print >>stderr, "max grid is 32x32!"
	return False

    print "z map %dx%d loaded" % (nx, ny)

    g = [[1111111]*nx for i in range(ny)]

    for p in points:
        i = index(p.x, x0, dx)
        j = index(p.y, y0, dy)
        g[j][i] = p.z

    b = get_buffer(nx, ny)

    struct.pack_into('ii', b, 0,    0,  0)	# 8  - disable while loading
    struct.pack_into('dd', b, 8,    x0, y0)	# 16
    struct.pack_into('dd', b, 8+16, dx, dy)	# 16

    for j in range(ny):
        for i in range(nx):
            z = g[j][i]
            if z > 1111:
                print "empty grid node, abort!"
                return False
            struct.pack_into('d', b, 8+32 + j*8*nx + i*8, g[j][i])

    struct.pack_into('ii', b, 0,    ny, nx)	# 8

def reset_grid():
    b = get_buffer(1, 1)
    struct.pack_into('ii', b, 0, 0, 0)	# nx, ny

def view_grid():
    b = get_buffer(1, 1)
    (nx, ny, x0, y0, dx, dy) = struct.unpack('iidddd',b[0:40])
    if nx > 0 and ny > 0:
        x1 = x0 + (nx-1)*dx
        y1 = y0 + (ny-1)*dy
        print "%dx%d\nX:%.2f..%.2f Y:%.2f..%.2f" % (nx, ny, x0, x1, y0, y1)
    else:
        print "z map is disabled"
        print x0, y0, dx, dy

def main():
    global opts
    (progdir, progname) = os.path.split(sys.argv[0])

    usage = "usage: %prog [options] file.txt"
    parser = OptionParser(usage=usage)
    parser.disable_interspersed_args()
    parser.add_options(options)
    (opts, args) = parser.parse_args()

    if opts.reset:
        reset_grid()
        return 0

    if opts.view:
        view_grid()
        return 0

    if len(args) > 0:
        filename = args[0]
        points = readpoints(args[0], opts.zlimit)
        if points:
            make_grid(points)
            return 0
    else:
        parser.error("filename is missing")

    return -1

if __name__ == '__main__':
    sys.exit(main())

