/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Adams.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Hacks to support "-a|c|n" flags on the command line which enable VJ
 * header compresion and disable ICMP.
 * If this is good all rights go to B & L Jolitz, otherwise send your
 * comments to Reagan (/dev/null).
 *
 * nerd@percival.rain.com (Michael Galassi) 92.09.03
 *
 * Hacked to change from sgtty to POSIX termio style serial line control
 * and added flag to enable cts/rts style flow control.
 *
 * blymn@awadi.com.au (Brett Lymn) 93.04.04
 *
 * Put slattach in it's own process group so it can't be killed
 * accidentally. Close the connection on SIGHUP and SIGINT. Write a
 * syslog entry upon opening and closing the connection.  Rich Murphey
 * and Brad Huntting.
 *
 * Add '-r command' option: runs 'command' upon recieving SIGHUP
 * resulting from loss of carrier.  Log any errors after forking.
 * Rich 8/13/93
 *
 * This version of slattach includes many changes by David Greenman, Brian
 * Smith, Chris Bradley, and me (Michael Galassi).  None of them are
 * represented as functional anywhere outside of RAINet though they do work
 * for us.  Documentation is limited to the usage message for now.  If you
 * make improovments please pass them back.
 *
 * Added '-u UCMD' which runs 'UCMD <old> <new>' whenever the slip
 * unit number changes where <old> and <new> are the old and new unit
 * numbers, respectively.  Also added the '-z' option which forces
 * invocation of the redial command (-r CMD) upon startup regardless
 * of whether the com driver claims (sometimes mistakenly) that
 * carrier is present. Also added '-e ECMD' which runs ECMD before
 * exiting.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)slattach.c	4.6 (Berkeley) 6/1/90";
static char rcsid[] = "$Header: /a/cvs/386BSD/src/sbin/slattach/slattach.c,v 1.5 1993/09/06 23:24:07 rgrimes Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_slvar.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <paths.h>
#include <syslog.h>
#include <signal.h>
#include <strings.h>
#define MSR_DCD 0x80

extern int errno;
extern char *sys_errlist[];

#define DEFAULT_BAUD	9600

void	sighup_handler();	/* SIGHUP handler */
void	sigint_handler();	/* SIGINT handler */
void	exit_handler(int ret);	/* run exit_cmd iff specified upon exit. */
void	setup_line();		/* configure slip line */
void	attach_line();		/* switch to slip line discipline */

int	fd = -1;
char	*dev = (char *)0;
int	slipdisc = SLIPDISC;
int	ttydisc = TTYDISC;
int	flow_control = 0;	/* non-zero to enable hardware flow control. */
int	modem_control = 0;	/* non-zero iff we watch carrier. */
int	redial_on_startup = 0;	/* iff non-zero execute redial_cmd on startup */
int	speed = DEFAULT_BAUD;
int	slflags = 0;		/* compression flags */
int	unit = -1;		/* slip device unit number */
FILE	*console;

char	devname[32];
char	hostname[MAXHOSTNAMELEN];
char	*redial_cmd = 0;	/* command to exec upon shutdown. */
char	*config_cmd = 0;	/* command to exec if slip unit changes. */
char	*exit_cmd = 0;		/* command to exec before exiting. */
char	string[100];

static char usage_str[] = "\
usage: %s [-achlnz] [-e command] [-r command] [-s speed] [-u command] device\n\
	-a	-- autoenable VJ compression\n\
	-c	-- enable VJ compression\n\
	-e ECMD	-- execute ECMD before exiting\n\
	-h	-- turn on cts/rts style flow control\n\
	-l	-- disable modem control (CLOCAL) and ignore carrier detect\n\
	-n	-- throw out ICMP packets\n\
	-r RCMD	-- execute RCMD upon loss of carrier\n\
	-s #	-- set baud rate (default 9600)\n\
	-u UCMD	-- execute 'UCMD <old> <new>' if slip unit number changes\n\
	-z	-- execute RCMD upon startup irrespective of carrier\n";

int main(int argc, char **argv)
{
	int option;
	int comstate;
	char name[32];
	extern char *optarg;
	extern int optind;

	while ((option = getopt(argc, argv, "ace:hlnr:s:u:z")) != EOF) {
		switch (option) {
		case 'a':
			slflags |= SC_AUTOCOMP;
			slflags &= ~SC_COMPRESS;
			break;
		case 'c':
			slflags |= SC_COMPRESS;
			slflags &= ~SC_AUTOCOMP;
			break;
		case 'e':
			exit_cmd = (char*) strdup (optarg);
			break;
		case 'h':
			flow_control |= CRTSCTS;
			break;
		case 'l':
			modem_control |= CLOCAL;
			break;
		case 'n':
			slflags |= SC_NOICMP;
			break;
		case 'r':
			redial_cmd = (char*) strdup (optarg);
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'u':
			config_cmd = (char*) strdup (optarg);
			break;
		case 'z':
			redial_on_startup = 1;
			break;
		case '?':
		default:
			fprintf(stderr, usage_str, argv[0]);
			exit_handler(1);
		}
	}

	if (optind == argc - 1)
		dev = argv[optind];


	if (dev == (char *)0) {
		fprintf(stderr, usage_str, argv[0]);
		exit_handler(2);
	}
	if (strncmp(_PATH_DEV, dev, sizeof(_PATH_DEV) - 1)) {
		strcpy(devname, _PATH_DEV);
		strcat(devname, "/");
		strncat(devname, dev, 10);
		dev = devname;
	}

	daemon(0,0);		/* fork, setsid, chdir /, and close std*. */

	/* Note: daemon() closes stderr, so log errors from here on. */
	(void)sprintf(name,"slattach[%d]", getpid());
	openlog(name,LOG_CONS,LOG_DAEMON);

	if ((fd = open(dev, O_RDWR | O_NONBLOCK)) < 0) {
                syslog(LOG_ERR, "open(%s): %m", dev);
		exit_handler(1);
	}
	/* acquire the serial line as a controling terminal. */
	if (ioctl(fd, TIOCSCTTY, 0) < 0)
		syslog(LOG_NOTICE,"ioctl(TIOCSCTTY) failed: %s\n",
		       strerror(errno));
	/* Make us the foreground process group associated with the
	   slip line which is our controlling terminal. */
	if (tcsetpgrp(fd, getpid()) < 0)
		syslog(LOG_NOTICE,"tcsetpgrp failed: %s\n",
		       strerror(errno));
	/* upon INT log a timestamp and exit.  */
	if ((int)signal(SIGINT,sigint_handler) < 0)
		syslog(LOG_NOTICE,"cannot install SIGINT handler: %s\n",
		       strerror(errno));
	/* upon HUP redial and reconnect.  */
	if ((int)signal(SIGHUP,sighup_handler) < 0)
		syslog(LOG_NOTICE,"cannot install SIGHUP handler: %s\n",
		       strerror(errno));

	setup_line();

	if (redial_on_startup)
		sighup_handler();
	else
		attach_line();
	if (!(modem_control & CLOCAL)) {
		ioctl(fd, TIOCMGET, &comstate);
		if (!(comstate & MSR_DCD)) { /* check for carrier */
			/* force a redial if no carrier */
			kill (getpid(), SIGHUP);
		}
	}
	for (;;)
		sigsuspend(0L);
}

void setup_line()
{
	struct termios tty;

	tty.c_lflag = tty.c_iflag = tty.c_oflag = 0;
	tty.c_cflag = CREAD | CS8 | flow_control | modem_control;
	tty.c_ispeed = tty.c_ospeed = speed;
	/* set the line speed and flow control */
	if (tcsetattr(fd, TCSAFLUSH, &tty) < 0) {
                syslog(LOG_ERR, "tcsetattr(TCSAFLUSH): %m");
		exit_handler(1);
	}
	/* set data terminal ready */
	if (ioctl(fd, TIOCSDTR) < 0) {
                perror("ioctl(TIOCSDTR)");
                close(fd);
                exit_handler(1);
        }
	/* Switch to slip line discipline. */
	if (ioctl(fd, TIOCSETD, &slipdisc) < 0) {
                syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		exit_handler(1);
	}
	/* Assert any compression or no-icmp flags. */
	if (ioctl(fd, SLIOCSFLAGS, &slflags) < 0) {
                syslog(LOG_ERR, "ioctl(SLIOCSFLAGS): %m");
		exit_handler(1);
	}
}

/* switch to slip line discipline and configure the network. */
void attach_line()
{
	int new_unit;

	/* find out what unit number we were assigned */
        if (ioctl(fd, SLIOCGUNIT, (caddr_t)&new_unit) < 0) {
                syslog(LOG_ERR, "ioctl(SLIOCGUNIT): %m");
                exit_handler(1);
        }
	/* don't compare unit numbers if this is the first time to attach. */
	if (unit < 0)
		unit = new_unit;
	/* iff the unit number changes either invoke config_cmd or punt. */
	if (config_cmd) {
		char *s;
		if (new_unit != unit) {
			syslog(LOG_ERR, "slip unit changed from sl%d to sl%d, but no -u CMD was specified!");
			exit_handler(1);
		}
		s = (char*) malloc(strlen(config_cmd) + 32);
		sprintf (s, "%s %d %d", config_cmd, unit, new_unit);
		syslog(LOG_NOTICE,"configuring sl%d, invoking: %s",
		       unit, s);
		system(s);
		free (s);
		unit = new_unit;
	}
	syslog(LOG_NOTICE,"sl%d connected to %s at %d b/s\n",unit,dev,speed);
}

/* Signal handler for SIGHUP when carrier is dropped. */
void sighup_handler()
{
	/* reset discipline */
	if (ioctl(fd, TIOCSETD, &ttydisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		exit_handler(1);
	}
	/* invoke a shell for redial_cmd or punt. */
	if (redial_cmd) {
		syslog(LOG_NOTICE,"sl%d caught SIGHUP on %s, running %s",
		       unit,dev,redial_cmd);
		system(redial_cmd);
	} else {
		syslog(LOG_NOTICE,"No redial phone number or command. aborting.\n",dev);
		exit_handler(0);
	}
	setup_line();
	attach_line();
}
/* Signal handler for SIGINT.  We just log and exit. */
void sigint_handler()
{
	syslog(LOG_NOTICE,"sl%d on %s caught SIGINT, exiting.\n",unit,dev);
	exit_handler(0);
}
/* Run config_cmd if specified before exiting. */
void exit_handler(int ret)
{
	/* invoke a shell for exit_cmd. */
	if (exit_cmd) {
		syslog(LOG_NOTICE,"exiting after running %s", exit_cmd);
		system(exit_cmd);
	}
	exit(ret);
}

/* local variables: */
/* c-indent-level: 8 */
/* c-argdecl-indent: 0 */
/* c-label-offset: -8 */
/* c-continued-statement-offset: 8 */
/* c-brace-offset: 0 */
/* comment-column: 32 */
/* end: */
