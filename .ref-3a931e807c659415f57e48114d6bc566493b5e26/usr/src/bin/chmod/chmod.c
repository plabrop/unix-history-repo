/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)chmod.c	8.8 (Berkeley) %G%";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FTS *ftsp;
	FTSENT *p;
	mode_t *set;
	long val;
	int oct, omode;
	int Hflag, Lflag, Pflag, Rflag, ch, fflag, fts_options, hflag, rval;
	char *ep, *mode;

	Hflag = Lflag = Pflag = Rflag = fflag = hflag = 0;
	while ((ch = getopt(argc, argv, "HLPRXfgorstuwx")) != EOF)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = Pflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = Pflag = 0;
			break;
		case 'P':
			Pflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':		/* XXX: undocumented. */
			fflag = 1;
			break;
		case 'h':
			/*
			 * In System V (and probably POSIX.2) the -h option
			 * causes chmod to change the mode of the symbolic
			 * link.  4.4BSD's symbolic links don't have modes,
			 * so it's an undocumented noop.  Do syntax checking,
			 * though.
			 */
			hflag = 1;
			break;
		/*
		 * XXX
		 * "-[rwx]" are valid mode commands.  If they are the entire
		 * argument, getopt has moved past them, so decrement optind.
		 * Regardless, we're done argument processing.
		 */
		case 'g': case 'o': case 'r': case 's':
		case 't': case 'u': case 'w': case 'X': case 'x':
			if (argv[optind - 1][0] == '-' &&
			    argv[optind - 1][1] == ch &&
			    argv[optind - 1][2] == '\0')
				--optind;
			goto done;
		case '?':
		default:
			usage();
		}
done:	argv += optind;
	argc -= optind;

	if (argc < 2)
		usage();

	fts_options = FTS_PHYSICAL;
	if (Rflag) {
		if (hflag)
			errx(1,
		"the -R and -h options may not be specified together.");
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	}

	mode = *argv;
	if (*mode >= '0' && *mode <= '7') {
		errno = 0;
		val = strtol(mode, &ep, 8);
		if (val > INT_MAX || val < 0)
			errno = ERANGE;
		if (errno)
			err(1, "invalid file mode: %s", mode);
		if (*ep)
			errx(1, "invalid file mode: %s", mode);
		omode = val;
		oct = 1;
	} else {
		if ((set = setmode(mode)) == NULL)
			errx(1, "invalid file mode: %s", mode);
		oct = 0;
	}

	if ((ftsp = fts_open(++argv, fts_options, 0)) == NULL)
		err(1, NULL);
	for (rval = 0; (p = fts_read(ftsp)) != NULL;) {
		switch (p->fts_info) {
		case FTS_D:
			if (Rflag)		/* Change it at FTS_DP. */
				continue;
			fts_set(ftsp, p, FTS_SKIP);
			break;
		case FTS_DNR:			/* Warn, chmod, continue. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
		case FTS_SL:			/* Ignore. */
		case FTS_SLNONE:
			/*
			 * The only symlinks that end up here are ones that
			 * don't point to anything and ones that we found
			 * doing a physical walk.
			 */
			continue;
		default:
			break;
		}
		if (chmod(p->fts_accpath, oct ? omode :
		    getmode(set, p->fts_statp->st_mode)) && !fflag) {
			warn(p->fts_path);
			rval = 1;
		}
	}
	if (errno)
		err(1, "fts_read");
	exit(rval);
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: chmod [-R [-H | -L | -P]] mode file ...\n");
	exit(1);
}
