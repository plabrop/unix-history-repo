/*-
 * Copyright (c) 1983, 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rsh.c	8.4 (Berkeley) %G%";
#endif /* not lint */

/*
 * $Source: mit/rsh/RCS/rsh.c,v $
 * $Header: mit/rsh/RCS/rsh.c,v 5.1 89/07/31 19:28:59 kfall Exp Locker: kfall $
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <netinet/in.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <varargs.h>

#include "pathnames.h"

#ifdef KERBEROS
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>

CREDENTIALS cred;
Key_schedule schedule;
int use_kerberos = 1, doencrypt;
char dst_realm_buf[REALM_SZ], *dest_realm;
extern char *krb_realmofhost();
#endif

/*
 * rsh - remote shell
 */
int	rfd2;

char   *copyargs __P((char **));
void	sendsig __P((int));
void	talk __P((int, long, pid_t, int));
void	usage __P((void));
void	warning __P(());

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct passwd *pw;
	struct servent *sp;
	long omask;
	int argoff, asrsh, ch, dflag, nflag, one, rem;
	pid_t pid;
	uid_t uid;
	char *args, *host, *p, *user;

	argoff = asrsh = dflag = nflag = 0;
	one = 1;
	host = user = NULL;

	/* if called as something other than "rsh", use it as the host name */
	if (p = strrchr(argv[0], '/'))
		++p;
	else
		p = argv[0];
	if (strcmp(p, "rsh"))
		host = p;
	else
		asrsh = 1;

	/* handle "rsh host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}

#ifdef KERBEROS
#define	OPTIONS	"8KLdek:l:nw"
#else
#define	OPTIONS	"8KLdel:nw"
#endif
	while ((ch = getopt(argc - argoff, argv + argoff, OPTIONS)) != EOF)
		switch(ch) {
		case 'K':
#ifdef KERBEROS
			use_kerberos = 0;
#endif
			break;
		case 'L':	/* -8Lew are ignored to allow rlogin aliases */
		case 'e':
		case 'w':
		case '8':
			break;
		case 'd':
			dflag = 1;
			break;
		case 'l':
			user = optarg;
			break;
#ifdef KERBEROS
		case 'k':
			dest_realm = dst_realm_buf;
			strncpy(dest_realm, optarg, REALM_SZ);
			break;
#endif
		case 'n':
			nflag = 1;
			break;
#ifdef KERBEROS
#endif
		case '?':
		default:
			usage();
		}
	optind += argoff;

	/* if haven't gotten a host yet, do so */
	if (!host && !(host = argv[optind++]))
		usage();

	/* if no further arguments, must have been called as rlogin. */
	if (!argv[optind]) {
		if (asrsh)
			*argv = "rlogin";
		execv(_PATH_RLOGIN, argv);
		err(1, "can't exec %s", _PATH_RLOGIN);
	}

	argc -= optind;
	argv += optind;

	if (!(pw = getpwuid(uid = getuid())))
		errx(1, "unknown user id");
	/* Accept user1@host format, though "-l user2" overrides user1 */
	p = strchr(host, '@');
	if (p) {
		*p = '\0';
		if (!user && p > host)
			user = host;
		host = p + 1;
		if (*host == '\0')
			usage();
	}
	if (!user)
		user = pw->pw_name;

#ifdef KERBEROS
#endif

	args = copyargs(argv);

	sp = NULL;
#ifdef KERBEROS
	if (use_kerberos) {
		sp = getservbyname((doencrypt ? "ekshell" : "kshell"), "tcp");
		if (sp == NULL) {
			use_kerberos = 0;
			warning("can't get entry for %s/tcp service",
			    doencrypt ? "ekshell" : "kshell");
		}
	}
#endif
	if (sp == NULL)
		sp = getservbyname("shell", "tcp");
	if (sp == NULL)
		errx(1, "shell/tcp: unknown service");

#ifdef KERBEROS
try_connect:
	if (use_kerberos) {
		struct hostent *hp;

		/* fully qualify hostname (needed for krb_realmofhost) */
		hp = gethostbyname(host);
		if (hp != NULL && !(host = strdup(hp->h_name)))
			err(1, NULL);

		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(host);

			rem = krcmd(&host, sp->s_port, user, args, &rfd2,
			    dest_realm);
		if (rem < 0) {
			use_kerberos = 0;
			sp = getservbyname("shell", "tcp");
			if (sp == NULL)
				errx(1, "shell/tcp: unknown service");
			if (errno == ECONNREFUSED)
				warning("remote host doesn't support Kerberos");
			if (errno == ENOENT)
				warning("can't provide Kerberos auth data");
			goto try_connect;
		}
	} else {
		if (doencrypt)
			errx(1, "the -x flag requires Kerberos authentication");
		rem = rcmd(&host, sp->s_port, pw->pw_name, user, args, &rfd2);
	}
#else
	rem = rcmd(&host, sp->s_port, pw->pw_name, user, args, &rfd2);
#endif

	if (rem < 0)
		exit(1);

	if (rfd2 < 0)
		errx(1, "can't establish stderr");
	if (dflag) {
		if (setsockopt(rem, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
		if (setsockopt(rfd2, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
	}

	(void)setuid(uid);
	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGTERM));
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, sendsig);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGQUIT, sendsig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		(void)signal(SIGTERM, sendsig);

	if (!nflag) {
		pid = fork();
		if (pid < 0)
			err(1, "fork");
	}

#ifdef KERBEROS
#endif
	{
		(void)ioctl(rfd2, FIONBIO, &one);
		(void)ioctl(rem, FIONBIO, &one);
	}

	talk(nflag, omask, pid, rem);

	if (!nflag)
		(void)kill(pid, SIGKILL);
	exit(0);
}

void
talk(nflag, omask, pid, rem)
	int nflag;
	long omask;
	pid_t pid;
	int rem;
{
	int cc, wc;
	fd_set readfrom, ready, rembits;
	char *bp, buf[BUFSIZ];

	if (!nflag && pid == 0) {
		(void)close(rfd2);

reread:		errno = 0;
		if ((cc = read(0, buf, sizeof buf)) <= 0)
			goto done;
		bp = buf;

rewrite:	
		FD_ZERO(&rembits);
		FD_SET(rem, &rembits);
		if (select(16, 0, &rembits, 0, 0) < 0) {
			if (errno != EINTR)
				err(1, "select");
			goto rewrite;
		}
		if (!FD_ISSET(rem, &rembits))
			goto rewrite;
#ifdef KERBEROS
#endif
			wc = write(rem, bp, cc);
		if (wc < 0) {
			if (errno == EWOULDBLOCK)
				goto rewrite;
			goto done;
		}
		bp += wc;
		cc -= wc;
		if (cc == 0)
			goto reread;
		goto rewrite;
done:
		(void)shutdown(rem, 1);
		exit(0);
	}

	(void)sigsetmask(omask);
	FD_ZERO(&readfrom);
	FD_SET(rfd2, &readfrom);
	FD_SET(rem, &readfrom);
	do {
		ready = readfrom;
		if (select(16, &ready, 0, 0, 0) < 0) {
			if (errno != EINTR)
				err(1, "select");
			continue;
		}
		if (FD_ISSET(rfd2, &ready)) {
			errno = 0;
#ifdef KERBEROS
#endif
				cc = read(rfd2, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rfd2, &readfrom);
			} else
				(void)write(2, buf, cc);
		}
		if (FD_ISSET(rem, &ready)) {
			errno = 0;
#ifdef KERBEROS
#endif
				cc = read(rem, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rem, &readfrom);
			} else
				(void)write(1, buf, cc);
		}
	} while (FD_ISSET(rfd2, &readfrom) || FD_ISSET(rem, &readfrom));
}

void
sendsig(sig)
	int sig;
{
	char signo;

	signo = sig;
#ifdef KERBEROS
#endif
		(void)write(rfd2, &signo, 1);
}

#ifdef KERBEROS
/* VARARGS */
void
warning(va_alist)
va_dcl
{
	va_list ap;
	char *fmt;

	(void)fprintf(stderr, "rsh: warning, using standard rsh: ");
	va_start(ap);
	fmt = va_arg(ap, char *);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, ".\n");
}
#endif

char *
copyargs(argv)
	char **argv;
{
	int cc;
	char **ap, *args, *p;

	cc = 0;
	for (ap = argv; *ap; ++ap)
		cc += strlen(*ap) + 1;
	if (!(args = malloc((u_int)cc)))
		err(1, NULL);
	for (p = args, ap = argv; *ap; ++ap) {
		(void)strcpy(p, *ap);
		for (p = strcpy(p, *ap); *p; ++p);
		if (ap[1])
			*p++ = ' ';
	}
	return (args);
}

void
usage()
{

	(void)fprintf(stderr,
	    "usage: rsh [-nd%s]%s[-l login] [login@]host [command]\n",
#ifdef KERBEROS
	    "", " [-k realm] ");
#else
	    "", " ");
#endif
	exit(1);
}
