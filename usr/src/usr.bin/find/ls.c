/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)ls.c	5.6 (Berkeley) %G%";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>
#include <tzfile.h>
#include <errno.h>
#include <utmp.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

/* Derived from the print routines in the ls(1) source code. */

void
printlong(name, accpath, sb)
	char *name;			/* filename to print */
	char *accpath;			/* current valid path to filename */
	struct stat *sb;		/* stat buffer */
{
	char modep[15], *user_from_uid(), *group_from_gid();

	(void)printf("%6lu %4ld ", sb->st_ino, sb->st_blocks);
	(void)strmode(sb->st_mode, modep);
	(void)printf("%s %3u %-*s %-*s ", modep, sb->st_nlink, UT_NAMESIZE,
	    user_from_uid(sb->st_uid, 0), UT_NAMESIZE,
	    group_from_gid(sb->st_gid, 0));

	if (S_ISCHR(sb->st_mode) || S_ISBLK(sb->st_mode))
		(void)printf("%3d, %3d ", major(sb->st_rdev),
		    minor(sb->st_rdev));
	else
		(void)printf("%8ld ", sb->st_size);
	printtime(sb->st_mtime);
	(void)printf("%s", name);
	if (S_ISLNK(sb->st_mode))
		printlink(accpath);
	(void)putchar('\n');
}

printtime(ftime)
	time_t ftime;
{
	int i;
	char *longstring, *ctime();
	time_t time();

	longstring = ctime((long *)&ftime);
	for (i = 4; i < 11; ++i)
		(void)putchar(longstring[i]);

#define	SIXMONTHS	((DAYSPERNYEAR / 2) * SECSPERDAY)
	if (ftime + SIXMONTHS > time((time_t *)NULL))
		for (i = 11; i < 16; ++i)
			(void)putchar(longstring[i]);
	else {
		(void)putchar(' ');
		for (i = 20; i < 24; ++i)
			(void)putchar(longstring[i]);
	}
	(void)putchar(' ');
}

printlink(name)
	char *name;
{
	int lnklen;
	char path[MAXPATHLEN + 1];

	if ((lnklen = readlink(name, path, MAXPATHLEN)) == -1) {
		(void)fprintf(stderr,
		    "\nfind: %s: %s\n", name, strerror(errno));
		return;
	}
	path[lnklen] = '\0';
	(void)printf(" -> %s", path);
}
