/*
 * Copyright (c) 1983 Eric P. Allman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)conf.h	8.78 (Berkeley) %G%
 */

/*
**  CONF.H -- All user-configurable parameters for sendmail
*/

# include <sys/param.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/file.h>
# include <sys/wait.h>
# include <fcntl.h>
# include <signal.h>

/**********************************************************************
**  Table sizes, etc....
**	There shouldn't be much need to change these....
**********************************************************************/

# define MAXLINE	2048		/* max line length */
# define MAXNAME	256		/* max length of a name */
# define MAXPV		40		/* max # of parms to mailers */
# define MAXATOM	200		/* max atoms per address */
# define MAXMAILERS	25		/* maximum mailers known to system */
# define MAXRWSETS	100		/* max # of sets of rewriting rules */
# define MAXPRIORITIES	25		/* max values for Precedence: field */
# define MAXMXHOSTS	20		/* max # of MX records */
# define SMTPLINELIM	990		/* maximum SMTP line length */
# define MAXKEY		128		/* maximum size of a database key */
# define MEMCHUNKSIZE	1024		/* chunk size for memory allocation */
# define MAXUSERENVIRON	100		/* max envars saved, must be >= 3 */
# define MAXALIASDB	12		/* max # of alias databases */

# ifndef QUEUESIZE
# define QUEUESIZE	1000		/* max # of jobs per queue run */
# endif

/**********************************************************************
**  Compilation options.
**
**	#define these if they are available; comment them out otherwise.
**********************************************************************/

# define LOG		1	/* enable logging */
# define UGLYUUCP	1	/* output ugly UUCP From lines */
# define NETUNIX	1	/* include unix domain support */
# define NETINET	1	/* include internet support */
# define SETPROCTITLE	1	/* munge argv to display current status */
# define NAMED_BIND	1	/* use Berkeley Internet Domain Server */
# define MATCHGECOS	1	/* match user names from gecos field */
# define XDEBUG		1	/* enable extended debugging */

# ifdef NEWDB
# define USERDB		1	/* look in user database (requires NEWDB) */
# endif

/*
**  Most systems have symbolic links today, so default them on.  You
**  can turn them off by #undef'ing this below.
*/

# define HASLSTAT	1	/* has lstat(2) call */

/*
**  General "standard C" defines.
**
**	These may be undone later, to cope with systems that claim to
**	be Standard C but aren't.  Gcc is the biggest offender -- it
**	doesn't realize that the library is part of the language.
**
**	Life would be much easier if we could get rid of this sort
**	of bozo problems.
*/

#ifdef __STDC__
# define HASSETVBUF	1	/* we have setvbuf(3) in libc */
#endif

/**********************************************************************
**  Operating system configuration.
**
**	Unless you are porting to a new OS, you shouldn't have to
**	change these.
**********************************************************************/

/*
**  Per-Operating System defines
*/


/*
**  HP-UX -- tested for 8.07, 9.00, and 9.01.
*/

# ifdef __hpux
/* avoid m_flags conflict between db.h & sys/sysmacros.h on HP 300 */
# undef m_flags
# define SYSTEM5	1	/* include all the System V defines */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASSETREUID	1	/* has setreuid(2) call */
# define setreuid(r, e)		setresuid(r, e, -1)	
# define LA_TYPE	LA_FLOAT
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# define GIDSET_T	gid_t
# define _PATH_UNIX	"/hp-ux"
# ifndef _PATH_SENDMAILCF
#  define _PATH_SENDMAILCF	"/usr/lib/sendmail.cf"
# endif
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* getusershell(3) causes core dumps */
# endif
# define syslog		hard_syslog
# ifdef __STDC__
extern int	syslog(int, char *, ...);
# endif
# endif


/*
**  IBM AIX 3.x -- actually tested for 3.2.3
*/

# ifdef _AIX3
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define FORK		fork	/* no vfork primitive available */
# undef  SETPROCTITLE		/* setproctitle confuses AIX */
# define SFS_TYPE	SFS_STATFS	/* use <sys/statfs.h> statfs() impl */
# endif


/*
**  Silicon Graphics IRIX
**
**	Compiles on 4.0.1.
*/

# ifdef IRIX
# include <sys/sysmacros.h>
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define FORK		fork	/* no vfork primitive available */
# define WAITUNION	1	/* use "union wait" as wait argument type */
# define setpgid	BSDsetpgrp
# define GIDSET_T	gid_t
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */
# endif


/*
**  SunOS and Solaris
**
**	Tested on SunOS 4.1.x (a.k.a. Solaris 1.1.x) and
**	Solaris 2.2 (a.k.a. SunOS 5.2).
*/

#if defined(sun) && !defined(BSD)

# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASGETUSERSHELL 1	/* DOES have getusershell(3) call in libc */
# define LA_TYPE	LA_INT

# ifdef SOLARIS_2_3
#  define SOLARIS
# endif

# ifdef SOLARIS
			/* Solaris 2.x (a.k.a. SunOS 5.x) */
#  ifndef __svr4__
#   define __svr4__		/* use all System V Releae 4 defines below */
#  endif
#  include <sys/time.h>
#  define gethostbyname	solaris_gethostbyname	/* get working version */
#  define gethostbyaddr	solaris_gethostbyaddr	/* get working version */
#  define GIDSET_T	gid_t
#  ifndef _PATH_UNIX
#   define _PATH_UNIX	"/kernel/unix"
#  endif
#  ifndef _PATH_SENDMAILCF
#   define _PATH_SENDMAILCF	"/etc/mail/sendmail.cf"
#  endif
#  ifndef _PATH_SENDMAILPID
#   define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
#  endif

# else
			/* SunOS 4.0.3 or 4.1.x */
#  define HASSETREUID	1	/* has setreuid(2) call */
#  define HASFLOCK	1	/* has flock(2) call */
#  define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
#  include <vfork.h>

#  ifdef SUNOS403
			/* special tweaking for SunOS 4.0.3 */
#   include <malloc.h>
#   define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
#   define WAITUNION	1	/* use "union wait" as wait argument type */
#   undef WIFEXITED
#   undef WEXITSTATUS
#   undef HASUNAME
#   define setpgid	setpgrp
typedef int		pid_t;
extern char		*getenv();

#  endif
# endif
#endif

/*
**  DG/UX
**
**	Tested on 5.4.2
*/

#ifdef	DGUX
# define SYSTEM5	1
# define LA_TYPE	LA_SUBR
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASSETSID	1	/* has Posix setsid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# undef SETPROCTITLE
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */

/* these include files must be included early on DG/UX */
# include <netinet/in.h>
# include <arpa/inet.h>

# define inet_addr	dgux_inet_addr
extern long	dgux_inet_addr();
#endif


/*
**  Digital Ultrix 4.2A or 4.3
**
**	Apparently, fcntl locking is broken on 4.2A, in that locks are
**	not dropped when the process exits.  This causes major problems,
**	so flock is the only alternative.
*/

#ifdef ultrix
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFLOCK	1	/* has flock(2) call */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# ifdef vax
#  define LA_TYPE	LA_FLOAT
# else
#  define LA_TYPE	LA_INT
#  define LA_AVENRUN	"avenrun"
# endif
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
#endif


/*
**  OSF/1 (tested on Alpha)
*/

#ifdef __osf__
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFLOCK	1	/* has flock(2) call */
# define LA_TYPE	LA_INT
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/var/run/sendmail.pid"
# endif
#endif


/*
**  NeXTstep
*/

#ifdef NeXT
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFLOCK	1	/* has flock(2) call */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define WAITUNION	1	/* use "union wait" as wait argument type */
# define sleep		sleepX
# define setpgid	setpgrp
# ifndef LA_TYPE
#  define LA_TYPE	LA_MACH
# endif
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# ifndef _POSIX_SOURCE
typedef int		pid_t;
#  undef WEXITSTATUS
#  undef WIFEXITED
# endif
# ifndef _PATH_SENDMAILCF
#  define _PATH_SENDMAILCF	"/etc/sendmail/sendmail.cf"
# endif
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail/sendmail.pid"
# endif
#endif


/*
**  4.4 BSD
**
**	See also BSD defines.
*/

#ifdef BSD4_4
# define HASUNSETENV	1	/* has unsetenv(3) call */
# include <sys/cdefs.h>
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# ifndef LA_TYPE
#  define LA_TYPE	LA_SUBR
# endif
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
#endif


/*
**  386BSD / FreeBSD 1.0E / NetBSD (all architectures, all versions)
**
**  4.3BSD clone, closer to 4.4BSD
**
**	See also BSD defines.
*/

#if defined(__386BSD__) || defined(__FreeBSD__) || defined(__NetBSD__)
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASSETSID	1	/* has the setsid(2) POSIX syscall */
# include <sys/cdefs.h>
# define ERRLIST_PREDEFINED	/* don't declare sys_errlist */
# ifndef LA_TYPE
#  define LA_TYPE	LA_SUBR
# endif
# define SFS_TYPE	SFS_MOUNT	/* use <sys/mount.h> statfs() impl */
#endif


/*
**  Mach386
**
**	For mt Xinu's Mach386 system.
*/

#if defined(MACH) && defined(i386)
# define MACH386	1
# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASFLOCK	1	/* has flock(2) call */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define NEEDSTRTOL	1	/* need the strtol() function */
# define setpgid	setpgrp
# ifndef LA_TYPE
#  define LA_TYPE	LA_FLOAT
# endif
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# undef HASSETVBUF		/* don't actually have setvbuf(3) */
# undef WEXITSTATUS
# undef WIFEXITED
# ifndef _PATH_SENDMAILCF
#  define _PATH_SENDMAILCF	"/usr/lib/sendmail.cf"
# endif
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif
#endif


/*
**  4.3 BSD -- this is for very old systems
**
**	You'll also have to install a new resolver library.
**	I don't guarantee that support for this environment is complete.
*/

#ifdef oldBSD43
# define NEEDVPRINTF	1	/* need a replacement for vprintf(3) */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define ARBPTR_T	char *
# define setpgid	setpgrp
# ifndef LA_TYPE
#  define LA_TYPE	LA_FLOAT
# endif
# ifndef _PATH_SENDMAILCF
#  define _PATH_SENDMAILCF	"/usr/lib/sendmail.cf"
# endif
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# undef WEXITSTATUS
# undef WIFEXITED
typedef short		pid_t;
extern int		errno;
#endif


/*
**  SCO Unix
**
**	This includes two parts -- the first is for SCO Open Server 3.2v4
**	(contributed by Philippe Brand <phb@colombo.telesys-innov.fr>).
**	The second is, I believe, for an older version.
*/

#ifdef _SCO_unix_4_2
# define _SCO_unix_
# define HASSETREUID	1	/* has setreuid(2) call */
# define _PATH_UNIX	"/unix"
# ifndef _PATH_SENDMAILCF
#  define _PATH_SENDMAILCF	"/usr/lib/sendmail.cf"
# endif
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif
#endif

#ifdef _SCO_unix_
# define SYSTEM5	1	/* include all the System V defines */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define FORK		fork
# define MAXPATHLEN	PATHSIZE
# define LA_TYPE	LA_SHORT
# define SFS_TYPE	SFS_STATFS	/* use <sys/statfs.h> statfs() impl */
# undef NETUNIX			/* no unix domain socket support */
#endif


/*
**  ConvexOS 11.0 and later
*/

#ifdef _CONVEX_SOURCE
# define BSD		1	/* include all the BSD defines */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASSETSID	1	/* has POSIX setsid(2) call */
# define NEEDGETOPT	1	/* need replacement for getopt(3) */
# define LA_TYPE	LA_FLOAT
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
#endif


/*
**  RISC/os 4.52
**
**	Gives a ton of warning messages, but otherwise compiles.
*/

#ifdef RISCOS

# define HASUNSETENV	1	/* has unsetenv(3) call */
# define HASFLOCK	1	/* has flock(2) call */
# define WAITUNION	1	/* use "union wait" as wait argument type */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define LA_TYPE	LA_INT
# define LA_AVENRUN	"avenrun"
# define _PATH_UNIX	"/unix"
# undef WIFEXITED

# define setpgid	setpgrp

extern int		errno;
typedef int		pid_t;
#define			SIGFUNC_DEFINED
typedef int		(*sigfunc_t)();
extern char		*getenv();
extern void		*malloc();

#endif


/*
**  Linux 0.99pl10 and above...
**	From Karl London <karl@borg.demon.co.uk>.
*/

#ifdef __linux__
# define BSD		1	/* pretend to be BSD based today */
# undef  NEEDVPRINTF	1	/* need a replacement for vprintf(3) */
# define NEEDGETOPT	1	/* need a replacement for getopt(3) */
# define HASUNSETENV	1	/* has unsetenv(3) call */
# ifndef LA_TYPE
#  define LA_TYPE	LA_FLOAT
# endif
# include <sys/sysmacros.h>
# define GIDSET_T	gid_t
#endif


/*
**  DELL SVR4 Issue 2.2, and others
**	From Kimmo Suominen <kim@grendel.lut.fi>
**
**	It's on #ifdef DELL_SVR4 because Solaris also gets __svr4__
**	defined, and the definitions conflict.
**
**	Peter Wemm <peter@perth.DIALix.oz.au> claims that the setreuid
**	trick works on DELL 2.2 (SVR4.0/386 version 4.0) and ESIX 4.0.3A
**	(SVR4.0/386 version 3.0).
*/

#ifdef DELL_SVR4
				/* no changes necessary */
				/* see general __svr4__ defines below */
#endif


/*
**  Apple A/UX 3.0
*/

#ifdef _AUX_SOURCE
# include <sys/sysmacros.h>
# define BSD			/* has BSD routines */
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASSETVBUF	1	/* we have setvbuf(3) in libc */
# define SIGFUNC_DEFINED	/* sigfunc_t already defined */
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
# define FORK		fork
# ifndef _PATH_SENDMAILCF
#  define _PATH_SENDMAILCF	"/usr/lib/sendmail.cf"
# endif
# ifndef LA_TYPE
#  define LA_TYPE	LA_ZERO
# endif
# define SFS_TYPE	SFS_VFS	/* use <sys/vfs.h> statfs() implementation */
# undef WIFEXITED
# undef WEXITSTATUS
#endif


/*
**  Encore UMAX V
**
**	Not extensively tested.
*/

#ifdef UMAXV
# include <limits.h>
# define HASUNAME	1	/* use System V uname(2) system call */
# define HASSETVBUF	1	/* we have setvbuf(3) in libc */
# define HASINITGROUPS	1	/* has initgroups(3) call */
# define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define SYS5SETPGRP	1	/* use System V setpgrp(2) syscall */
# define FORK		fork	/* no vfork(2) primitive available */
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */
# define MAXPATHLEN	PATH_MAX
extern struct passwd	*getpwent(), *getpwnam(), *getpwuid();
extern struct group	*getgrent(), *getgrnam(), *getgrgid();
# undef WIFEXITED
# undef WEXITSTATUS
#endif


/*
**  Stardent Titan 3000 running TitanOS 4.2.
**
**	Must be compiled in "cc -43" mode.
**
**	From Kate Hedstrom <kate@ahab.rutgers.edu>.
**
**	Note the tweaking below after the BSD defines are set.
*/

#ifdef titan
# define setpgid	setpgrp
typedef int		pid_t;
# undef WIFEXITED
# undef WEXITSTATUS
#endif


/*
**  Sequent DYNIX 3.2.0
**
**	From Jim Davis <jdavis@cs.arizona.edu>.
*/

#ifdef sequent
# define BSD		1
# define HASUNSETENV	1
# define BSD4_3		1	/* to get signal() in conf.c */
# define WAITUNION	1
# define LA_TYPE	LA_FLOAT
# ifdef	_POSIX_VERSION
#  undef _POSIX_VERSION		/* set in <unistd.h> */
# endif
# undef HASSETVBUF		/* don't actually have setvbuf(3) */
# define setpgid	setpgrp

/* Have to redefine WIFEXITED to take an int, to work with waitfor() */
# undef	WIFEXITED
# define WIFEXITED(s)	(((union wait*)&(s))->w_stopval != WSTOPPED && \
			 ((union wait*)&(s))->w_termsig == 0)
# define WEXITSTATUS(s)	(((union wait*)&(s))->w_retcode)
typedef int		pid_t;
# define isgraph(c)	(isprint(c) && (c != ' '))

# ifndef _PATH_UNIX
#  define _PATH_UNIX	"/dynix"
# endif
# ifndef _PATH_SENDMAILCF
#  define _PATH_SENDMAILCF	"/usr/lib/sendmail.cf"
# endif

#endif


/*
**  Cray Unicos
**
**	Ported by David L. Kensiski, Sterling Sofware <kensiski@nas.nasa.gov>
*/

#ifdef UNICOS
# define SYSTEM5	1	/* include all the System V defines */
# define SYS5SIGNALS	1	/* SysV signal semantics -- reset on each sig */
# define MAXPATHLEN	PATHSIZE
# define LA_TYPE	LA_ZERO
# define SFS_TYPE	SFS_4ARGS	/* four argument statfs() call */
#endif


/*
**  Apollo DomainOS
**
**  From Todd Martin <tmartint@tus.ssi1.com> & Don Lewis <gdonl@gv.ssi1.com>
** 
**  15 Jan 1994
**
*/

#ifdef apollo
# define HASSTATFS	1	/* has the statfs(2) syscall */
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(2) call */
# undef  SETPROCTITLE
# define LA_TYPE	LA_SUBR		/* use getloadavg.c */
# define SFS_TYPE	SFS_MOUNT
# ifndef _PATH_SENDMAILCF
#  define _PATH_SENDMAILCF	"/usr/lib/sendmail.cf"
# endif
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif
# undef  S_IFSOCK		/* S_IFSOCK and S_IFIFO are the same */
# undef  S_IFIFO
# define S_IFIFO	0010000
# ifndef IDENTPROTO
#  define IDENTPROTO	0	/* TCP/IP implementation is broken */
# endif
#endif 




/**********************************************************************
**  End of Per-Operating System defines
**********************************************************************/

/**********************************************************************
**  More general defines
**********************************************************************/

/* general BSD defines */
#ifdef BSD
# define HASGETDTABLESIZE 1	/* has getdtablesize(2) call */
# define HASSETREUID	1	/* has setreuid(2) call */
# define HASINITGROUPS	1	/* has initgroups(2) call */
# define HASFLOCK	1	/* has flock(2) call */
#endif

/* general System V Release 4 defines */
#ifdef __svr4__
# define SYSTEM5	1
# define HASSETREUID	1	/* has seteuid(2) call & working saved uids */
# ifndef HASGETUSERSHELL
#  define HASGETUSERSHELL 0	/* does not have getusershell(3) call */
# endif
# define setreuid(r, e)	seteuid(e)

# ifndef _PATH_UNIX
#  define _PATH_UNIX		"/unix"
# endif
# ifndef _PATH_SENDMAILCF
#  define _PATH_SENDMAILCF	"/usr/ucblib/sendmail.cf"
# endif
# ifndef _PATH_SENDMAILPID
#  define _PATH_SENDMAILPID	"/usr/ucblib/sendmail.pid"
# endif
# ifndef SYSLOG_BUFSIZE
#  define SYSLOG_BUFSIZE	128
# endif
#endif

/* general System V defines */
# ifdef SYSTEM5
# include <sys/sysmacros.h>
# define HASUNAME	1	/* use System V uname(2) system call */
# define SYS5SETPGRP	1	/* use System V setpgrp(2) syscall */
# define HASSETVBUF	1	/* we have setvbuf(3) in libc */
# ifndef LA_TYPE
#  define LA_TYPE	LA_INT		/* assume integer load average */
# endif
# ifndef SFS_TYPE
#  define SFS_TYPE	SFS_USTAT	/* use System V ustat(2) syscall */
# endif
# define bcopy(s, d, l)		(memmove((d), (s), (l)))
# define bzero(d, l)		(memset((d), '\0', (l)))
# define bcmp(s, d, l)		(memcmp((s), (d), (l)))
# endif

/* general POSIX defines */
#ifdef _POSIX_VERSION
# define HASSETSID	1	/* has Posix setsid(2) call */
# define HASWAITPID	1	/* has Posix waitpid(2) call */
#endif

/*
**  If no type for argument two of getgroups call is defined, assume
**  it's an integer -- unfortunately, there seem to be several choices
**  here.
*/

#ifndef GIDSET_T
# define GIDSET_T	int
#endif

/*
**  Tweaking for systems that (for example) claim to be BSD but
**  don't have all the standard BSD routines (boo hiss).
*/

#ifdef titan
# undef HASINITGROUPS		/* doesn't have initgroups(3) call */
#endif

/*
**  Due to a "feature" in some operating systems such as Ultrix 4.3 and
**  HPUX 8.0, if you receive a "No route to host" message (ICMP message
**  ICMP_UNREACH_HOST) on _any_ connection, all connections to that host
**  are closed.  Some firewalls return this error if you try to connect
**  to the IDENT port (113), so you can't receive email from these hosts
**  on these systems.  The firewall really should use a more specific
**  message such as ICMP_UNREACH_PROTOCOL or _PORT or _NET_PROHIB.  If
**  not explicitly set to zero above, default it on.
*/

#ifndef IDENTPROTO
# define IDENTPROTO	1	/* use IDENT proto (RFC 1413) */
#endif

#ifndef HASGETUSERSHELL
# define HASGETUSERSHELL 1	/* libc has getusershell(3) call */
#endif


/**********************************************************************
**  Remaining definitions should never have to be changed.  They are
**  primarily to provide back compatibility for older systems -- for
**  example, it includes some POSIX compatibility definitions
**********************************************************************/

/* System 5 compatibility */
#ifndef S_ISREG
# define S_ISREG(foo)	((foo & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
# define S_ISLNK(foo)	((foo & S_IFMT) == S_IFLNK)
#endif
#ifndef S_IWGRP
#define S_IWGRP		020
#endif
#ifndef S_IWOTH
#define S_IWOTH		002
#endif

/*
**  Older systems don't have this error code -- it should be in
**  /usr/include/sysexits.h.
*/

# ifndef EX_CONFIG
# define EX_CONFIG	78	/* configuration error */
# endif

/* pseudo-code used in server SMTP */
# define EX_QUIT	22	/* drop out of server immediately */


/*
**  These are used in a few cases where we need some special
**  error codes, but where the system doesn't provide something
**  reasonable.  They are printed in errstring.
*/

#ifndef E_PSEUDOBASE
# define E_PSEUDOBASE	256
#endif

#define EOPENTIMEOUT	(E_PSEUDOBASE + 0)	/* timeout on open */
#define E_DNSBASE	(E_PSEUDOBASE + 20)	/* base for DNS h_errno */

/* type of arbitrary pointer */
#ifndef ARBPTR_T
# define ARBPTR_T	void *
#endif

#ifndef __P
# include "cdefs.h"
#endif

/*
**  Do some required dependencies
*/

#if defined(NETINET) || defined(NETISO)
# define SMTP		1	/* enable user and server SMTP */
# define QUEUE		1	/* enable queueing */
# define DAEMON		1	/* include the daemon (requires IPC & SMTP) */
#endif


/*
**  Arrange to use either varargs or stdargs
*/

# ifdef __STDC__

# include <stdarg.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap, f)
# define VA_END		va_end(ap)

# else

# include <varargs.h>

# define VA_LOCAL_DECL	va_list ap;
# define VA_START(f)	va_start(ap)
# define VA_END		va_end(ap)

# endif

#ifdef HASUNAME
# include <sys/utsname.h>
# ifdef newstr
#  undef newstr
# endif
#else /* ! HASUNAME */
# define NODE_LENGTH 32
struct utsname
{
	char nodename[NODE_LENGTH+1];
};
#endif /* HASUNAME */

#if !defined(MAXHOSTNAMELEN) && !defined(_SCO_unix_)
# define MAXHOSTNAMELEN	256
#endif

#if !defined(SIGCHLD) && defined(SIGCLD)
# define SIGCHLD	SIGCLD
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO	0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO	1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO	2
#endif

#ifndef LOCK_SH
# define LOCK_SH	0x01	/* shared lock */
# define LOCK_EX	0x02	/* exclusive lock */
# define LOCK_NB	0x04	/* non-blocking lock */
# define LOCK_UN	0x08	/* unlock */
#endif

#ifndef SIG_ERR
# define SIG_ERR	((void (*)()) -1)
#endif

#ifndef WEXITSTATUS
# define WEXITSTATUS(st)	(((st) >> 8) & 0377)
#endif
#ifndef WIFEXITED
# define WIFEXITED(st)		(((st) & 0377) == 0)
#endif

#ifndef SIGFUNC_DEFINED
typedef void		(*sigfunc_t) __P((int));
#endif

/* size of syslog buffer */
#ifndef SYSLOG_BUFSIZE
# define SYSLOG_BUFSIZE	1024
#endif

/*
**  Size of tobuf (deliver.c)
**	Tweak this to match your syslog implementation.  It will have to
**	allow for the extra information printed.
*/

#ifndef TOBUFSIZE
# if (SYSLOG_BUFSIZE) > 512
#  define TOBUFSIZE	(SYSLOG_BUFSIZE - 256)
# else
#  define TOBUFSIZE	256
# endif
#endif

/*
**  Size of prescan buffer.
**	Despite comments in the _sendmail_ book, this probably should
**	not be changed; there are some hard-to-define dependencies.
*/

# define PSBUFSIZE	(MAXNAME + MAXATOM)	/* size of prescan buffer */
/* fork routine -- set above using #ifdef _osname_ or in Makefile */
# ifndef FORK
# define FORK		vfork		/* function to call to fork mailer */
# endif
