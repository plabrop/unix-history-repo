/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
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
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ppt.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdio.h>

void	putppt();

main(argc, argv)
	int argc;
	char **argv;
{
	register int c;
	register char *p;

	(void) puts("___________");
	if (argc > 1)
		while (p = *++argv)
			for (; *p; ++p)
				putppt((int)*p);
	else while ((c = getchar()) != EOF)
		putppt(c);
	(void) puts("___________");
	exit(0);
}

static void
putppt(c)
	register int c;
{
	register int i;

	(void) putchar('|');
	for (i = 7; i >= 0; i--) {
		if (i == 2)
			(void) putchar('.');	/* feed hole */
		if ((c&(1<<i)) != 0)
			(void) putchar('o');
		else
			(void) putchar(' ');
	}
	(void) putchar('|');
	(void) putchar('\n');
}
