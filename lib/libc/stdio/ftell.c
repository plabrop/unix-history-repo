/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)ftell.c	8.2 (Berkeley) 5/4/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include "un-namespace.h"
#include "local.h"
#include "libc_private.h"

/*
 * standard ftell function.
 */
long
ftell(fp)
	register FILE *fp;
{
	register off_t rv;

	rv = ftello(fp);
	if (rv > LONG_MAX) {
		errno = EOVERFLOW;
		return (-1);
	}
	return (rv);
}

/*
 * ftello: return current offset.
 */
off_t
ftello(fp)
	register FILE *fp;
{
	register off_t rv;

	/* make sure stdio is set up */
	if (!__sdidinit)
		__sinit();

	FLOCKFILE(fp);
	rv = _ftello(fp);
	FUNLOCKFILE(fp);
	return (rv);
}

off_t
_ftello(fp)
	register FILE *fp;
{
	register fpos_t pos, spos;
	size_t n;

	if (fp->_seek == NULL) {
		errno = ESPIPE;			/* historic practice */
		return (-1);
	}

	/*
	 * Find offset of underlying I/O object, then
	 * adjust for buffered bytes.
	 */
	if (fp->_flags & __SOFF) {
		pos = fp->_offset;
		spos = -1;
	} else {
get_real_pos:
		spos = pos = (*fp->_seek)(fp->_cookie, (fpos_t)0, SEEK_CUR);
		if (pos == -1)
			return (-1);
	}
	if (fp->_flags & __SRD) {
		/*
		 * Reading.  Any unread characters (including
		 * those from ungetc) cause the position to be
		 * smaller than that in the underlying object.
		 */
		if ((pos -= fp->_r) < 0 ||
		    (HASUB(fp) && (pos -= fp->_ur) < 0)) {
			fp->_p = fp->_bf._base;
			fp->_r = 0;
			if (HASUB(fp))
				FREEUB(fp);
			if (spos == -1)
				goto get_real_pos;
			pos = spos;
		}
	} else if ((fp->_flags & __SWR) && fp->_p != NULL) {
		/*
		 * Writing.  Any buffered characters cause the
		 * position to be greater than that in the
		 * underlying object.
		 */
		n = fp->_p - fp->_bf._base;
		if (pos > OFF_MAX - n) {
			errno = EOVERFLOW;
			return (-1);
		}
		pos += n;
	}
	return (pos);
}
