/*
 *	Copyright (c) 1984, 1985, 1986 by the Regents of the
 *	University of California and by Gregory Glenn Minshall.
 *
 *	Permission to use, copy, modify, and distribute these
 *	programs and their documentation for any purpose and
 *	without fee is hereby granted, provided that this
 *	copyright and permission appear on all copies and
 *	supporting documentation, the name of the Regents of
 *	the University of California not be used in advertising
 *	or publicity pertaining to distribution of the programs
 *	without specific prior permission, and notice be given in
 *	supporting documentation that copying and distribution is
 *	by permission of the Regents of the University of California
 *	and by Gregory Glenn Minshall.  Neither the Regents of the
 *	University of California nor Gregory Glenn Minshall make
 *	representations about the suitability of this software
 *	for any purpose.  It is provided "as is" without
 *	express or implied warranty.
 */

#ifndef lint
static	char	sccsid[] = "@(#)outbound.c	3.1  10/29/86";
#endif	/* lint */


#if	defined(unix)
#include <signal.h>
#endif	/* defined(unix) */
#include <stdio.h>

#include "hostctlr.h"
#include "screen.h"
#include "ebc_disp.h"

#include "../system/globals.h"
#include "options.ext"
#include "../telnet.ext"
#include "inbound.ext"
#include "outbound.ext"
#include "bsubs.ext"

#define SetHighestLowest(position) { \
					if (position < Lowest) { \
					    Lowest = position; \
					} \
					if (position > Highest) { \
					    Highest = position; \
					} \
				    }

#if	defined(unix)
extern int	tin, tout;		/* file descriptors */
#endif	/* defined(unix) */


#if	defined(unix)
static int tcflag = -1;			/* transparent mode command flag */
static int savefd[2];			/* for storing fds during transcom */
#endif	/* defined(unix) */

/* some globals */

int	OutputClock = 0;		/* what time it is */
int	TransparentClock = 0;		/* time we were last in transparent */


char CIABuffer[64] = {
    0x40, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f
};

/* What we know is that table is of size ScreenSize */

FieldFind(table, position, failure)
register char *table;		/* which table of bytes to use */
register int position;		/* what position to start from */
int failure;			/* if unformatted, what value to return */
{
    register int ourp;

    ourp = position + 1 + bskip(table+position+1, ScreenSize-position-1, 0);
    if (ourp < ScreenSize) {
	return(ourp);
    }
    /* No fields in table after position.  Look for fields from beginning
     * of table.
     */
    ourp = bskip(table, position+1, 0);
    if (ourp <= position) {
	return(ourp);
    }
    return(failure);
}

/* Clear3270 - called to clear the screen */

void
Clear3270()
{
    bzero((char *)Host, sizeof(Host));
    DeleteAllFields();		/* get rid of all fields */
    BufferAddress = SetBufferAddress(0,0);
    CursorAddress = SetBufferAddress(0,0);
    Lowest = LowestScreen();
    Highest = HighestScreen();
}

/* AddHost - called to add a character to the buffer.
 *	We use a macro in this module, since we call it so
 *	often from loops.
 *
 *	NOTE: It is a macro, so don't go around using AddHost(p, *c++), or
 *	anything similar.  (I don't define any temporary variables, again
 *	just for the speed.)
 */
void
AddHost(position, character)
int	position;
char	character;
{
#if	defined(SLOWSCREEN)
#   define	AddHostA(p,c)					\
    {								\
	if (IsStartField(p)) {					\
	    DeleteField(p);					\
	    Highest = HighestScreen();				\
	    Lowest = LowestScreen();				\
	    SetHighestLowest(p);				\
	}							\
	SetHost(p, c);						\
    }
#   define	AddHost(p,c)					\
    {								\
	if (c != GetHost(p)) {					\
	    SetHighestLowest(p);				\
	}							\
	AddHostA(p,c);						\
    }	/* end of macro of AddHost */
#else	/* defined(SLOWSCREEN) */
#   define	AddHost(p,c)					\
    {								\
	if (IsStartField(p)) {					\
	    DeleteField(p);					\
	    Highest = HighestScreen();				\
	    Lowest = LowestScreen();				\
	    SetHost(p, c);					\
	} else {						\
	    SetHost(p, c);					\
	    SetHighestLowest(p);				\
	}							\
    }	/* end of macro of AddHost */
#endif	/* defined(SLOWSCREEN) */

    AddHost(position, character);
}

/* returns the number of characters consumed */
int
DataFromNetwork(buffer, count, control)
register unsigned char	*buffer;		/* what the data is */
register int	count;				/* and how much there is */
int	control;				/* this buffer ended block? */
{
    int origCount;
    register int c;
    register int i;
    static int Command;
    static int Wcc;
    static int	LastWasTerminated = 1;	/* was "control" = 1 last time? */
#if	defined(unix)
    extern char *transcom;
    int inpipefd[2], outpipefd[2], savemode, aborttc();
#endif	/* defined(unix) */

    origCount = count;

    if (LastWasTerminated) {

	if (count < 2) {
	    if (count == 0) {
		StringToTerminal("Short count received from host!\n");
		return(count);
	    }
	    Command = buffer[0];
	    switch (Command) {		/* This had better be a read command */
	    case CMD_READ_MODIFIED:
	    case CMD_SNA_READ_MODIFIED:
	    case CMD_SNA_READ_MODIFIED_ALL:
		DoReadModified(Command);
		break;
	    case CMD_READ_BUFFER:
	    case CMD_SNA_READ_BUFFER:
		DoReadBuffer();
		break;
	    default:
		break;
	    }
	    return(1);			/* We consumed everything */
	}
	Command = buffer[0];
	Wcc = buffer[1];
	if (Wcc & WCC_RESET_MDT) {
	    i = c = WhereAttrByte(LowestScreen());
	    do {
		if (HasMdt(i)) {
		    TurnOffMdt(i);
		}
		i = FieldInc(i);
	    } while (i != c);
	}

	switch (Command) {
	case CMD_ERASE_WRITE:
	case CMD_ERASE_WRITE_ALTERNATE:
	case CMD_SNA_ERASE_WRITE:
	case CMD_SNA_ERASE_WRITE_ALTERNATE:
	    {
		int newlines, newcolumns;

#if	defined(unix)
		if (tcflag == 0) {
		   tcflag = -1;
		   (void) signal(SIGCHLD, SIG_DFL);
		} else if (tcflag > 0) {
		   setcommandmode();
		   (void) close(tin);
		   (void) close(tout);
		   tin = savefd[0];
		   tout = savefd[1];
		   setconnmode();
		   tcflag = -1;
		   (void) signal(SIGCHLD, SIG_DFL);
		}
#endif	/* defined(unix) */
		if ((Command == CMD_ERASE_WRITE)
				|| (Command == CMD_SNA_ERASE_WRITE)) {
		    newlines = 24;
		    newcolumns = 80;
		} else {
		    newlines = MaxNumberLines;
		    newcolumns = MaxNumberColumns;
		}
		if ((newlines != NumberLines)
				|| (newcolumns != NumberColumns)) {
			/*
			 * The LocalClearScreen() is really for when we
			 * are going from a larger screen to a smaller
			 * screen, and we need to clear off the stuff
			 * at the end of the lines, or the lines at
			 * the end of the screen.
			 */
		    LocalClearScreen();
		    NumberLines = newlines;
		    NumberColumns = newcolumns;
		    ScreenSize = NumberLines * NumberColumns;
		}
		Clear3270();
		if (TransparentClock == OutputClock) {
		    RefreshScreen();
		}
		break;
	    }

	case CMD_ERASE_ALL_UNPROTECTED:
	case CMD_SNA_ERASE_ALL_UNPROTECTED:
	    CursorAddress = HighestScreen()+1;
	    for (i = LowestScreen(); i <= HighestScreen(); i = ScreenInc(i)) {
		if (IsUnProtected(i)) {
		    if (CursorAddress > i) {
			CursorAddress = i;
		    }
		    AddHost(i, '\0');
		}
		if (HasMdt(i)) {
		    TurnOffMdt(i);
		}
	    }
	    if (CursorAddress == HighestScreen()+1) {
		CursorAddress = SetBufferAddress(0,0);
	    }
	    UnLocked = 1;
	    AidByte = 0;
	    break;
	case CMD_WRITE:
	case CMD_SNA_WRITE:
	    break;
	default:
	    {
		char buffer[100];

		sprintf(buffer, "Unexpected command code 0x%x received.\n",
								Command);
		ExitString(stderr, buffer, 1);
		break;
	    }
	}

	count -= 2;			/* strip off command and wcc */
	buffer += 2;

    }
    LastWasTerminated = 0;		/* then, reset at end... */

    while (count) {
	count--;
	c = *buffer++;
	if (IsOrder(c)) {
	    /* handle an order */
	    switch (c) {
#		define Ensure(x)	if (count < x) { \
					    if (!control) { \
						return(origCount-(count+1)); \
					    } else { \
						/* XXX - should not occur */ \
						count = 0; \
						break; \
					    } \
					}
	    case ORDER_SF:
		Ensure(1);
		c = *buffer++;
		count--;
		if ( ! (IsStartField(BufferAddress) &&
					FieldAttributes(BufferAddress) == c)) {
		    SetHighestLowest(BufferAddress);
		    NewField(BufferAddress,c);
		}
		BufferAddress = ScreenInc(BufferAddress);
		break;
	    case ORDER_SBA:
		Ensure(2);
		i = buffer[0];
		c = buffer[1];
		if (!i && !c) { /* transparent write */
		    if (!control) {
			return(origCount-(count+1));
		    } else {
			while (DoTerminalOutput() == 0) {
#if defined(unix)
			    HaveInput = 0;
#endif /* defined(unix) */
			}
			TransparentClock = OutputClock; 	/* this clock */
#if	defined(unix)
			if (transcom && tcflag == -1) {
			   while (1) {			  /* go thru once */
				 if (pipe(outpipefd) < 0) {
				    break;
				 }
				 if (pipe(inpipefd) < 0) {
				    break;
				 }
			         if ((tcflag = fork()) == 0) {
				    (void) close(outpipefd[1]);
				    (void) close(0);
				    if (dup(outpipefd[0]) < 0) {
				       exit(1);
				    }
				    (void) close(outpipefd[0]);
				    (void) close(inpipefd[0]);
				    (void) close(1);
				    if (dup(inpipefd[1]) < 0) {
				       exit(1);
				    }
				    (void) close(inpipefd[1]);
				    if (execl("/bin/csh", "csh", "-c", transcom, (char *) 0)) {
					exit(1);
				    }
				 }
				 (void) close(inpipefd[1]);
				 (void) close(outpipefd[0]);
				 savefd[0] = tin;
				 savefd[1] = tout;
				 setcommandmode();
				 tin = inpipefd[0];
				 tout = outpipefd[1];
				 (void) signal(SIGCHLD, aborttc);
				 setconnmode();
				 tcflag = 1;
				 break;
			   }
			   if (tcflag < 1) {
			      tcflag = 0;
			   }
			}
#endif	/* defined(unix) */
			(void) DataToTerminal(buffer+2, count-2);
			SendToIBM();
			TransparentClock = OutputClock+1;	/* clock next */
			buffer += count;
			count -= count;
		    }
		} else {
		    BufferAddress = Addr3270(i, c);
		    buffer += 2;
		    count -= 2;
		}
		break;
	    case ORDER_IC:
		CursorAddress = BufferAddress;
		break;
	    case ORDER_PT:
		for (i = ScreenInc(BufferAddress); (i != HighestScreen());
				i = ScreenInc(i)) {
		    if (IsStartField(i)) {
			i = ScreenInc(i);
			if (!IsProtected(ScreenInc(i))) {
			    break;
			}
			if (i == HighestScreen()) {
			    break;
			}
		    }
		}
		CursorAddress = i;
		break;
	    case ORDER_RA:
		Ensure(3);
		i = Addr3270(buffer[0], buffer[1]);
		c = buffer[2];
		do {
		    AddHost(BufferAddress, ebc_disp[c]);
		    BufferAddress = ScreenInc(BufferAddress);
		} while (BufferAddress != i);
		buffer += 3;
		count -= 3;
		break;
	    case ORDER_EUA:    /* (from [here,there), ie: half open interval] */
		Ensure(2);
		c = FieldAttributes(WhereAttrByte(BufferAddress));
		for (i = Addr3270(buffer[0], buffer[1]); i != BufferAddress;
				BufferAddress = ScreenInc(BufferAddress)) {
		    if (!IsProtectedAttr(BufferAddress, c)) {
			AddHost(BufferAddress, 0);
		    }
		}
		buffer += 2;
		count -= 2;
		break;
	    case ORDER_YALE:		/* special YALE defined order */
		Ensure(2);	/* need at least two characters */
		if (*buffer == 0x5b) {
		    i = OptOrder(buffer+1, count-1, control);
		    if (i == 0) {
			return(origCount-(count+1));	/* come here again */
		    } else {
			buffer += 1 + i;
			count  -= (1 + i);
		    }
		}
		break;
	    default:
		break;				/* XXX ? */
	    }
	    if (count < 0) {
		count = 0;
	    }
	} else {
	    /* Data comes in large clumps - take it all */
	    i = BufferAddress;
#if	!defined(SLOWSCREEN)
	    AddHost(i, ebc_disp[c]);
#else	/* !defined(SLOWSCREEN) */
	    AddHostA(i, ebc_disp[c]);
	    SetHighestLowest(i);
#endif	/* !defined(SLOWSCREEN) */
	    i = ScreenInc(i);
	    c = *buffer;
	    while (count && !IsOrder(c)) {
#if	!defined(SLOWSCREEN)
		AddHost(i, ebc_disp[c]);
#else	/* !defined(SLOWSCREEN) */
		AddHostA(i, ebc_disp[c]);
#endif	/* !defined(SLOWSCREEN) */
		i = ScreenInc(i);
#if	defined(SLOWSCREEN)
		if (i == LowestScreen()) {
		    SetHighestLowest(HighestScreen());
		}
#endif	/* defined(SLOWSCREEN) */
		count--;
		buffer++;
		c = *buffer;
	    }
#if	defined(SLOWSCREEN)
	    SetHighestLowest(i);
#endif	/* defined(SLOWSCREEN) */
	    BufferAddress = i;
	}
    }
    if (count == 0) {
	OutputClock++;		/* time rolls on */
	if (control) {
	    if (Wcc & WCC_RESTORE) {
		if (TransparentClock != OutputClock) {
		    AidByte = 0;
		}
		UnLocked = 1;
	    }
	    if (Wcc & WCC_ALARM) {
		RingBell(0);
	    }
	}
	LastWasTerminated = control;	/* state for next time */
	return(origCount);
    } else {
	return(origCount-count);
    }
}


#if	defined(unix)
aborttc()
{
	int savemode;

	setcommandmode();
	(void) close(tin);
	(void) close(tout);
	tin = savefd[0];
	tout = savefd[1];
	setconnmode();
	tcflag = 0;
}
#endif	/* defined(unix) */
