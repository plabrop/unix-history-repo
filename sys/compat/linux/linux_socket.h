/*-
 * Copyright (c) 2000 Assar Westerlund
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _LINUX_SOCKET_H_
#define _LINUX_SOCKET_H_

/* msg flags in recvfrom/recvmsg */

#define LINUX_MSG_OOB		0x01
#define LINUX_MSG_PEEK		0x02
#define LINUX_MSG_DONTROUTE	0x04
#define LINUX_MSG_CTRUNC	0x08
#define LINUX_MSG_PROXY		0x10
#define LINUX_MSG_TRUNC		0x20
#define LINUX_MSG_DONTWAIT	0x40
#define LINUX_MSG_EOR		0x80
#define LINUX_MSG_WAITALL	0x100
#define LINUX_MSG_FIN		0x200
#define LINUX_MSG_SYN		0x400
#define LINUX_MSG_CONFIRM	0x800
#define LINUX_MSG_RST		0x1000
#define LINUX_MSG_ERRQUEUE	0x2000
#define LINUX_MSG_NOSIGNAL	0x4000
#define LINUX_MSG_CMSG_CLOEXEC	0x40000000

/* Socket-level control message types */

#define LINUX_SCM_RIGHTS	0x01
#define LINUX_SCM_CREDENTIALS   0x02

struct l_msghdr {
	l_uintptr_t	msg_name;
	l_int		msg_namelen;
	l_uintptr_t	msg_iov;
	l_size_t	msg_iovlen;
	l_uintptr_t	msg_control;
	l_size_t	msg_controllen;
	l_uint		msg_flags;
};

struct l_cmsghdr {
	l_size_t	cmsg_len;
	l_int		cmsg_level;
	l_int		cmsg_type;
};

/* Ancilliary data object information macros */

#define LINUX_CMSG_ALIGN(len)	roundup2(len, sizeof(l_ulong))
#define LINUX_CMSG_DATA(cmsg)	((void *)((char *)(cmsg) + \
				    LINUX_CMSG_ALIGN(sizeof(struct l_cmsghdr))))
#define LINUX_CMSG_SPACE(len)	(LINUX_CMSG_ALIGN(sizeof(struct l_cmsghdr)) + \
				    LINUX_CMSG_ALIGN(len))
#define LINUX_CMSG_LEN(len)	(LINUX_CMSG_ALIGN(sizeof(struct l_cmsghdr)) + \
				    (len))
#define LINUX_CMSG_FIRSTHDR(msg) \
				((msg)->msg_controllen >= \
				    sizeof(struct l_cmsghdr) ? \
				    (struct l_cmsghdr *) \
				        PTRIN((msg)->msg_control) : \
				    (struct l_cmsghdr *)(NULL))
#define LINUX_CMSG_NXTHDR(msg, cmsg) \
				((((char *)(cmsg) + \
				    LINUX_CMSG_ALIGN((cmsg)->cmsg_len) + \
				    sizeof(*(cmsg))) > \
				    (((char *)PTRIN((msg)->msg_control)) + \
				    (msg)->msg_controllen)) ? \
				    (struct l_cmsghdr *) NULL : \
				    (struct l_cmsghdr *)((char *)(cmsg) + \
				    LINUX_CMSG_ALIGN((cmsg)->cmsg_len)))

#define CMSG_HDRSZ		CMSG_LEN(0)
#define L_CMSG_HDRSZ		LINUX_CMSG_LEN(0)

/* Supported address families */

#define	LINUX_AF_UNSPEC		0
#define	LINUX_AF_UNIX		1
#define	LINUX_AF_INET		2
#define	LINUX_AF_AX25		3
#define	LINUX_AF_IPX		4
#define	LINUX_AF_APPLETALK	5
#define	LINUX_AF_INET6		10

/* Supported socket types */

#define	LINUX_SOCK_STREAM	1
#define	LINUX_SOCK_DGRAM	2
#define	LINUX_SOCK_RAW		3
#define	LINUX_SOCK_RDM		4
#define	LINUX_SOCK_SEQPACKET	5

#define	LINUX_SOCK_MAX		LINUX_SOCK_SEQPACKET

#define	LINUX_SOCK_TYPE_MASK	0xf

/* Flags for socket, socketpair, accept4 */

#define	LINUX_SOCK_CLOEXEC	LINUX_O_CLOEXEC
#define	LINUX_SOCK_NONBLOCK	LINUX_O_NONBLOCK

struct l_ucred {
	uint32_t	pid;
	uint32_t	uid;
	uint32_t	gid;
};

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))

struct linux_sendto_args {
	int s;
	l_uintptr_t msg;
	int len;
	int flags;
	l_uintptr_t to;
	int tolen;
};

struct linux_socket_args {
	int domain;
	int type;
	int protocol;
};

struct linux_bind_args {
	int s;
	l_uintptr_t name;
	int namelen;
};

struct linux_connect_args {
	int s;
	l_uintptr_t name;
	int namelen;
};

struct linux_listen_args {
	int s;
	int backlog;
};

struct linux_accept_args {
	int s;
	l_uintptr_t addr;
	l_uintptr_t namelen;
};

struct linux_accept4_args {
	int s;
	l_uintptr_t addr;
	l_uintptr_t namelen;
	int flags;
};

struct linux_getsockname_args {
	int s;
	l_uintptr_t addr;
	l_uintptr_t namelen;
};

struct linux_getpeername_args {
	int s;
	l_uintptr_t addr;
	l_uintptr_t namelen;
};

struct linux_socketpair_args {
	int domain;
	int type;
	int protocol;
	l_uintptr_t rsv;
};

struct linux_recvfrom_args {
	int s;
	l_uintptr_t buf;
	int len;
	int flags;
	l_uintptr_t from;
	l_uintptr_t fromlen;
};

struct linux_sendmsg_args {
	int s;
	l_uintptr_t msg;
	int flags;
};

struct linux_recvmsg_args {
	int s;
	l_uintptr_t msg;
	int flags;
};

struct linux_shutdown_args {
	int s;
	int how;
};

struct linux_setsockopt_args {
	int s;
	int level;
	int optname;
	l_uintptr_t optval;
	int optlen;
};

struct linux_getsockopt_args {
	int s;
	int level;
	int optname;
	l_uintptr_t optval;
	l_uintptr_t optlen;
};

int linux_socket(struct thread *td, struct linux_socket_args *args);
int linux_bind(struct thread *td, struct linux_bind_args *args);
int linux_connect(struct thread *, struct linux_connect_args *);
int linux_listen(struct thread *td, struct linux_listen_args *args);
int linux_accept(struct thread *td, struct linux_accept_args *args);
int linux_accept4(struct thread *td, struct linux_accept4_args *args);
int linux_getsockname(struct thread *td, struct linux_getsockname_args *args);
int linux_getpeername(struct thread *td, struct linux_getpeername_args *args);
int linux_socketpair(struct thread *td, struct linux_socketpair_args *args);
int linux_sendto(struct thread *td, struct linux_sendto_args *args);
int linux_recvfrom(struct thread *td, struct linux_recvfrom_args *args);
int linux_sendmsg(struct thread *td, struct linux_sendmsg_args *args);
int linux_recvmsg(struct thread *td, struct linux_recvmsg_args *args);
int linux_shutdown(struct thread *td, struct linux_shutdown_args *args);
int linux_setsockopt(struct thread *td, struct linux_setsockopt_args *args);
int linux_getsockopt(struct thread *td, struct linux_getsockopt_args *args);

#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

/* Operations for socketcall */

#define	LINUX_SOCKET 		1
#define	LINUX_BIND		2
#define	LINUX_CONNECT 		3
#define	LINUX_LISTEN 		4
#define	LINUX_ACCEPT 		5
#define	LINUX_GETSOCKNAME	6
#define	LINUX_GETPEERNAME	7
#define	LINUX_SOCKETPAIR	8
#define	LINUX_SEND		9
#define	LINUX_RECV		10
#define	LINUX_SENDTO 		11
#define	LINUX_RECVFROM 		12
#define	LINUX_SHUTDOWN 		13
#define	LINUX_SETSOCKOPT	14
#define	LINUX_GETSOCKOPT	15
#define	LINUX_SENDMSG		16
#define	LINUX_RECVMSG		17
#define	LINUX_ACCEPT4		18

/* Socket options */
#define	LINUX_IP_TOS		1
#define	LINUX_IP_TTL		2
#define	LINUX_IP_HDRINCL	3
#define	LINUX_IP_OPTIONS	4

#define	LINUX_IP_MULTICAST_IF		32
#define	LINUX_IP_MULTICAST_TTL		33
#define	LINUX_IP_MULTICAST_LOOP		34
#define	LINUX_IP_ADD_MEMBERSHIP		35
#define	LINUX_IP_DROP_MEMBERSHIP	36

#define	LINUX_TCP_NODELAY	1
#define	LINUX_TCP_MAXSEG	2
#define	LINUX_TCP_KEEPIDLE	4
#define	LINUX_TCP_KEEPINTVL	5
#define	LINUX_TCP_KEEPCNT	6
#define	LINUX_TCP_MD5SIG	14

#endif /* _LINUX_SOCKET_H_ */
