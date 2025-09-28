#!/bin/sh
set -xe

HEADERS=../../../include
LIBDIR=../../../lib
PYTHON_CPPFLAGS=-I/usr/include/python3.12

g++ -o use-rs274 use-rs274.cc -g \
    -Wall -Wextra -Wno-return-type -Wno-unused-parameter \
    -I $HEADERS $PYTHON_CPPFLAGS -L $LIBDIR -Wl,-rpath,$LIBDIR $PYTHON_EXTRA_LDFLAGS $PYTHON_LIBS $PYTHON_EXTRA_LIBS -lrs274 -ltooldata

export INI_FILE_NAME=../../../configs/sim/axis/axis_mm.ini
LD_BIND_NOW=YesPlease ./use-rs274 3D_Chips.ngc
