/*
 * $XConsortium: XTextExt.c,v 11.26 91/01/06 11:48:29 rws Exp $
 *
 * Copyright 1989 Massachusetts Institute of Technology
 */

/*
Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation, and that the name of M.I.T. not be used in advertising or
publicity pertaining to distribution of the software without specific,
written prior permission.  M.I.T. makes no representations about the
suitability of this software for any purpose.  It is provided "as is"
without express or implied warranty.
*/


#include "Xlibint.h"

#define min_byte2 min_char_or_byte2
#define max_byte2 max_char_or_byte2


/* 
 * CI_GET_ROWZERO_CHAR_INFO_2D - do the same thing as CI_GET_CHAR_INFO_1D,
 * except that the font has more than one row.  This is special case of more
 * general version used in XTextExt16.c since row == 0.  This is used when
 * max_byte2 is not zero.  A further optimization would do the check for
 * min_byte1 being zero ahead of time.
 */

#define CI_GET_ROWZERO_CHAR_INFO_2D(fs,col,def,cs) \
{ \
    cs = def; \
    if (fs->min_byte1 == 0 && \
	col >= fs->min_byte2 && col <= fs->max_byte2) { \
	if (fs->per_char == NULL) { \
	    cs = &fs->min_bounds; \
	} else { \
	    cs = &fs->per_char[(col - fs->min_byte2)]; \
	    if (CI_NONEXISTCHAR(cs)) cs = def; \
	} \
    } \
}


/*
 * XTextExtents - compute the extents of string given as a sequences of eight
 * bit bytes.  Since we know that the input characters will always be from the
 * first row of the font (i.e. byte1 == 0), we can do some optimizations beyond
 * what is done in XTextExtents16.
 */
#if NeedFunctionPrototypes
XTextExtents (
    XFontStruct *fs,
    _Xconst char *string,
    int nchars,
    int *dir,           /* RETURN font information */
    int *font_ascent,   /* RETURN font information */
    int *font_descent,  /* RETURN font information */
    register XCharStruct *overall)	/* RETURN character information */
#else
XTextExtents (fs, string, nchars, dir, font_ascent, font_descent, overall)
    XFontStruct *fs;
    char *string;
    int nchars;
    int *dir, *font_ascent, *font_descent;  /* RETURN font information */
    register XCharStruct *overall;	/* RETURN character information */
#endif
{
    int i;				/* iterator */
    Bool singlerow = (fs->max_byte1 == 0);  /* optimization */
    int nfound = 0;			/* number of characters found */
    XCharStruct *def;			/* info about default char */
    unsigned char *us;  		/* be 8bit clean */

    if (singlerow) {			/* optimization */
	CI_GET_DEFAULT_INFO_1D (fs, def);
    } else {
	CI_GET_DEFAULT_INFO_2D (fs, def);
    }

    *dir = fs->direction;
    *font_ascent = fs->ascent;
    *font_descent = fs->descent;

    /*
     * Iterate over the input string getting the appropriate * char struct.
     * The default (which may be null if there is no def_char) will be returned
     * if the character doesn't exist.  On the first time * through the loop,
     * assign the values to overall; otherwise, compute * the new values.
     */

    for (i = 0, us = (unsigned char *) string; i < nchars; i++, us++) {
	register unsigned uc = (unsigned) *us;	/* since about to do macro */
	register XCharStruct *cs;

	if (singlerow) {		/* optimization */
	    CI_GET_CHAR_INFO_1D (fs, uc, def, cs);
	} else {
	    CI_GET_ROWZERO_CHAR_INFO_2D (fs, uc, def, cs);
	}

	if (cs) {
	    if (nfound++ == 0) {
		*overall = *cs;
	    } else {
		overall->ascent = max (overall->ascent, cs->ascent);
		overall->descent = max (overall->descent, cs->descent);
		overall->lbearing = min (overall->lbearing, 
					 overall->width + cs->lbearing);
		overall->rbearing = max (overall->rbearing,
					 overall->width + cs->rbearing);
		overall->width += cs->width;
	    }
	}
    }

    /*
     * if there were no characters, then set everything to 0
     */
    if (nfound == 0) {
	overall->width = overall->ascent = overall->descent = 
	  overall->lbearing = overall->rbearing = 0;
    }

    return;
}


/*
 * XTextWidth - compute the width of a string of eightbit bytes.  This is a 
 * subset of XTextExtents.
 */
#if NeedFunctionPrototypes
int XTextWidth (
    XFontStruct *fs,
    _Xconst char *string,
    int count)
#else
int XTextWidth (fs, string, count)
    XFontStruct *fs;
    char *string;
    int count;
#endif
{
    int i;				/* iterator */
    Bool singlerow = (fs->max_byte1 == 0);  /* optimization */
    XCharStruct *def;			/* info about default char */
    unsigned char *us;  		/* be 8bit clean */
    int width = 0;			/* RETURN value */

    if (singlerow) {			/* optimization */
	CI_GET_DEFAULT_INFO_1D (fs, def);
    } else {
	CI_GET_DEFAULT_INFO_2D (fs, def);
    }

    /*
     * Iterate over all character in the input string; only consider characters
     * that exist.
     */
    for (i = 0, us = (unsigned char *) string; i < count; i++, us++) {
	register unsigned uc = (unsigned) *us;	/* since about to do macro */
	register XCharStruct *cs;

	if (singlerow) {		/* optimization */
	    CI_GET_CHAR_INFO_1D (fs, uc, def, cs);
	} else {
	    CI_GET_ROWZERO_CHAR_INFO_2D (fs, uc, def, cs);
	}

	if (cs) width += cs->width;
    }

    return width;
}
