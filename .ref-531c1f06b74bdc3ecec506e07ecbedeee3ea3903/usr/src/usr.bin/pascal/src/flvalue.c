/* Copyright (c) 1980 Regents of the University of California */

#ifndef lint
static char sccsid[] = "@(#)flvalue.c 1.15 %G%";
#endif

#include "whoami.h"
#include "0.h"
#include "tree.h"
#include "opcode.h"
#include "objfmt.h"
#include "tree_ty.h"
#ifdef PC
#   include "pc.h"
#   include "pcops.h"
#endif PC
#include "tmps.h"

    /*
     *	flvalue generates the code to either pass on a formal routine,
     *	or construct the structure which is the environment for passing.
     *	it tells the difference by looking at the tree it's given.
     */
struct nl *
flvalue( r , formalp )
    struct tnode *r; 	/* T_VAR */
    struct nl	*formalp;
    {
	struct nl	*p;
	struct nl	*tempnlp;
	char		*typename;
#ifdef PC
	char		extname[ BUFSIZ ];
#endif PC

	if ( r == TR_NIL ) {
	    return NLNIL;
	}
	typename = formalp -> class == FFUNC ? "function":"procedure";
	if ( r->tag != T_VAR ) {
	    error("Expression given, %s required for %s parameter %s" ,
		    typename , typename , formalp -> symbol );
	    return NLNIL;
	}
	p = lookup(r->var_node.cptr);
	if (p == NLNIL) {
	    return NLNIL;
	}
	switch ( p -> class ) {
	    case FFUNC:
	    case FPROC:
		    if ( r->var_node.qual != TR_NIL ) {
			error("Formal %s %s cannot be qualified" ,
				typename , p -> symbol );
			return NLNIL;
		    }
#		    ifdef OBJ
			(void) put(2, PTR_RV | bn << 8+INDX, (int)p->value[NL_OFFS]);
#		    endif OBJ
#		    ifdef PC
			putRV( p -> symbol , bn , p -> value[ NL_OFFS ] , 
				p -> extra_flags ,
				p2type( p ) );
#		    endif PC
		    return p;
	    case FUNC:
	    case PROC:
		    if ( r->var_node.qual != TR_NIL ) {
			error("%s %s cannot be qualified" , typename ,
				p -> symbol );
			return NLNIL;
		    }
		    if (bn == 0) {
			error("Built-in %s %s cannot be passed as a parameter" ,
				typename , p -> symbol );
			return NLNIL;
		    }
			/*
			 *	allocate space for the thunk
			 */
		    tempnlp = tmpalloc((long) (sizeof(struct formalrtn)), NLNIL, NOREG);
#		    ifdef OBJ
			(void) put(2 , O_LV | cbn << 8 + INDX ,
				(int)tempnlp -> value[ NL_OFFS ] );
			(void) put(2, O_FSAV | bn << 8, (long)p->value[NL_ENTLOC]);
#		    endif OBJ
#		    ifdef PC
			putleaf( P2ICON , 0 , 0 ,
			    ADDTYPE( P2PTR , ADDTYPE( P2FTN , P2PTR|P2STRTY ) ) ,
			    "_FSAV" );
			sprintf( extname , "%s" , FORMALPREFIX );
			sextname( &extname[ strlen( extname ) ] ,
				    p -> symbol , bn );
			putleaf( P2ICON , 0 , 0 , p2type( p ) , extname );
			putleaf( P2ICON , bn , 0 , P2INT , (char *) 0 );
			putop( P2LISTOP , P2INT );
			putLV( (char *) 0 , cbn , tempnlp -> value[NL_OFFS] ,
				tempnlp -> extra_flags , P2STRTY );
			putop( P2LISTOP , P2INT );
			putop( P2CALL , P2PTR | P2STRTY );
#		    endif PC
		    return p;
	    default:
		    error("Variable given, %s required for %s parameter %s" ,
			    typename , typename , formalp -> symbol );
		    return NLNIL;
	}
    }
