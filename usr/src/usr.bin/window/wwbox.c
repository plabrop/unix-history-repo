/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)wwbox.c	3.8 (Berkeley) 6/6/90";
#endif /* not lint */

#include "ww.h"
#include "tt.h"

wwbox(w, r, c, nr, nc)
register struct ww *w;
register r, c;
int nr, nc;
{
	register r1, c1;
	register i;

	r1 = r + nr - 1;
	c1 = c + nc - 1;
	wwframec(w, r, c, WWF_D|WWF_R);
	for (i = c + 1; i < c1; i++)
		wwframec(w, r, i, WWF_L|WWF_R);
	wwframec(w, r, i, WWF_L|WWF_D);
	for (i = r + 1; i < r1; i++)
		wwframec(w, i, c1, WWF_U|WWF_D);
	wwframec(w, i, c1, WWF_U|WWF_L);
	for (i = c1 - 1; i > c; i--)
		wwframec(w, r1, i, WWF_R|WWF_L);
	wwframec(w, r1, i, WWF_R|WWF_U);
	for (i = r1 - 1; i > r; i--)
		wwframec(w, i, c, WWF_D|WWF_U);
}
