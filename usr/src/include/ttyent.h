/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)ttyent.h	5.4 (Berkeley) %G%
 */

#define	_PATH_TTYS	"/etc/ttys"

#define	_TTYS_OFF	"off"
#define	_TTYS_ON	"on"
#define	_TTYS_SECURE	"secure"
#define	_TTYS_WINDOW	"window"

struct ttyent {
	char	*ty_name;	/* terminal device name */
	char	*ty_getty;	/* command to execute, usually getty */
	char	*ty_type;	/* terminal type for termcap */
#define	TTY_ON		0x01	/* enable logins (start ty_getty program) */
#define	TTY_SECURE	0x02	/* allow uid of 0 to login */
	int	ty_status;	/* status flags */
	char 	*ty_window;	/* command to start up window manager */
	char	*ty_comment;	/* comment field */
};

#if __STDC__ || c_plusplus
extern struct ttyent *getttyent(void);
extern struct ttyent *getttynam(const char *);
extern int setttyent(void);
extern int endttyent(void);
#else
extern struct ttyent *getttyent();
extern struct ttyent *getttynam();
extern int setttyent();
extern int endttyent();
#endif
