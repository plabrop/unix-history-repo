/*	displayq.c	4.4	83/05/27	*/
/*
 * Routines to display the state of the queue.
 */

#include "lp.h"

#define	JOBCOL		40	/* column for job # in -l format */
#define OWNCOL		7	/* start of Owner column in normal */

/*
 * Stuff for handling job specifications
 */
extern char	*user[];	/* users to process */
extern int	users;		/* # of users in user array */
extern int	requ[];		/* job number of spool entries */
extern int	requests;	/* # of spool requests */

int	lflag;			/* long output option */
char	current[40];		/* current file being printed */
int	garbage;		/* # of garbage cf files */
int	rank;			/* order to be printed (-1=none, 0=active) */
long	totsize;		/* total print job size in bytes */
int	first;			/* first file in ``files'' column? */
int	col;			/* column on screen */
int	SIZCOL = 62;		/* start of Size column in normal */
int	sendtorem;		/* are we sending to a remote? */
char	file[132];		/* print file name */

char	*head0 = "Rank   Owner      Job  Files";
char	*head1 = "Total Size\n";

/*
 * Display the current state of the queue. Format = 1 if long format.
 */
displayq(format)
	int format;
{
	register struct queue *q;
	register int i, nitems, fd;
	struct queue **queue;
	struct stat statb;
	FILE *fp;

	lflag = format;
	totsize = 0;
	rank = -1;

	if ((i = pgetent(line, printer)) < 0)
		fatal("cannot open printer description file");
	else if (i == 0)
		fatal("unknown printer");
	if ((LP = pgetstr("lp", &bp)) == NULL)
		LP = DEFDEVLP;
	if ((RP = pgetstr("rp", &bp)) == NULL)
		RP = DEFLP;
	if ((SD = pgetstr("sd", &bp)) == NULL)
		SD = DEFSPOOL;
	if ((LO = pgetstr("lo", &bp)) == NULL)
		LO = DEFLOCK;
	if ((ST = pgetstr("st", &bp)) == NULL)
		ST = DEFSTAT;
	RM = pgetstr("rm", &bp);

	/*
	 * If there is no local printer, then print the queue on
	 * the remote machine and then what's in the queue here.
	 * Note that a file in transit may not show up in either queue.
	 */
	if (*LP == '\0') {
		register char *cp;
		char c;

		sendtorem++;
		(void) sprintf(line, "%c%s", format + '\3', RP);
		cp = line;
		for (i = 0; i < requests; i++) {
			cp += strlen(cp);
			(void) sprintf(cp, " %d", requ[i]);
		}
		for (i = 0; i < users; i++) {
			cp += strlen(cp);
			*cp++ = ' ';
			strcpy(cp, user[i]);
		}
		strcat(line, "\n");
		fd = getport(RM);
		if (fd < 0) {
			if (from != host)
				printf("%s: ", host);
			printf("connection to %s is down\n", RM);
		} else {
			i = strlen(line);
			if (write(fd, line, i) != i)
				fatal("Lost connection");
			while ((i = read(fd, line, sizeof(line))) > 0)
				(void) fwrite(line, 1, i, stdout);
			(void) close(fd);
		}
	}
	/*
	 * Find all the control files in the spooling directory
	 */
	if (chdir(SD) < 0)
		fatal("cannot chdir to spooling directory");
	if ((nitems = getq(&queue)) < 0)
		fatal("cannot examine spooling area\n");
	if (nitems == 0) {
		if (sendtorem)
			printf("\n%s: ", host);
		printf("no entries\n");
		return(0);
	}
	fp = fopen(LO, "r");
	if (fp == NULL || flock(fileno(fp), FSHLOCK|FNBLOCK) == 0) {
		if (fp != NULL)
			fclose(fp);
		garbage = nitems;
		if (stat(LO, &statb) >= 0 && (statb.st_mode & 0100))
			printf("Warning: %s is down\n", printer);
		else
			printf("Warning: no daemon present\n");
	} else {
		register char *cp = current;

		/* skip daemon pid */
		while ((*cp = getc(fp)) != EOF && *cp != '\n');
		/* read current file name */
		while ((*cp = getc(fp)) != EOF && *cp != '\n')
			cp++;
		*cp = '\0';
		fclose(fp);
		/*
		 * Print the status file to show what the daemon is doing.
		 */
		if (sendtorem)
			printf("\n%s: ", host);
		if ((fd = open(ST, FRDONLY|FSHLOCK)) >= 0) {
			while ((i = read(fd, line, sizeof(line))) > 0)
				(void) fwrite(line, 1, i, stdout);
			(void) close(fd);
		} else
			putchar('\n');
	}
	/*
	 * Now, examine the control files and print out the jobs to
	 * be done for each user.
	 */
	if (!lflag)
		header();
	for (i = 0; i < nitems; i++) {
		q = queue[i];
		inform(q->q_name);
		free(q);
	}
	free(queue);
	return(nitems-garbage);
}

/*
 * Print the header for the short listing format
 */
header()
{
	printf(head0);
	col = strlen(head0)+1;
	blankfill(SIZCOL);
	printf(head1);
}

inform(cf)
	char *cf;
{
	register int j, k;
	register char *cp;
	FILE *cfp;

	/*
	 * There's a chance the control file has gone away
	 * in the meantime; if this is the case just keep going
	 */
	if ((cfp = fopen(cf, "r")) == NULL)
		return;

	if (rank < 0)
		rank = 0;
	if (sendtorem || garbage || strcmp(cf, current))
		rank++;
	j = 0;
	while (getline(cfp)) {
		switch (line[0]) {
		case 'P': /* Was this file specified in the user's list? */
			if (!inlist(line+1, cf)) {
				fclose(cfp);
				return;
			}
			if (lflag) {
				printf("\n%s: ", line+1);
				col = strlen(line+1) + 2;
				prank(rank);
				blankfill(JOBCOL);
				printf(" [job %s]\n", cf+3);
			} else {
				col = 0;
				prank(rank);
				blankfill(OWNCOL);
				printf("%-10s %-3d  ", line+1, atoi(cf+3));
				col += 16;
				first = 1;
			}
			continue;
		default: /* some format specifer and file name? */
			if (line[0] < 'a' || line[0] > 'z')
				continue;
			if (j == 0 || strcmp(file, line+1) != 0)
				strcpy(file, line+1);
			j++;
			continue;
		case 'N':
			show(line+1, file, j);
			file[0] = '\0';
			j = 0;
		}
	}
	fclose(cfp);
	if (!lflag) {
		blankfill(SIZCOL);
		printf("%D bytes\n", totsize);
		totsize = 0;
	}
}

inlist(name, file)
	char *name, *file;
{
	register int *r, n;
	register char **u, *cp;

	if (users == 0 && requests == 0)
		return(1);
	/*
	 * Check to see if it's in the user list
	 */
	for (u = user; u < &user[users]; u++)
		if (!strcmp(*u, name))
			return(1);
	/*
	 * Check the request list
	 */
	for (n = 0, cp = file+3; isdigit(*cp); )
		n = n * 10 + (*cp++ - '0');
	for (r = requ; r < &requ[requests]; r++)
		if (*r == n && !strcmp(cp, from))
			return(1);
	return(0);
}

show(nfile, file, copies)
	register char *nfile, *file;
{
	if (strcmp(nfile, " ") == 0)
		nfile = "(standard input)";
	if (lflag)
		ldump(nfile, file, copies);
	else
		dump(nfile, file, copies);
}

/*
 * Fill the line with blanks to the specified column
 */
blankfill(n)
	register int n;
{
	while (col++ < n)
		putchar(' ');
}

/*
 * Give the abbreviated dump of the file names
 */
dump(nfile, file, copies)
	char *nfile, *file;
{
	register short n, fill;
	struct stat lbuf;

	/*
	 * Print as many files as will fit
	 *  (leaving room for the total size)
	 */
	 fill = first ? 0 : 2;	/* fill space for ``, '' */
	 if (((n = strlen(nfile)) + col + fill) >= SIZCOL-4) {
		if (col < SIZCOL) {
			printf(" ..."), col += 4;
			blankfill(SIZCOL);
		}
	} else {
		if (first)
			first = 0;
		else
			printf(", ");
		printf("%s", nfile);
		col += n+fill;
	}
	if (*file && !stat(file, &lbuf))
		totsize += copies * lbuf.st_size;
}

/*
 * Print the long info about the file
 */
ldump(nfile, file, copies)
	char *nfile, *file;
{
	struct stat lbuf;

	putchar('\t');
	if (copies > 1)
		printf("%-2d copies of %-19s", copies, nfile);
	else
		printf("%-32s", nfile);
	if (*file && !stat(file, &lbuf))
		printf(" %D bytes", lbuf.st_size);
	else
		printf(" ??? bytes");
	putchar('\n');
}

/*
 * Print the job's rank in the queue,
 *   update col for screen management
 */
prank(n)
{
	char line[100];
	static char *r[] = {
		"th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"
	};

	if (n == 0) {
		printf("active");
		col += 6;
		return;
	}
	if ((n/10) == 1)
		(void) sprintf(line, "%dth", n);
	else
		(void) sprintf(line, "%d%s", n, r[n%10]);
	col += strlen(line);
	printf("%s", line);
}
