/*
 * Copyright (c) 1981 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)clear.c	5.6 (Berkeley) %G%";
#endif	/* not lint */

#include <curses.h>

/*
 * wclear --
 *	Clear the window.
 */
int
wclear(win)
	register WINDOW *win;
{
	if (werase(win) == OK) {
		win->flags |= __CLEAROK;
		return (OK);
	}
	return (ERR);
}
