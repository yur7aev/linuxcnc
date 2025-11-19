
/* Copyright (c) Mark J. Kilgard, 1994, 2001. */

/* This program is freely distributable without licensing fees
   and is provided without guarantee or warrantee expressed or
   implied. This program is -not- in the public domain. */

#include <GL/gl.h>
#include "glutstroke.h"
#include <stdio.h>
#include <stdarg.h>

extern StrokeFontRec glutStrokeMonoRoman;
extern StrokeFontRec glutStrokeRoman;

#define FONT glutStrokeRoman
const float spacing = 20;

void glutStrokeCharacter(int c)
{
  const StrokeCharRec *ch;
  const StrokeRec *stroke;
  const CoordRec *coord;
  StrokeFontPtr fontinfo;
  int i, j;

  fontinfo = (StrokeFontPtr) &FONT;

  if (c < 0 || c >= fontinfo->num_chars) {
    return;
  }
  ch = &(fontinfo->ch[c]);
  if (ch) {
    for (i = ch->num_strokes, stroke = ch->stroke;
      i > 0; i--, stroke++) {
      glBegin(GL_LINE_STRIP);
      for (j = stroke->num_coords, coord = stroke->coord;
        j > 0; j--, coord++) {
        glVertex2f(coord->x, coord->y);
      }
      glEnd();
    }
    glTranslatef(ch->right + spacing, 0.0, 0.0);
  }
}

float glutStrlen(const char *s)
{
	if (!s) return 0.0f;
	StrokeFontPtr fontinfo = (StrokeFontPtr) &FONT;
	float l = 0.0f;
	int c;
	while ((c = *s++)) 
		if (c > 0 && c < fontinfo->num_chars)
			l += fontinfo->ch[c].right + spacing;
	return l;
}

int glutPrintf(const char *fmt, ...)
{
	static char buf[256];
	char *s = buf;
	va_list args;
	va_start(args, fmt);
	int rc = vsnprintf(s, 255, fmt, args);
	va_end(args);
	if (rc > 0) while (*s) glutStrokeCharacter(*s++);
	return rc;
}

int glutAPrintf(int align, const char *fmt, ...)
{
	static char buf[256];
	char *s = buf;
	va_list args;
	va_start(args, fmt);
	int rc = vsnprintf(s, 255, fmt, args);
	va_end(args);

	if (rc > 0) {
		StrokeFontPtr fontinfo = (StrokeFontPtr) &FONT;
		float height = fontinfo->top - fontinfo->bottom;
		float len = glutStrlen(s);

		if (align & ALIGN_TOP) glTranslatef(0, -fontinfo->top, 0);
		else if (align & ALIGN_MIDDLE) glTranslatef(0, -fontinfo->top/2, 0);
		if (align & ALIGN_RIGHT) glTranslatef(-len, 0, 0);
		else if (align & ALIGN_CENTER) glTranslatef(-len/2, 0, 0);

		while (*s) glutStrokeCharacter(*s++);
	}
	return rc;
}
