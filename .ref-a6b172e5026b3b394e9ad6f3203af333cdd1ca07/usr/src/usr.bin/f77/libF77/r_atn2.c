/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)r_atn2.c	5.2	%G%
 */

float r_atn2(x,y)
float *x, *y;
{
double atan2();
return( atan2(*x,*y) );
}
