/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kevin Ruddy.
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
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)fold.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

#include <stdio.h>
#include <string.h>

#define	DEFLINEWIDTH	80

main(argc, argv)
	int argc;
	char **argv;
{
	extern int errno, optind;
	extern char *optarg;
	register int ch;
	int width;
	char *p;

	width = -1;
	while ((ch = getopt(argc, argv, "0123456789w:")) != EOF)
		switch (ch) {
		case 'w':
			if ((width = atoi(optarg)) <= 0) {
				(void)fprintf(stderr,
				    "fold: illegal width value.\n");
				exit(1);
			}
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (width == -1) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					width = atoi(++p);
				else
					width = atoi(argv[optind] + 1);
			}
			break;
		default:
			(void)fprintf(stderr,
			    "usage: fold [-w width] [file ...]\n");
			exit(1);
		}
	argv += optind;
	argc -= optind;

	if (width == -1)
		width = DEFLINEWIDTH;
	if (!*argv)
		fold(width);
	else for (; *argv; ++argv)
		if (!freopen(*argv, "r", stdin)) {
			(void)fprintf(stderr,
			    "fold: %s: %s\n", *argv, strerror(errno));
			exit(1);
		} else
			fold(width);
	exit(0);
}

fold(width)
	register int width;
{
	register int ch, col, new;

	for (col = 0;;) {
		switch (ch = getchar()) {
		case EOF:
			return;
		case '\b':
			new = col ? col - 1 : 0;
			break;
		case '\n':
		case '\r':
			new = 0;
			break;
		case '\t':
			new = (col + 8) & ~7;
			break;
		default:
			new = col + 1;
			break;
		}

		if (new > width) {
			putchar('\n');
			col = 0;
		}
		putchar(ch);

		switch (ch) {
		case '\b':
			if (col > 0)
				--col;
			break;
		case '\n':
		case '\r':
			col = 0;
			break;
		case '\t':
			col += 8;
			col &= ~7;
			break;
		default:
			++col;
			break;
		}
	}
}
