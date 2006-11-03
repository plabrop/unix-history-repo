/*-
 * Copyright (c) 2001-2006, Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $KAME: sctp_input.c,v 1.27 2005/03/06 16:04:17 itojun Exp $	 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ipsec.h"
#include "opt_compat.h"
#include "opt_inet6.h"
#include "opt_inet.h"
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <sys/limits.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif				/* INET6 */

#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>

#include <netinet/sctp_os.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_input.h>
#include <netinet/sctp_auth.h>
#include <netinet/sctp_indata.h>
#include <netinet/sctp_asconf.h>

#include <netinet/ip_options.h>


#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#endif				/* IPSEC */


#ifdef SCTP_DEBUG
extern uint32_t sctp_debug_on;

#endif


static void
sctp_stop_all_cookie_timers(struct sctp_tcb *stcb)
{
	struct sctp_nets *net;

	SCTP_TCB_LOCK_ASSERT(stcb);
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		if ((callout_pending(&net->rxt_timer.timer)) &&
		    (net->rxt_timer.type == SCTP_TIMER_TYPE_COOKIE)) {
			sctp_timer_stop(SCTP_TIMER_TYPE_COOKIE,
			    stcb->sctp_ep,
			    stcb,
			    net);
		}
	}
}

/* INIT handler */
static void
sctp_handle_init(struct mbuf *m, int iphlen, int offset, struct sctphdr *sh,
    struct sctp_init_chunk *cp, struct sctp_inpcb *inp, struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	struct sctp_init *init;
	struct mbuf *op_err;
	uint32_t init_limit;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
		printf("sctp_handle_init: handling INIT tcb:%p\n", stcb);
	}
#endif
	op_err = NULL;
	init = &cp->init;
	/* First are we accepting? */
	if ((inp->sctp_socket->so_qlimit == 0) && (stcb == NULL)) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("sctp_handle_init: Abort, so_qlimit:%d\n", inp->sctp_socket->so_qlimit);
		}
#endif
		/*
		 * FIX ME ?? What about TCP model and we have a
		 * match/restart case?
		 */
		sctp_abort_association(inp, stcb, m, iphlen, sh, op_err);
		return;
	}
	if (ntohs(cp->ch.chunk_length) < sizeof(struct sctp_init_chunk)) {
		/* Invalid length */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(inp, stcb, m, iphlen, sh, op_err);
		return;
	}
	/* validate parameters */
	if (init->initiate_tag == 0) {
		/* protocol error... send abort */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(inp, stcb, m, iphlen, sh, op_err);
		return;
	}
	if (ntohl(init->a_rwnd) < SCTP_MIN_RWND) {
		/* invalid parameter... send abort */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(inp, stcb, m, iphlen, sh, op_err);
		return;
	}
	if (init->num_inbound_streams == 0) {
		/* protocol error... send abort */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(inp, stcb, m, iphlen, sh, op_err);
		return;
	}
	if (init->num_outbound_streams == 0) {
		/* protocol error... send abort */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(inp, stcb, m, iphlen, sh, op_err);
		return;
	}
	init_limit = offset + ntohs(cp->ch.chunk_length);
	if (sctp_validate_init_auth_params(m, offset + sizeof(*cp),
	    init_limit)) {
		/* auth parameter(s) error... send abort */
		sctp_abort_association(inp, stcb, m, iphlen, sh, NULL);
		return;
	}
	/* send an INIT-ACK w/cookie */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
		printf("sctp_handle_init: sending INIT-ACK\n");
	}
#endif
	sctp_send_initiate_ack(inp, stcb, m, iphlen, offset, sh, cp);
}

/*
 * process peer "INIT/INIT-ACK" chunk returns value < 0 on error
 */
static int
sctp_process_init(struct sctp_init_chunk *cp, struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	struct sctp_init *init;
	struct sctp_association *asoc;
	struct sctp_nets *lnet;
	unsigned int i;

	init = &cp->init;
	asoc = &stcb->asoc;
	/* save off parameters */
	asoc->peer_vtag = ntohl(init->initiate_tag);
	asoc->peers_rwnd = ntohl(init->a_rwnd);
	if (TAILQ_FIRST(&asoc->nets)) {
		/* update any ssthresh's that may have a default */
		TAILQ_FOREACH(lnet, &asoc->nets, sctp_next) {
			lnet->ssthresh = asoc->peers_rwnd;

#if defined(SCTP_CWND_MONITOR) || defined(SCTP_CWND_LOGGING)
			sctp_log_cwnd(stcb, lnet, 0, SCTP_CWND_INITIALIZATION);
#endif

		}
	}
	if (asoc->pre_open_streams > ntohs(init->num_inbound_streams)) {
		unsigned int newcnt;
		struct sctp_stream_out *outs;
		struct sctp_stream_queue_pending *sp;

		/* cut back on number of streams */
		newcnt = ntohs(init->num_inbound_streams);
		/* This if is probably not needed but I am cautious */
		if (asoc->strmout) {
			/* First make sure no data chunks are trapped */
			for (i = newcnt; i < asoc->pre_open_streams; i++) {
				outs = &asoc->strmout[i];
				sp = TAILQ_FIRST(&outs->outqueue);
				while (sp) {
					TAILQ_REMOVE(&outs->outqueue, sp,
					    next);
					asoc->stream_queue_cnt--;
					sctp_ulp_notify(SCTP_NOTIFY_SPECIAL_SP_FAIL,
					    stcb, SCTP_NOTIFY_DATAGRAM_UNSENT,
					    sp);
					if (sp->data) {
						sctp_m_freem(sp->data);
						sp->data = NULL;
					}
					sctp_free_remote_addr(sp->net);
					sp->net = NULL;
					/* Free the chunk */
					printf("sp:%p tcb:%p weird free case\n",
					    (u_int)sp, (u_int)stcb);

					sctp_free_a_strmoq(stcb, sp);
					sp = TAILQ_FIRST(&outs->outqueue);
				}
			}
		}
		/* cut back the count and abandon the upper streams */
		asoc->pre_open_streams = newcnt;
	}
	asoc->streamincnt = ntohs(init->num_outbound_streams);
	if (asoc->streamincnt > MAX_SCTP_STREAMS) {
		asoc->streamincnt = MAX_SCTP_STREAMS;
	}
	asoc->streamoutcnt = asoc->pre_open_streams;
	/* init tsn's */
	asoc->highest_tsn_inside_map = asoc->asconf_seq_in = ntohl(init->initial_tsn) - 1;
#ifdef SCTP_MAP_LOGGING
	sctp_log_map(0, 5, asoc->highest_tsn_inside_map, SCTP_MAP_SLIDE_RESULT);
#endif
	/* This is the next one we expect */
	asoc->str_reset_seq_in = asoc->asconf_seq_in + 1;

	asoc->mapping_array_base_tsn = ntohl(init->initial_tsn);
	asoc->cumulative_tsn = asoc->asconf_seq_in;
	asoc->last_echo_tsn = asoc->asconf_seq_in;
	asoc->advanced_peer_ack_point = asoc->last_acked_seq;
	/* open the requested streams */
	if (asoc->strmin != NULL) {
		/* Free the old ones */
		SCTP_FREE(asoc->strmin);
	}
	SCTP_MALLOC(asoc->strmin, struct sctp_stream_in *, asoc->streamincnt *
	    sizeof(struct sctp_stream_in), "StreamsIn");
	if (asoc->strmin == NULL) {
		/* we didn't get memory for the streams! */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("process_init: couldn't get memory for the streams!\n");
		}
#endif
		return (-1);
	}
	for (i = 0; i < asoc->streamincnt; i++) {
		asoc->strmin[i].stream_no = i;
		asoc->strmin[i].last_sequence_delivered = 0xffff;
		/*
		 * U-stream ranges will be set when the cookie is unpacked.
		 * Or for the INIT sender they are un set (if pr-sctp not
		 * supported) when the INIT-ACK arrives.
		 */
		TAILQ_INIT(&asoc->strmin[i].inqueue);
		/*
		 * we are not on any wheel, pr-sctp streams will go on the
		 * wheel when they have data waiting for reorder.
		 */
		asoc->strmin[i].next_spoke.tqe_next = 0;
		asoc->strmin[i].next_spoke.tqe_prev = 0;
	}

	/*
	 * load_address_from_init will put the addresses into the
	 * association when the COOKIE is processed or the INIT-ACK is
	 * processed. Both types of COOKIE's existing and new call this
	 * routine. It will remove addresses that are no longer in the
	 * association (for the restarting case where addresses are
	 * removed). Up front when the INIT arrives we will discard it if it
	 * is a restart and new addresses have been added.
	 */
	return (0);
}

/*
 * INIT-ACK message processing/consumption returns value < 0 on error
 */
static int
sctp_process_init_ack(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_init_ack_chunk *cp, struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	struct sctp_association *asoc;
	struct mbuf *op_err;
	int retval, abort_flag;
	uint32_t initack_limit;

	/* First verify that we have no illegal param's */
	abort_flag = 0;
	op_err = NULL;

	op_err = sctp_arethere_unrecognized_parameters(m,
	    (offset + sizeof(struct sctp_init_chunk)),
	    &abort_flag, (struct sctp_chunkhdr *)cp);
	if (abort_flag) {
		/* Send an abort and notify peer */
		if (op_err != NULL) {
			sctp_send_operr_to(m, iphlen, op_err, cp->init.initiate_tag);
		} else {
			/*
			 * Just notify (abort_assoc does this if we send an
			 * abort).
			 */
			sctp_abort_notification(stcb, 0);
			/*
			 * No sense in further INIT's since we will get the
			 * same param back
			 */
			sctp_free_assoc(stcb->sctp_ep, stcb, 0);
		}
		return (-1);
	}
	asoc = &stcb->asoc;
	/* process the peer's parameters in the INIT-ACK */
	retval = sctp_process_init((struct sctp_init_chunk *)cp, stcb, net);
	if (retval < 0) {
		return (retval);
	}
	initack_limit = offset + ntohs(cp->ch.chunk_length);
	/* load all addresses */
	if ((retval = sctp_load_addresses_from_init(stcb, m, iphlen,
	    (offset + sizeof(struct sctp_init_chunk)), initack_limit, sh,
	    NULL))) {
		/* Huh, we should abort */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
			printf("Load addresses from INIT causes an abort %d\n", retval);
		}
#endif
		sctp_abort_association(stcb->sctp_ep, stcb, m, iphlen, sh,
		    NULL);
		return (-1);
	}
	stcb->asoc.peer_hmac_id = sctp_negotiate_hmacid(stcb->asoc.peer_hmacs,
	    stcb->asoc.local_hmacs);
	if (op_err) {
		sctp_queue_op_err(stcb, op_err);
		/* queuing will steal away the mbuf chain to the out queue */
		op_err = NULL;
	}
	/* extract the cookie and queue it to "echo" it back... */
	stcb->asoc.overall_error_count = 0;
	net->error_count = 0;

	/*
	 * Cancel the INIT timer, We do this first before queueing the
	 * cookie. We always cancel at the primary to assue that we are
	 * canceling the timer started by the INIT which always goes to the
	 * primary.
	 */
	sctp_timer_stop(SCTP_TIMER_TYPE_INIT, stcb->sctp_ep, stcb,
	    asoc->primary_destination);

	retval = sctp_send_cookie_echo(m, offset, stcb, net);
	if (retval < 0) {
		/*
		 * No cookie, we probably should send a op error. But in any
		 * case if there is no cookie in the INIT-ACK, we can
		 * abandon the peer, its broke.
		 */
		if (retval == -3) {
			/* We abort with an error of missing mandatory param */
			struct mbuf *op_err;

			op_err =
			    sctp_generate_invmanparam(SCTP_CAUSE_MISSING_PARAM);
			if (op_err) {
				/*
				 * Expand beyond to include the mandatory
				 * param cookie
				 */
				struct sctp_inv_mandatory_param *mp;

				op_err->m_len =
				    sizeof(struct sctp_inv_mandatory_param);
				mp = mtod(op_err,
				    struct sctp_inv_mandatory_param *);
				/* Subtract the reserved param */
				mp->length =
				    htons(sizeof(struct sctp_inv_mandatory_param) - 2);
				mp->num_param = htonl(1);
				mp->param = htons(SCTP_STATE_COOKIE);
				mp->resv = 0;
			}
			sctp_abort_association(stcb->sctp_ep, stcb, m, iphlen,
			    sh, op_err);
		}
		return (retval);
	}
	/* calculate the RTO */
	net->RTO = sctp_calculate_rto(stcb, asoc, net, &asoc->time_entered);

	return (0);
}

static void
sctp_handle_heartbeat_ack(struct sctp_heartbeat_chunk *cp,
    struct sctp_tcb *stcb, struct sctp_nets *net)
{
	struct sockaddr_storage store;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sctp_nets *r_net;
	struct timeval tv;

	if (ntohs(cp->ch.chunk_length) != sizeof(struct sctp_heartbeat_chunk)) {
		/* Invalid length */
		return;
	}
	sin = (struct sockaddr_in *)&store;
	sin6 = (struct sockaddr_in6 *)&store;

	memset(&store, 0, sizeof(store));
	if (cp->heartbeat.hb_info.addr_family == AF_INET &&
	    cp->heartbeat.hb_info.addr_len == sizeof(struct sockaddr_in)) {
		sin->sin_family = cp->heartbeat.hb_info.addr_family;
		sin->sin_len = cp->heartbeat.hb_info.addr_len;
		sin->sin_port = stcb->rport;
		memcpy(&sin->sin_addr, cp->heartbeat.hb_info.address,
		    sizeof(sin->sin_addr));
	} else if (cp->heartbeat.hb_info.addr_family == AF_INET6 &&
	    cp->heartbeat.hb_info.addr_len == sizeof(struct sockaddr_in6)) {
		sin6->sin6_family = cp->heartbeat.hb_info.addr_family;
		sin6->sin6_len = cp->heartbeat.hb_info.addr_len;
		sin6->sin6_port = stcb->rport;
		memcpy(&sin6->sin6_addr, cp->heartbeat.hb_info.address,
		    sizeof(sin6->sin6_addr));
	} else {
		return;
	}
	r_net = sctp_findnet(stcb, (struct sockaddr *)sin);
	if (r_net == NULL) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
			printf("Huh? I can't find the address I sent it to, discard\n");
		}
#endif
		return;
	}
	if ((r_net && (r_net->dest_state & SCTP_ADDR_UNCONFIRMED)) &&
	    (r_net->heartbeat_random1 == cp->heartbeat.hb_info.random_value1) &&
	    (r_net->heartbeat_random2 == cp->heartbeat.hb_info.random_value2)) {
		/*
		 * If the its a HB and it's random value is correct when can
		 * confirm the destination.
		 */
		r_net->dest_state &= ~SCTP_ADDR_UNCONFIRMED;
		sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_CONFIRMED,
		    stcb, 0, (void *)r_net);
	}
	r_net->error_count = 0;
	r_net->hb_responded = 1;
	tv.tv_sec = cp->heartbeat.hb_info.time_value_1;
	tv.tv_usec = cp->heartbeat.hb_info.time_value_2;
	if (r_net->dest_state & SCTP_ADDR_NOT_REACHABLE) {
		r_net->dest_state &= ~SCTP_ADDR_NOT_REACHABLE;
		r_net->dest_state |= SCTP_ADDR_REACHABLE;
		sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_UP, stcb,
		    SCTP_HEARTBEAT_SUCCESS, (void *)r_net);
		/* now was it the primary? if so restore */
		if (r_net->dest_state & SCTP_ADDR_WAS_PRIMARY) {
			sctp_set_primary_addr(stcb, (struct sockaddr *)NULL, r_net);
		}
	}
	/* Now lets do a RTO with this */
	r_net->RTO = sctp_calculate_rto(stcb, &stcb->asoc, r_net, &tv);
}

static void
sctp_handle_abort(struct sctp_abort_chunk *cp,
    struct sctp_tcb *stcb, struct sctp_nets *net)
{

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
		printf("sctp_handle_abort: handling ABORT\n");
	}
#endif
	if (stcb == NULL)
		return;
	/* verify that the destination addr is in the association */
	/* ignore abort for addresses being deleted */

	/* stop any receive timers */
	sctp_timer_stop(SCTP_TIMER_TYPE_RECV, stcb->sctp_ep, stcb, net);
	/* notify user of the abort and clean up... */
	sctp_abort_notification(stcb, 0);
	/* free the tcb */
	SCTP_STAT_INCR_COUNTER32(sctps_aborted);
	if ((SCTP_GET_STATE(&stcb->asoc) == SCTP_STATE_OPEN) ||
	    (SCTP_GET_STATE(&stcb->asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
		SCTP_STAT_DECR_GAUGE32(sctps_currestab);
	}
	sctp_free_assoc(stcb->sctp_ep, stcb, 0);
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
		printf("sctp_handle_abort: finished\n");
	}
#endif
}

static void
sctp_handle_shutdown(struct sctp_shutdown_chunk *cp,
    struct sctp_tcb *stcb, struct sctp_nets *net, int *abort_flag)
{
	struct sctp_association *asoc;
	int some_on_streamwheel;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
		printf("sctp_handle_shutdown: handling SHUTDOWN\n");
	}
#endif
	if (stcb == NULL)
		return;

	if ((SCTP_GET_STATE(&stcb->asoc) == SCTP_STATE_COOKIE_WAIT) ||
	    (SCTP_GET_STATE(&stcb->asoc) == SCTP_STATE_COOKIE_ECHOED)) {
		return;
	}
	if (ntohs(cp->ch.chunk_length) != sizeof(struct sctp_shutdown_chunk)) {
		/* Shutdown NOT the expected size */
		return;
	} else {
		sctp_update_acked(stcb, cp, net, abort_flag);
	}
	asoc = &stcb->asoc;
	if (stcb->asoc.control_pdapi) {
		/*
		 * With a normal shutdown we assume the end of last record.
		 */
		SCTP_INP_READ_LOCK(stcb->sctp_ep);
		stcb->asoc.control_pdapi->end_added = 1;
		if (stcb->asoc.control_pdapi->tail_mbuf) {
			stcb->asoc.control_pdapi->tail_mbuf->m_flags |= M_EOR;
		}
		stcb->asoc.control_pdapi = NULL;
		SCTP_INP_READ_UNLOCK(stcb->sctp_ep);
	}
	/* goto SHUTDOWN_RECEIVED state to block new requests */
	if (stcb->sctp_socket) {
		if ((SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_RECEIVED) &&
		    (SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_SENT)) {
			asoc->state = SCTP_STATE_SHUTDOWN_RECEIVED;
			/*
			 * notify upper layer that peer has initiated a
			 * shutdown
			 */
			sctp_ulp_notify(SCTP_NOTIFY_PEER_SHUTDOWN, stcb, 0, NULL);

			/* reset time */
			SCTP_GETTIME_TIMEVAL(&asoc->time_entered);
		}
	}
	if (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_SENT) {
		/*
		 * stop the shutdown timer, since we WILL move to
		 * SHUTDOWN-ACK-SENT.
		 */
		sctp_timer_stop(SCTP_TIMER_TYPE_SHUTDOWN, stcb->sctp_ep, stcb, net);
	}
	/* Now are we there yet? */
	some_on_streamwheel = 0;
	if (!TAILQ_EMPTY(&asoc->out_wheel)) {
		/* Check to see if some data queued */
		struct sctp_stream_out *outs;

		TAILQ_FOREACH(outs, &asoc->out_wheel, next_spoke) {
			if (!TAILQ_EMPTY(&outs->outqueue)) {
				some_on_streamwheel = 1;
				break;
			}
		}
	}
	if (!TAILQ_EMPTY(&asoc->send_queue) ||
	    !TAILQ_EMPTY(&asoc->sent_queue) ||
	    some_on_streamwheel) {
		/* By returning we will push more data out */
		return;
	} else {
		/* no outstanding data to send, so move on... */
		/* send SHUTDOWN-ACK */
		sctp_send_shutdown_ack(stcb, stcb->asoc.primary_destination);
		/* move to SHUTDOWN-ACK-SENT state */
		if (asoc->state == SCTP_STATE_SHUTDOWN_RECEIVED) {
			SCTP_STAT_DECR_GAUGE32(sctps_currestab);
		}
		asoc->state = SCTP_STATE_SHUTDOWN_ACK_SENT;

		/* start SHUTDOWN timer */
		sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNACK, stcb->sctp_ep,
		    stcb, net);
	}
}

static void
sctp_handle_shutdown_ack(struct sctp_shutdown_ack_chunk *cp,
    struct sctp_tcb *stcb, struct sctp_nets *net)
{
	struct sctp_association *asoc;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
		printf("sctp_handle_shutdown_ack: handling SHUTDOWN ACK\n");
	}
#endif
	if (stcb == NULL)
		return;

	asoc = &stcb->asoc;
	/* process according to association state */
	if ((SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_SENT) &&
	    (SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_ACK_SENT)) {
		/* unexpected SHUTDOWN-ACK... so ignore... */
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	if (stcb->asoc.control_pdapi) {
		/*
		 * With a normal shutdown we assume the end of last record.
		 */
		SCTP_INP_READ_LOCK(stcb->sctp_ep);
		stcb->asoc.control_pdapi->end_added = 1;
		if (stcb->asoc.control_pdapi->tail_mbuf) {
			stcb->asoc.control_pdapi->tail_mbuf->m_flags |= M_EOR;
		}
		stcb->asoc.control_pdapi = NULL;
		SCTP_INP_READ_UNLOCK(stcb->sctp_ep);
	}
	/* are the queues empty? */
	if (!TAILQ_EMPTY(&asoc->send_queue) ||
	    !TAILQ_EMPTY(&asoc->sent_queue) ||
	    !TAILQ_EMPTY(&asoc->out_wheel)) {
		sctp_report_all_outbound(stcb);
	}
	/* stop the timer */
	sctp_timer_stop(SCTP_TIMER_TYPE_SHUTDOWN, stcb->sctp_ep, stcb, net);
	/* send SHUTDOWN-COMPLETE */
	sctp_send_shutdown_complete(stcb, net);
	/* notify upper layer protocol */
	if (stcb->sctp_socket) {
		sctp_ulp_notify(SCTP_NOTIFY_ASSOC_DOWN, stcb, 0, NULL);
		if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
		    (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
			/* Set the connected flag to disconnected */
			stcb->sctp_ep->sctp_socket->so_snd.sb_cc = 0;
		}
	}
	SCTP_STAT_INCR_COUNTER32(sctps_shutdown);
	/* free the TCB but first save off the ep */
	sctp_free_assoc(stcb->sctp_ep, stcb, 0);
}

/*
 * Skip past the param header and then we will find the chunk that caused the
 * problem. There are two possiblities ASCONF or FWD-TSN other than that and
 * our peer must be broken.
 */
static void
sctp_process_unrecog_chunk(struct sctp_tcb *stcb, struct sctp_paramhdr *phdr,
    struct sctp_nets *net)
{
	struct sctp_chunkhdr *chk;

	chk = (struct sctp_chunkhdr *)((caddr_t)phdr + sizeof(*phdr));
	switch (chk->chunk_type) {
	case SCTP_ASCONF_ACK:
	case SCTP_ASCONF:
		sctp_asconf_cleanup(stcb, net);
		break;
	case SCTP_FORWARD_CUM_TSN:
		stcb->asoc.peer_supports_prsctp = 0;
		break;
	default:
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("Peer does not support chunk type %d(%x)??\n",
			    chk->chunk_type, (uint32_t) chk->chunk_type);
		}
#endif
		break;
	}
}

/*
 * Skip past the param header and then we will find the param that caused the
 * problem.  There are a number of param's in a ASCONF OR the prsctp param
 * these will turn of specific features.
 */
static void
sctp_process_unrecog_param(struct sctp_tcb *stcb, struct sctp_paramhdr *phdr)
{
	struct sctp_paramhdr *pbad;

	pbad = phdr + 1;
	switch (ntohs(pbad->param_type)) {
		/* pr-sctp draft */
	case SCTP_PRSCTP_SUPPORTED:
		stcb->asoc.peer_supports_prsctp = 0;
		break;
	case SCTP_SUPPORTED_CHUNK_EXT:
		break;
		/* draft-ietf-tsvwg-addip-sctp */
	case SCTP_ECN_NONCE_SUPPORTED:
		stcb->asoc.peer_supports_ecn_nonce = 0;
		stcb->asoc.ecn_nonce_allowed = 0;
		stcb->asoc.ecn_allowed = 0;
		break;
	case SCTP_ADD_IP_ADDRESS:
	case SCTP_DEL_IP_ADDRESS:
	case SCTP_SET_PRIM_ADDR:
		stcb->asoc.peer_supports_asconf = 0;
		break;
	case SCTP_SUCCESS_REPORT:
	case SCTP_ERROR_CAUSE_IND:
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("Huh, the peer does not support success? or error cause?\n");
			printf("Turning off ASCONF to this strange peer\n");
		}
#endif
		stcb->asoc.peer_supports_asconf = 0;
		break;
	default:
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("Peer does not support param type %d(%x)??\n",
			    pbad->param_type, (uint32_t) pbad->param_type);
		}
#endif
		break;
	}
}

static int
sctp_handle_error(struct sctp_chunkhdr *ch,
    struct sctp_tcb *stcb, struct sctp_nets *net)
{
	int chklen;
	struct sctp_paramhdr *phdr;
	uint16_t error_type;
	uint16_t error_len;
	struct sctp_association *asoc;

	int adjust;

	/* parse through all of the errors and process */
	asoc = &stcb->asoc;
	phdr = (struct sctp_paramhdr *)((caddr_t)ch +
	    sizeof(struct sctp_chunkhdr));
	chklen = ntohs(ch->chunk_length) - sizeof(struct sctp_chunkhdr);
	while ((size_t)chklen >= sizeof(struct sctp_paramhdr)) {
		/* Process an Error Cause */
		error_type = ntohs(phdr->param_type);
		error_len = ntohs(phdr->param_length);
		if ((error_len > chklen) || (error_len == 0)) {
			/* invalid param length for this param */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
				printf("Bogus length in error param- chunk left:%d errorlen:%d\n",
				    chklen, error_len);
			}
#endif				/* SCTP_DEBUG */
			return (0);
		}
		switch (error_type) {
		case SCTP_CAUSE_INVALID_STREAM:
		case SCTP_CAUSE_MISSING_PARAM:
		case SCTP_CAUSE_INVALID_PARAM:
		case SCTP_CAUSE_NO_USER_DATA:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
				printf("Software error we got a %d back? We have a bug :/ (or do they?)\n",
				    error_type);
			}
#endif
			break;
		case SCTP_CAUSE_STALE_COOKIE:
			/*
			 * We only act if we have echoed a cookie and are
			 * waiting.
			 */
			if (SCTP_GET_STATE(asoc) == SCTP_STATE_COOKIE_ECHOED) {
				int *p;

				p = (int *)((caddr_t)phdr + sizeof(*phdr));
				/* Save the time doubled */
				asoc->cookie_preserve_req = ntohl(*p) << 1;
				asoc->stale_cookie_count++;
				if (asoc->stale_cookie_count >
				    asoc->max_init_times) {
					sctp_abort_notification(stcb, 0);
					/* now free the asoc */
					sctp_free_assoc(stcb->sctp_ep, stcb, 0);
					return (-1);
				}
				/* blast back to INIT state */
				asoc->state &= ~SCTP_STATE_COOKIE_ECHOED;
				asoc->state |= SCTP_STATE_COOKIE_WAIT;

				sctp_stop_all_cookie_timers(stcb);
				sctp_send_initiate(stcb->sctp_ep, stcb);
			}
			break;
		case SCTP_CAUSE_UNRESOLVABLE_ADDR:
			/*
			 * Nothing we can do here, we don't do hostname
			 * addresses so if the peer does not like my IPv6
			 * (or IPv4 for that matter) it does not matter. If
			 * they don't support that type of address, they can
			 * NOT possibly get that packet type... i.e. with no
			 * IPv6 you can't recieve a IPv6 packet. so we can
			 * safely ignore this one. If we ever added support
			 * for HOSTNAME Addresses, then we would need to do
			 * something here.
			 */
			break;
		case SCTP_CAUSE_UNRECOG_CHUNK:
			sctp_process_unrecog_chunk(stcb, phdr, net);
			break;
		case SCTP_CAUSE_UNRECOG_PARAM:
			sctp_process_unrecog_param(stcb, phdr);
			break;
		case SCTP_CAUSE_COOKIE_IN_SHUTDOWN:
			/*
			 * We ignore this since the timer will drive out a
			 * new cookie anyway and there timer will drive us
			 * to send a SHUTDOWN_COMPLETE. We can't send one
			 * here since we don't have their tag.
			 */
			break;
		case SCTP_CAUSE_DELETING_LAST_ADDR:
		case SCTP_CAUSE_RESOURCE_SHORTAGE:
		case SCTP_CAUSE_DELETING_SRC_ADDR:
			/*
			 * We should NOT get these here, but in a
			 * ASCONF-ACK. n
			 */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
				printf("Peer sends ASCONF errors in a Operational Error?<%d>?\n",
				    error_type);
			}
#endif
			break;
		case SCTP_CAUSE_OUT_OF_RESC:
			/*
			 * And what, pray tell do we do with the fact that
			 * the peer is out of resources? Not really sure we
			 * could do anything but abort. I suspect this n		 *
			 * should have came WITH an abort instead of in a
			 * OP-ERROR.
			 */
			break;
		default:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
				/* don't know what this error cause is... */
				printf("sctp_handle_error: unknown error type = 0x%xh\n",
				    error_type);
			}
#endif				/* SCTP_DEBUG */
			break;
		}
		adjust = SCTP_SIZE32(error_len);
		chklen -= adjust;
		phdr = (struct sctp_paramhdr *)((caddr_t)phdr + adjust);
	}
	return (0);
}

static int
sctp_handle_init_ack(struct mbuf *m, int iphlen, int offset, struct sctphdr *sh,
    struct sctp_init_ack_chunk *cp, struct sctp_tcb *stcb,
    struct sctp_nets *net)
{
	struct sctp_init_ack *init_ack;
	int *state;
	struct mbuf *op_err;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
		printf("sctp_handle_init_ack: handling INIT-ACK\n");
	}
#endif
	if (stcb == NULL) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("sctp_handle_init_ack: TCB is null\n");
		}
#endif
		return (-1);
	}
	if (ntohs(cp->ch.chunk_length) < sizeof(struct sctp_init_ack_chunk)) {
		/* Invalid length */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(stcb->sctp_ep, stcb, m, iphlen, sh,
		    op_err);
		return (-1);
	}
	init_ack = &cp->init;
	/* validate parameters */
	if (init_ack->initiate_tag == 0) {
		/* protocol error... send an abort */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(stcb->sctp_ep, stcb, m, iphlen, sh,
		    op_err);
		return (-1);
	}
	if (ntohl(init_ack->a_rwnd) < SCTP_MIN_RWND) {
		/* protocol error... send an abort */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(stcb->sctp_ep, stcb, m, iphlen, sh,
		    op_err);
		return (-1);
	}
	if (init_ack->num_inbound_streams == 0) {
		/* protocol error... send an abort */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(stcb->sctp_ep, stcb, m, iphlen, sh,
		    op_err);
		return (-1);
	}
	if (init_ack->num_outbound_streams == 0) {
		/* protocol error... send an abort */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_INVALID_PARAM);
		sctp_abort_association(stcb->sctp_ep, stcb, m, iphlen, sh,
		    op_err);
		return (-1);
	}
	/* process according to association state... */
	state = &stcb->asoc.state;
	switch (*state & SCTP_STATE_MASK) {
	case SCTP_STATE_COOKIE_WAIT:
		/* this is the expected state for this chunk */
		/* process the INIT-ACK parameters */
		if (stcb->asoc.primary_destination->dest_state &
		    SCTP_ADDR_UNCONFIRMED) {
			/*
			 * The primary is where we sent the INIT, we can
			 * always consider it confirmed when the INIT-ACK is
			 * returned. Do this before we load addresses
			 * though.
			 */
			stcb->asoc.primary_destination->dest_state &=
			    ~SCTP_ADDR_UNCONFIRMED;
			sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_CONFIRMED,
			    stcb, 0, (void *)stcb->asoc.primary_destination);
		}
		if (sctp_process_init_ack(m, iphlen, offset, sh, cp, stcb, net
		    ) < 0) {
			/* error in parsing parameters */
			return (-1);
		}
		/* update our state */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("moving to COOKIE-ECHOED state\n");
		}
#endif
		if (*state & SCTP_STATE_SHUTDOWN_PENDING) {
			*state = SCTP_STATE_COOKIE_ECHOED |
			    SCTP_STATE_SHUTDOWN_PENDING;
		} else {
			*state = SCTP_STATE_COOKIE_ECHOED;
		}

		/* reset the RTO calc */
		stcb->asoc.overall_error_count = 0;
		SCTP_GETTIME_TIMEVAL(&stcb->asoc.time_entered);
		/*
		 * collapse the init timer back in case of a exponential
		 * backoff n
		 */
		sctp_timer_start(SCTP_TIMER_TYPE_COOKIE, stcb->sctp_ep,
		    stcb, net);
		/*
		 * the send at the end of the inbound data processing will
		 * cause the cookie to be sent
		 */
		break;
	case SCTP_STATE_SHUTDOWN_SENT:
		/* incorrect state... discard */
		break;
	case SCTP_STATE_COOKIE_ECHOED:
		/* incorrect state... discard */
		break;
	case SCTP_STATE_OPEN:
		/* incorrect state... discard */
		break;
	case SCTP_STATE_EMPTY:
	case SCTP_STATE_INUSE:
	default:
		/* incorrect state... discard */
		return (-1);
		break;
	}			/* end switch asoc state */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
		printf("Leaving handle-init-ack end\n");
	}
#endif
	return (0);
}


/*
 * handle a state cookie for an existing association m: input packet mbuf
 * chain-- assumes a pullup on IP/SCTP/COOKIE-ECHO chunk note: this is a
 * "split" mbuf and the cookie signature does not exist offset: offset into
 * mbuf to the cookie-echo chunk
 */
static struct sctp_tcb *
sctp_process_cookie_existing(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_state_cookie *cookie, int cookie_len,
    struct sctp_inpcb *inp, struct sctp_tcb *stcb, struct sctp_nets *net,
    struct sockaddr *init_src, int *notification, sctp_assoc_t * sac_assoc_id)
{
	struct sctp_association *asoc;
	struct sctp_init_chunk *init_cp, init_buf;
	struct sctp_init_ack_chunk *initack_cp, initack_buf;
	int chk_length;
	int init_offset, initack_offset;
	int retval;

	/* I know that the TCB is non-NULL from the caller */
	asoc = &stcb->asoc;

	if (SCTP_GET_STATE(asoc) == SCTP_STATE_SHUTDOWN_ACK_SENT) {
		/* SHUTDOWN came in after sending INIT-ACK */
		struct mbuf *op_err;
		struct sctp_paramhdr *ph;

		sctp_send_shutdown_ack(stcb, stcb->asoc.primary_destination);
		op_err = sctp_get_mbuf_for_msg(sizeof(struct sctp_paramhdr),
		    1, M_DONTWAIT, 1, MT_DATA);
		if (op_err == NULL) {
			/* FOOBAR */
			return (NULL);
		}
		/* pre-reserve some space */
		op_err->m_data += sizeof(struct ip6_hdr);
		op_err->m_data += sizeof(struct sctphdr);
		op_err->m_data += sizeof(struct sctp_chunkhdr);
		/* Set the len */
		op_err->m_len = op_err->m_pkthdr.len = sizeof(struct sctp_paramhdr);
		ph = mtod(op_err, struct sctp_paramhdr *);
		ph->param_type = htons(SCTP_CAUSE_COOKIE_IN_SHUTDOWN);
		ph->param_length = htons(sizeof(struct sctp_paramhdr));
		sctp_send_operr_to(m, iphlen, op_err, cookie->peers_vtag);
		return (NULL);
	}
	/*
	 * find and validate the INIT chunk in the cookie (peer's info) the
	 * INIT should start after the cookie-echo header struct (chunk
	 * header, state cookie header struct)
	 */
	init_offset = offset += sizeof(struct sctp_cookie_echo_chunk);

	init_cp = (struct sctp_init_chunk *)
	    sctp_m_getptr(m, init_offset, sizeof(struct sctp_init_chunk),
	    (uint8_t *) & init_buf);
	if (init_cp == NULL) {
		/* could not pull a INIT chunk in cookie */
		return (NULL);
	}
	chk_length = ntohs(init_cp->ch.chunk_length);
	if (init_cp->ch.chunk_type != SCTP_INITIATION) {
		return (NULL);
	}
	/*
	 * find and validate the INIT-ACK chunk in the cookie (my info) the
	 * INIT-ACK follows the INIT chunk
	 */
	initack_offset = init_offset + SCTP_SIZE32(chk_length);
	initack_cp = (struct sctp_init_ack_chunk *)
	    sctp_m_getptr(m, initack_offset, sizeof(struct sctp_init_ack_chunk),
	    (uint8_t *) & initack_buf);
	if (initack_cp == NULL) {
		/* could not pull INIT-ACK chunk in cookie */
		return (NULL);
	}
	chk_length = ntohs(initack_cp->ch.chunk_length);
	if (initack_cp->ch.chunk_type != SCTP_INITIATION_ACK) {
		return (NULL);
	}
	if ((ntohl(initack_cp->init.initiate_tag) == asoc->my_vtag) &&
	    (ntohl(init_cp->init.initiate_tag) == asoc->peer_vtag)) {
		/*
		 * case D in Section 5.2.4 Table 2: MMAA process accordingly
		 * to get into the OPEN state
		 */
		switch SCTP_GET_STATE
			(asoc) {
		case SCTP_STATE_COOKIE_WAIT:
			/*
			 * INIT was sent, but got got a COOKIE_ECHO with the
			 * correct tags... just accept it...
			 */
			/* First we must process the INIT !! */
			retval = sctp_process_init(init_cp, stcb, net);
			if (retval < 0) {
				return (NULL);
			}
			/* intentional fall through to below... */

		case SCTP_STATE_COOKIE_ECHOED:
			/* Duplicate INIT case */
			/* we have already processed the INIT so no problem */
			sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb,
			    net);
			sctp_timer_stop(SCTP_TIMER_TYPE_INIT, inp, stcb, net);
			sctp_stop_all_cookie_timers(stcb);
			/* update current state */
			if (asoc->state & SCTP_STATE_SHUTDOWN_PENDING) {
				asoc->state = SCTP_STATE_OPEN |
				    SCTP_STATE_SHUTDOWN_PENDING;
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
				    stcb->sctp_ep, stcb, asoc->primary_destination);

			} else if ((asoc->state & SCTP_STATE_SHUTDOWN_SENT) == 0) {
				/* if ok, move to OPEN state */
				asoc->state = SCTP_STATE_OPEN;
			}
			if (((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
			    (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) &&
			    (inp->sctp_socket->so_qlimit == 0)
			    ) {
				/*
				 * Here is where collision would go if we
				 * did a connect() and instead got a
				 * init/init-ack/cookie done before the
				 * init-ack came back..
				 */
				stcb->sctp_ep->sctp_flags |=
				    SCTP_PCB_FLAGS_CONNECTED;
				soisconnected(stcb->sctp_ep->sctp_socket);
			}
			/* notify upper layer */
			*notification = SCTP_NOTIFY_ASSOC_UP;
			/*
			 * since we did not send a HB make sure we don't
			 * double things
			 */
			net->hb_responded = 1;

			if (stcb->asoc.sctp_autoclose_ticks &&
			    (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTOCLOSE))) {
				sctp_timer_start(SCTP_TIMER_TYPE_AUTOCLOSE,
				    inp, stcb, NULL);
			}
			break;
		default:
			/*
			 * we're in the OPEN state (or beyond), so peer must
			 * have simply lost the COOKIE-ACK
			 */
			break;
			}	/* end switch */

		/*
		 * We ignore the return code here.. not sure if we should
		 * somehow abort.. but we do have an existing asoc. This
		 * really should not fail.
		 */
		if (sctp_load_addresses_from_init(stcb, m, iphlen,
		    init_offset + sizeof(struct sctp_init_chunk),
		    initack_offset, sh, init_src)) {
			return (NULL);
		}
		/* respond with a COOKIE-ACK */
		sctp_send_cookie_ack(stcb);
		return (stcb);
	}			/* end if */
	if (ntohl(initack_cp->init.initiate_tag) != asoc->my_vtag &&
	    ntohl(init_cp->init.initiate_tag) == asoc->peer_vtag &&
	    cookie->tie_tag_my_vtag == 0 &&
	    cookie->tie_tag_peer_vtag == 0) {
		/*
		 * case C in Section 5.2.4 Table 2: XMOO silently discard
		 */
		return (NULL);
	}
	if (ntohl(initack_cp->init.initiate_tag) == asoc->my_vtag &&
	    (ntohl(init_cp->init.initiate_tag) != asoc->peer_vtag ||
	    init_cp->init.initiate_tag == 0)) {
		/*
		 * case B in Section 5.2.4 Table 2: MXAA or MOAA my info
		 * should be ok, re-accept peer info
		 */
		sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
		sctp_timer_stop(SCTP_TIMER_TYPE_INIT, inp, stcb, net);
		sctp_stop_all_cookie_timers(stcb);
		/*
		 * since we did not send a HB make sure we don't double
		 * things
		 */
		net->hb_responded = 1;
		if (stcb->asoc.sctp_autoclose_ticks &&
		    sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTOCLOSE)) {
			sctp_timer_start(SCTP_TIMER_TYPE_AUTOCLOSE, inp, stcb,
			    NULL);
		}
		asoc->my_rwnd = ntohl(initack_cp->init.a_rwnd);
		asoc->pre_open_streams =
		    ntohs(initack_cp->init.num_outbound_streams);
		asoc->init_seq_number = ntohl(initack_cp->init.initial_tsn);
		asoc->sending_seq = asoc->asconf_seq_out = asoc->str_reset_seq_out =
		    asoc->init_seq_number;
		asoc->last_cwr_tsn = asoc->init_seq_number - 1;
		asoc->asconf_seq_in = asoc->last_acked_seq = asoc->init_seq_number - 1;
		asoc->str_reset_seq_in = asoc->init_seq_number;
		asoc->advanced_peer_ack_point = asoc->last_acked_seq;

		/* process the INIT info (peer's info) */
		retval = sctp_process_init(init_cp, stcb, net);
		if (retval < 0) {
			return (NULL);
		}
		if (sctp_load_addresses_from_init(stcb, m, iphlen,
		    init_offset + sizeof(struct sctp_init_chunk),
		    initack_offset, sh, init_src)) {
			return (NULL);
		}
		if ((asoc->state & SCTP_STATE_COOKIE_WAIT) ||
		    (asoc->state & SCTP_STATE_COOKIE_ECHOED)) {
			*notification = SCTP_NOTIFY_ASSOC_UP;

			if (((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
			    (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) &&
			    (inp->sctp_socket->so_qlimit == 0)) {
				stcb->sctp_ep->sctp_flags |=
				    SCTP_PCB_FLAGS_CONNECTED;
				soisconnected(stcb->sctp_ep->sctp_socket);
			}
		}
		if (asoc->state & SCTP_STATE_SHUTDOWN_PENDING) {
			asoc->state = SCTP_STATE_OPEN |
			    SCTP_STATE_SHUTDOWN_PENDING;
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
			    stcb->sctp_ep, stcb, asoc->primary_destination);

		} else {
			asoc->state = SCTP_STATE_OPEN;
		}
		sctp_stop_all_cookie_timers(stcb);
		sctp_send_cookie_ack(stcb);
		return (stcb);
	}
	if ((ntohl(initack_cp->init.initiate_tag) != asoc->my_vtag &&
	    ntohl(init_cp->init.initiate_tag) != asoc->peer_vtag) &&
	    cookie->tie_tag_my_vtag == asoc->my_vtag_nonce &&
	    cookie->tie_tag_peer_vtag == asoc->peer_vtag_nonce &&
	    cookie->tie_tag_peer_vtag != 0) {
		struct sctpasochead *head;

		/*
		 * case A in Section 5.2.4 Table 2: XXMM (peer restarted)
		 */
		sctp_timer_stop(SCTP_TIMER_TYPE_INIT, inp, stcb, net);
		sctp_timer_stop(SCTP_TIMER_TYPE_HEARTBEAT, inp, stcb, net);
		*sac_assoc_id = sctp_get_associd(stcb);
		/* notify upper layer */
		*notification = SCTP_NOTIFY_ASSOC_RESTART;


		/* send up all the data */
		sctp_report_all_outbound(stcb);

		/* process the INIT-ACK info (my info) */
		asoc->my_vtag = ntohl(initack_cp->init.initiate_tag);
		asoc->my_rwnd = ntohl(initack_cp->init.a_rwnd);

		/* pull from vtag hash */
		LIST_REMOVE(stcb, sctp_asocs);
		/* re-insert to new vtag position */
		head = &sctppcbinfo.sctp_asochash[SCTP_PCBHASH_ASOC(stcb->asoc.my_vtag,
		    sctppcbinfo.hashasocmark)];
		/*
		 * put it in the bucket in the vtag hash of assoc's for the
		 * system
		 */
		LIST_INSERT_HEAD(head, stcb, sctp_asocs);

		/* Is this the first restart? */
		if (stcb->asoc.in_restart_hash == 0) {
			/* Ok add it to assoc_id vtag hash */
			head = &sctppcbinfo.sctp_restarthash[SCTP_PCBHASH_ASOC(stcb->asoc.assoc_id,
			    sctppcbinfo.hashrestartmark)];
			LIST_INSERT_HEAD(head, stcb, sctp_tcbrestarhash);
			stcb->asoc.in_restart_hash = 1;
		}
		asoc->pre_open_streams =
		    ntohs(initack_cp->init.num_outbound_streams);
		asoc->init_seq_number = ntohl(initack_cp->init.initial_tsn);
		asoc->sending_seq = asoc->asconf_seq_out = asoc->str_reset_seq_out = asoc->init_seq_number;

		asoc->last_cwr_tsn = asoc->init_seq_number - 1;
		asoc->asconf_seq_in = asoc->last_acked_seq = asoc->init_seq_number - 1;

		asoc->str_reset_seq_in = asoc->init_seq_number;

		asoc->advanced_peer_ack_point = asoc->last_acked_seq;
		if (asoc->mapping_array)
			memset(asoc->mapping_array, 0,
			    asoc->mapping_array_size);
		/* process the INIT info (peer's info) */
		retval = sctp_process_init(init_cp, stcb, net);
		if (retval < 0) {
			return (NULL);
		}
		/*
		 * since we did not send a HB make sure we don't double
		 * things
		 */
		net->hb_responded = 1;

		if (sctp_load_addresses_from_init(stcb, m, iphlen,
		    init_offset + sizeof(struct sctp_init_chunk),
		    initack_offset, sh, init_src)) {
			return (NULL);
		}
		if (asoc->state & SCTP_STATE_SHUTDOWN_PENDING) {
			asoc->state = SCTP_STATE_OPEN |
			    SCTP_STATE_SHUTDOWN_PENDING;
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
			    stcb->sctp_ep, stcb, asoc->primary_destination);

		} else if (!(asoc->state & SCTP_STATE_SHUTDOWN_SENT)) {
			/* move to OPEN state, if not in SHUTDOWN_SENT */
			asoc->state = SCTP_STATE_OPEN;
		}
		/* respond with a COOKIE-ACK */
		sctp_stop_all_cookie_timers(stcb);
		sctp_send_cookie_ack(stcb);

		return (stcb);
	}
	/* if we are not a restart we need the assoc_id field pop'd */
	asoc->assoc_id = ntohl(initack_cp->init.initiate_tag);

	/* all other cases... */
	return (NULL);
}

/*
 * handle a state cookie for a new association m: input packet mbuf chain--
 * assumes a pullup on IP/SCTP/COOKIE-ECHO chunk note: this is a "split" mbuf
 * and the cookie signature does not exist offset: offset into mbuf to the
 * cookie-echo chunk length: length of the cookie chunk to: where the init
 * was from returns a new TCB
 */
static struct sctp_tcb *
sctp_process_cookie_new(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_state_cookie *cookie, int cookie_len,
    struct sctp_inpcb *inp, struct sctp_nets **netp,
    struct sockaddr *init_src, int *notification,
    int auth_skipped, uint32_t auth_offset, uint32_t auth_len)
{
	struct sctp_tcb *stcb;
	struct sctp_init_chunk *init_cp, init_buf;
	struct sctp_init_ack_chunk *initack_cp, initack_buf;
	struct sockaddr_storage sa_store;
	struct sockaddr *initack_src = (struct sockaddr *)&sa_store;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sctp_association *asoc;
	int chk_length;
	int init_offset, initack_offset, initack_limit;
	int retval;
	int error = 0;
	uint32_t old_tag;
	uint8_t chunk_buf[DEFAULT_CHUNK_BUFFER];

	/*
	 * find and validate the INIT chunk in the cookie (peer's info) the
	 * INIT should start after the cookie-echo header struct (chunk
	 * header, state cookie header struct)
	 */
	init_offset = offset + sizeof(struct sctp_cookie_echo_chunk);
	init_cp = (struct sctp_init_chunk *)
	    sctp_m_getptr(m, init_offset, sizeof(struct sctp_init_chunk),
	    (uint8_t *) & init_buf);
	if (init_cp == NULL) {
		/* could not pull a INIT chunk in cookie */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
			printf("process_cookie_new: could not pull INIT chunk hdr\n");
		}
#endif				/* SCTP_DEBUG */
		return (NULL);
	}
	chk_length = ntohs(init_cp->ch.chunk_length);
	if (init_cp->ch.chunk_type != SCTP_INITIATION) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
			printf("HUH? process_cookie_new: could not find INIT chunk!\n");
		}
#endif				/* SCTP_DEBUG */
		return (NULL);
	}
	initack_offset = init_offset + SCTP_SIZE32(chk_length);
	/*
	 * find and validate the INIT-ACK chunk in the cookie (my info) the
	 * INIT-ACK follows the INIT chunk
	 */
	initack_cp = (struct sctp_init_ack_chunk *)
	    sctp_m_getptr(m, initack_offset, sizeof(struct sctp_init_ack_chunk),
	    (uint8_t *) & initack_buf);
	if (initack_cp == NULL) {
		/* could not pull INIT-ACK chunk in cookie */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
			printf("process_cookie_new: could not pull INIT-ACK chunk hdr\n");
		}
#endif				/* SCTP_DEBUG */
		return (NULL);
	}
	chk_length = ntohs(initack_cp->ch.chunk_length);
	if (initack_cp->ch.chunk_type != SCTP_INITIATION_ACK) {
		return (NULL);
	}
	/*
	 * NOTE: We can't use the INIT_ACK's chk_length to determine the
	 * "initack_limit" value.  This is because the chk_length field
	 * includes the length of the cookie, but the cookie is omitted when
	 * the INIT and INIT_ACK are tacked onto the cookie...
	 */
	initack_limit = offset + cookie_len;

	/*
	 * now that we know the INIT/INIT-ACK are in place, create a new TCB
	 * and popluate
	 */
	stcb = sctp_aloc_assoc(inp, init_src, 0, &error,
	    ntohl(initack_cp->init.initiate_tag));
	if (stcb == NULL) {
		struct mbuf *op_err;

		/* memory problem? */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
			printf("process_cookie_new: no room for another TCB!\n");
		}
#endif				/* SCTP_DEBUG */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_OUT_OF_RESC);
		sctp_abort_association(inp, (struct sctp_tcb *)NULL, m, iphlen,
		    sh, op_err);
		return (NULL);
	}
	/* get the correct sctp_nets */
	*netp = sctp_findnet(stcb, init_src);
	asoc = &stcb->asoc;
	/* get scope variables out of cookie */
	asoc->ipv4_local_scope = cookie->ipv4_scope;
	asoc->site_scope = cookie->site_scope;
	asoc->local_scope = cookie->local_scope;
	asoc->loopback_scope = cookie->loopback_scope;

	if ((asoc->ipv4_addr_legal != cookie->ipv4_addr_legal) ||
	    (asoc->ipv6_addr_legal != cookie->ipv6_addr_legal)) {
		struct mbuf *op_err;

		/*
		 * Houston we have a problem. The EP changed while the
		 * cookie was in flight. Only recourse is to abort the
		 * association.
		 */
		op_err = sctp_generate_invmanparam(SCTP_CAUSE_OUT_OF_RESC);
		sctp_abort_association(inp, (struct sctp_tcb *)NULL, m, iphlen,
		    sh, op_err);
		return (NULL);
	}
	/* process the INIT-ACK info (my info) */
	old_tag = asoc->my_vtag;
	asoc->assoc_id = asoc->my_vtag = ntohl(initack_cp->init.initiate_tag);
	asoc->my_rwnd = ntohl(initack_cp->init.a_rwnd);
	asoc->pre_open_streams = ntohs(initack_cp->init.num_outbound_streams);
	asoc->init_seq_number = ntohl(initack_cp->init.initial_tsn);
	asoc->sending_seq = asoc->asconf_seq_out = asoc->str_reset_seq_out = asoc->init_seq_number;
	asoc->last_cwr_tsn = asoc->init_seq_number - 1;
	asoc->asconf_seq_in = asoc->last_acked_seq = asoc->init_seq_number - 1;
	asoc->str_reset_seq_in = asoc->init_seq_number;

	asoc->advanced_peer_ack_point = asoc->last_acked_seq;

	/* process the INIT info (peer's info) */
	retval = sctp_process_init(init_cp, stcb, *netp);
	if (retval < 0) {
		sctp_free_assoc(inp, stcb, 0);
		return (NULL);
	}
	/* load all addresses */
	if (sctp_load_addresses_from_init(stcb, m, iphlen,
	    init_offset + sizeof(struct sctp_init_chunk), initack_offset, sh,
	    init_src)) {
		sctp_free_assoc(inp, stcb, 0);
		return (NULL);
	}
	/*
	 * verify any preceding AUTH chunk that was skipped
	 */
	/* pull the local authentication parameters from the cookie/init-ack */
	sctp_auth_get_cookie_params(stcb, m,
	    initack_offset + sizeof(struct sctp_init_ack_chunk),
	    initack_limit - (initack_offset + sizeof(struct sctp_init_ack_chunk)));
	if (auth_skipped) {
		struct sctp_auth_chunk *auth;

		auth = (struct sctp_auth_chunk *)
		    sctp_m_getptr(m, auth_offset, auth_len, chunk_buf);
		if (sctp_handle_auth(stcb, auth, m, auth_offset)) {
			/* auth HMAC failed, dump the assoc and packet */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_AUTH1)
				printf("COOKIE-ECHO: AUTH failed\n");
#endif				/* SCTP_DEBUG */
			sctp_free_assoc(inp, stcb, 0);
			return (NULL);
		} else {
			/* remaining chunks checked... good to go */
			stcb->asoc.authenticated = 1;
		}
	}
	/* update current state */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
		printf("moving to OPEN state\n");
	}
#endif
	if (asoc->state & SCTP_STATE_SHUTDOWN_PENDING) {
		asoc->state = SCTP_STATE_OPEN | SCTP_STATE_SHUTDOWN_PENDING;
		sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
		    stcb->sctp_ep, stcb, asoc->primary_destination);
	} else {
		asoc->state = SCTP_STATE_OPEN;
	}
	SCTP_STAT_INCR_COUNTER32(sctps_passiveestab);
	SCTP_STAT_INCR_GAUGE32(sctps_currestab);
	sctp_stop_all_cookie_timers(stcb);
	/* calculate the RTT */
	(*netp)->RTO = sctp_calculate_rto(stcb, asoc, *netp,
	    &cookie->time_entered);

	/*
	 * if we're doing ASCONFs, check to see if we have any new local
	 * addresses that need to get added to the peer (eg. addresses
	 * changed while cookie echo in flight).  This needs to be done
	 * after we go to the OPEN state to do the correct asconf
	 * processing. else, make sure we have the correct addresses in our
	 * lists
	 */

	/* warning, we re-use sin, sin6, sa_store here! */
	/* pull in local_address (our "from" address) */
	if (cookie->laddr_type == SCTP_IPV4_ADDRESS) {
		/* source addr is IPv4 */
		sin = (struct sockaddr_in *)initack_src;
		memset(sin, 0, sizeof(*sin));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(struct sockaddr_in);
		sin->sin_addr.s_addr = cookie->laddress[0];
	} else if (cookie->laddr_type == SCTP_IPV6_ADDRESS) {
		/* source addr is IPv6 */
		sin6 = (struct sockaddr_in6 *)initack_src;
		memset(sin6, 0, sizeof(*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		sin6->sin6_scope_id = cookie->scope_id;
		memcpy(&sin6->sin6_addr, cookie->laddress,
		    sizeof(sin6->sin6_addr));
	} else {
		sctp_free_assoc(inp, stcb, 0);
		return (NULL);
	}

	sctp_check_address_list(stcb, m,
	    initack_offset + sizeof(struct sctp_init_ack_chunk),
	    initack_limit - (initack_offset + sizeof(struct sctp_init_ack_chunk)),
	    initack_src, cookie->local_scope, cookie->site_scope,
	    cookie->ipv4_scope, cookie->loopback_scope);


	/* set up to notify upper layer */
	*notification = SCTP_NOTIFY_ASSOC_UP;
	if (((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
	    (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) &&
	    (inp->sctp_socket->so_qlimit == 0)) {
		/*
		 * This is an endpoint that called connect() how it got a
		 * cookie that is NEW is a bit of a mystery. It must be that
		 * the INIT was sent, but before it got there.. a complete
		 * INIT/INIT-ACK/COOKIE arrived. But of course then it
		 * should have went to the other code.. not here.. oh well..
		 * a bit of protection is worth having..
		 */
		stcb->sctp_ep->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
		soisconnected(stcb->sctp_ep->sctp_socket);
	} else if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) &&
	    (inp->sctp_socket->so_qlimit)) {
		/*
		 * We don't want to do anything with this one. Since it is
		 * the listening guy. The timer will get started for
		 * accepted connections in the caller.
		 */
		;
	}
	/* since we did not send a HB make sure we don't double things */
	(*netp)->hb_responded = 1;

	if (stcb->asoc.sctp_autoclose_ticks &&
	    sctp_is_feature_on(inp, SCTP_PCB_FLAGS_AUTOCLOSE)) {
		sctp_timer_start(SCTP_TIMER_TYPE_AUTOCLOSE, inp, stcb, NULL);
	}
	/* respond with a COOKIE-ACK */
	sctp_send_cookie_ack(stcb);
	return (stcb);
}


/*
 * handles a COOKIE-ECHO message stcb: modified to either a new or left as
 * existing (non-NULL) TCB
 */
static struct mbuf *
sctp_handle_cookie_echo(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_cookie_echo_chunk *cp,
    struct sctp_inpcb **inp_p, struct sctp_tcb **stcb, struct sctp_nets **netp,
    int auth_skipped, uint32_t auth_offset, uint32_t auth_len)
{
	struct sctp_state_cookie *cookie;
	struct sockaddr_in6 sin6;
	struct sockaddr_in sin;
	struct sctp_tcb *l_stcb = *stcb;
	struct sctp_inpcb *l_inp;
	struct sockaddr *to;
	sctp_assoc_t sac_restart_id;
	struct sctp_pcb *ep;
	struct mbuf *m_sig;
	uint8_t calc_sig[SCTP_SIGNATURE_SIZE], tmp_sig[SCTP_SIGNATURE_SIZE];
	uint8_t *sig;
	uint8_t cookie_ok = 0;
	unsigned int size_of_pkt, sig_offset, cookie_offset;
	unsigned int cookie_len;
	struct timeval now;
	struct timeval time_expires;
	struct sockaddr_storage dest_store;
	struct sockaddr *localep_sa = (struct sockaddr *)&dest_store;
	struct ip *iph;
	int notification = 0;
	struct sctp_nets *netl;
	int had_a_existing_tcb = 0;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
		printf("sctp_handle_cookie: handling COOKIE-ECHO\n");
	}
#endif

	if (inp_p == NULL) {
		return (NULL);
	}
	/* First get the destination address setup too. */
	iph = mtod(m, struct ip *);
	if (iph->ip_v == IPVERSION) {
		/* its IPv4 */
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)(localep_sa);
		memset(sin, 0, sizeof(*sin));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_port = sh->dest_port;
		sin->sin_addr.s_addr = iph->ip_dst.s_addr;
	} else if (iph->ip_v == (IPV6_VERSION >> 4)) {
		/* its IPv6 */
		struct ip6_hdr *ip6;
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)(localep_sa);
		memset(sin6, 0, sizeof(*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		ip6 = mtod(m, struct ip6_hdr *);
		sin6->sin6_port = sh->dest_port;
		sin6->sin6_addr = ip6->ip6_dst;
	} else {
		return (NULL);
	}

	cookie = &cp->cookie;
	cookie_offset = offset + sizeof(struct sctp_chunkhdr);
	cookie_len = ntohs(cp->ch.chunk_length);

	if ((cookie->peerport != sh->src_port) &&
	    (cookie->myport != sh->dest_port) &&
	    (cookie->my_vtag != sh->v_tag)) {
		/*
		 * invalid ports or bad tag.  Note that we always leave the
		 * v_tag in the header in network order and when we stored
		 * it in the my_vtag slot we also left it in network order.
		 * This maintians the match even though it may be in the
		 * opposite byte order of the machine :->
		 */
		return (NULL);
	}
	/* compute size of packet */
	if (m->m_flags & M_PKTHDR) {
		size_of_pkt = m->m_pkthdr.len;
	} else {
		/* Should have a pkt hdr really */
		struct mbuf *mat;

		mat = m;
		size_of_pkt = 0;
		while (mat != NULL) {
			size_of_pkt += mat->m_len;
			mat = mat->m_next;
		}
	}
	if (cookie_len > size_of_pkt ||
	    cookie_len < sizeof(struct sctp_cookie_echo_chunk) +
	    sizeof(struct sctp_init_chunk) +
	    sizeof(struct sctp_init_ack_chunk) + SCTP_SIGNATURE_SIZE) {
		/* cookie too long!  or too small */
		return (NULL);
	}
	/*
	 * split off the signature into its own mbuf (since it should not be
	 * calculated in the sctp_hmac_m() call).
	 */
	sig_offset = offset + cookie_len - SCTP_SIGNATURE_SIZE;
	if (sig_offset > size_of_pkt) {
		/* packet not correct size! */
		/* XXX this may already be accounted for earlier... */
		return (NULL);
	}
	m_sig = m_split(m, sig_offset, M_DONTWAIT);
	if (m_sig == NULL) {
		/* out of memory or ?? */
		return (NULL);
	}
	/*
	 * compute the signature/digest for the cookie
	 */
	ep = &(*inp_p)->sctp_ep;
	l_inp = *inp_p;
	if (l_stcb) {
		SCTP_TCB_UNLOCK(l_stcb);
	}
	SCTP_INP_RLOCK(l_inp);
	if (l_stcb) {
		SCTP_TCB_LOCK(l_stcb);
	}
	/* which cookie is it? */
	if ((cookie->time_entered.tv_sec < (long)ep->time_of_secret_change) &&
	    (ep->current_secret_number != ep->last_secret_number)) {
		/* it's the old cookie */
		sctp_hmac_m(SCTP_HMAC,
		    (uint8_t *) ep->secret_key[(int)ep->last_secret_number],
		    SCTP_SECRET_SIZE, m, cookie_offset, calc_sig);
	} else {
		/* it's the current cookie */
		sctp_hmac_m(SCTP_HMAC,
		    (uint8_t *) ep->secret_key[(int)ep->current_secret_number],
		    SCTP_SECRET_SIZE, m, cookie_offset, calc_sig);
	}
	/* get the signature */
	SCTP_INP_RUNLOCK(l_inp);
	sig = (uint8_t *) sctp_m_getptr(m_sig, 0, SCTP_SIGNATURE_SIZE, (uint8_t *) & tmp_sig);
	if (sig == NULL) {
		/* couldn't find signature */
		return (NULL);
	}
	/* compare the received digest with the computed digest */
	if (memcmp(calc_sig, sig, SCTP_SIGNATURE_SIZE) != 0) {
		/* try the old cookie? */
		if ((cookie->time_entered.tv_sec == (long)ep->time_of_secret_change) &&
		    (ep->current_secret_number != ep->last_secret_number)) {
			/* compute digest with old */
			sctp_hmac_m(SCTP_HMAC,
			    (uint8_t *) ep->secret_key[(int)ep->last_secret_number],
			    SCTP_SECRET_SIZE, m, cookie_offset, calc_sig);
			/* compare */
			if (memcmp(calc_sig, sig, SCTP_SIGNATURE_SIZE) == 0)
				cookie_ok = 1;
		}
	} else {
		cookie_ok = 1;
	}

	/*
	 * Now before we continue we must reconstruct our mbuf so that
	 * normal processing of any other chunks will work.
	 */
	{
		struct mbuf *m_at;

		m_at = m;
		while (m_at->m_next != NULL) {
			m_at = m_at->m_next;
		}
		m_at->m_next = m_sig;
		if (m->m_flags & M_PKTHDR) {
			/*
			 * We should only do this if and only if the front
			 * mbuf has a m_pkthdr... it should in theory.
			 */
			if (m_sig->m_flags & M_PKTHDR) {
				/* Add back to the pkt hdr of main m chain */
				m->m_pkthdr.len += m_sig->m_pkthdr.len;
			} else {
				/*
				 * Got a problem, no pkthdr in split chain.
				 * TSNH but we will handle it just in case
				 */
				int mmlen = 0;
				struct mbuf *lat;

				printf("Warning: Hitting m_split join TSNH code - fixed\n");
				lat = m_sig;
				while (lat) {
					mmlen += lat->m_len;
					lat = lat->m_next;
				}
				m->m_pkthdr.len += mmlen;
			}
		}
	}

	if (cookie_ok == 0) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("handle_cookie_echo: cookie signature validation failed!\n");
			printf("offset = %u, cookie_offset = %u, sig_offset = %u\n",
			    (uint32_t) offset, cookie_offset, sig_offset);
		}
#endif
		return (NULL);
	}
	/*
	 * check the cookie timestamps to be sure it's not stale
	 */
	SCTP_GETTIME_TIMEVAL(&now);
	/* Expire time is in Ticks, so we convert to seconds */
	time_expires.tv_sec = cookie->time_entered.tv_sec + cookie->cookie_life;
	time_expires.tv_usec = cookie->time_entered.tv_usec;
	if (timevalcmp(&now, &time_expires, >)) {
		/* cookie is stale! */
		struct mbuf *op_err;
		struct sctp_stale_cookie_msg *scm;
		uint32_t tim;

		op_err = sctp_get_mbuf_for_msg(sizeof(struct sctp_stale_cookie_msg),
		    1, M_DONTWAIT, 1, MT_DATA);
		if (op_err == NULL) {
			/* FOOBAR */
			return (NULL);
		}
		/* pre-reserve some space */
		op_err->m_data += sizeof(struct ip6_hdr);
		op_err->m_data += sizeof(struct sctphdr);
		op_err->m_data += sizeof(struct sctp_chunkhdr);

		/* Set the len */
		op_err->m_len = op_err->m_pkthdr.len = sizeof(struct sctp_stale_cookie_msg);
		scm = mtod(op_err, struct sctp_stale_cookie_msg *);
		scm->ph.param_type = htons(SCTP_CAUSE_STALE_COOKIE);
		scm->ph.param_length = htons((sizeof(struct sctp_paramhdr) +
		    (sizeof(uint32_t))));
		/* seconds to usec */
		tim = (now.tv_sec - time_expires.tv_sec) * 1000000;
		/* add in usec */
		if (tim == 0)
			tim = now.tv_usec - cookie->time_entered.tv_usec;
		scm->time_usec = htonl(tim);
		sctp_send_operr_to(m, iphlen, op_err, cookie->peers_vtag);
		return (NULL);
	}
	/*
	 * Now we must see with the lookup address if we have an existing
	 * asoc. This will only happen if we were in the COOKIE-WAIT state
	 * and a INIT collided with us and somewhere the peer sent the
	 * cookie on another address besides the single address our assoc
	 * had for him. In this case we will have one of the tie-tags set at
	 * least AND the address field in the cookie can be used to look it
	 * up.
	 */
	to = NULL;
	if (cookie->addr_type == SCTP_IPV6_ADDRESS) {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_port = sh->src_port;
		sin6.sin6_scope_id = cookie->scope_id;
		memcpy(&sin6.sin6_addr.s6_addr, cookie->address,
		    sizeof(sin6.sin6_addr.s6_addr));
		to = (struct sockaddr *)&sin6;
	} else if (cookie->addr_type == SCTP_IPV4_ADDRESS) {
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		sin.sin_port = sh->src_port;
		sin.sin_addr.s_addr = cookie->address[0];
		to = (struct sockaddr *)&sin;
	}
	if ((*stcb == NULL) && to) {
		/* Yep, lets check */
		*stcb = sctp_findassociation_ep_addr(inp_p, to, netp, localep_sa, NULL);
		if (*stcb == NULL) {
			/*
			 * We should have only got back the same inp. If we
			 * got back a different ep we have a problem. The
			 * original findep got back l_inp and now
			 */
			if (l_inp != *inp_p) {
				printf("Bad problem find_ep got a diff inp then special_locate?\n");
			}
		}
	}
	cookie_len -= SCTP_SIGNATURE_SIZE;
	if (*stcb == NULL) {
		/* this is the "normal" case... get a new TCB */
		*stcb = sctp_process_cookie_new(m, iphlen, offset, sh, cookie,
		    cookie_len, *inp_p, netp, to, &notification,
		    auth_skipped, auth_offset, auth_len);
	} else {
		/* this is abnormal... cookie-echo on existing TCB */
		had_a_existing_tcb = 1;
		*stcb = sctp_process_cookie_existing(m, iphlen, offset, sh,
		    cookie, cookie_len, *inp_p, *stcb, *netp, to, &notification,
		    &sac_restart_id);
	}

	if (*stcb == NULL) {
		/* still no TCB... must be bad cookie-echo */
		return (NULL);
	}
	/*
	 * Ok, we built an association so confirm the address we sent the
	 * INIT-ACK to.
	 */
	netl = sctp_findnet(*stcb, to);
	/*
	 * This code should in theory NOT run but
	 */
	if (netl == NULL) {
		/* TSNH! Huh, why do I need to add this address here? */
		int ret;

		ret = sctp_add_remote_addr(*stcb, to, 0, 100);
		netl = sctp_findnet(*stcb, to);
	}
	if (netl) {
		if (netl->dest_state & SCTP_ADDR_UNCONFIRMED) {
			netl->dest_state &= ~SCTP_ADDR_UNCONFIRMED;
			sctp_set_primary_addr((*stcb), (struct sockaddr *)NULL,
			    netl);
			sctp_ulp_notify(SCTP_NOTIFY_INTERFACE_CONFIRMED,
			    (*stcb), 0, (void *)netl);
		}
	}
	if (*stcb) {
		sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, *inp_p,
		    *stcb, NULL);
	}
	if ((*inp_p)->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		if (!had_a_existing_tcb ||
		    (((*inp_p)->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) == 0)) {
			/*
			 * If we have a NEW cookie or the connect never
			 * reached the connected state during collision we
			 * must do the TCP accept thing.
			 */
			struct socket *so, *oso;
			struct sctp_inpcb *inp;

			if (notification == SCTP_NOTIFY_ASSOC_RESTART) {
				/*
				 * For a restart we will keep the same
				 * socket, no need to do anything. I THINK!!
				 */
				sctp_ulp_notify(notification, *stcb, 0, (void *)&sac_restart_id);
				return (m);
			}
			oso = (*inp_p)->sctp_socket;
			/*
			 * We do this to keep the sockets side happy durin
			 * the sonewcon ONLY.
			 */
			NET_LOCK_GIANT();
			SCTP_TCB_UNLOCK((*stcb));
			so = sonewconn(oso, SS_ISCONNECTED
			    );
			NET_UNLOCK_GIANT();
			SCTP_INP_WLOCK((*stcb)->sctp_ep);
			SCTP_TCB_LOCK((*stcb));
			SCTP_INP_WUNLOCK((*stcb)->sctp_ep);
			if (so == NULL) {
				struct mbuf *op_err;

				/* Too many sockets */
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
					printf("process_cookie_new: no room for another socket!\n");
				}
#endif				/* SCTP_DEBUG */
				op_err = sctp_generate_invmanparam(SCTP_CAUSE_OUT_OF_RESC);
				sctp_abort_association(*inp_p, NULL, m, iphlen,
				    sh, op_err);
				sctp_free_assoc(*inp_p, *stcb, 0);
				return (NULL);
			}
			inp = (struct sctp_inpcb *)so->so_pcb;
			inp->sctp_flags = (SCTP_PCB_FLAGS_TCPTYPE |
			    SCTP_PCB_FLAGS_CONNECTED |
			    SCTP_PCB_FLAGS_IN_TCPPOOL |
			    (SCTP_PCB_COPY_FLAGS & (*inp_p)->sctp_flags) |
			    SCTP_PCB_FLAGS_DONT_WAKE);
			inp->sctp_features = (*inp_p)->sctp_features;
			inp->sctp_socket = so;
			inp->sctp_frag_point = (*inp_p)->sctp_frag_point;
			inp->partial_delivery_point = (*inp_p)->partial_delivery_point;
			inp->sctp_context = (*inp_p)->sctp_context;
			inp->inp_starting_point_for_iterator = NULL;
			/*
			 * copy in the authentication parameters from the
			 * original endpoint
			 */
			if (inp->sctp_ep.local_hmacs)
				sctp_free_hmaclist(inp->sctp_ep.local_hmacs);
			inp->sctp_ep.local_hmacs =
			    sctp_copy_hmaclist((*inp_p)->sctp_ep.local_hmacs);
			if (inp->sctp_ep.local_auth_chunks)
				sctp_free_chunklist(inp->sctp_ep.local_auth_chunks);
			inp->sctp_ep.local_auth_chunks =
			    sctp_copy_chunklist((*inp_p)->sctp_ep.local_auth_chunks);
			(void)sctp_copy_skeylist(&(*inp_p)->sctp_ep.shared_keys,
			    &inp->sctp_ep.shared_keys);

			/*
			 * Now we must move it from one hash table to
			 * another and get the tcb in the right place.
			 */
			sctp_move_pcb_and_assoc(*inp_p, inp, *stcb);

			sctp_pull_off_control_to_new_inp((*inp_p), inp, *stcb);

			/* Switch over to the new guy */
			*inp_p = inp;

			sctp_ulp_notify(notification, *stcb, 0, NULL);
			return (m);
		}
	}
	if ((notification) && ((*inp_p)->sctp_flags & SCTP_PCB_FLAGS_UDPTYPE)) {
		sctp_ulp_notify(notification, *stcb, 0, NULL);
	}
	return (m);
}

static void
sctp_handle_cookie_ack(struct sctp_cookie_ack_chunk *cp,
    struct sctp_tcb *stcb, struct sctp_nets *net)
{
	/* cp must not be used, others call this without a c-ack :-) */
	struct sctp_association *asoc;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
		printf("sctp_handle_cookie_ack: handling COOKIE-ACK\n");
	}
#endif
	if (stcb == NULL)
		return;

	asoc = &stcb->asoc;

	sctp_stop_all_cookie_timers(stcb);
	/* process according to association state */
	if (SCTP_GET_STATE(asoc) == SCTP_STATE_COOKIE_ECHOED) {
		/* state change only needed when I am in right state */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("moving to OPEN state\n");
		}
#endif
		if (asoc->state & SCTP_STATE_SHUTDOWN_PENDING) {
			asoc->state = SCTP_STATE_OPEN | SCTP_STATE_SHUTDOWN_PENDING;
			sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD,
			    stcb->sctp_ep, stcb, asoc->primary_destination);

		} else {
			asoc->state = SCTP_STATE_OPEN;
		}
		/* update RTO */
		SCTP_STAT_INCR_COUNTER32(sctps_activeestab);
		SCTP_STAT_INCR_GAUGE32(sctps_currestab);
		if (asoc->overall_error_count == 0) {
			net->RTO = sctp_calculate_rto(stcb, asoc, net,
			    &asoc->time_entered);
		}
		SCTP_GETTIME_TIMEVAL(&asoc->time_entered);
		sctp_ulp_notify(SCTP_NOTIFY_ASSOC_UP, stcb, 0, NULL);
		if ((stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
		    (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
			stcb->sctp_ep->sctp_flags |= SCTP_PCB_FLAGS_CONNECTED;
			soisconnected(stcb->sctp_ep->sctp_socket);
		}
		sctp_timer_start(SCTP_TIMER_TYPE_HEARTBEAT, stcb->sctp_ep,
		    stcb, net);
		/*
		 * since we did not send a HB make sure we don't double
		 * things
		 */
		net->hb_responded = 1;

		if (stcb->asoc.sctp_autoclose_ticks &&
		    sctp_is_feature_on(stcb->sctp_ep, SCTP_PCB_FLAGS_AUTOCLOSE)) {
			sctp_timer_start(SCTP_TIMER_TYPE_AUTOCLOSE,
			    stcb->sctp_ep, stcb, NULL);
		}
		/*
		 * set ASCONF timer if ASCONFs are pending and allowed (eg.
		 * addresses changed when init/cookie echo in flight)
		 */
		if ((sctp_is_feature_on(stcb->sctp_ep, SCTP_PCB_FLAGS_DO_ASCONF)) &&
		    (stcb->asoc.peer_supports_asconf) &&
		    (!TAILQ_EMPTY(&stcb->asoc.asconf_queue))) {
			sctp_timer_start(SCTP_TIMER_TYPE_ASCONF,
			    stcb->sctp_ep, stcb,
			    stcb->asoc.primary_destination);
		}
	}
	/* Toss the cookie if I can */
	sctp_toss_old_cookies(stcb, asoc);
	if (!TAILQ_EMPTY(&asoc->sent_queue)) {
		/* Restart the timer if we have pending data */
		struct sctp_tmit_chunk *chk;

		chk = TAILQ_FIRST(&asoc->sent_queue);
		if (chk) {
			sctp_timer_start(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
			    stcb, chk->whoTo);
		}
	}
}

static void
sctp_handle_ecn_echo(struct sctp_ecne_chunk *cp,
    struct sctp_tcb *stcb)
{
	struct sctp_nets *net;
	struct sctp_tmit_chunk *lchk;
	uint32_t tsn;

	if (ntohs(cp->ch.chunk_length) != sizeof(struct sctp_ecne_chunk)) {
		return;
	}
	SCTP_STAT_INCR(sctps_recvecne);
	tsn = ntohl(cp->tsn);
	/* ECN Nonce stuff: need a resync and disable the nonce sum check */
	/* Also we make sure we disable the nonce_wait */
	lchk = TAILQ_FIRST(&stcb->asoc.send_queue);
	if (lchk == NULL) {
		stcb->asoc.nonce_resync_tsn = stcb->asoc.sending_seq;
	} else {
		stcb->asoc.nonce_resync_tsn = lchk->rec.data.TSN_seq;
	}
	stcb->asoc.nonce_wait_for_ecne = 0;
	stcb->asoc.nonce_sum_check = 0;

	/* Find where it was sent, if possible */
	net = NULL;
	lchk = TAILQ_FIRST(&stcb->asoc.sent_queue);
	while (lchk) {
		if (lchk->rec.data.TSN_seq == tsn) {
			net = lchk->whoTo;
			break;
		}
		if (compare_with_wrap(lchk->rec.data.TSN_seq, tsn, MAX_SEQ))
			break;
		lchk = TAILQ_NEXT(lchk, sctp_next);
	}
	if (net == NULL)
		/* default is we use the primary */
		net = stcb->asoc.primary_destination;

	if (compare_with_wrap(tsn, stcb->asoc.last_cwr_tsn, MAX_TSN)) {
#ifdef SCTP_CWND_MONITOR
		int old_cwnd;

		old_cwnd = net->cwnd;
#endif
		SCTP_STAT_INCR(sctps_ecnereducedcwnd);
		net->ssthresh = net->cwnd / 2;
		if (net->ssthresh < net->mtu) {
			net->ssthresh = net->mtu;
			/* here back off the timer as well, to slow us down */
			net->RTO <<= 2;
		}
		net->cwnd = net->ssthresh;
#ifdef SCTP_CWND_MONITOR
		sctp_log_cwnd(stcb, net, (net->cwnd - old_cwnd), SCTP_CWND_LOG_FROM_SAT);
#endif
		/*
		 * we reduce once every RTT. So we will only lower cwnd at
		 * the next sending seq i.e. the resync_tsn.
		 */
		stcb->asoc.last_cwr_tsn = stcb->asoc.nonce_resync_tsn;
	}
	/*
	 * We always send a CWR this way if our previous one was lost our
	 * peer will get an update, or if it is not time again to reduce we
	 * still get the cwr to the peer.
	 */
	sctp_send_cwr(stcb, net, tsn);
}

static void
sctp_handle_ecn_cwr(struct sctp_cwr_chunk *cp, struct sctp_tcb *stcb)
{
	/*
	 * Here we get a CWR from the peer. We must look in the outqueue and
	 * make sure that we have a covered ECNE in teh control chunk part.
	 * If so remove it.
	 */
	struct sctp_tmit_chunk *chk;
	struct sctp_ecne_chunk *ecne;

	TAILQ_FOREACH(chk, &stcb->asoc.control_send_queue, sctp_next) {
		if (chk->rec.chunk_id.id != SCTP_ECN_ECHO) {
			continue;
		}
		/*
		 * Look for and remove if it is the right TSN. Since there
		 * is only ONE ECNE on the control queue at any one time we
		 * don't need to worry about more than one!
		 */
		ecne = mtod(chk->data, struct sctp_ecne_chunk *);
		if (compare_with_wrap(ntohl(cp->tsn), ntohl(ecne->tsn),
		    MAX_TSN) || (cp->tsn == ecne->tsn)) {
			/* this covers this ECNE, we can remove it */
			stcb->asoc.ecn_echo_cnt_onq--;
			TAILQ_REMOVE(&stcb->asoc.control_send_queue, chk,
			    sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			stcb->asoc.ctrl_queue_cnt--;
			sctp_free_remote_addr(chk->whoTo);
			sctp_free_a_chunk(stcb, chk);
			break;
		}
	}
}

static void
sctp_handle_shutdown_complete(struct sctp_shutdown_complete_chunk *cp,
    struct sctp_tcb *stcb, struct sctp_nets *net)
{
	struct sctp_association *asoc;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
		printf("sctp_handle_shutdown_complete: handling SHUTDOWN-COMPLETE\n");
	}
#endif
	if (stcb == NULL)
		return;

	asoc = &stcb->asoc;
	/* process according to association state */
	if (SCTP_GET_STATE(asoc) != SCTP_STATE_SHUTDOWN_ACK_SENT) {
		/* unexpected SHUTDOWN-COMPLETE... so ignore... */
		SCTP_TCB_UNLOCK(stcb);
		return;
	}
	/* notify upper layer protocol */
	if (stcb->sctp_socket) {
		sctp_ulp_notify(SCTP_NOTIFY_ASSOC_DOWN, stcb, 0, NULL);
		/* are the queues empty? they should be */
		if (!TAILQ_EMPTY(&asoc->send_queue) ||
		    !TAILQ_EMPTY(&asoc->sent_queue) ||
		    !TAILQ_EMPTY(&asoc->out_wheel)) {
			sctp_report_all_outbound(stcb);
		}
	}
	/* stop the timer */
	sctp_timer_stop(SCTP_TIMER_TYPE_SHUTDOWN, stcb->sctp_ep, stcb, net);
	SCTP_STAT_INCR_COUNTER32(sctps_shutdown);
	/* free the TCB */
	sctp_free_assoc(stcb->sctp_ep, stcb, 0);
	return;
}

static int
process_chunk_drop(struct sctp_tcb *stcb, struct sctp_chunk_desc *desc,
    struct sctp_nets *net, uint8_t flg)
{
	switch (desc->chunk_type) {
		case SCTP_DATA:
		/* find the tsn to resend (possibly */
		{
			uint32_t tsn;
			struct sctp_tmit_chunk *tp1;

			tsn = ntohl(desc->tsn_ifany);
			tp1 = TAILQ_FIRST(&stcb->asoc.sent_queue);
			while (tp1) {
				if (tp1->rec.data.TSN_seq == tsn) {
					/* found it */
					break;
				}
				if (compare_with_wrap(tp1->rec.data.TSN_seq, tsn,
				    MAX_TSN)) {
					/* not found */
					tp1 = NULL;
					break;
				}
				tp1 = TAILQ_NEXT(tp1, sctp_next);
			}
			if (tp1 == NULL) {
				/*
				 * Do it the other way , aka without paying
				 * attention to queue seq order.
				 */
				SCTP_STAT_INCR(sctps_pdrpdnfnd);
				tp1 = TAILQ_FIRST(&stcb->asoc.sent_queue);
				while (tp1) {
					if (tp1->rec.data.TSN_seq == tsn) {
						/* found it */
						break;
					}
					tp1 = TAILQ_NEXT(tp1, sctp_next);
				}
			}
			if (tp1 == NULL) {
				SCTP_STAT_INCR(sctps_pdrptsnnf);
			}
			if ((tp1) && (tp1->sent < SCTP_DATAGRAM_ACKED)) {
				uint8_t *ddp;

				if ((stcb->asoc.peers_rwnd == 0) &&
				    ((flg & SCTP_FROM_MIDDLE_BOX) == 0)) {
					SCTP_STAT_INCR(sctps_pdrpdiwnp);
					return (0);
				}
				if (stcb->asoc.peers_rwnd == 0 &&
				    (flg & SCTP_FROM_MIDDLE_BOX)) {
					SCTP_STAT_INCR(sctps_pdrpdizrw);
					return (0);
				}
				ddp = (uint8_t *) (mtod(tp1->data, caddr_t)+
				    sizeof(struct sctp_data_chunk));
				{
					unsigned int iii;

					for (iii = 0; iii < sizeof(desc->data_bytes);
					    iii++) {
						if (ddp[iii] != desc->data_bytes[iii]) {
							SCTP_STAT_INCR(sctps_pdrpbadd);
							return (-1);
						}
					}
				}
				/*
				 * We zero out the nonce so resync not
				 * needed
				 */
				tp1->rec.data.ect_nonce = 0;

				if (tp1->do_rtt) {
					/*
					 * this guy had a RTO calculation
					 * pending on it, cancel it
					 */
					tp1->whoTo->rto_pending = 0;
					tp1->do_rtt = 0;
				}
				SCTP_STAT_INCR(sctps_pdrpmark);
				if (tp1->sent != SCTP_DATAGRAM_RESEND)
					sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
				tp1->sent = SCTP_DATAGRAM_RESEND;
				/*
				 * mark it as if we were doing a FR, since
				 * we will be getting gap ack reports behind
				 * the info from the router.
				 */
				tp1->rec.data.doing_fast_retransmit = 1;
				/*
				 * mark the tsn with what sequences can
				 * cause a new FR.
				 */
				if (TAILQ_EMPTY(&stcb->asoc.send_queue)) {
					tp1->rec.data.fast_retran_tsn = stcb->asoc.sending_seq;
				} else {
					tp1->rec.data.fast_retran_tsn = (TAILQ_FIRST(&stcb->asoc.send_queue))->rec.data.TSN_seq;
				}

				/* restart the timer */
				sctp_timer_stop(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, tp1->whoTo);
				sctp_timer_start(SCTP_TIMER_TYPE_SEND, stcb->sctp_ep,
				    stcb, tp1->whoTo);

				/* fix counts and things */
				if (tp1->whoTo->flight_size >= tp1->book_size)
					tp1->whoTo->flight_size -= tp1->book_size;
				else
					tp1->whoTo->flight_size = 0;

				if (stcb->asoc.total_flight >= tp1->book_size) {
					stcb->asoc.total_flight -= tp1->book_size;
					if (stcb->asoc.total_flight_count > 0)
						stcb->asoc.total_flight_count--;
				} else {
					stcb->asoc.total_flight = 0;
					stcb->asoc.total_flight_count = 0;
				}
				tp1->snd_count--;
			} {
				/* audit code */
				unsigned int audit;

				audit = 0;
				TAILQ_FOREACH(tp1, &stcb->asoc.sent_queue, sctp_next) {
					if (tp1->sent == SCTP_DATAGRAM_RESEND)
						audit++;
				}
				TAILQ_FOREACH(tp1, &stcb->asoc.control_send_queue,
				    sctp_next) {
					if (tp1->sent == SCTP_DATAGRAM_RESEND)
						audit++;
				}
				if (audit != stcb->asoc.sent_queue_retran_cnt) {
					printf("**Local Audit finds cnt:%d asoc cnt:%d\n",
					    audit, stcb->asoc.sent_queue_retran_cnt);
#ifndef SCTP_AUDITING_ENABLED
					stcb->asoc.sent_queue_retran_cnt = audit;
#endif
				}
			}
		}
		break;
	case SCTP_ASCONF:
		{
			struct sctp_tmit_chunk *asconf;

			TAILQ_FOREACH(asconf, &stcb->asoc.control_send_queue,
			    sctp_next) {
				if (asconf->rec.chunk_id.id == SCTP_ASCONF) {
					break;
				}
			}
			if (asconf) {
				if (asconf->sent != SCTP_DATAGRAM_RESEND)
					sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
				asconf->sent = SCTP_DATAGRAM_RESEND;
				asconf->snd_count--;
			}
		}
		break;
	case SCTP_INITIATION:
		/* resend the INIT */
		stcb->asoc.dropped_special_cnt++;
		if (stcb->asoc.dropped_special_cnt < SCTP_RETRY_DROPPED_THRESH) {
			/*
			 * If we can get it in, in a few attempts we do
			 * this, otherwise we let the timer fire.
			 */
			sctp_timer_stop(SCTP_TIMER_TYPE_INIT, stcb->sctp_ep,
			    stcb, net);
			sctp_send_initiate(stcb->sctp_ep, stcb);
		}
		break;
	case SCTP_SELECTIVE_ACK:
		/* resend the sack */
		sctp_send_sack(stcb);
		break;
	case SCTP_HEARTBEAT_REQUEST:
		/* resend a demand HB */
		sctp_send_hb(stcb, 1, net);
		break;
	case SCTP_SHUTDOWN:
		sctp_send_shutdown(stcb, net);
		break;
	case SCTP_SHUTDOWN_ACK:
		sctp_send_shutdown_ack(stcb, net);
		break;
	case SCTP_COOKIE_ECHO:
		{
			struct sctp_tmit_chunk *cookie;

			cookie = NULL;
			TAILQ_FOREACH(cookie, &stcb->asoc.control_send_queue,
			    sctp_next) {
				if (cookie->rec.chunk_id.id == SCTP_COOKIE_ECHO) {
					break;
				}
			}
			if (cookie) {
				if (cookie->sent != SCTP_DATAGRAM_RESEND)
					sctp_ucount_incr(stcb->asoc.sent_queue_retran_cnt);
				cookie->sent = SCTP_DATAGRAM_RESEND;
				sctp_stop_all_cookie_timers(stcb);
			}
		}
		break;
	case SCTP_COOKIE_ACK:
		sctp_send_cookie_ack(stcb);
		break;
	case SCTP_ASCONF_ACK:
		/* resend last asconf ack */
		sctp_send_asconf_ack(stcb, 1);
		break;
	case SCTP_FORWARD_CUM_TSN:
		send_forward_tsn(stcb, &stcb->asoc);
		break;
		/* can't do anything with these */
	case SCTP_PACKET_DROPPED:
	case SCTP_INITIATION_ACK:	/* this should not happen */
	case SCTP_HEARTBEAT_ACK:
	case SCTP_ABORT_ASSOCIATION:
	case SCTP_OPERATION_ERROR:
	case SCTP_SHUTDOWN_COMPLETE:
	case SCTP_ECN_ECHO:
	case SCTP_ECN_CWR:
	default:
		break;
	}
	return (0);
}

void
sctp_reset_in_stream(struct sctp_tcb *stcb, int number_entries, uint16_t * list)
{
	int i;
	uint16_t temp;

	/*
	 * We set things to 0xffff since this is the last delivered sequence
	 * and we will be sending in 0 after the reset.
	 */

	if (number_entries) {
		for (i = 0; i < number_entries; i++) {
			temp = ntohs(list[i]);
			if (temp >= stcb->asoc.streamincnt) {
				continue;
			}
			stcb->asoc.strmin[temp].last_sequence_delivered = 0xffff;
		}
	} else {
		list = NULL;
		for (i = 0; i < stcb->asoc.streamincnt; i++) {
			stcb->asoc.strmin[i].last_sequence_delivered = 0xffff;
		}
	}
	sctp_ulp_notify(SCTP_NOTIFY_STR_RESET_RECV, stcb, number_entries, (void *)list);
}

static void
sctp_reset_out_streams(struct sctp_tcb *stcb, int number_entries, uint16_t * list)
{
	int i;

	if (number_entries == 0) {
		for (i = 0; i < stcb->asoc.streamoutcnt; i++) {
			stcb->asoc.strmout[i].next_sequence_sent = 0;
		}
	} else if (number_entries) {
		for (i = 0; i < number_entries; i++) {
			uint16_t temp;

			temp = ntohs(list[i]);
			if (temp >= stcb->asoc.streamoutcnt) {
				/* no such stream */
				continue;
			}
			stcb->asoc.strmout[temp].next_sequence_sent = 0;
		}
	}
	sctp_ulp_notify(SCTP_NOTIFY_STR_RESET_SEND, stcb, number_entries, (void *)list);
}


struct sctp_stream_reset_out_request *
sctp_find_stream_reset(struct sctp_tcb *stcb, uint32_t seq, struct sctp_tmit_chunk **bchk)
{
	struct sctp_association *asoc;
	struct sctp_stream_reset_out_req *req;
	struct sctp_stream_reset_out_request *r;
	struct sctp_tmit_chunk *chk;
	int len, clen;

	asoc = &stcb->asoc;
	if (TAILQ_EMPTY(&stcb->asoc.control_send_queue)) {
		return (NULL);
	}
	if (stcb->asoc.str_reset == NULL) {
		return (NULL);
	}
	chk = stcb->asoc.str_reset;
	if (chk->data == NULL) {
		return (NULL);
	}
	if (bchk) {
		/* he wants a copy of the chk pointer */
		*bchk = chk;
	}
	clen = chk->send_size;
	req = mtod(chk->data, struct sctp_stream_reset_out_req *);
	r = &req->sr_req;
	if (ntohl(r->request_seq) == seq) {
		/* found it */
		return (r);
	}
	len = SCTP_SIZE32(ntohs(r->ph.param_length));
	if (clen > (len + (int)sizeof(struct sctp_chunkhdr))) {
		/* move to the next one, there can only be a max of two */
		r = (struct sctp_stream_reset_out_request *)((caddr_t)r + len);
		if (ntohl(r->request_seq) == seq) {
			return (r);
		}
	}
	/* that seq is not here */
	return (NULL);
}

static void
sctp_clean_up_stream_reset(struct sctp_tcb *stcb)
{
	struct sctp_association *asoc;

	asoc = &stcb->asoc;
	struct sctp_tmit_chunk *chk = stcb->asoc.str_reset;

	if (stcb->asoc.str_reset == NULL) {
		return;
	}
	sctp_timer_stop(SCTP_TIMER_TYPE_STRRESET, stcb->sctp_ep, stcb, chk->whoTo);
	TAILQ_REMOVE(&asoc->control_send_queue,
	    chk,
	    sctp_next);
	if (chk->data) {
		sctp_m_freem(chk->data);
		chk->data = NULL;
	}
	asoc->ctrl_queue_cnt--;
	sctp_free_remote_addr(chk->whoTo);

	sctp_free_a_chunk(stcb, chk);
	stcb->asoc.str_reset = NULL;
}


static int
sctp_handle_stream_reset_response(struct sctp_tcb *stcb,
    uint32_t seq, uint32_t action,
    struct sctp_stream_reset_response *respin
)
{
	uint16_t type;
	int lparm_len;
	struct sctp_association *asoc = &stcb->asoc;
	struct sctp_tmit_chunk *chk;
	struct sctp_stream_reset_out_request *srparam;
	int number_entries;

	if (asoc->stream_reset_outstanding == 0) {
		/* duplicate */
		return (0);
	}
	if (seq == stcb->asoc.str_reset_seq_out) {
		srparam = sctp_find_stream_reset(stcb, seq, &chk);
		if (srparam) {
			stcb->asoc.str_reset_seq_out++;
			type = ntohs(srparam->ph.param_type);
			lparm_len = ntohs(srparam->ph.param_length);
			number_entries = (lparm_len - sizeof(struct sctp_stream_reset_out_request)) / sizeof(uint16_t);
			if (type == SCTP_STR_RESET_OUT_REQUEST) {
				asoc->stream_reset_out_is_outstanding = 0;
				if (asoc->stream_reset_outstanding)
					asoc->stream_reset_outstanding--;
				if (action == SCTP_STREAM_RESET_PERFORMED) {
					/* do it */
					sctp_reset_out_streams(stcb, number_entries, srparam->list_of_streams);
				} else {
					sctp_ulp_notify(SCTP_NOTIFY_STR_RESET_FAILED_OUT, stcb, number_entries, srparam->list_of_streams);
				}
			} else if (type == SCTP_STR_RESET_IN_REQUEST) {
				/* Answered my request */
				if (asoc->stream_reset_outstanding)
					asoc->stream_reset_outstanding--;
				if (action != SCTP_STREAM_RESET_PERFORMED) {
					sctp_ulp_notify(SCTP_NOTIFY_STR_RESET_FAILED_IN, stcb, number_entries, srparam->list_of_streams);
				}
			} else if (type == SCTP_STR_RESET_TSN_REQUEST) {
				/**
				 * a) Adopt the new in tsn.
				 * b) reset the map
				 * c) Adopt the new out-tsn
				 */
				struct sctp_stream_reset_response_tsn *resp;
				struct sctp_forward_tsn_chunk fwdtsn;
				int abort_flag = 0;

				if (respin == NULL) {
					/* huh ? */
					return (0);
				}
				if (action == SCTP_STREAM_RESET_PERFORMED) {
					resp = (struct sctp_stream_reset_response_tsn *)respin;
					asoc->stream_reset_outstanding--;
					fwdtsn.ch.chunk_length = htons(sizeof(struct sctp_forward_tsn_chunk));
					fwdtsn.ch.chunk_type = SCTP_FORWARD_CUM_TSN;
					fwdtsn.new_cumulative_tsn = htonl(ntohl(resp->senders_next_tsn) - 1);
					sctp_handle_forward_tsn(stcb, &fwdtsn, &abort_flag);
					if (abort_flag) {
						return (1);
					}
					stcb->asoc.highest_tsn_inside_map = (ntohl(resp->senders_next_tsn) - 1);
					stcb->asoc.cumulative_tsn = stcb->asoc.highest_tsn_inside_map;
					stcb->asoc.mapping_array_base_tsn = ntohl(resp->senders_next_tsn);
					memset(stcb->asoc.mapping_array, 0, stcb->asoc.mapping_array_size);
					stcb->asoc.sending_seq = ntohl(resp->receivers_next_tsn);
					stcb->asoc.last_acked_seq = stcb->asoc.cumulative_tsn;

					sctp_reset_out_streams(stcb, 0, (uint16_t *) NULL);
					sctp_reset_in_stream(stcb, 0, (uint16_t *) NULL);

				}
			}
			/* get rid of the request and get the request flags */
			if (asoc->stream_reset_outstanding == 0) {
				sctp_clean_up_stream_reset(stcb);
			}
		}
	}
	return (0);
}

static void
sctp_handle_str_reset_request_in(struct sctp_tcb *stcb,
    struct sctp_tmit_chunk *chk,
    struct sctp_stream_reset_in_request *req)
{
	uint32_t seq;
	int len, i;
	int number_entries;
	uint16_t temp;

	/*
	 * peer wants me to send a str-reset to him for my outgoing seq's if
	 * seq_in is right.
	 */
	struct sctp_association *asoc = &stcb->asoc;

	seq = ntohl(req->request_seq);
	if (asoc->str_reset_seq_in == seq) {
		if (stcb->asoc.stream_reset_out_is_outstanding == 0) {
			len = ntohs(req->ph.param_length);
			number_entries = ((len - sizeof(struct sctp_stream_reset_in_request)) / sizeof(uint16_t));
			for (i = 0; i < number_entries; i++) {
				temp = ntohs(req->list_of_streams[i]);
				req->list_of_streams[i] = temp;
			}
			/* move the reset action back one */
			asoc->last_reset_action[1] = asoc->last_reset_action[0];
			asoc->last_reset_action[0] = SCTP_STREAM_RESET_PERFORMED;
			sctp_add_stream_reset_out(chk, number_entries, req->list_of_streams,
			    asoc->str_reset_seq_out,
			    seq, (asoc->sending_seq - 1));
			asoc->stream_reset_out_is_outstanding = 1;
			asoc->str_reset = chk;
			sctp_timer_start(SCTP_TIMER_TYPE_STRRESET, stcb->sctp_ep, stcb, chk->whoTo);
			stcb->asoc.stream_reset_outstanding++;
		} else {
			/* Can't do it, since we have sent one out */
			asoc->last_reset_action[1] = asoc->last_reset_action[0];
			asoc->last_reset_action[0] = SCTP_STREAM_RESET_TRY_LATER;
			sctp_add_stream_reset_result(chk, seq, asoc->last_reset_action[0]);
		}
		asoc->str_reset_seq_in++;
	} else if (asoc->str_reset_seq_in - 1 == seq) {
		sctp_add_stream_reset_result(chk, seq, asoc->last_reset_action[0]);
	} else if (asoc->str_reset_seq_in - 2 == seq) {
		sctp_add_stream_reset_result(chk, seq, asoc->last_reset_action[1]);
	} else {
		sctp_add_stream_reset_result(chk, seq, SCTP_STREAM_RESET_BAD_SEQNO);
	}
}

static int
sctp_handle_str_reset_request_tsn(struct sctp_tcb *stcb,
    struct sctp_tmit_chunk *chk,
    struct sctp_stream_reset_tsn_request *req)
{
	/* reset all in and out and update the tsn */
	/*
	 * A) reset my str-seq's on in and out. B) Select a receive next,
	 * and set cum-ack to it. Also process this selected number as a
	 * fwd-tsn as well. C) set in the response my next sending seq.
	 */
	struct sctp_forward_tsn_chunk fwdtsn;
	struct sctp_association *asoc = &stcb->asoc;
	int abort_flag = 0;
	uint32_t seq;

	seq = ntohl(req->request_seq);
	if (asoc->str_reset_seq_in == seq) {
		fwdtsn.ch.chunk_length = htons(sizeof(struct sctp_forward_tsn_chunk));
		fwdtsn.ch.chunk_type = SCTP_FORWARD_CUM_TSN;
		fwdtsn.ch.chunk_flags = 0;
		fwdtsn.new_cumulative_tsn = htonl(stcb->asoc.highest_tsn_inside_map + 1);
		sctp_handle_forward_tsn(stcb, &fwdtsn, &abort_flag);
		if (abort_flag) {
			return (1);
		}
		stcb->asoc.highest_tsn_inside_map += SCTP_STREAM_RESET_TSN_DELTA;
		stcb->asoc.cumulative_tsn = stcb->asoc.highest_tsn_inside_map;
		stcb->asoc.mapping_array_base_tsn = stcb->asoc.highest_tsn_inside_map + 1;
		memset(stcb->asoc.mapping_array, 0, stcb->asoc.mapping_array_size);
		atomic_add_int(&stcb->asoc.sending_seq, 1);
		/* save off historical data for retrans */
		stcb->asoc.last_sending_seq[1] = stcb->asoc.last_sending_seq[0];
		stcb->asoc.last_sending_seq[0] = stcb->asoc.sending_seq;
		stcb->asoc.last_base_tsnsent[1] = stcb->asoc.last_base_tsnsent[0];
		stcb->asoc.last_base_tsnsent[0] = stcb->asoc.mapping_array_base_tsn;

		sctp_add_stream_reset_result_tsn(chk,
		    ntohl(req->request_seq),
		    SCTP_STREAM_RESET_PERFORMED,
		    stcb->asoc.sending_seq,
		    stcb->asoc.mapping_array_base_tsn);
		sctp_reset_out_streams(stcb, 0, (uint16_t *) NULL);
		sctp_reset_in_stream(stcb, 0, (uint16_t *) NULL);
		stcb->asoc.last_reset_action[1] = stcb->asoc.last_reset_action[0];
		stcb->asoc.last_reset_action[0] = SCTP_STREAM_RESET_PERFORMED;

		asoc->str_reset_seq_in++;
	} else if (asoc->str_reset_seq_in - 1 == seq) {
		sctp_add_stream_reset_result_tsn(chk, seq, asoc->last_reset_action[0],
		    stcb->asoc.last_sending_seq[0],
		    stcb->asoc.last_base_tsnsent[0]
		    );
	} else if (asoc->str_reset_seq_in - 2 == seq) {
		sctp_add_stream_reset_result_tsn(chk, seq, asoc->last_reset_action[1],
		    stcb->asoc.last_sending_seq[1],
		    stcb->asoc.last_base_tsnsent[1]
		    );
	} else {
		sctp_add_stream_reset_result(chk, seq, SCTP_STREAM_RESET_BAD_SEQNO);
	}
	return (0);
}

static void
sctp_handle_str_reset_request_out(struct sctp_tcb *stcb,
    struct sctp_tmit_chunk *chk,
    struct sctp_stream_reset_out_request *req)
{
	uint32_t seq, tsn;
	int number_entries, len;
	struct sctp_association *asoc = &stcb->asoc;

	seq = ntohl(req->request_seq);

	/* now if its not a duplicate we process it */
	if (asoc->str_reset_seq_in == seq) {
		len = ntohs(req->ph.param_length);
		number_entries = ((len - sizeof(struct sctp_stream_reset_out_request)) / sizeof(uint16_t));
		/*
		 * the sender is resetting, handle the list issue.. we must
		 * a) verify if we can do the reset, if so no problem b) If
		 * we can't do the reset we must copy the request. c) queue
		 * it, and setup the data in processor to trigger it off
		 * when needed and dequeue all the queued data.
		 */
		tsn = ntohl(req->send_reset_at_tsn);

		/* move the reset action back one */
		asoc->last_reset_action[1] = asoc->last_reset_action[0];
		if ((tsn == asoc->cumulative_tsn) ||
		    (compare_with_wrap(asoc->cumulative_tsn, tsn, MAX_TSN))) {
			/* we can do it now */
			sctp_reset_in_stream(stcb, number_entries, req->list_of_streams);
			sctp_add_stream_reset_result(chk, seq, SCTP_STREAM_RESET_PERFORMED);
			asoc->last_reset_action[0] = SCTP_STREAM_RESET_PERFORMED;
		} else {
			/*
			 * we must queue it up and thus wait for the TSN's
			 * to arrive that are at or before tsn
			 */
			struct sctp_stream_reset_list *liste;
			int siz;

			siz = sizeof(struct sctp_stream_reset_list) + (number_entries * sizeof(uint16_t));
			SCTP_MALLOC(liste, struct sctp_stream_reset_list *,
			    siz, "StrRstList");
			if (liste == NULL) {
				/* gak out of memory */
				sctp_add_stream_reset_result(chk, seq, SCTP_STREAM_RESET_DENIED);
				asoc->last_reset_action[0] = SCTP_STREAM_RESET_DENIED;
				return;
			}
			liste->tsn = tsn;
			liste->number_entries = number_entries;
			memcpy(&liste->req, req,
			    (sizeof(struct sctp_stream_reset_out_request) + (number_entries * sizeof(uint16_t))));
			TAILQ_INSERT_TAIL(&asoc->resetHead, liste, next_resp);
			sctp_add_stream_reset_result(chk, seq, SCTP_STREAM_RESET_PERFORMED);
			asoc->last_reset_action[0] = SCTP_STREAM_RESET_PERFORMED;
		}
		asoc->str_reset_seq_in++;
	} else if ((asoc->str_reset_seq_in - 1) == seq) {
		/*
		 * one seq back, just echo back last action since my
		 * response was lost.
		 */
		sctp_add_stream_reset_result(chk, seq, asoc->last_reset_action[0]);
	} else if ((asoc->str_reset_seq_in - 2) == seq) {
		/*
		 * two seq back, just echo back last action since my
		 * response was lost.
		 */
		sctp_add_stream_reset_result(chk, seq, asoc->last_reset_action[1]);
	} else {
		sctp_add_stream_reset_result(chk, seq, SCTP_STREAM_RESET_BAD_SEQNO);
	}
}

static int
sctp_handle_stream_reset(struct sctp_tcb *stcb, struct sctp_stream_reset_out_req *sr_req)
{
	int chk_length, param_len, ptype;
	uint32_t seq;
	int num_req = 0;
	struct sctp_tmit_chunk *chk;
	struct sctp_chunkhdr *ch;
	struct sctp_paramhdr *ph;

	/* now it may be a reset or a reset-response */
	chk_length = ntohs(sr_req->ch.chunk_length);
	int ret_code = 0;
	int num_param = 0;

	/* setup for adding the response */
	sctp_alloc_a_chunk(stcb, chk);
	if (chk == NULL) {
		return (ret_code);
	}
	chk->rec.chunk_id.id = SCTP_STREAM_RESET;
	chk->asoc = &stcb->asoc;
	chk->no_fr_allowed = 0;
	chk->book_size = chk->send_size = sizeof(struct sctp_chunkhdr);
	chk->data = sctp_get_mbuf_for_msg(MCLBYTES, 1, M_DONTWAIT, 1, MT_DATA);
	if (chk->data == NULL) {
strres_nochunk:
		if (chk->data) {
			sctp_m_freem(chk->data);
			chk->data = NULL;
		}
		sctp_free_a_chunk(stcb, chk);
		return (ret_code);
	}
	chk->data->m_data += SCTP_MIN_OVERHEAD;

	/* setup chunk parameters */
	chk->sent = SCTP_DATAGRAM_UNSENT;
	chk->snd_count = 0;
	chk->whoTo = stcb->asoc.primary_destination;
	atomic_add_int(&chk->whoTo->ref_count, 1);

	ch = mtod(chk->data, struct sctp_chunkhdr *);
	ch->chunk_type = SCTP_STREAM_RESET;
	ch->chunk_flags = 0;
	ch->chunk_length = htons(chk->send_size);
	chk->data->m_pkthdr.len = chk->data->m_len = SCTP_SIZE32(chk->send_size);


	ph = (struct sctp_paramhdr *)&sr_req->sr_req;
	while ((size_t)chk_length >= sizeof(struct sctp_stream_reset_tsn_request)) {
		param_len = ntohs(ph->param_length);
		if (param_len < (int)sizeof(struct sctp_stream_reset_tsn_request)) {
			/* bad param */
			break;
		}
		ptype = ntohs(ph->param_type);
		num_param++;
		if (num_param > SCTP_MAX_RESET_PARAMS) {
			/* hit the max of parameters already sorry.. */
			break;
		}
		if (ptype == SCTP_STR_RESET_OUT_REQUEST) {
			struct sctp_stream_reset_out_request *req_out;

			req_out = (struct sctp_stream_reset_out_request *)ph;
			num_req++;
			if (stcb->asoc.stream_reset_outstanding) {
				seq = ntohl(req_out->response_seq);
				if (seq == stcb->asoc.str_reset_seq_out) {
					/* implicit ack */
					sctp_handle_stream_reset_response(stcb, seq, SCTP_STREAM_RESET_PERFORMED, NULL);
				}
			}
			sctp_handle_str_reset_request_out(stcb, chk, req_out);
		} else if (ptype == SCTP_STR_RESET_IN_REQUEST) {
			struct sctp_stream_reset_in_request *req_in;

			num_req++;
			req_in = (struct sctp_stream_reset_in_request *)ph;
			sctp_handle_str_reset_request_in(stcb, chk, req_in);
		} else if (ptype == SCTP_STR_RESET_TSN_REQUEST) {
			struct sctp_stream_reset_tsn_request *req_tsn;

			num_req++;
			req_tsn = (struct sctp_stream_reset_tsn_request *)ph;
			if (sctp_handle_str_reset_request_tsn(stcb, chk, req_tsn)) {
				ret_code = 1;
				goto strres_nochunk;
			}
			/* no more */
			break;
		} else if (ptype == SCTP_STR_RESET_RESPONSE) {
			struct sctp_stream_reset_response *resp;
			uint32_t result;

			resp = (struct sctp_stream_reset_response *)ph;
			seq = ntohl(resp->response_seq);
			result = ntohl(resp->result);
			if (sctp_handle_stream_reset_response(stcb, seq, result, resp)) {
				ret_code = 1;
				goto strres_nochunk;
			}
		} else {
			break;
		}

		ph = (struct sctp_paramhdr *)((caddr_t)ph + SCTP_SIZE32(param_len));
		chk_length -= SCTP_SIZE32(param_len);
	}
	if (num_req == 0) {
		/* we have no response free the stuff */
		goto strres_nochunk;
	}
	/* ok we have a chunk to link in */
	TAILQ_INSERT_TAIL(&stcb->asoc.control_send_queue,
	    chk,
	    sctp_next);
	stcb->asoc.ctrl_queue_cnt++;
	return (ret_code);
}

/*
 * Handle a router or endpoints report of a packet loss, there are two ways
 * to handle this, either we get the whole packet and must disect it
 * ourselves (possibly with truncation and or corruption) or it is a summary
 * from a middle box that did the disectting for us.
 */
static void
sctp_handle_packet_dropped(struct sctp_pktdrop_chunk *cp,
    struct sctp_tcb *stcb, struct sctp_nets *net)
{
	uint32_t bottle_bw, on_queue;
	uint16_t trunc_len;
	unsigned int chlen;
	unsigned int at;
	struct sctp_chunk_desc desc;
	struct sctp_chunkhdr *ch;

	chlen = ntohs(cp->ch.chunk_length);
	chlen -= sizeof(struct sctp_pktdrop_chunk);
	/* XXX possible chlen underflow */
	if (chlen == 0) {
		ch = NULL;
		if (cp->ch.chunk_flags & SCTP_FROM_MIDDLE_BOX)
			SCTP_STAT_INCR(sctps_pdrpbwrpt);
	} else {
		ch = (struct sctp_chunkhdr *)(cp->data + sizeof(struct sctphdr));
		chlen -= sizeof(struct sctphdr);
		/* XXX possible chlen underflow */
		memset(&desc, 0, sizeof(desc));
	}
	trunc_len = (uint16_t) ntohs(cp->trunc_len);
	/* now the chunks themselves */
	while ((ch != NULL) && (chlen >= sizeof(struct sctp_chunkhdr))) {
		desc.chunk_type = ch->chunk_type;
		/* get amount we need to move */
		at = ntohs(ch->chunk_length);
		if (at < sizeof(struct sctp_chunkhdr)) {
			/* corrupt chunk, maybe at the end? */
			SCTP_STAT_INCR(sctps_pdrpcrupt);
			break;
		}
		if (trunc_len == 0) {
			/* we are supposed to have all of it */
			if (at > chlen) {
				/* corrupt skip it */
				SCTP_STAT_INCR(sctps_pdrpcrupt);
				break;
			}
		} else {
			/* is there enough of it left ? */
			if (desc.chunk_type == SCTP_DATA) {
				if (chlen < (sizeof(struct sctp_data_chunk) +
				    sizeof(desc.data_bytes))) {
					break;
				}
			} else {
				if (chlen < sizeof(struct sctp_chunkhdr)) {
					break;
				}
			}
		}
		if (desc.chunk_type == SCTP_DATA) {
			/* can we get out the tsn? */
			if ((cp->ch.chunk_flags & SCTP_FROM_MIDDLE_BOX))
				SCTP_STAT_INCR(sctps_pdrpmbda);

			if (chlen >= (sizeof(struct sctp_data_chunk) + sizeof(uint32_t))) {
				/* yep */
				struct sctp_data_chunk *dcp;
				uint8_t *ddp;
				unsigned int iii;

				dcp = (struct sctp_data_chunk *)ch;
				ddp = (uint8_t *) (dcp + 1);
				for (iii = 0; iii < sizeof(desc.data_bytes); iii++) {
					desc.data_bytes[iii] = ddp[iii];
				}
				desc.tsn_ifany = dcp->dp.tsn;
			} else {
				/* nope we are done. */
				SCTP_STAT_INCR(sctps_pdrpnedat);
				break;
			}
		} else {
			if ((cp->ch.chunk_flags & SCTP_FROM_MIDDLE_BOX))
				SCTP_STAT_INCR(sctps_pdrpmbct);
		}

		if (process_chunk_drop(stcb, &desc, net, cp->ch.chunk_flags)) {
			SCTP_STAT_INCR(sctps_pdrppdbrk);
			break;
		}
		if (SCTP_SIZE32(at) > chlen) {
			break;
		}
		chlen -= SCTP_SIZE32(at);
		if (chlen < sizeof(struct sctp_chunkhdr)) {
			/* done, none left */
			break;
		}
		ch = (struct sctp_chunkhdr *)((caddr_t)ch + SCTP_SIZE32(at));
	}
	/* Now update any rwnd --- possibly */
	if ((cp->ch.chunk_flags & SCTP_FROM_MIDDLE_BOX) == 0) {
		/* From a peer, we get a rwnd report */
		uint32_t a_rwnd;

		SCTP_STAT_INCR(sctps_pdrpfehos);

		bottle_bw = ntohl(cp->bottle_bw);
		on_queue = ntohl(cp->current_onq);
		if (bottle_bw && on_queue) {
			/* a rwnd report is in here */
			if (bottle_bw > on_queue)
				a_rwnd = bottle_bw - on_queue;
			else
				a_rwnd = 0;

			if (a_rwnd == 0)
				stcb->asoc.peers_rwnd = 0;
			else {
				if (a_rwnd > stcb->asoc.total_flight) {
					stcb->asoc.peers_rwnd =
					    a_rwnd - stcb->asoc.total_flight;
				} else {
					stcb->asoc.peers_rwnd = 0;
				}
				if (stcb->asoc.peers_rwnd <
				    stcb->sctp_ep->sctp_ep.sctp_sws_sender) {
					/* SWS sender side engages */
					stcb->asoc.peers_rwnd = 0;
				}
			}
		}
	} else {
		SCTP_STAT_INCR(sctps_pdrpfmbox);
	}

	/* now middle boxes in sat networks get a cwnd bump */
	if ((cp->ch.chunk_flags & SCTP_FROM_MIDDLE_BOX) &&
	    (stcb->asoc.sat_t3_loss_recovery == 0) &&
	    (stcb->asoc.sat_network)) {
		/*
		 * This is debateable but for sat networks it makes sense
		 * Note if a T3 timer has went off, we will prohibit any
		 * changes to cwnd until we exit the t3 loss recovery.
		 */
		uint32_t bw_avail;
		int rtt, incr;

#ifdef SCTP_CWND_MONITOR
		int old_cwnd = net->cwnd;

#endif
		/* need real RTT for this calc */
		rtt = ((net->lastsa >> 2) + net->lastsv) >> 1;
		/* get bottle neck bw */
		bottle_bw = ntohl(cp->bottle_bw);
		/* and whats on queue */
		on_queue = ntohl(cp->current_onq);
		/*
		 * adjust the on-queue if our flight is more it could be
		 * that the router has not yet gotten data "in-flight" to it
		 */
		if (on_queue < net->flight_size)
			on_queue = net->flight_size;

		/* calculate the available space */
		bw_avail = (bottle_bw * rtt) / 1000;
		if (bw_avail > bottle_bw) {
			/*
			 * Cap the growth to no more than the bottle neck.
			 * This can happen as RTT slides up due to queues.
			 * It also means if you have more than a 1 second
			 * RTT with a empty queue you will be limited to the
			 * bottle_bw per second no matter if other points
			 * have 1/2 the RTT and you could get more out...
			 */
			bw_avail = bottle_bw;
		}
		if (on_queue > bw_avail) {
			/*
			 * No room for anything else don't allow anything
			 * else to be "added to the fire".
			 */
			int seg_inflight, seg_onqueue, my_portion;

			net->partial_bytes_acked = 0;

			/* how much are we over queue size? */
			incr = on_queue - bw_avail;
			if (stcb->asoc.seen_a_sack_this_pkt) {
				/*
				 * undo any cwnd adjustment that the sack
				 * might have made
				 */
				net->cwnd = net->prev_cwnd;
			}
			/* Now how much of that is mine? */
			seg_inflight = net->flight_size / net->mtu;
			seg_onqueue = on_queue / net->mtu;
			my_portion = (incr * seg_inflight) / seg_onqueue;

			/* Have I made an adjustment already */
			if (net->cwnd > net->flight_size) {
				/*
				 * for this flight I made an adjustment we
				 * need to decrease the portion by a share
				 * our previous adjustment.
				 */
				int diff_adj;

				diff_adj = net->cwnd - net->flight_size;
				if (diff_adj > my_portion)
					my_portion = 0;
				else
					my_portion -= diff_adj;
			}
			/*
			 * back down to the previous cwnd (assume we have
			 * had a sack before this packet). minus what ever
			 * portion of the overage is my fault.
			 */
			net->cwnd -= my_portion;

			/* we will NOT back down more than 1 MTU */
			if (net->cwnd <= net->mtu) {
				net->cwnd = net->mtu;
			}
			/* force into CA */
			net->ssthresh = net->cwnd - 1;
		} else {
			/*
			 * Take 1/4 of the space left or max burst up ..
			 * whichever is less.
			 */
			incr = min((bw_avail - on_queue) >> 2,
			    (int)stcb->asoc.max_burst * (int)net->mtu);
			net->cwnd += incr;
		}
		if (net->cwnd > bw_avail) {
			/* We can't exceed the pipe size */
			net->cwnd = bw_avail;
		}
		if (net->cwnd < net->mtu) {
			/* We always have 1 MTU */
			net->cwnd = net->mtu;
		}
#ifdef SCTP_CWND_MONITOR
		if (net->cwnd - old_cwnd != 0) {
			/* log only changes */
			sctp_log_cwnd(stcb, net, (net->cwnd - old_cwnd),
			    SCTP_CWND_LOG_FROM_SAT);
		}
#endif
	}
}

extern int sctp_strict_init;
extern int sctp_abort_if_one_2_one_hits_limit;

/*
 * handles all control chunks in a packet inputs: - m: mbuf chain, assumed to
 * still contain IP/SCTP header - stcb: is the tcb found for this packet -
 * offset: offset into the mbuf chain to first chunkhdr - length: is the
 * length of the complete packet outputs: - length: modified to remaining
 * length after control processing - netp: modified to new sctp_nets after
 * cookie-echo processing - return NULL to discard the packet (ie. no asoc,
 * bad packet,...) otherwise return the tcb for this packet
 */
static struct sctp_tcb *
sctp_process_control(struct mbuf *m, int iphlen, int *offset, int length,
    struct sctphdr *sh, struct sctp_chunkhdr *ch, struct sctp_inpcb *inp,
    struct sctp_tcb *stcb, struct sctp_nets **netp, int *fwd_tsn_seen)
{
	struct sctp_association *asoc;
	uint32_t vtag_in;
	int num_chunks = 0;	/* number of control chunks processed */
	int chk_length;
	int ret;

	/*
	 * How big should this be, and should it be alloc'd? Lets try the
	 * d-mtu-ceiling for now (2k) and that should hopefully work ...
	 * until we get into jumbo grams and such..
	 */
	uint8_t chunk_buf[DEFAULT_CHUNK_BUFFER];
	struct sctp_tcb *locked_tcb = stcb;
	int got_auth = 0;
	uint32_t auth_offset = 0, auth_len = 0;
	int auth_skipped = 0;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
		printf("sctp_process_control: iphlen=%u, offset=%u, length=%u stcb:%p\n",
		    iphlen, *offset, length, stcb);
	}
#endif				/* SCTP_DEBUG */

	/* validate chunk header length... */
	if (ntohs(ch->chunk_length) < sizeof(*ch)) {
		return (NULL);
	}
	/*
	 * validate the verification tag
	 */
	vtag_in = ntohl(sh->v_tag);

	if (ch->chunk_type == SCTP_INITIATION) {
		if (vtag_in != 0) {
			/* protocol error- silently discard... */
			SCTP_STAT_INCR(sctps_badvtag);
			if (locked_tcb)
				SCTP_TCB_UNLOCK(locked_tcb);
			return (NULL);
		}
	} else if (ch->chunk_type != SCTP_COOKIE_ECHO) {
		/*
		 * If there is no stcb, skip the AUTH chunk and process
		 * later after a stcb is found (to validate the lookup was
		 * valid.
		 */
		if ((ch->chunk_type == SCTP_AUTHENTICATION) &&
		    (stcb == NULL) && !sctp_auth_disable) {
			/* save this chunk for later processing */
			auth_skipped = 1;
			auth_offset = *offset;
			auth_len = ntohs(ch->chunk_length);

			/* (temporarily) move past this chunk */
			*offset += SCTP_SIZE32(auth_len);
			if (*offset >= length) {
				/* no more data left in the mbuf chain */
				*offset = length;
				return (NULL);
			}
			ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, *offset,
			    sizeof(struct sctp_chunkhdr), chunk_buf);
		}
		if (ch->chunk_type == SCTP_COOKIE_ECHO) {
			goto process_control_chunks;
		}
		/*
		 * first check if it's an ASCONF with an unknown src addr we
		 * need to look inside to find the association
		 */
		if (ch->chunk_type == SCTP_ASCONF && stcb == NULL) {
			/* inp's refcount may be reduced */
			SCTP_INP_INCR_REF(inp);

			stcb = sctp_findassociation_ep_asconf(m, iphlen,
			    *offset, sh, &inp, netp);
			if (stcb == NULL) {
				/*
				 * reduce inp's refcount if not reduced in
				 * sctp_findassociation_ep_asconf().
				 */
				SCTP_INP_DECR_REF(inp);
			}
			/* now go back and verify any auth chunk to be sure */
			if (auth_skipped && (stcb != NULL)) {
				struct sctp_auth_chunk *auth;

				auth = (struct sctp_auth_chunk *)
				    sctp_m_getptr(m, auth_offset,
				    auth_len, chunk_buf);
				got_auth = 1;
				auth_skipped = 0;
				if (sctp_handle_auth(stcb, auth, m,
				    auth_offset)) {
					/* auth HMAC failed so dump it */
					*offset = length;
					return (NULL);
				} else {
					/* remaining chunks are HMAC checked */
					stcb->asoc.authenticated = 1;
				}
			}
		}
		if (stcb == NULL) {
			/* no association, so it's out of the blue... */
			sctp_handle_ootb(m, iphlen, *offset, sh, inp, NULL);
			*offset = length;
			if (locked_tcb)
				SCTP_TCB_UNLOCK(locked_tcb);
			return (NULL);
		}
		asoc = &stcb->asoc;
		/* ABORT and SHUTDOWN can use either v_tag... */
		if ((ch->chunk_type == SCTP_ABORT_ASSOCIATION) ||
		    (ch->chunk_type == SCTP_SHUTDOWN_COMPLETE) ||
		    (ch->chunk_type == SCTP_PACKET_DROPPED)) {
			if ((vtag_in == asoc->my_vtag) ||
			    ((ch->chunk_flags & SCTP_HAD_NO_TCB) &&
			    (vtag_in == asoc->peer_vtag))) {
				/* this is valid */
			} else {
				/* drop this packet... */
				SCTP_STAT_INCR(sctps_badvtag);
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);
				return (NULL);
			}
		} else if (ch->chunk_type == SCTP_SHUTDOWN_ACK) {
			if (vtag_in != asoc->my_vtag) {
				/*
				 * this could be a stale SHUTDOWN-ACK or the
				 * peer never got the SHUTDOWN-COMPLETE and
				 * is still hung; we have started a new asoc
				 * but it won't complete until the shutdown
				 * is completed
				 */
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);
				sctp_handle_ootb(m, iphlen, *offset, sh, inp,
				    NULL);
				return (NULL);
			}
		} else {
			/* for all other chunks, vtag must match */
			if (vtag_in != asoc->my_vtag) {
				/* invalid vtag... */
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
					printf("invalid vtag: %xh, expect %xh\n", vtag_in, asoc->my_vtag);
				}
#endif				/* SCTP_DEBUG */
				SCTP_STAT_INCR(sctps_badvtag);
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);
				*offset = length;
				return (NULL);
			}
		}
	}			/* end if !SCTP_COOKIE_ECHO */
	/*
	 * process all control chunks...
	 */
	if (((ch->chunk_type == SCTP_SELECTIVE_ACK) ||
	    (ch->chunk_type == SCTP_HEARTBEAT_REQUEST)) &&
	    (SCTP_GET_STATE(&stcb->asoc) == SCTP_STATE_COOKIE_ECHOED)) {
		/* implied cookie-ack.. we must have lost the ack */
		stcb->asoc.overall_error_count = 0;
		sctp_handle_cookie_ack((struct sctp_cookie_ack_chunk *)ch, stcb,
		    *netp);
	}
process_control_chunks:

	while (IS_SCTP_CONTROL(ch)) {
		/* validate chunk length */
		chk_length = ntohs(ch->chunk_length);
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT2) {
			printf("sctp_process_control: processing a chunk type=%u, len=%u\n",
			    ch->chunk_type, chk_length);
		}
#endif				/* SCTP_DEBUG */
		if ((size_t)chk_length < sizeof(*ch) ||
		    (*offset + chk_length) > length) {
			*offset = length;
			if (locked_tcb)
				SCTP_TCB_UNLOCK(locked_tcb);
			return (NULL);
		}
		SCTP_STAT_INCR_COUNTER64(sctps_incontrolchunks);
		/*
		 * INIT-ACK only gets the init ack "header" portion only
		 * because we don't have to process the peer's COOKIE. All
		 * others get a complete chunk.
		 */
		if (ch->chunk_type == SCTP_INITIATION_ACK) {
			/* get an init-ack chunk */
			ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, *offset,
			    sizeof(struct sctp_init_ack_chunk), chunk_buf);
			if (ch == NULL) {
				*offset = length;
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);
				return (NULL);
			}
		} else {
			/* get a complete chunk... */
			if ((size_t)chk_length > sizeof(chunk_buf)) {
				struct mbuf *oper;
				struct sctp_paramhdr *phdr;

				oper = NULL;
				oper = sctp_get_mbuf_for_msg(sizeof(struct sctp_paramhdr),
				    1, M_DONTWAIT, 1, MT_DATA);
				if (oper) {
					/* pre-reserve some space */
					oper->m_data += sizeof(struct sctp_chunkhdr);
					oper->m_len = sizeof(struct sctp_paramhdr);
					oper->m_pkthdr.len = oper->m_len;
					phdr = mtod(oper, struct sctp_paramhdr *);
					phdr->param_type = htons(SCTP_CAUSE_OUT_OF_RESC);
					phdr->param_length = htons(sizeof(struct sctp_paramhdr));
					sctp_queue_op_err(stcb, oper);
				}
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);
				return (NULL);
			}
			ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, *offset,
			    chk_length, chunk_buf);
			if (ch == NULL) {
				printf("sctp_process_control: Can't get the all data....\n");
				*offset = length;
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);
				return (NULL);
			}
		}
		num_chunks++;
		/* Save off the last place we got a control from */
		if (stcb != NULL) {
			if ((*netp != NULL) || (ch->chunk_type == SCTP_ASCONF)) {
				/*
				 * allow last_control to be NULL if
				 * ASCONF... ASCONF processing will find the
				 * right net later
				 */
				stcb->asoc.last_control_chunk_from = *netp;
			}
		}
#ifdef SCTP_AUDITING_ENABLED
		sctp_audit_log(0xB0, ch->chunk_type);
#endif

		/* check to see if this chunk required auth, but isn't */
		if ((stcb != NULL) && !sctp_auth_disable &&
		    sctp_auth_is_required_chunk(ch->chunk_type,
		    stcb->asoc.local_auth_chunks) &&
		    !stcb->asoc.authenticated) {
			/* "silently" ignore */
			SCTP_STAT_INCR(sctps_recvauthmissing);
			goto next_chunk;
		}
		switch (ch->chunk_type) {
		case SCTP_INITIATION:
			/* must be first and only chunk */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_INIT\n");
			}
#endif				/* SCTP_DEBUG */
			if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
				/* We are not interested anymore? */
				if ((stcb) && (stcb->asoc.total_output_queue_size)) {
					/*
					 * collision case where we are
					 * sending to them too
					 */
					;
				} else {
					if (locked_tcb)
						SCTP_TCB_UNLOCK(locked_tcb);
					*offset = length;
					return (NULL);
				}
			}
			if ((num_chunks > 1) ||
			    (sctp_strict_init && (length - *offset > SCTP_SIZE32(chk_length)))) {
				*offset = length;
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);
				return (NULL);
			}
			if ((stcb != NULL) &&
			    (SCTP_GET_STATE(&stcb->asoc) ==
			    SCTP_STATE_SHUTDOWN_ACK_SENT)) {
				sctp_send_shutdown_ack(stcb,
				    stcb->asoc.primary_destination);
				*offset = length;
				sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_CONTROL_PROC);
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);
				return (NULL);
			}
			sctp_handle_init(m, iphlen, *offset, sh,
			    (struct sctp_init_chunk *)ch, inp, stcb, *netp);
			*offset = length;
			if (locked_tcb)
				SCTP_TCB_UNLOCK(locked_tcb);
			return (NULL);
			break;
		case SCTP_INITIATION_ACK:
			/* must be first and only chunk */
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_INIT-ACK\n");
			}
#endif				/* SCTP_DEBUG */
			if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
				/* We are not interested anymore */
				if ((stcb) && (stcb->asoc.total_output_queue_size)) {
					;
				} else {
					if (locked_tcb)
						SCTP_TCB_UNLOCK(locked_tcb);
					*offset = length;
					if (stcb) {
						sctp_free_assoc(inp, stcb, 0);
					}
					return (NULL);
				}
			}
			if ((num_chunks > 1) ||
			    (sctp_strict_init && (length - *offset > SCTP_SIZE32(chk_length)))) {
				*offset = length;
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);
				return (NULL);
			}
			ret = sctp_handle_init_ack(m, iphlen, *offset, sh,
			    (struct sctp_init_ack_chunk *)ch, stcb, *netp);
			/*
			 * Special case, I must call the output routine to
			 * get the cookie echoed
			 */
			if ((stcb) && ret == 0)
				sctp_chunk_output(stcb->sctp_ep, stcb, SCTP_OUTPUT_FROM_CONTROL_PROC);
			*offset = length;
			if (locked_tcb)
				SCTP_TCB_UNLOCK(locked_tcb);
			return (NULL);
			break;
		case SCTP_SELECTIVE_ACK:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_SACK\n");
			}
#endif				/* SCTP_DEBUG */
			SCTP_STAT_INCR(sctps_recvsacks);
			{
				struct sctp_sack_chunk *sack;
				int abort_now = 0;
				uint32_t a_rwnd, cum_ack;
				uint16_t num_seg;
				int nonce_sum_flag;

				sack = (struct sctp_sack_chunk *)ch;

				nonce_sum_flag = ch->chunk_flags & SCTP_SACK_NONCE_SUM;
				cum_ack = ntohl(sack->sack.cum_tsn_ack);
				num_seg = ntohs(sack->sack.num_gap_ack_blks);
				a_rwnd = (uint32_t) ntohl(sack->sack.a_rwnd);
				stcb->asoc.seen_a_sack_this_pkt = 1;
				if ((stcb->asoc.pr_sctp_cnt == 0) &&
				    (num_seg == 0) &&
				    ((compare_with_wrap(cum_ack, stcb->asoc.last_acked_seq, MAX_TSN)) ||
				    (cum_ack == stcb->asoc.last_acked_seq)) &&
				    (stcb->asoc.saw_sack_with_frags == 0) &&
				    (!TAILQ_EMPTY(&stcb->asoc.sent_queue))
				    ) {
					/*
					 * We have a SIMPLE sack having no
					 * prior segments and data on sent
					 * queue to be acked.. Use the
					 * faster path sack processing. We
					 * also allow window update sacks
					 * with no missing segments to go
					 * this way too.
					 */
					sctp_express_handle_sack(stcb, cum_ack, a_rwnd, nonce_sum_flag, &abort_now);
				} else {
					sctp_handle_sack(sack, stcb, *netp, &abort_now);
				}
				if (abort_now) {
					/* ABORT signal from sack processing */
					*offset = length;
					return (NULL);
				}
			}
			break;
		case SCTP_HEARTBEAT_REQUEST:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_HEARTBEAT\n");
			}
#endif				/* SCTP_DEBUG */
			SCTP_STAT_INCR(sctps_recvheartbeat);
			sctp_send_heartbeat_ack(stcb, m, *offset, chk_length,
			    *netp);

			/* He's alive so give him credit */
			stcb->asoc.overall_error_count = 0;
			break;
		case SCTP_HEARTBEAT_ACK:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_HEARTBEAT-ACK\n");
			}
#endif				/* SCTP_DEBUG */

			/* He's alive so give him credit */
			stcb->asoc.overall_error_count = 0;
			SCTP_STAT_INCR(sctps_recvheartbeatack);
			sctp_handle_heartbeat_ack((struct sctp_heartbeat_chunk *)ch,
			    stcb, *netp);
			break;
		case SCTP_ABORT_ASSOCIATION:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_ABORT\n");
			}
#endif				/* SCTP_DEBUG */
			sctp_handle_abort((struct sctp_abort_chunk *)ch,
			    stcb, *netp);
			*offset = length;
			return (NULL);
			break;
		case SCTP_SHUTDOWN:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_SHUTDOWN\n");
			}
#endif				/* SCTP_DEBUG */
			{
				int abort_flag = 0;

				sctp_handle_shutdown((struct sctp_shutdown_chunk *)ch,
				    stcb, *netp, &abort_flag);
				if (abort_flag) {
					*offset = length;
					return (NULL);
				}
			}
			break;
		case SCTP_SHUTDOWN_ACK:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_SHUTDOWN-ACK\n");
			}
#endif				/* SCTP_DEBUG */
			sctp_handle_shutdown_ack((struct sctp_shutdown_ack_chunk *)ch, stcb, *netp);
			*offset = length;
			return (NULL);
			break;
		case SCTP_OPERATION_ERROR:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_OP-ERR\n");
			}
#endif				/* SCTP_DEBUG */
			if (sctp_handle_error(ch, stcb, *netp) < 0) {
				*offset = length;
				return (NULL);
			}
			break;
		case SCTP_COOKIE_ECHO:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_COOKIE-ECHO stcb is %p\n", stcb);
			}
#endif				/* SCTP_DEBUG */
			if ((stcb) && (stcb->asoc.total_output_queue_size)) {
				;
			} else {
				if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) &&
				    (stcb == NULL)) {
					/* We are not interested anymore */
					*offset = length;
					return (NULL);
				}
			}
			/*
			 * First are we accepting? We do this again here
			 * since it is possible that a previous endpoint WAS
			 * listening responded to a INIT-ACK and then
			 * closed. We opened and bound.. and are now no
			 * longer listening.
			 */
			if (inp->sctp_socket->so_qlimit == 0) {
				if ((stcb) && (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
					/*
					 * special case, is this a retran'd
					 * COOKIE-ECHO or a restarting assoc
					 * that is a peeled off or
					 * one-to-one style socket.
					 */
					goto process_cookie_anyway;
				}
				sctp_abort_association(inp, stcb, m, iphlen, sh,
				    NULL);
				*offset = length;
				return (NULL);
			} else if (inp->sctp_socket->so_qlimit) {
				/* we are accepting so check limits like TCP */
				if (inp->sctp_socket->so_qlen >
				    inp->sctp_socket->so_qlimit) {
					/* no space */
					struct mbuf *oper;
					struct sctp_paramhdr *phdr;

					if (sctp_abort_if_one_2_one_hits_limit) {
						oper = NULL;
						oper = sctp_get_mbuf_for_msg(sizeof(struct sctp_paramhdr),
						    1, M_DONTWAIT, 1, MT_DATA);
						if (oper) {
							oper->m_len =
							    oper->m_pkthdr.len =
							    sizeof(struct sctp_paramhdr);
							phdr = mtod(oper,
							    struct sctp_paramhdr *);
							phdr->param_type =
							    htons(SCTP_CAUSE_OUT_OF_RESC);
							phdr->param_length =
							    htons(sizeof(struct sctp_paramhdr));
						}
						sctp_abort_association(inp, stcb, m,
						    iphlen, sh, oper);
					}
					*offset = length;
					return (NULL);
				}
			}
	process_cookie_anyway:
			{
				struct mbuf *ret_buf;

				ret_buf =
				    sctp_handle_cookie_echo(m, iphlen,
				    *offset, sh,
				    (struct sctp_cookie_echo_chunk *)ch,
				    &inp, &stcb, netp,
				    auth_skipped,
				    auth_offset,
				    auth_len);

				if (ret_buf == NULL) {
					if (locked_tcb) {
						SCTP_TCB_UNLOCK(locked_tcb);
					}
#ifdef SCTP_DEBUG
					if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
						printf("GAK, null buffer\n");
					}
#endif				/* SCTP_DEBUG */
					auth_skipped = 0;
					*offset = length;
					return (NULL);
				}
				/* if AUTH skipped, see if it verified... */
				if (auth_skipped) {
					got_auth = 1;
					auth_skipped = 0;
				}
				if (!TAILQ_EMPTY(&stcb->asoc.sent_queue)) {
					/*
					 * Restart the timer if we have
					 * pending data
					 */
					struct sctp_tmit_chunk *chk;

					chk = TAILQ_FIRST(&stcb->asoc.sent_queue);
					if (chk) {
						sctp_timer_start(SCTP_TIMER_TYPE_SEND,
						    stcb->sctp_ep, stcb,
						    chk->whoTo);
					}
				}
			}
			break;
		case SCTP_COOKIE_ACK:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_COOKIE-ACK\n");
			}
#endif				/* SCTP_DEBUG */

			if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
				/* We are not interested anymore */
				if ((stcb) && (stcb->asoc.total_output_queue_size)) {
					;
				} else {
					sctp_free_assoc(inp, stcb, 0);
					*offset = length;
					return (NULL);
				}
			}
			/* He's alive so give him credit */
			stcb->asoc.overall_error_count = 0;
			sctp_handle_cookie_ack((struct sctp_cookie_ack_chunk *)ch, stcb, *netp);
			break;
		case SCTP_ECN_ECHO:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_ECN-ECHO\n");
			}
#endif				/* SCTP_DEBUG */
			/* He's alive so give him credit */
			stcb->asoc.overall_error_count = 0;
			sctp_handle_ecn_echo((struct sctp_ecne_chunk *)ch,
			    stcb);
			break;
		case SCTP_ECN_CWR:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_ECN-CWR\n");
			}
#endif				/* SCTP_DEBUG */
			/* He's alive so give him credit */
			stcb->asoc.overall_error_count = 0;

			sctp_handle_ecn_cwr((struct sctp_cwr_chunk *)ch, stcb);
			break;
		case SCTP_SHUTDOWN_COMPLETE:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_SHUTDOWN-COMPLETE\n");
			}
#endif				/* SCTP_DEBUG */
			/* must be first and only chunk */
			if ((num_chunks > 1) ||
			    (length - *offset > SCTP_SIZE32(chk_length))) {
				*offset = length;
				if (locked_tcb)
					SCTP_TCB_UNLOCK(locked_tcb);

				return (NULL);
			}
			sctp_handle_shutdown_complete((struct sctp_shutdown_complete_chunk *)ch,
			    stcb, *netp);
			*offset = length;
			return (NULL);
			break;
		case SCTP_ASCONF:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_ASCONF\n");
			}
#endif				/* SCTP_DEBUG */
			/* He's alive so give him credit */
			stcb->asoc.overall_error_count = 0;

			sctp_handle_asconf(m, *offset,
			    (struct sctp_asconf_chunk *)ch, stcb);
			break;
		case SCTP_ASCONF_ACK:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_ASCONF-ACK\n");
			}
#endif				/* SCTP_DEBUG */
			/* He's alive so give him credit */
			stcb->asoc.overall_error_count = 0;

			sctp_handle_asconf_ack(m, *offset,
			    (struct sctp_asconf_ack_chunk *)ch, stcb, *netp);
			break;
		case SCTP_FORWARD_CUM_TSN:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_FWD-TSN\n");
			}
#endif				/* SCTP_DEBUG */
			/* He's alive so give him credit */
			{
				int abort_flag = 0;

				stcb->asoc.overall_error_count = 0;
				*fwd_tsn_seen = 1;
				if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
					/* We are not interested anymore */
					sctp_free_assoc(inp, stcb, 0);
					*offset = length;
					return (NULL);
				}
				sctp_handle_forward_tsn(stcb,
				    (struct sctp_forward_tsn_chunk *)ch, &abort_flag);
				if (abort_flag) {
					*offset = length;
					return (NULL);
				} else {
					stcb->asoc.overall_error_count = 0;
				}

			}
			break;
		case SCTP_STREAM_RESET:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_STREAM_RESET\n");
			}
#endif				/* SCTP_DEBUG */
			ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, *offset,
			    chk_length, chunk_buf);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
				/* We are not interested anymore */
				sctp_free_assoc(inp, stcb, 0);
				*offset = length;
				return (NULL);
			}
			if (stcb->asoc.peer_supports_strreset == 0) {
				/*
				 * hmm, peer should have announced this, but
				 * we will turn it on since he is sending us
				 * a stream reset.
				 */
				stcb->asoc.peer_supports_strreset = 1;
			}
			if (sctp_handle_stream_reset(stcb, (struct sctp_stream_reset_out_req *)ch)) {
				/* stop processing */
				*offset = length;
				return (NULL);
			}
			break;
		case SCTP_PACKET_DROPPED:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_PACKET_DROPPED\n");
			}
#endif				/* SCTP_DEBUG */
			/* re-get it all please */
			ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, *offset,
			    chk_length, chunk_buf);

			sctp_handle_packet_dropped((struct sctp_pktdrop_chunk *)ch,
			    stcb, *netp);

			break;

		case SCTP_AUTHENTICATION:
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("SCTP_AUTHENTICATION\n");
			}
#endif				/* SCTP_DEBUG */
			if (sctp_auth_disable)
				goto unknown_chunk;

			if (stcb == NULL) {
				/* save the first AUTH for later processing */
				if (auth_skipped == 0) {
					auth_offset = *offset;
					auth_len = chk_length;
					auth_skipped = 1;
				}
				/* skip this chunk (temporarily) */
				goto next_chunk;
			}
			if (got_auth == 1) {
				/* skip this chunk... it's already auth'd */
				goto next_chunk;
			}
			ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, *offset,
			    chk_length, chunk_buf);
			got_auth = 1;
			if (sctp_handle_auth(stcb, (struct sctp_auth_chunk *)ch,
			    m, *offset)) {
				/* auth HMAC failed so dump the packet */
				*offset = length;
				return (stcb);
			} else {
				/* remaining chunks are HMAC checked */
				stcb->asoc.authenticated = 1;
			}
			break;

		default:
	unknown_chunk:
			/* it's an unknown chunk! */
			if ((ch->chunk_type & 0x40) && (stcb != NULL)) {
				struct mbuf *mm;
				struct sctp_paramhdr *phd;

				mm = sctp_get_mbuf_for_msg(sizeof(struct sctp_paramhdr),
				    1, M_DONTWAIT, 1, MT_DATA);
				if (mm) {
					phd = mtod(mm, struct sctp_paramhdr *);
					/*
					 * We cheat and use param type since
					 * we did not bother to define a
					 * error cause struct. They are the
					 * same basic format with different
					 * names.
					 */
					phd->param_type = htons(SCTP_CAUSE_UNRECOG_CHUNK);
					phd->param_length = htons(chk_length + sizeof(*phd));
					mm->m_len = sizeof(*phd);
					mm->m_next = sctp_m_copym(m, *offset, SCTP_SIZE32(chk_length),
					    M_DONTWAIT);
					if (mm->m_next) {
						mm->m_pkthdr.len = SCTP_SIZE32(chk_length) + sizeof(*phd);
						sctp_queue_op_err(stcb, mm);
					} else {
						sctp_m_freem(mm);
					}
				}
			}
			if ((ch->chunk_type & 0x80) == 0) {
				/* discard this packet */
				*offset = length;
				return (stcb);
			}	/* else skip this bad chunk and continue... */
			break;
		}		/* switch (ch->chunk_type) */


next_chunk:
		/* get the next chunk */
		*offset += SCTP_SIZE32(chk_length);
		if (*offset >= length) {
			/* no more data left in the mbuf chain */
			break;
		}
		ch = (struct sctp_chunkhdr *)sctp_m_getptr(m, *offset,
		    sizeof(struct sctp_chunkhdr), chunk_buf);
		if (ch == NULL) {
			if (locked_tcb)
				SCTP_TCB_UNLOCK(locked_tcb);
			*offset = length;
			return (NULL);
		}
	}			/* while */
	return (stcb);
}


/*
 * Process the ECN bits we have something set so we must look to see if it is
 * ECN(0) or ECN(1) or CE
 */
static __inline void
sctp_process_ecn_marked_a(struct sctp_tcb *stcb, struct sctp_nets *net,
    uint8_t ecn_bits)
{
	if ((ecn_bits & SCTP_CE_BITS) == SCTP_CE_BITS) {
		;
	} else if ((ecn_bits & SCTP_ECT1_BIT) == SCTP_ECT1_BIT) {
		/*
		 * we only add to the nonce sum for ECT1, ECT0 does not
		 * change the NS bit (that we have yet to find a way to send
		 * it yet).
		 */

		/* ECN Nonce stuff */
		stcb->asoc.receiver_nonce_sum++;
		stcb->asoc.receiver_nonce_sum &= SCTP_SACK_NONCE_SUM;

		/*
		 * Drag up the last_echo point if cumack is larger since we
		 * don't want the point falling way behind by more than
		 * 2^^31 and then having it be incorrect.
		 */
		if (compare_with_wrap(stcb->asoc.cumulative_tsn,
		    stcb->asoc.last_echo_tsn, MAX_TSN)) {
			stcb->asoc.last_echo_tsn = stcb->asoc.cumulative_tsn;
		}
	} else if ((ecn_bits & SCTP_ECT0_BIT) == SCTP_ECT0_BIT) {
		/*
		 * Drag up the last_echo point if cumack is larger since we
		 * don't want the point falling way behind by more than
		 * 2^^31 and then having it be incorrect.
		 */
		if (compare_with_wrap(stcb->asoc.cumulative_tsn,
		    stcb->asoc.last_echo_tsn, MAX_TSN)) {
			stcb->asoc.last_echo_tsn = stcb->asoc.cumulative_tsn;
		}
	}
}

static __inline void
sctp_process_ecn_marked_b(struct sctp_tcb *stcb, struct sctp_nets *net,
    uint32_t high_tsn, uint8_t ecn_bits)
{
	if ((ecn_bits & SCTP_CE_BITS) == SCTP_CE_BITS) {
		/*
		 * we possibly must notify the sender that a congestion
		 * window reduction is in order. We do this by adding a ECNE
		 * chunk to the output chunk queue. The incoming CWR will
		 * remove this chunk.
		 */
		if (compare_with_wrap(high_tsn, stcb->asoc.last_echo_tsn,
		    MAX_TSN)) {
			/* Yep, we need to add a ECNE */
			sctp_send_ecn_echo(stcb, net, high_tsn);
			stcb->asoc.last_echo_tsn = high_tsn;
		}
	}
}

/*
 * common input chunk processing (v4 and v6)
 */
int
sctp_common_input_processing(struct mbuf **mm, int iphlen, int offset,
    int length, struct sctphdr *sh, struct sctp_chunkhdr *ch,
    struct sctp_inpcb *inp, struct sctp_tcb *stcb, struct sctp_nets *net,
    uint8_t ecn_bits)
{
	/*
	 * Control chunk processing
	 */
	uint32_t high_tsn;
	int fwd_tsn_seen = 0, data_processed = 0;
	struct mbuf *m = *mm;
	int abort_flag = 0;
	int un_sent;

	SCTP_STAT_INCR(sctps_recvdatagrams);
#ifdef SCTP_AUDITING_ENABLED
	sctp_audit_log(0xE0, 1);
	sctp_auditing(0, inp, stcb, net);
#endif

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
		printf("Ok, Common input processing called, m:%x iphlen:%d offset:%d\n",
		    (uint32_t) m, iphlen, offset);
	}
#endif				/* SCTP_DEBUG */

	if (stcb) {
		/* always clear this before beginning a packet */
		stcb->asoc.authenticated = 0;
		stcb->asoc.seen_a_sack_this_pkt = 0;
	}
	if (IS_SCTP_CONTROL(ch)) {
		/* process the control portion of the SCTP packet */
		stcb = sctp_process_control(m, iphlen, &offset, length, sh, ch,
		    inp, stcb, &net, &fwd_tsn_seen);
		if (stcb) {
			/*
			 * This covers us if the cookie-echo was there and
			 * it changes our INP.
			 */
			inp = stcb->sctp_ep;
		}
	} else {
		/*
		 * no control chunks, so pre-process DATA chunks (these
		 * checks are taken care of by control processing)
		 */

		/*
		 * if DATA only packet, and auth is required, then punt...
		 * can't have authenticated without any AUTH (control)
		 * chunks
		 */
		if ((stcb != NULL) && !sctp_auth_disable &&
		    sctp_auth_is_required_chunk(SCTP_DATA,
		    stcb->asoc.local_auth_chunks)) {
			/* "silently" ignore */
			SCTP_STAT_INCR(sctps_recvauthmissing);
			SCTP_TCB_UNLOCK(stcb);
			return (1);
		}
		if (stcb == NULL) {
			/* out of the blue DATA chunk */
			sctp_handle_ootb(m, iphlen, offset, sh, inp, NULL);
			return (1);
		}
		if (stcb->asoc.my_vtag != ntohl(sh->v_tag)) {
			/* v_tag mismatch! */
			SCTP_STAT_INCR(sctps_badvtag);
			SCTP_TCB_UNLOCK(stcb);
			return (1);
		}
	}

	if (stcb == NULL) {
		/*
		 * no valid TCB for this packet, or we found it's a bad
		 * packet while processing control, or we're done with this
		 * packet (done or skip rest of data), so we drop it...
		 */
		return (1);
	}
	/*
	 * DATA chunk processing
	 */
	/* plow through the data chunks while length > offset */

	/*
	 * Rest should be DATA only.  Check authentication state if AUTH for
	 * DATA is required.
	 */
	if ((stcb != NULL) && !sctp_auth_disable &&
	    sctp_auth_is_required_chunk(SCTP_DATA,
	    stcb->asoc.local_auth_chunks) &&
	    !stcb->asoc.authenticated) {
		/* "silently" ignore */
		SCTP_STAT_INCR(sctps_recvauthmissing);
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_AUTH1)
			printf("Data chunk requires AUTH, skipped\n");
#endif
		SCTP_TCB_UNLOCK(stcb);
		return (1);
	}
	if (length > offset) {
		int retval;

		/*
		 * First check to make sure our state is correct. We would
		 * not get here unless we really did have a tag, so we don't
		 * abort if this happens, just dump the chunk silently.
		 */
		switch (SCTP_GET_STATE(&stcb->asoc)) {
		case SCTP_STATE_COOKIE_ECHOED:
			/*
			 * we consider data with valid tags in this state
			 * shows us the cookie-ack was lost. Imply it was
			 * there.
			 */
			stcb->asoc.overall_error_count = 0;
			sctp_handle_cookie_ack((struct sctp_cookie_ack_chunk *)ch, stcb, net);
			break;
		case SCTP_STATE_COOKIE_WAIT:
			/*
			 * We consider OOTB any data sent during asoc setup.
			 */
			sctp_handle_ootb(m, iphlen, offset, sh, inp, NULL);
			SCTP_TCB_UNLOCK(stcb);
			return (1);
			break;
		case SCTP_STATE_EMPTY:	/* should not happen */
		case SCTP_STATE_INUSE:	/* should not happen */
		case SCTP_STATE_SHUTDOWN_RECEIVED:	/* This is a peer error */
		case SCTP_STATE_SHUTDOWN_ACK_SENT:
		default:
			SCTP_TCB_UNLOCK(stcb);
			return (1);
			break;
		case SCTP_STATE_OPEN:
		case SCTP_STATE_SHUTDOWN_SENT:
			break;
		}
		/* take care of ECN, part 1. */
		if (stcb->asoc.ecn_allowed &&
		    (ecn_bits & (SCTP_ECT0_BIT | SCTP_ECT1_BIT))) {
			sctp_process_ecn_marked_a(stcb, net, ecn_bits);
		}
		/* plow through the data chunks while length > offset */
		retval = sctp_process_data(mm, iphlen, &offset, length, sh,
		    inp, stcb, net, &high_tsn);
		if (retval == 2) {
			/*
			 * The association aborted, NO UNLOCK needed since
			 * the association is destroyed.
			 */
			return (0);
		}
		data_processed = 1;
		if (retval == 0) {
			/* take care of ecn part 2. */
			if (stcb->asoc.ecn_allowed &&
			    (ecn_bits & (SCTP_ECT0_BIT | SCTP_ECT1_BIT))) {
				sctp_process_ecn_marked_b(stcb, net, high_tsn,
				    ecn_bits);
			}
		}
		/*
		 * Anything important needs to have been m_copy'ed in
		 * process_data
		 */
	}
	if ((data_processed == 0) && (fwd_tsn_seen)) {
		int was_a_gap = 0;

		if (compare_with_wrap(stcb->asoc.highest_tsn_inside_map,
		    stcb->asoc.cumulative_tsn, MAX_TSN)) {
			/* there was a gap before this data was processed */
			was_a_gap = 1;
		}
		sctp_sack_check(stcb, 1, was_a_gap, &abort_flag);
		if (abort_flag) {
			/* Again, we aborted so NO UNLOCK needed */
			return (0);
		}
	}
	/* trigger send of any chunks in queue... */
#ifdef SCTP_AUDITING_ENABLED
	sctp_audit_log(0xE0, 2);
	sctp_auditing(1, inp, stcb, net);
#endif
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
		printf("Check for chunk output prw:%d tqe:%d tf=%d\n",
		    stcb->asoc.peers_rwnd,
		    TAILQ_EMPTY(&stcb->asoc.control_send_queue),
		    stcb->asoc.total_flight);
	}
#endif
	un_sent = (stcb->asoc.total_output_queue_size - stcb->asoc.total_flight);

	if (!TAILQ_EMPTY(&stcb->asoc.control_send_queue) ||
	    ((un_sent) &&
	    (stcb->asoc.peers_rwnd > 0 ||
	    (stcb->asoc.peers_rwnd <= 0 && stcb->asoc.total_flight == 0)))) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
			printf("Calling chunk OUTPUT\n");
		}
#endif
		sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_CONTROL_PROC);
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
			printf("chunk OUTPUT returns\n");
		}
#endif
	}
#ifdef SCTP_AUDITING_ENABLED
	sctp_audit_log(0xE0, 3);
	sctp_auditing(2, inp, stcb, net);
#endif
	SCTP_TCB_UNLOCK(stcb);
	return (0);
}

extern int sctp_no_csum_on_loopback;


void
sctp_input(m, off)
	struct mbuf *m;
	int off;

{
#ifdef SCTP_MBUF_LOGGING
	struct mbuf *mat;

#endif
	int iphlen;
	int s;
	uint8_t ecn_bits;
	struct ip *ip;
	struct sctphdr *sh;
	struct sctp_inpcb *inp = NULL;

	uint32_t check, calc_check;
	struct sctp_nets *net;
	struct sctp_tcb *stcb = NULL;
	struct sctp_chunkhdr *ch;
	int refcount_up = 0;
	int length, mlen, offset;


	iphlen = off;
	net = NULL;
	SCTP_STAT_INCR(sctps_recvpackets);
	SCTP_STAT_INCR_COUNTER64(sctps_inpackets);

	/*
	 * Strip IP options, we don't allow any in or out.
	 */
#ifdef SCTP_MBUF_LOGGING
	/* Log in any input mbufs */
	mat = m;
	while (mat) {
		if (mat->m_flags & M_EXT) {
			sctp_log_mb(mat, SCTP_MBUF_INPUT);
		}
		mat = mat->m_next;
	}
#endif
	if ((size_t)iphlen > sizeof(struct ip)) {
		ip_stripoptions(m, (struct mbuf *)0);
		iphlen = sizeof(struct ip);
	}
	/*
	 * Get IP, SCTP, and first chunk header together in first mbuf.
	 */
	ip = mtod(m, struct ip *);
	offset = iphlen + sizeof(*sh) + sizeof(*ch);
	if (m->m_len < offset) {
		if ((m = m_pullup(m, offset)) == 0) {
			SCTP_STAT_INCR(sctps_hdrops);
			return;
		}
		ip = mtod(m, struct ip *);
	}
	sh = (struct sctphdr *)((caddr_t)ip + iphlen);
	ch = (struct sctp_chunkhdr *)((caddr_t)sh + sizeof(*sh));

	/* SCTP does not allow broadcasts or multicasts */
	if (IN_MULTICAST(ntohl(ip->ip_dst.s_addr))) {
		goto bad;
	}
	if (((ch->chunk_type == SCTP_INITIATION) ||
	    (ch->chunk_type == SCTP_INITIATION_ACK) ||
	    (ch->chunk_type == SCTP_COOKIE_ECHO)) &&
	    (in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))) {
		/*
		 * We only look at broadcast if its a front state, All
		 * others we will not have a tcb for anyway.
		 */
		goto bad;
	}
	/* destination port of 0 is illegal, based on RFC2960. */
	if (sh->dest_port == 0) {
		SCTP_STAT_INCR(sctps_hdrops);
		goto bad;
	}
	/* validate SCTP checksum */
	if ((sctp_no_csum_on_loopback == 0) ||
	    (m->m_pkthdr.rcvif == NULL) ||
	    (m->m_pkthdr.rcvif->if_type != IFT_LOOP)) {
		/*
		 * we do NOT validate things from the loopback if the sysctl
		 * is set to 1.
		 */
		check = sh->checksum;	/* save incoming checksum */
		if ((check == 0) && (sctp_no_csum_on_loopback)) {
			/*
			 * special hook for where we got a local address
			 * somehow routed across a non IFT_LOOP type
			 * interface
			 */
			if (ip->ip_src.s_addr == ip->ip_dst.s_addr)
				goto sctp_skip_csum_4;
		}
		sh->checksum = 0;	/* prepare for calc */
		calc_check = sctp_calculate_sum(m, &mlen, iphlen);
		if (calc_check != check) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
				printf("Bad CSUM on SCTP packet calc_check:%x check:%x  m:%x mlen:%d iphlen:%d\n",
				    calc_check, check, (uint32_t) m, mlen, iphlen);
			}
#endif

			stcb = sctp_findassociation_addr(m, iphlen,
			    offset - sizeof(*ch),
			    sh, ch, &inp, &net);
			if ((inp) && (stcb)) {
				sctp_send_packet_dropped(stcb, net, m, iphlen, 1);
				sctp_chunk_output(inp, stcb, SCTP_OUTPUT_FROM_INPUT_ERROR);
			} else if ((inp != NULL) && (stcb == NULL)) {
				refcount_up = 1;
			}
			SCTP_STAT_INCR(sctps_badsum);
			SCTP_STAT_INCR_COUNTER32(sctps_checksumerrors);
			goto bad;
		}
		sh->checksum = calc_check;
	} else {
sctp_skip_csum_4:
		mlen = m->m_pkthdr.len;
	}
	/* validate mbuf chain length with IP payload length */
	if (mlen < (ip->ip_len - iphlen)) {
		SCTP_STAT_INCR(sctps_hdrops);
		goto bad;
	}
	/*
	 * Locate pcb and tcb for datagram sctp_findassociation_addr() wants
	 * IP/SCTP/first chunk header...
	 */
	stcb = sctp_findassociation_addr(m, iphlen, offset - sizeof(*ch),
	    sh, ch, &inp, &net);
	/* inp's ref-count increased && stcb locked */
	if (inp == NULL) {
		struct sctp_init_chunk *init_chk, chunk_buf;

		SCTP_STAT_INCR(sctps_noport);
#ifdef ICMP_BANDLIM
		/*
		 * we use the bandwidth limiting to protect against sending
		 * too many ABORTS all at once. In this case these count the
		 * same as an ICMP message.
		 */
		if (badport_bandlim(0) < 0)
			goto bad;
#endif				/* ICMP_BANDLIM */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT1) {
			printf("Sending a ABORT from packet entry!\n");
		}
#endif
		if (ch->chunk_type == SCTP_INITIATION) {
			/*
			 * we do a trick here to get the INIT tag, dig in
			 * and get the tag from the INIT and put it in the
			 * common header.
			 */
			init_chk = (struct sctp_init_chunk *)sctp_m_getptr(m,
			    iphlen + sizeof(*sh), sizeof(*init_chk),
			    (uint8_t *) & chunk_buf);
			if (init_chk != NULL)
				sh->v_tag = init_chk->init.initiate_tag;
		}
		sctp_send_abort(m, iphlen, sh, 0, NULL);
		goto bad;
	} else if (stcb == NULL) {
		refcount_up = 1;
	}
#ifdef IPSEC
	/*
	 * I very much doubt any of the IPSEC stuff will work but I have no
	 * idea, so I will leave it in place.
	 */

	if (ipsec4_in_reject_so(m, inp->ip_inp.inp.inp_socket)) {
		ipsecstat.in_polvio++;
		SCTP_STAT_INCR(sctps_hdrops);
		goto bad;
	}
#endif				/* IPSEC */



	/*
	 * common chunk processing
	 */
	length = ip->ip_len + iphlen;
	offset -= sizeof(struct sctp_chunkhdr);

	ecn_bits = ip->ip_tos;
	s = splnet();

	sctp_common_input_processing(&m, iphlen, offset, length, sh, ch,
	    inp, stcb, net, ecn_bits);
	/* inp's ref-count reduced && stcb unlocked */
	splx(s);
	if (m) {
		sctp_m_freem(m);
	}
	if ((inp) && (refcount_up)) {
		/* reduce ref-count */
		SCTP_INP_WLOCK(inp);
		SCTP_INP_DECR_REF(inp);
		SCTP_INP_WUNLOCK(inp);
	}
	return;
bad:
	if (stcb)
		SCTP_TCB_UNLOCK(stcb);

	if ((inp) && (refcount_up)) {
		/* reduce ref-count */
		SCTP_INP_WLOCK(inp);
		SCTP_INP_DECR_REF(inp);
		SCTP_INP_WUNLOCK(inp);
	}
	if (m) {
		sctp_m_freem(m);
	}
	return;
}
