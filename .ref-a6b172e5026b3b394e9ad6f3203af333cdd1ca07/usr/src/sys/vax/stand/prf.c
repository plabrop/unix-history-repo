/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)prf.c	7.7 (Berkeley) %G%
 */

#include "sys/param.h"

#include "../include/mtpr.h"
#include "../vax/cons.h"

/*
 * Scaled down version of C Library printf.
 * Used to print diagnostic information directly on console tty.
 * Since it is not interrupt driven, all system activities are
 * suspended.  Printf should not be used for chit-chat.
 *
 * One additional format: %b is supported to decode error registers.
 * Usage is:
 *	printf("reg=%b\n", regval, "<base><arg>*");
 * Where <base> is the output base expressed as a control character,
 * e.g. \10 gives octal; \20 gives hex.  Each arg is a sequence of
 * characters, the first of which gives the bit number to be inspected
 * (origin 1), and the next characters (up to a control character, i.e.
 * a character <= 32), give the name of the register.  Thus
 *	printf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 * would produce output:
 *	reg=2<BITTWO,BITONE>
 */
/*VARARGS1*/
printf(fmt, x1)
	char *fmt;
	unsigned x1;
{

	prf(fmt, &x1);
}

prf(fmt, adx)
	register char *fmt;
	register u_int *adx;
{
	register int b, c, i;
	char *s;
	int any;

loop:
	while ((c = *fmt++) != '%') {
		if(c == '\0')
			return;
		putchar(c);
	}
again:
	c = *fmt++;
	/* THIS CODE IS VAX DEPENDENT IN HANDLING %l? AND %c */
	switch (c) {

	case 'l':
		goto again;
	case 'x': case 'X':
		b = 16;
		goto number;
	case 'd': case 'D':
	case 'u':		/* what a joke */
		b = 10;
		goto number;
	case 'o': case 'O':
		b = 8;
number:
		printn((u_long)*adx, b);
		break;
	case 'c':
		b = *adx;
		for (i = 24; i >= 0; i -= 8)
			if (c = (b >> i) & 0x7f)
				putchar(c);
		break;
	case 'b':
		b = *adx++;
		s = (char *)*adx;
		printn((u_long)b, *s++);
		any = 0;
		if (b) {
			while (i = *s++) {
				if (b & (1 << (i-1))) {
					putchar(any? ',' : '<');
					any = 1;
					for (; (c = *s) > 32; s++)
						putchar(c);
				} else
					for (; *s > 32; s++)
						;
			}
			if (any)
				putchar('>');
		}
		break;

	case 's':
		s = (char *)*adx;
		while (c = *s++)
			putchar(c);
		break;
	}
	adx++;
	goto loop;
}

/*
 * Print a character on console.
 */
putchar(c)
	register c;
{
	register s, timo;
#if VAX630 || VAX650
	extern (*v_putc)();

	if (v_putc) {
		(*v_putc)(c);
		if (c == '\n')
			(*v_putc)('\r');
		return;
	}
#endif
	timo = 30000;
	/*
	 * Try waiting for the console tty to come ready,
	 * otherwise give up after a reasonable time.
	 */
	while((mfpr(TXCS)&TXCS_RDY) == 0)
		if(--timo == 0)
			break;
	if(c == 0)
		return;
	s = mfpr(TXCS);
	mtpr(TXCS,0);
	mtpr(TXDB, c&0xff);
	if(c == '\n')
		putchar('\r');
	putchar(0);
	mtpr(TXCS, s);
}

getchar()
{
	register c;
#if VAX630 || VAX650
	extern (*v_getc)();

	if (v_getc) {
		c = (*v_getc)();
	} else {
#endif
	while((mfpr(RXCS)&RXCS_DONE) == 0)
		;
	c = mfpr(RXDB)&0177;
#if VAX630 || VAX650
	}
#endif
	if (c=='\r')
		c = '\n';
	if (c != '\b' && c != '\177')
		putchar(c);
	return(c);
}
