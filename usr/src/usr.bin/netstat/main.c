/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif not lint

#ifndef lint
static char sccsid[] = "@(#)main.c	5.5 (Berkeley) %G%";
#endif not lint

#include <sys/param.h>
#include <sys/vmmac.h>
#include <sys/socket.h>
#include <machine/pte.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <nlist.h>
#include <stdio.h>

struct nlist nl[] = {
#define	N_MBSTAT	0
	{ "_mbstat" },
#define	N_IPSTAT	1
	{ "_ipstat" },
#define	N_TCB		2
	{ "_tcb" },
#define	N_TCPSTAT	3
	{ "_tcpstat" },
#define	N_UDB		4
	{ "_udb" },
#define	N_UDPSTAT	5
	{ "_udpstat" },
#define	N_RAWCB		6
	{ "_rawcb" },
#define	N_SYSMAP	7
	{ "_Sysmap" },
#define	N_SYSSIZE	8
	{ "_Syssize" },
#define	N_IFNET		9
	{ "_ifnet" },
#define	N_HOSTS		10
	{ "_hosts" },
#define	N_RTHOST	11
	{ "_rthost" },
#define	N_RTNET		12
	{ "_rtnet" },
#define	N_ICMPSTAT	13
	{ "_icmpstat" },
#define	N_RTSTAT	14
	{ "_rtstat" },
#define	N_NFILE		15
	{ "_nfile" },
#define	N_FILE		16
	{ "_file" },
#define	N_UNIXSW	17
	{ "_unixsw" },
#define N_RTHASHSIZE	18
	{ "_rthashsize" },
#define N_IDP		19
	{ "_nspcb"},
#define N_IDPSTAT	20
	{ "_idpstat"},
#define N_SPPSTAT	21
	{ "_spp_istat"},
#define N_NSERR		22
	{ "_ns_errstat"},

    /* BBN Internet protocol implementation */
#define	N_TCP		23
	{ "_tcp" },
#define	N_UDP		24
	{ "_udp" },
#define N_RDP		25
	{ "_rdp" },
#define	N_RDPSTAT	26
	{ "_rdpstat" },

	"",
};

/* internet protocols */
extern	int protopr(), bbnprotopr();
extern	int tcp_stats(), udp_stats(), ip_stats(), icmp_stats();
extern	int tcpstats(), udpstats(), ipstats(), icmpstats(), rdpstats();
extern	int nsprotopr();
extern	int spp_stats(), idp_stats(), nserr_stats();

struct protox {
	u_char	pr_index;		/* index into nlist of cb head */
	u_char	pr_sindex;		/* index into nlist of stat block */
	u_char	pr_wanted;		/* 1 if wanted, 0 otherwise */
	int	(*pr_cblocks)();	/* control blocks printing routine */
	int	(*pr_stats)();		/* statistics printing routine */
	char	*pr_name;		/* well-known name */
};

struct  protox berkprotox[] = {
	{ N_TCB,	N_TCPSTAT,	1,	protopr,
	  tcp_stats,	"tcp" },
	{ N_UDB,	N_UDPSTAT,	1,	protopr,
	  udp_stats,	"udp" },
	{ -1,		N_IPSTAT,	1,	0,
	  ip_stats,	"ip" },
	{ -1,		N_ICMPSTAT,	1,	0,
	  icmp_stats,	"icmp" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};

struct protox bbnprotox[] = {
	{ N_TCP,	N_TCPSTAT,	1,	bbnprotopr,
	  tcpstats,	"tcp" },
	{ N_UDP,	N_UDPSTAT,	1,	bbnprotopr,
	  udpstats,	"udp" },
	{ N_RDP,	N_RDPSTAT,	1,	bbnprotopr,
	  rdpstats,	"rdp" },
	{ N_RAWCB,	0,		1,	bbnprotopr,
	  0,		"raw" },
	{ -1,		N_IPSTAT,	1,	0,
	  ipstats,	"ip" },
	{ -1,		N_ICMPSTAT,	1,	0,
	  icmpstats,	"icmp" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};

struct protox nsprotox[] = {
	{ N_IDP,	N_IDPSTAT,	1,	nsprotopr,
	  idp_stats,	"idp" },
	{ N_IDP,	N_SPPSTAT,	1,	nsprotopr,
	  spp_stats,	"spp" },
	{ -1,		N_NSERR,	1,	0,
	  nserr_stats,	"ns_err" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};

struct	pte *Sysmap;

char	*system = "/vmunix";
char	*kmemf = "/dev/kmem";
int	kmem;
int	kflag;
int	Aflag;
int	aflag;
int	hflag;
int	iflag;
int	mflag;
int	nflag;
int	rflag;
int	sflag;
int	tflag;
int	uflag;
int	fflag;
int	interval;
char	*interface;
int	unit;
char	usage[] = "[ -Aaihmnrstu ] [-f address_family] [-I interface] [ interval ] [ system ] [ core ]";

int	af = AF_UNSPEC;
main(argc, argv)
	int argc;
	char *argv[];
{
	int i;
	char *cp, *name;
	register struct protoent *p;
	register struct protox *tp;

	name = argv[0];
	argc--, argv++;
  	while (argc > 0 && **argv == '-') {
		for (cp = &argv[0][1]; *cp; cp++)
		switch(*cp) {

		case 'A':
			Aflag++;
			break;

		case 'a':
			aflag++;
			break;

		case 'h':
			hflag++;
			break;

		case 'i':
			iflag++;
			break;

		case 'm':
			mflag++;
			break;

		case 'n':
			nflag++;
			break;

		case 'r':
			rflag++;
			break;

		case 's':
			sflag++;
			break;

		case 't':
			tflag++;
			break;

		case 'u':
			uflag++;
			break;

		case 'f':
			argv++;
			argc--;
			if (strcmp(*argv,"ns")==0) {
				af = AF_NS;
			} else if (strcmp(*argv,"inet")==0) {
				af = AF_INET;
			}
			break;
			
		case 'I':
			iflag++;
			if (*(interface = cp + 1) == 0) {
				if ((interface = argv[1]) == 0)
					break;
				argv++;
				argc--;
			}
			for (cp = interface; isalpha(*cp); cp++)
				;
			unit = atoi(cp);
			*cp-- = 0;
			break;

		default:
use:
			printf("usage: %s %s\n", name, usage);
			exit(1);
		}
		argv++, argc--;
	}
	if (argc > 0 && isdigit(argv[0][0])) {
		interval = atoi(argv[0]);
		if (interval <= 0)
			goto use;
		argv++, argc--;
		iflag++;
	}
	if (argc > 0) {
		system = *argv;
		argv++, argc--;
	}
	nlist(system, nl);
	if (nl[0].n_type == 0) {
		fprintf(stderr, "%s: no namelist\n", system);
		exit(1);
	}
	if (argc > 0) {
		kmemf = *argv;
		kflag++;
	}
	kmem = open(kmemf, 0);
	if (kmem < 0) {
		fprintf(stderr, "cannot open ");
		perror(kmemf);
		exit(1);
	}
	if (kflag) {
		off_t off;

		off = nl[N_SYSMAP].n_value & 0x7fffffff;
		lseek(kmem, off, 0);
		nl[N_SYSSIZE].n_value *= 4;
		Sysmap = (struct pte *)malloc(nl[N_SYSSIZE].n_value);
		if (Sysmap == 0) {
			perror("Sysmap");
			exit(1);
		}
		read(kmem, Sysmap, nl[N_SYSSIZE].n_value);
	}
	if (mflag) {
		mbpr(nl[N_MBSTAT].n_value);
		exit(0);
	}
	if (uflag) {
		unixpr(nl[N_NFILE].n_value, nl[N_FILE].n_value,
		    nl[N_UNIXSW].n_value);
		exit(0);
	}
	/*
	 * Keep file descriptors open to avoid overhead
	 * of open/close on each call to get* routines.
	 */
	sethostent(1);
	setnetent(1);
	if (iflag) {
		intpr(interval, nl[N_IFNET].n_value);
		exit(0);
	}
	if (hflag) {
		hostpr(nl[N_HOSTS].n_value);
		exit(0);
	}
	if (rflag) {
		if (sflag)
			rt_stats(nl[N_RTSTAT].n_value);
		else
			routepr(nl[N_RTHOST].n_value, nl[N_RTNET].n_value,
				nl[N_RTHASHSIZE].n_value);
		exit(0);
	}
    if (af == AF_INET || af == AF_UNSPEC) {
	struct protox *head;

	head = (nl[N_TCB].n_type == 0) ? bbnprotox : berkprotox;
	setprotoent(1);
	setservent(1);

	for (tp = head; tp->pr_name; tp++) {
		if (tp->pr_wanted == 0)
			continue;

		if (sflag) {
			if (tp->pr_stats)
			    (*tp->pr_stats)(nl[tp->pr_sindex].n_value, tp->pr_name);
		} else if (tp->pr_cblocks)
			(*tp->pr_cblocks)(nl[tp->pr_index].n_value, tp->pr_name);
	}
	endprotoent();
    }
    if (af == AF_NS || af == AF_UNSPEC) {
	for (tp = nsprotox; tp->pr_name; tp++) {
		if (sflag && tp->pr_stats) {
			(*tp->pr_stats)(nl[tp->pr_sindex].n_value, tp->pr_name);
			continue;
		}
		if (tp->pr_cblocks)
			(*tp->pr_cblocks)(nl[tp->pr_index].n_value, tp->pr_name);
	}
    }
    exit(0);
}

/*
 * Seek into the kernel for a value.
 */
klseek(fd, base, off)
	int fd, base, off;
{

	if (kflag) {
		/* get kernel pte */
#ifdef vax
		base &= 0x7fffffff;
#endif
		base = ctob(Sysmap[btop(base)].pg_pfnum) + (base & PGOFSET);
	}
	lseek(fd, base, off);
}

char *
plural(n)
	int n;
{

	return (n != 1 ? "s" : "");
}
