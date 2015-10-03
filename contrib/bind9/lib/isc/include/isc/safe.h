/*
 * Copyright (C) 2013, 2015  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id$ */

#ifndef ISC_SAFE_H
#define ISC_SAFE_H 1

/*! \file isc/safe.h */

#include <isc/types.h>

ISC_LANG_BEGINDECLS

isc_boolean_t
isc_safe_memequal(const void *s1, const void *s2, size_t n);
/*%<
 * Returns ISC_TRUE iff. two blocks of memory are equal, otherwise
 * ISC_FALSE.
 *
 */

int
isc_safe_memcompare(const void *b1, const void *b2, size_t len);
/*%<
 * Clone of libc memcmp() which is safe to differential timing attacks.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_SAFE_H */
