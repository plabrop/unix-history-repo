/*
 * Copyright (c) 1982, 1986, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution is only permitted until one year after the first shipment
 * of 4.4BSD by the Regents.  Otherwise, redistribution and use in source and
 * binary forms are permitted provided that: (1) source distributions retain
 * this entire copyright notice and comment, and (2) distributions including
 * binaries display the following acknowledgement:  This product includes
 * software developed by the University of California, Berkeley and its
 * contributors'' in the documentation or other materials provided with the
 * distribution and in all advertising materials mentioning features or use
 * of this software.  Neither the name of the University nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)in_pcb.h	7.6 (Berkeley) 6/28/90
 */

/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */
struct inpcb {
	struct	inpcb *inp_next,*inp_prev;
					/* pointers to other pcb's */
	struct	inpcb *inp_head;	/* pointer back to chain of inpcb's
					   for this protocol */
	struct	in_addr inp_faddr;	/* foreign host table entry */
	u_short	inp_fport;		/* foreign port */
	struct	in_addr inp_laddr;	/* local host table entry */
	u_short	inp_lport;		/* local port */
	struct	socket *inp_socket;	/* back pointer to socket */
	caddr_t	inp_ppcb;		/* pointer to per-protocol pcb */
	struct	route inp_route;	/* placeholder for routing entry */
	int	inp_flags;		/* generic IP/datagram flags */
	struct	ip inp_ip;		/* header prototype; should have more */
	struct	mbuf *inp_options;	/* IP options */
};

/* flags in inp_flags: */
#define	INP_RECVOPTS		0x01	/* receive incoming IP options */
#define	INP_RECVRETOPTS		0x02	/* receive IP options for reply */
#define	INP_RECVDSTADDR		0x04	/* receive IP dst address */
#define	INP_CONTROLOPTS		(INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR)

#ifdef sotorawcb
/*
 * Common structure pcb for raw internet protocol access.
 * Here are internet specific extensions to the raw control block,
 * and space is allocated to the necessary sockaddrs.
 */
struct raw_inpcb {
	struct	rawcb rinp_rcb;	/* common control block prefix */
	struct	mbuf *rinp_options;	/* IP options */
	int	rinp_flags;		/* flags, e.g. raw sockopts */
#define	RINPF_HDRINCL	0x1		/* user supplies entire IP header */
	struct	sockaddr_in rinp_faddr;	/* foreign address */
	struct	sockaddr_in rinp_laddr;	/* local address */
	struct	route rinp_route;	/* placeholder for routing entry */
};
#endif

#define	INPLOOKUP_WILDCARD	1
#define	INPLOOKUP_SETLOCAL	2

#define	sotoinpcb(so)	((struct inpcb *)(so)->so_pcb)
#define	sotorawinpcb(so)	((struct raw_inpcb *)(so)->so_pcb)

#ifdef KERNEL
struct	inpcb *in_pcblookup();
#endif
