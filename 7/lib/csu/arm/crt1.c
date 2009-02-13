/* LINTLIBRARY */
/*-
 * Copyright 2001 David E. O'Brien.
 * All rights reserved.
 * Copyright 1996-1998 John D. Polstra.
 * All rights reserved.
 * Copyright (c) 1997 Jason R. Thorpe.
 * Copyright (c) 1995 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *          This product includes software developed for the
 *          FreeBSD Project.  See http://www.freebsd.org/ for
 *          information about FreeBSD.
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.netbsd.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#ifndef lint
#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif
#endif /* lint */

#include <stdlib.h>

#include "libc_private.h"
#include "crtbrand.c"
#include <machine/asm.h>

struct Struct_Obj_Entry;
struct ps_strings;

extern int _DYNAMIC;
#pragma weak _DYNAMIC

extern void _fini(void);
extern void _init(void);
extern int main(int, char **, char **);
extern void _start(int, char **, char **, const struct Struct_Obj_Entry *,
    void (*)(void), struct ps_strings *);

#ifdef GCRT
extern void _mcleanup(void);
extern void monstartup(void *, void *);
extern int eprol;
extern int etext;
#endif

char **environ;
const char *__progname = "";
struct ps_strings *__ps_strings;

/* The entry function. */
__asm("	.text			\n"
"	.align	0		\n"
"	.globl	_start		\n"
"	_start:			\n"
"	mov	r5, r2		/* cleanup */		\n"
"	mov	r4, r1		/* obj_main */		\n"
"	mov	r3, r0		/* ps_strings */	\n"
"	/* Get argc, argv, and envp from stack */	\n"
"	ldr	r0, [sp, #0x0000]	\n"
"	add	r1, sp, #0x0004		\n"
"	add	r2, r1, r0, lsl #2	\n"
"	add	r2, r2, #0x0004		\n"
"	/* Ensure the stack is properly aligned before calling C code. */\n"
"	bic	sp, sp, #7	\n"
"	sub	sp, sp, #8	\n"
"	str	r5, [sp, #4]	\n"
"	str	r4, [sp, #0]	\n"
"\n"
"	b	 __start  ");
/* ARGSUSED */
void
__start(int argc, char **argv, char **env, struct ps_strings *ps_strings,
    const struct Struct_Obj_Entry *obj __unused, void (*cleanup)(void))
{
	const char *s;

	environ = env;

	if (argc > 0 && argv[0] != NULL) {
		__progname = argv[0];
		for (s = __progname; *s != '\0'; s++)
			if (*s == '/')
				__progname = s + 1;
	}

	if (ps_strings != (struct ps_strings *)0)
		__ps_strings = ps_strings;

	if (&_DYNAMIC != NULL)
		atexit(cleanup);
	else
		_init_tls();
#ifdef GCRT
	atexit(_mcleanup);
#endif
	atexit(_fini);
#ifdef GCRT
	monstartup(&eprol, &etext);
#endif
	_init();
	exit( main(argc, argv, env) );
}

#ifdef GCRT
__asm__(".text");
__asm__("eprol:");
__asm__(".previous");
#endif

__asm__(".ident\t\"$FreeBSD$\"");
