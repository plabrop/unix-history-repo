static	char sccsid[] = "@(#)sym.c 4.3 4/27/87";
/*
 * adb - symbol table routines
 */
#include "defs.h"
#include <stab.h>

/*
 * Lookup a symbol by name.
 */
struct nlist *
lookup(symstr)
	char *symstr;
{
	register struct nlist *sp;

	cursym = 0;
	if (symtab)
	for (sp = symtab; sp < esymtab; sp++)
		/* SHOULD DO SOME OF EQSYM INLINE TO SAVE TIME */
		if ((sp->n_type&N_STAB)==0 && eqsym(sp->n_un.n_name, symstr, '_'))
			return(cursym = sp);
	return (0);
}

/*
 * Find the closest symbol to val, and return
 * the difference between val and the symbol found.
 * Leave a pointer to the symbol found as cursym.
 */
findsym(val, type)
	long val;
	int type;
{
	long diff;
	register struct nlist *sp;

	cursym = 0;
	diff = MAXINT;
	if (type == NSYM || symtab == 0)
		return (diff);
	for (sp = symtab; sp < esymtab; sp++) {
		if (sp->n_type & N_STAB || !(sp->n_type & N_EXT)
		    || sp->n_type == (N_FN|N_EXT))
			continue;
		if (val - sp->n_value < diff && val >= sp->n_value) {
			diff = val - sp->n_value;
			cursym = sp;
			if (diff == 0)
				break;
		}
	}
	return (diff);
}

/*
 * Advance cursym to the next local variable.
 * Leave its value in localval as a side effect.
 * Return 0 at end of file.
 */
localsym(cframe, cargp)
	ADDR cframe, cargp;
{
	register int type;
	register struct nlist *sp;

	if (cursym)
	for (sp = cursym; ++sp < esymtab; ) {
		if (sp->n_un.n_name[0] =='_' || sp->n_type == N_FN)
			return (0);
		type = sp->n_type;
		switch (sp->n_type) {

		case N_TEXT:
		case N_TEXT|N_EXT:
		case N_DATA:
		case N_DATA|N_EXT:
		case N_BSS:
		case N_BSS|N_EXT:
			localval = sp->n_value;
			cursym = sp;
			return (1);

		case N_LSYM:
			localval = cframe - sp->n_value;
			cursym = sp;
			return (1);

		case N_PSYM:
			/* code below works since n_value > 0 */
		case N_ABS:
			if (sp->n_value < 0)
				localval = cframe + sp->n_value;
			else
				localval = cargp + sp->n_value;
			cursym = sp;
			return (1);
		}
	}
	cursym = 0;
	return (0);
}

/*
 * Print value v and then the string s.
 * If v is not zero, then we look for a nearby symbol
 * and print name+offset if we find a symbol for which
 * offset is small enough.
 *
 * For values which are just into kernel address space
 * that they match exactly or that they be more than maxoff
 * bytes into kernel space.
 */
psymoff(v, type, s)
	long v;
	int type;
	char *s;
{
	long w;

	if (v) 
		w = findsym(v, type);
	if (v==0 || w >= maxoff || (INKERNEL(v) && KVTOPH(v) < maxoff && w))
		printf(LPRMODE, v);
	else {
		printf("%s", cursym->n_un.n_name);
		if (w)
			printf(OFFMODE, w);
	}
	printf(s);
}

/*
 * Print value v symbolically if it has a reasonable
 * interpretation as name+offset.  If not, print nothing.
 * Used in printing out registers $r.
 */
valpr(v, idsp)
	long v;
{
	off_t d;

	d = findsym(v, idsp);
	if (d >= maxoff)
		return;
	printf("%s", cursym->n_un.n_name);
	if (d)
		printf(OFFMODE, d);
}
