/*
 * Copyright (c) 1985 The Regents of the University of California.
 * All rights reserved.
 *
 * This file includes significant work done at Cornell University by
 * Bill Nesheim.  That work included by permission.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)startup.c	5.12 (Berkeley) %G%";
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <sys/ioctl.h>
#include <sys/kinfo.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <nlist.h>
#include <stdlib.h>

struct	interface *ifnet;
int	lookforinterfaces = 1;
int	performnlist = 1;
int	gateway = 0;
int	externalinterfaces = 0;		/* # of remote and local interfaces */
char ether_broadcast_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


void
quit(s)
	char *s;
{
	extern int errno;
	int sverrno = errno;

	(void) fprintf(stderr, "route: ");
	if (s)
		(void) fprintf(stderr, "%s: ", s);
	(void) fprintf(stderr, "%s\n", strerror(sverrno));
	exit(1);
	/* NOTREACHED */
}

struct rt_addrinfo info;
/* Sleazy use of local variables throughout file, warning!!!! */
#define netmask	info.rti_info[RTAX_NETMASK]
#define ifaaddr	info.rti_info[RTAX_IFA]
#define brdaddr	info.rti_info[RTAX_BRD]

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

void
rt_xaddrs(cp, cplim, rtinfo)
	register caddr_t cp, cplim;
	register struct rt_addrinfo *rtinfo;
{
	register struct sockaddr *sa;
	register int i;

	bzero(rtinfo->rti_info, sizeof(rtinfo->rti_info));
	for (i = 0; (i < RTAX_MAX) && (cp < cplim); i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		rtinfo->rti_info[i] = sa = (struct sockaddr *)cp;
		ADVANCE(cp, sa);
	}
}

/*
 * Find the network interfaces which have configured themselves.
 * If the interface is present but not yet up (for example an
 * ARPANET IMP), set the lookforinterfaces flag so we'll
 * come back later and look again.
 */
ifinit()
{
	struct interface ifs, *ifp;
	int needed, rlen, no_nsaddr = 0, flags = 0;
	char *buf, *cplim, *cp;
	register struct if_msghdr *ifm;
	register struct ifa_msghdr *ifam;
	struct sockaddr_dl *sdl;
	u_long i;

	if ((needed = getkerninfo(KINFO_RT_IFLIST, 0, 0, 0)) < 0)
		quit("route-getkerninfo-estimate");
	if ((buf = malloc(needed)) == NULL)
		quit("malloc");
	if ((rlen = getkerninfo(KINFO_RT_IFLIST, buf, &needed, 0)) < 0)
		quit("actual retrieval of interface table");
	lookforinterfaces = 0;
	cplim = buf + rlen;
	for (cp = buf; cp < cplim; cp += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)cp;
		if (ifm->ifm_type == RTM_IFINFO) {
			bzero(&ifs, sizeof(ifs));
			ifs.int_flags = flags = ifm->ifm_flags | IFF_INTERFACE;
			if ((flags & IFF_UP) == 0 || no_nsaddr)
				lookforinterfaces = 1;
			sdl = (struct sockaddr_dl *) (ifm + 1);
			sdl->sdl_data[sdl->sdl_nlen] = 0;
			no_nsaddr = 1;
			continue;
		}
		if (ifm->ifm_type != RTM_NEWADDR)
			quit("ifinit: out of sync");
		if ((flags & IFF_UP) == 0)
			continue;
		ifam = (struct ifa_msghdr *)ifm;
		info.rti_addrs = ifam->ifam_addrs;
		rt_xaddrs((char *)(ifam + 1), cp + ifam->ifam_msglen, &info);
		if (ifaaddr == 0) {
			syslog(LOG_ERR, "%s: (get addr)", sdl->sdl_data);
			continue;
		}
		ifs.int_addr = *ifaaddr;
		if (ifs.int_addr.sa_family != AF_NS)
			continue;
		no_nsaddr = 0;
		if (ifs.int_flags & IFF_POINTOPOINT) {
			if (brdaddr == 0) {
				syslog(LOG_ERR, "%s: (get dstaddr)",
					sdl->sdl_data);
				continue;
			}
			if (brdaddr->sa_family == AF_UNSPEC) {
				lookforinterfaces = 1;
				continue;
			}
			ifs.int_dstaddr = *brdaddr;
		}
		if (ifs.int_flags & IFF_BROADCAST) {
			if (brdaddr == 0) {
				syslog(LOG_ERR, "%s: (get broadaddr)",
					sdl->sdl_data);
				continue;
			}
			ifs.int_dstaddr = *brdaddr;
		}
		/* 
		 * already known to us? 
		 * what makes a POINTOPOINT if unique is its dst addr,
		 * NOT its source address 
		 */
		if ( ((ifs.int_flags & IFF_POINTOPOINT) &&
			if_ifwithdstaddr(&ifs.int_dstaddr)) ||
			( ((ifs.int_flags & IFF_POINTOPOINT) == 0) &&
			if_ifwithaddr(&ifs.int_addr)))
			continue;
		/* no one cares about software loopback interfaces */
		if (ifs.int_flags & IFF_LOOPBACK)
			continue;
		ifp = (struct interface *)
			malloc(sdl->sdl_nlen + 1 + sizeof(ifs));
		if (ifp == 0) {
			syslog(LOG_ERR, "XNSrouted: out of memory\n");
			lookforinterfaces = 1;
			break;
		}
		*ifp = ifs;
		/*
		 * Count the # of directly connected networks
		 * and point to point links which aren't looped
		 * back to ourself.  This is used below to
		 * decide if we should be a routing ``supplier''.
		 */
		if ((ifs.int_flags & IFF_POINTOPOINT) == 0 ||
		    if_ifwithaddr(&ifs.int_dstaddr) == 0)
			externalinterfaces++;
		/*
		 * If we have a point-to-point link, we want to act
		 * as a supplier even if it's our only interface,
		 * as that's the only way our peer on the other end
		 * can tell that the link is up.
		 */
		if ((ifs.int_flags & IFF_POINTOPOINT) && supplier < 0)
			supplier = 1;
		ifp->int_name = (char *)(ifp + 1);
		strcpy(ifp->int_name, sdl->sdl_data);
		ifp->int_metric = ifam->ifam_metric;
		ifp->int_next = ifnet;
		ifnet = ifp;
		traceinit(ifp);
		addrouteforif(ifp);
	}
	if (externalinterfaces > 1 && supplier < 0)
		supplier = 1;
	free(buf);
}

addrouteforif(ifp)
	struct interface *ifp;
{
	struct sockaddr_ns net;
	struct sockaddr *dst;
	int state, metric;
	struct rt_entry *rt;

	if (ifp->int_flags & IFF_POINTOPOINT) {
		int (*match)();
		register struct interface *ifp2 = ifnet;
		register struct interface *ifprev = ifnet;
		
		dst = &ifp->int_dstaddr;

		/* Search for interfaces with the same net */
		ifp->int_sq.n = ifp->int_sq.p = &(ifp->int_sq);
		match = afswitch[dst->sa_family].af_netmatch;
		if (match)
		for (ifp2 = ifnet; ifp2; ifp2 =ifp2->int_next) {
			if (ifp->int_flags & IFF_POINTOPOINT == 0)
				continue;
			if ((*match)(&ifp2->int_dstaddr,&ifp->int_dstaddr)) {
				insque(&ifp2->int_sq,&ifp->int_sq);
				break;
			}
		}
	} else {
		dst = &ifp->int_broadaddr;
	}
	rt = rtlookup(dst);
	if (rt)
		rtdelete(rt);
	if (tracing)
		fprintf(stderr, "Adding route to interface %s\n", ifp->int_name);
	if (ifp->int_transitions++ > 0)
		syslog(LOG_ERR, "re-installing interface %s", ifp->int_name);
	rtadd(dst, &ifp->int_addr, ifp->int_metric,
		ifp->int_flags & (IFF_INTERFACE|IFF_PASSIVE|IFF_REMOTE));
}

