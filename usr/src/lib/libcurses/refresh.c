/*
 * Copyright (c) 1981 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)refresh.c	5.16 (Berkeley) %G%";
#endif /* not lint */

#include <curses.h>
#include <string.h>

/* Equality of characters in terms of standout */
#define __SOEQ(a, b)	!(((a) & __STANDOUT) ^ ((b) & __STANDOUT))

static int curwin;
static short ly, lx;

int doqch = 1;

WINDOW *_win;

static void	domvcur __P((int, int, int, int));
static int	makech __P((WINDOW *, int));
static void	quickch __P((WINDOW *));	
static void	scrolln __P((WINDOW *, int, int, int, int, int));
/*
 * wrefresh --
 *	Make the current screen look like "win" over the area coverd by
 *	win.
 */
int
wrefresh(win)
	register WINDOW *win;
{
	register LINE *wlp;
	register int retval;
	register short wy;

	/* Make sure were in visual state. */
	if (__endwin) {
		tputs(VS, 0, __cputchar);
		tputs(TI, 0, __cputchar);
		__endwin = 0;
	}

	/* Initialize loop parameters. */

	ly = curscr->cury;
	lx = curscr->curx;
	wy = 0;
	_win = win;
	curwin = (win == curscr);

	if (!curwin)
		for (wy = 0; wy < win->maxy; wy++) {
			wlp = win->lines[wy];
			if (wlp->flags & __ISDIRTY)
				/* need standout characteristics as well */
				wlp->hash = __hash(wlp->line, 2 * win->maxx);
		}

	if (win->flags & __CLEAROK || curscr->flags & __CLEAROK || curwin) {
		if ((win->flags & __FULLWIN) || curscr->flags & __CLEAROK) {
			tputs(CL, 0, __cputchar);
			ly = 0;
			lx = 0;
			if (!curwin) {
				curscr->flags &= ~__CLEAROK;
				curscr->cury = 0;
				curscr->curx = 0;
				werase(curscr);
			}
			touchwin(win);
		}
		win->flags &= ~__CLEAROK;
	}
	if (!CA) {
		if (win->curx != 0)
			putchar('\n');
		if (!curwin)
			werase(curscr);
	}
#ifdef DEBUG
	__TRACE("wrefresh: (%0.2o): curwin = %d\n", win, curwin);
	__TRACE("wrefresh: \tfirstch\tlastch\n");
#endif

#ifndef NOQCH
	if (!__noqch && (win->flags & __FULLWIN) && !curwin)
    		quickch(win);
#endif
	for (wy = 0; wy < win->maxy; wy++) {
#ifdef DEBUG
		__TRACE("%d\t%d\t%d\n",
		    wy, win->lines[wy]->firstch, win->lines[wy]->lastch);
#endif
		if (!curwin)
			curscr->lines[wy]->hash = win->lines[wy]->hash;
		if (win->lines[wy]->flags & __ISDIRTY)
			if (makech(win, wy) == ERR)
				return (ERR);
			else {
				if (win->lines[wy]->firstch >= win->ch_off)
					win->lines[wy]->firstch = win->maxx +
					    win->ch_off;
				if (win->lines[wy]->lastch < win->maxx +
				    win->ch_off)
					win->lines[wy]->lastch = win->ch_off;
				if (win->lines[wy]->lastch < 
				    win->lines[wy]->firstch)
					win->lines[wy]->flags &= ~__ISDIRTY;
			}
#ifdef DEBUG
		__TRACE("\t%d\t%d\n", win->lines[wy]->firstch, 
			win->lines[wy]->lastch);
#endif
	}
	
#ifdef DEBUG
	__TRACE("refresh: ly=%d, lx=%d\n", ly, lx);
#endif

	if (win == curscr)
		domvcur(ly, lx, win->cury, win->curx);
	else {
		if (win->flags & __LEAVEOK) {
			curscr->cury = ly;
			curscr->curx = lx;
			ly -= win->begy;
			lx -= win->begx;
			if (ly >= 0 && ly < win->maxy && lx >= 0 &&
			    lx < win->maxx) {
				win->cury = ly;
				win->curx = lx;
			} else
				win->cury = win->curx = 0;
		} else {
			domvcur(ly, lx, win->cury + win->begy,
			    win->curx + win->begx);
			curscr->cury = win->cury + win->begy;
			curscr->curx = win->curx + win->begx;
		}
	}
	retval = OK;

	_win = NULL;
	(void)fflush(stdout);
	return (retval);
}

/*
 * makech --
 *	Make a change on the screen.
 */
static int
makech(win, wy)
	register WINDOW *win;
	int wy;
{
	register int nlsp, clsp;		/* Last space in lines. */
	register short wx, lch, y;
	register char *nsp, *csp, *ce, *nsop, *csop;
	char zero, soeq;
	
	zero = '\0';
	/* Is the cursor still on the end of the last line? */
	if (wy > 0 && win->lines[wy - 1]->flags & __ISPASTEOL) {
		win->lines[wy - 1]->flags &= ~__ISPASTEOL;
		domvcur(ly, lx, ly + 1, 0);
		ly++;
		lx = 0;
	}
	if (!(win->lines[wy]->flags & __ISDIRTY))
		return (OK);
	wx = win->lines[wy]->firstch - win->ch_off;
	if (wx >= win->maxx)
		return (OK);
	else if (wx < 0)
		wx = 0;
	lch = win->lines[wy]->lastch - win->ch_off;
	if (lch < 0)
		return (OK);
	else if (lch >= win->maxx)
		lch = win->maxx - 1;
	y = wy + win->begy;

	if (curwin)
		csp = " ";
	else
		csp = &curscr->lines[wy + win->begy]->line[wx + win->begx];

	nsp = &win->lines[wy]->line[wx];
	if (CE && !curwin) {
		for (ce = &win->lines[wy]->line[win->maxx - 1]; 
		     *ce == ' ' && !(*(ce + win->maxx) & __STANDOUT); ce--)
			if (ce <= win->lines[wy]->line)
				break;
		nlsp = ce - win->lines[wy]->line;
	}
	if (!curwin)
		ce = CE;
	else
		ce = NULL;

	while (wx <= lch) {
		if (*nsp == *csp && 
		    __SOEQ(*(nsp + win->maxx), *(csp + win->maxx))){
			if (wx <= lch) {
				while (*nsp == *csp && 
				    __SOEQ(*(nsp + win->maxx), 
				    *(csp + win->maxx)) &&
				    wx <= lch) {
					    nsp++;
					if (!curwin)
						csp++;
					++wx;
				}
				continue;
			}
			break;
		}
		domvcur(ly, lx, y, wx + win->begx);

#ifdef DEBUG
		__TRACE("makech: 1: wx = %d, ly= %d, lx = %d, newy = %d, newx = %d\n", 
		    wx, ly, lx, y, wx + win->begx);
#endif
		ly = y;
		lx = wx + win->begx;
		nsop = nsp + win->maxx;
		if (!curwin)
			csop = csp + win->maxx;
		else
			csop = &zero;
		while ((*nsp != *csp || !__SOEQ(*nsop, *csop)) && wx <= lch) {
#ifdef notdef
			/* XXX
			 * The problem with this code is that we can't count on
			 * terminals wrapping around after the 
			 * last character on the previous line has been output
			 * In effect, what then could happen is that the CE 
			 * clear the previous line and do nothing to the
			 * next line.
			 */
			if (ce != NULL && wx >= nlsp && *nsp == ' ') {
				/* Check for clear to end-of-line. */
				ce = &curscr->lines[wy]->line[COLS - 1];
				while (*ce == ' ')
					if (ce-- <= csp)
						break;
				clsp = ce - curscr->lines[wy]->line - 
				       win->begx;
#ifdef DEBUG
			__TRACE("makech: clsp = %d, nlsp = %d\n", clsp, nlsp);
#endif
				if (clsp - nlsp >= strlen(CE) &&
				    clsp < win->maxx) {
#ifdef DEBUG
					__TRACE("makech: using CE\n");
#endif
					tputs(CE, 0, __cputchar);
					lx = wx + win->begx;
					while (wx++ <= clsp) {
						*csp++ = ' ';
						*csop++ &= ~__STANDOUT;
					}
					return (OK);
				}
				ce = NULL;
			}
#endif

			/* Enter/exit standout mode as appropriate. */
			if (SO && (*nsop & __STANDOUT) !=
			    (curscr->flags & __WSTANDOUT)) {
				if (*nsop & __STANDOUT) {
					tputs(SO, 0, __cputchar);
					curscr->flags |= __WSTANDOUT;
				} else {
					tputs(SE, 0, __cputchar);
					curscr->flags &= ~__WSTANDOUT;
				}
			}

			wx++;
			if (wx >= win->maxx && wy == win->maxy - 1 && !curwin)
				if (win->flags & __SCROLLOK) {
					if (curscr->flags & __WSTANDOUT
					    && win->flags & __ENDLINE)
						if (!MS) {
							tputs(SE, 0,
							    __cputchar);
							curscr->flags &=
							    ~__WSTANDOUT;
						}
					if (!curwin) {
						*csop = *nsop;
						putchar(*csp = *nsp);
					} else
						putchar(*nsp);
#ifdef notdef
					if (win->flags & __FULLWIN && !curwin)
						scroll(curscr);
#endif
					domvcur(ly, wx + win->begx, 
						win->begy + win->maxy - 1,
						win->begx + win->maxx - 1);
					ly = win->begy + win->maxy - 1;
					lx = win->begx + win->maxx - 1;
					return (OK);
				} else
					if (win->flags & __SCROLLWIN) {
						lx = --wx;
						return (ERR);
					}
			if (!curwin) {
				*csop++ = *nsop;
				putchar(*csp++ = *nsp);
			} else
				putchar(*nsp);
			
#ifdef DEBUG
			__TRACE("makech: putchar(%c)\n", *nsp & 0177);
#endif
			if (UC && (*nsop & __STANDOUT)) {
				putchar('\b');
				tputs(UC, 0, __cputchar);
			}
			nsp++;
			nsop++;
		}
#ifdef DEBUG
		__TRACE("makech: 2: wx = %d, lx = %d\n", wx, lx);
#endif
		if (lx == wx + win->begx)	/* If no change. */
			break;
		lx = wx + win->begx;
		if (lx >= COLS && AM) {
			/*
			 * xn glitch: chomps a newline after auto-wrap.
			 * we just feed it now and forget about it.
			 */
			if (XN) {
				lx = 0;
				ly++;
				putchar('\n');
				putchar('\r');
			} else {
				if (wy != LINES)
					win->lines[wy]->flags |= __ISPASTEOL;
				lx = COLS - 1;
			}
		} else if (wx >= win->maxx) {
			if (wy != win->maxy)
				win->lines[wy]->flags |= __ISPASTEOL;
			domvcur(ly, lx, ly, win->maxx + win->begx - 1);
			lx = win->maxx + win->begx - 1;
		}

#ifdef DEBUG
		__TRACE("makech: 3: wx = %d, lx = %d\n", wx, lx);
#endif
	}
	return (OK);
}

/*
 * domvcur --
 *	Do a mvcur, leaving standout mode if necessary.
 */
static void
domvcur(oy, ox, ny, nx)
	int oy, ox, ny, nx;
{
	if (curscr->flags & __WSTANDOUT && !MS) {
		tputs(SE, 0, __cputchar);
		curscr->flags &= ~__WSTANDOUT;
	}

	mvcur(oy, ox, ny, nx);
}

/*
 * Quickch() attempts to detect a pattern in the change of the window
 * in order to optimize the change, e.g., scroll n lines as opposed to 
 * repainting the screen line by line.
 */

static void
quickch(win)
	WINDOW *win;
{
#define THRESH		win->maxy / 4

	register LINE *clp, *tmp1, *tmp2;
	register int bsize, curs, curw, starts, startw, i, j;
	int n, target, remember, bot, top;
	char buf[1024];
	u_int blank_hash;

	/*
	 * Search for the largest block of text not changed.
         */
	for (bsize = win->maxy; bsize >= THRESH; bsize--)
		for (startw = 0; startw <= win->maxy - bsize; startw++)
			for (starts = 0; starts <= win->maxy - bsize; 
			     starts++) {
				for (curw = startw, curs = starts;
				     curs < starts + bsize; curw++, curs++)
					if (win->lines[curw]->hash !=
					    curscr->lines[curs]->hash ||
				            bcmp(&win->lines[curw], 
					    &curscr->lines[curs], 
					    2 * win->maxx) != 0)
						break;
				if (curs == starts + bsize)
					goto done;
			}
 done:
	/* Did not find anything or block is in correct place already. */
	if (bsize < THRESH || starts == startw)	
		return;

	/* 
	 * Find how many lines from the top of the screen are unchanged.
	 */
	if (starts != 0) {
		for (top = 0; top < win->maxy; top++)
			if (win->lines[top]->hash != curscr->lines[top]->hash 
			    || bcmp(&win->lines[top], &curscr->lines[top], 
				 2 * win->maxx) != 0)
				break;
	} else
		top = 0;
	
       /*
	* Find how many lines from bottom of screen are unchanged. 
	*/
	if (curs != win->maxy) {
		for (bot = win->maxy - 1; bot >= 0; bot--)
			if (win->lines[bot]->hash != curscr->lines[bot]->hash 
			    || bcmp(&win->lines[bot], &curscr->lines[bot], 
				 2 * win->maxx) != 0)
				break;
	} else
		bot = win->maxy - 1;

#ifdef DEBUG
	__TRACE("quickch:bsize=%d,starts=%d,startw=%d,curw=%d,curs=%d,top=%d,bot=%d\n", 
		bsize, starts, startw, curw, curs, top, bot);
#endif

	/* 
	 * Make sure that there is no overlap between the bottom and top 
	 * regions and the middle scrolled block.
	 */
	if (bot < curw)
		bot = curw - 1;
	if (top > startw)
		top = startw;

	scrolln(win, starts, startw, curs, top, bot);

	n = startw - starts;

	/* So we don't have to call __hash() each time */
	(void)memset(buf, ' ', win->maxx);
	(void)memset(buf + win->maxx, '\0', win->maxx);
	blank_hash = __hash(buf, 2 * win->maxx);

	/*
	 * Perform the rotation to maintain the consistency of curscr.
	 */
	i = 0;
	tmp1 = curscr->lines[0];
	remember = 0;
	for (j = 0; j < win->maxy; j++) {
		target = (i + n + win->maxy) % win->maxy;
		tmp2 = curscr->lines[target];
		curscr->lines[target] = tmp1;
		/* Mark block as clean and blank out scrolled lines. */
		clp = curscr->lines[target];
#ifdef DEBUG
		__TRACE("quickch: n=%d startw=%d curw=%d i = %d target=%d ",
			n, startw, curw, i, target);
#endif
		if (target >= startw && target < curw || target < top || 
		    target > bot) {
#ifdef DEBUG
			__TRACE("-- notdirty");
#endif
			win->lines[target]->flags &= ~__ISDIRTY;
		} else if ((n < 0 && target >= win->maxy + n) || 
			 (n > 0 && target < n)) {
			if (clp->hash != blank_hash || 
			    bcmp(clp->line, buf, 2 * win->maxx) != 0) {
				(void)bcopy(clp->line, buf, 2 * win->maxx);
#ifdef DEBUG
				__TRACE("-- bcopy ");
#endif
				clp->hash = blank_hash;
			} else 
#ifdef DEBUG
				__TRACE(" -- no bcopy");
#endif
			touchline(win, target, 0, win->maxx - 1);
		} else {
#ifdef DEBUG
			__TRACE(" -- just dirty");
#endif
			touchline(win, target, 0, win->maxx - 1);
		}
#ifdef DEBUG
		__TRACE("\n");
#endif
		if (target == remember) {
			i = target + 1;
			tmp1 = curscr->lines[i];
			remember = i;
		} else {
			tmp1 = tmp2;
			i = target;
		}
	}
#ifdef DEBUG
	__TRACE("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
	for (i = 0; i < curscr->maxy; i++)
		__TRACE("Q: %d: %.70s\n", i,
	           curscr->lines[i]->line);
#endif
}

static void
scrolln(win, starts, startw, curs, top, bot)
	WINDOW *win;
	int starts, startw, curs, top, bot;
{
	int i, oy, ox, n;

	oy = curscr->cury;
	ox = curscr->curx;
	n = starts - startw;

	if (n > 0) {
		mvcur(oy, ox, top, 0);
		/* Scroll up the block */
		if (DL)
			tputs(tscroll(DL, n), 0, __cputchar);
		else
			for(i = 0; i < n; i++)
				tputs(dl, 0, __cputchar);
		/* Push back down the bottom region */
		if (bot < win->maxy - 1) {
			mvcur(top, 0, bot - n + 1, 0);
			if (AL)
				tputs(tscroll(AL, n), 0, __cputchar);
			else
				for(i = 0; i < n; i++)
					tputs(al, 0, __cputchar);
			mvcur(bot - n + 1, 0, oy, ox);
		} else
			mvcur(top, 0, oy, ox);
	} else {
		/* Preserve the bottom lines. (Pull them up) */
		if (bot < win->maxy - 1) {
			mvcur(oy, ox, curs, 0);
			if (DL)
				tputs(tscroll(DL, -n), 0, __cputchar);
			else
			       	for(i = n; i < 0; i++)
					tputs(dl, 0, __cputchar);
			mvcur(curs, 0, starts, 0);
		} else
			mvcur(oy, ox, starts,  0);

		/* Scroll the block down */
		if (AL)
			tputs(tscroll(AL, -n), 0, __cputchar);
		else
			for(i = n; i < 0; i++)
				tputs(al, 0, __cputchar);
		mvcur(starts, 0, oy, ox);
	}		
}


