#ifndef lint
static char *sccsid = "@(#)nice.c	4.2 (Berkeley) %G%";
#endif

#include <stdio.h>

#include <sys/time.h>
#include <sys/resource.h>

main(argc, argv)
	int argc;
	char *argv[];
{
	int nicarg = 10;

	if (argc > 1 && argv[1][0] == '-') {
		nicarg = atoi(&argv[1][1]);
		argc--, argv++;
	}
	if (argc < 2) {
		fputs("usage: nice [ -n ] command\n", stderr);
		exit(1);
	}
	if (setpriority(PRIO_PROCESS, 0, nicarg) < 0) {
		perror("setpriority");
		exit(1);
	}
	execvp(argv[1], &argv[1]);
	perror(argv[1]);
	exit(1);
}
