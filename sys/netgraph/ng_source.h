/*
 * ng_source.h
 */

/*-
 * Copyright 2002 Sandvine Inc.
 * All rights reserved.
 *
 * Subject to the following obligations and disclaimer of warranty, use and
 * redistribution of this software, in source or object code forms, with or
 * without modifications are expressly permitted by Sandvine Inc.;
provided,
 * however, that:
 * 1. Any and all reproductions of the source or object code must include
the
 *    copyright notice above and the following disclaimer of warranties;
and
 * 2. No rights are granted, in any manner or form, to use Sandvine Inc.
 *    trademarks, including the mark "SANDVINE" on advertising,
endorsements,
 *    or otherwise except as such appears in the above copyright notice or
in
 *    the software.
 *
 * THIS SOFTWARE IS BEING PROVIDED BY SANDVINE "AS IS", AND TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, SANDVINE MAKES NO REPRESENTATIONS OR
WARRANTIES,
 * EXPRESS OR IMPLIED, REGARDING THIS SOFTWARE, INCLUDING WITHOUT
LIMITATION,
 * ANY AND ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT.  SANDVINE DOES NOT WARRANT, GUARANTEE, OR
 * MAKE ANY REPRESENTATIONS REGARDING THE USE OF, OR THE RESULTS OF THE
 * USE OF THIS SOFTWARE IN TERMS OF ITS CORRECTNESS, ACCURACY, RELIABILITY
 * OR OTHERWISE.  IN NO EVENT SHALL SANDVINE BE LIABLE FOR ANY DAMAGES
 * RESULTING FROM OR ARISING OUT OF ANY USE OF THIS SOFTWARE, INCLUDING
 * WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY,
 * PUNITIVE, OR CONSEQUENTIAL DAMAGES, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES, LOSS OF USE, DATA OR PROFITS, HOWEVER CAUSED AND UNDER ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF SANDVINE IS ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Dave Chapeskie <dchapeskie@sandvine.com>
 *
 * $FreeBSD$
 */

#ifndef _NETGRAPH_NG_SOURCE_H_
#define _NETGRAPH_NG_SOURCE_H_

/* Node type name and magic cookie */
#define NG_SOURCE_NODE_TYPE	"source"
#define NGM_SOURCE_COOKIE	1034346805

/* Hook names */
#define NG_SOURCE_HOOK_INPUT	"input"
#define NG_SOURCE_HOOK_OUTPUT	"output"

/* Statistics structure returned by NGM_SOURCE_GET_STATS */
struct ng_source_stats {
	u_int64_t	outOctets;
	u_int64_t	outFrames;
	u_int32_t	queueOctets;
	u_int32_t	queueFrames;
	struct timeval	startTime;
	struct timeval	endTime;
	struct timeval	elapsedTime;
};

extern const struct ng_parse_type ng_source_timeval_type;
/* Keep this in sync with the above structure definition */
#define NG_SOURCE_STATS_TYPE_INFO	{			\
	  { "outOctets",	&ng_parse_uint64_type	},	\
	  { "outFrames",	&ng_parse_uint64_type	},	\
	  { "queueOctets",	&ng_parse_uint32_type	},	\
	  { "queueFrames",	&ng_parse_uint32_type	},	\
	  { "startTime",	&ng_source_timeval_type },	\
	  { "endTime",		&ng_source_timeval_type },	\
	  { "elapsedTime",	&ng_source_timeval_type },	\
	  { NULL }						\
}

/* Netgraph commands */
enum {
	NGM_SOURCE_GET_STATS = 1,	/* get stats */
	NGM_SOURCE_CLR_STATS,		/* clear stats */
	NGM_SOURCE_GETCLR_STATS,	/* atomically get and clear stats */
	NGM_SOURCE_START,		/* start sending queued data */
	NGM_SOURCE_STOP,		/* stop sending queued data */
	NGM_SOURCE_CLR_DATA,		/* clear the queued data */
	NGM_SOURCE_START_NOW,		/* start on non-ether output */
};

#endif /* _NETGRAPH_NG_SOURCE_H_ */
