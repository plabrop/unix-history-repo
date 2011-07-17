/*	$FreeBSD$	*/
/*	$KAME: timer.c,v 1.9 2002/06/10 19:59:47 itojun Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/time.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <syslog.h>
#include <stdio.h>
#include <inttypes.h>

#include "timer.h"
#include "timer_subr.h"

struct timeval *
rtadvd_timer_rest(struct rtadvd_timer *rat)
{
	static struct timeval returnval, now;

	gettimeofday(&now, NULL);
	if (TIMEVAL_LEQ(&rat->rat_tm, &now)) {
		syslog(LOG_DEBUG,
		    "<%s> a timer must be expired, but not yet",
		    __func__);
		returnval.tv_sec = returnval.tv_usec = 0;
	}
	else
		TIMEVAL_SUB(&rat->rat_tm, &now, &returnval);

	return (&returnval);
}

/* result = a + b */
void
TIMEVAL_ADD(struct timeval *a, struct timeval *b, struct timeval *result)
{
	long l;

	if ((l = a->tv_usec + b->tv_usec) < MILLION) {
		result->tv_usec = l;
		result->tv_sec = a->tv_sec + b->tv_sec;
	}
	else {
		result->tv_usec = l - MILLION;
		result->tv_sec = a->tv_sec + b->tv_sec + 1;
	}
}

/*
 * result = a - b
 * XXX: this function assumes that a >= b.
 */
void
TIMEVAL_SUB(struct timeval *a, struct timeval *b, struct timeval *result)
{
	long l;

	if ((l = a->tv_usec - b->tv_usec) >= 0) {
		result->tv_usec = l;
		result->tv_sec = a->tv_sec - b->tv_sec;
	}
	else {
		result->tv_usec = MILLION + l;
		result->tv_sec = a->tv_sec - b->tv_sec - 1;
	}
}

char *
sec2str(uint32_t s, char *buf)
{
	uint32_t day;
	uint32_t hour;
	uint32_t min;
	uint32_t sec;
	char *p;

	min = s / 60;
	sec = s % 60;

	hour = min / 60;
	min = min % 60;

	day = hour / 24;
	hour = hour % 24;

	p = buf;
	if (day > 0)
		p += sprintf(p, "%" PRIu32 "d", day);
	if (hour > 0)
		p += sprintf(p, "%" PRIu32 "h", hour);
	if (min > 0)
		p += sprintf(p, "%" PRIu32 "m", min);

	if ((p == buf) || (sec > 0 && p > buf))
		sprintf(p, "%" PRIu32 "s", sec);

	return (buf);
}
