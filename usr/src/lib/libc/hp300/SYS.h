/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)SYS.h	5.3 (Berkeley) 5/30/90
 */

#include <sys/syscall.h>

/* vax/tahoe compat */
#define	ret	rts
#define	r0	d0
#define	r1	d1

#ifdef PROF
#ifdef __GNUC__
#define	ENTRY(x)	.globl _/**/x; .even; _/**/x:; .data; PROF/**/x:; \
			.long 0; .text; link a6,#0; lea PROF/**/x,a0; \
			jbsr mcount; unlk a6
#else
#define	ENTRY(x)	.globl _/**/x; .even; _/**/x:; .data; PROF/**/x:; \
			.long 0; .text; lea PROF/**/x,a0; jbsr mcount
#endif
#else
#define	ENTRY(x)	.globl _/**/x; .even; _/**/x:
#endif PROF
#define	SYSCALL(x)	.even; err: jmp cerror; ENTRY(x); movl #SYS_/**/x,d0; \
			trap #0; jcs err
#define	PSEUDO(x,y)	ENTRY(x); movl #SYS_/**/y,d0; trap #0;

#define	ASMSTR		.asciz

	.globl	cerror
