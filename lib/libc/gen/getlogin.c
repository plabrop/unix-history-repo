/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getlogin.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <errno.h>
#include <pwd.h>
#include <utmp.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libc_private.h>

#ifndef _THREAD_SAFE
#define	THREAD_LOCK()
#define	THREAD_UNLOCK()
#else
#include <pthread.h>
#include "pthread_private.h"
static struct pthread_mutex logname_lock = PTHREAD_MUTEX_STATIC_INITIALIZER;
static pthread_mutex_t logname_mutex = &logname_lock;
#define	THREAD_LOCK()	if (__isthreaded) pthread_mutex_lock(&logname_mutex)
#define	THREAD_UNLOCK()	if (__isthreaded) pthread_mutex_unlock(&logname_mutex)
#endif /* _THREAD_SAFE */

int		_logname_valid;		/* known to setlogin() */

static char *
getlogin_basic(int *status)
{
	static char logname[MAXLOGNAME];

	if (_logname_valid == 0) {
#ifdef __NETBSD_SYSCALLS
		if (__getlogin(logname, sizeof(logname) - 1) < 0) {
#else
		if (_getlogin(logname, sizeof(logname)) < 0) {
#endif
			*status = errno;
			return (NULL);
		}
		_logname_valid = 1;
	}
	*status = 0;
	return (*logname ? logname : NULL);
}

char *
getlogin(void)
{
	char	*result;
	int	status;

	THREAD_LOCK();
	result = getlogin_basic(&status);
	THREAD_UNLOCK();
	return (result);
}

int
getlogin_r(char *logname, int namelen)
{
	char	*result;
	int	len;
	int	status;
	
	THREAD_LOCK();
	result = getlogin_basic(&status);
	if (status == 0) {
		if ((len = strlen(result) + 1) > namelen)
			status = ERANGE;
		else
			strncpy(logname, result, len);
	}
	THREAD_UNLOCK();
	return (status);
}
