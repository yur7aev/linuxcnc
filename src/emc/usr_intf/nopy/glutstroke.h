#ifndef __glutstroke_h__
#define __glutstroke_h__

/* Copyright (c) Mark J. Kilgard, 1994. */

/* This program is freely distributable without licensing fees
   and is provided without guarantee or warrantee expressed or
   implied. This program is -not- in the public domain. */

typedef struct {
	float x;
	float y;
} CoordRec, *CoordPtr;

typedef struct {
	int num_coords;
	const CoordRec *coord;
} StrokeRec, *StrokePtr;

typedef struct {
	int num_strokes;
	const StrokeRec *stroke;
	float center;
	float right;
} StrokeCharRec, *StrokeCharPtr;

typedef struct {
	const char *name;
	int num_chars;
	const StrokeCharRec *ch;
	float top;
	float bottom;
} StrokeFontRec, *StrokeFontPtr;

typedef void *GLUTstrokeFont;

void glutStrokeCharacter(int c);
int glutPrintf(const char *fmt, ...);
#define ALIGN_LEFT   0x01
#define ALIGN_CENTER 0x02
#define ALIGN_RIGHT  0x04
#define ALIGN_TOP    0x08
#define ALIGN_MIDDLE 0x10
#define ALIGN_BOTTOM 0x20
int glutAPrintf(int align, const char *fmt, ...);

#endif /* __glutstroke_h__ */
