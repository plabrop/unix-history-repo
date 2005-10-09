/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1996-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: symtab.h,v 1.16.206.1 2004/03/06 08:14:49 marka Exp $ */

#ifndef ISC_SYMTAB_H
#define ISC_SYMTAB_H 1

/*****
 ***** Module Info
 *****/

/*
 * Symbol Table
 *
 * Provides a simple memory-based symbol table.
 *
 * Keys are C strings, and key comparisons are case-insenstive.  A type may
 * be specified when looking up, defining, or undefining.  A type value of
 * 0 means "match any type"; any other value will only match the given
 * type.
 *
 * It's possible that a client will attempt to define a <key, type, value>
 * tuple when a tuple with the given key and type already exists in the table.
 * What to do in this case is specified by the client.  Possible policies are:
 *
 *	isc_symexists_reject	Disallow the define, returning ISC_R_EXISTS
 *	isc_symexists_replace	Replace the old value with the new.  The
 *				undefine action (if provided) will be called
 *				with the old <key, type, value> tuple.
 *	isc_symexists_add	Add the new tuple, leaving the old tuple in
 *				the table.  Subsequent lookups will retrieve
 *				the most-recently-defined tuple.
 *
 * A lookup of a key using type 0 will return the most-recently defined
 * symbol with that key.  An undefine of a key using type 0 will undefine the
 * most-recently defined symbol with that key.  Trying to define a key with
 * type 0 is illegal.
 *
 * The symbol table library does not make a copy the key field, so the
 * caller must ensure that any key it passes to isc_symtab_define() will not
 * change until it calls isc_symtab_undefine() or isc_symtab_destroy().
 *
 * A user-specified action will be called (if provided) when a symbol is
 * undefined.  It can be used to free memory associated with keys and/or
 * values.
 *
 * MP:
 *	The callers of this module must ensure any required synchronization.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	<TBS>
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */

/***
 *** Imports.
 ***/

#include <isc/lang.h>
#include <isc/types.h>

/***
 *** Symbol Tables.
 ***/

typedef union isc_symvalue {
	void *				as_pointer;
	int				as_integer;
	unsigned int			as_uinteger;
} isc_symvalue_t;

typedef void (*isc_symtabaction_t)(char *key, unsigned int type,
				   isc_symvalue_t value, void *userarg);

typedef enum {
	isc_symexists_reject = 0,
	isc_symexists_replace = 1,
	isc_symexists_add = 2
} isc_symexists_t;

ISC_LANG_BEGINDECLS

isc_result_t
isc_symtab_create(isc_mem_t *mctx, unsigned int size,
		  isc_symtabaction_t undefine_action, void *undefine_arg,
		  isc_boolean_t case_sensitive, isc_symtab_t **symtabp);

void
isc_symtab_destroy(isc_symtab_t **symtabp);

isc_result_t
isc_symtab_lookup(isc_symtab_t *symtab, const char *key, unsigned int type,
		  isc_symvalue_t *value);

isc_result_t
isc_symtab_define(isc_symtab_t *symtab, const char *key, unsigned int type,
		  isc_symvalue_t value, isc_symexists_t exists_policy);

isc_result_t
isc_symtab_undefine(isc_symtab_t *symtab, const char *key, unsigned int type);

ISC_LANG_ENDDECLS

#endif /* ISC_SYMTAB_H */
