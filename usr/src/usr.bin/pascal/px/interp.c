/* Copyright (c) 1979 Regents of the University of California */

static char sccsid[] = "@(#)interp.c 1.5 %G%";

#include <math.h>
#include "vars.h"
#include "panics.h"
#include "h02opcs.h"
#include "machdep.h"
#include "h01errs.h"
#include "libpc.h"

/*
 * program variables
 */
union disply	_display;
struct disp	*_dp;
long	_lino = 0;
int	_argc;
char	**_argv;
long	_mode;
long	_runtst = TRUE;
long	_nodump = FALSE;
long	_stlim = 500000;
long	_stcnt = 0;
long	_seed = 1;
char	*_minptr = (char *)0x7fffffff;
char	*_maxptr = (char *)0;
long	*_pcpcount = (long *)0;
long	_cntrs = 0;
long	_rtns = 0;

/*
 * file record variables
 */
long		_filefre = PREDEF;
struct iorechd	_fchain = {
	0, 0, 0, 0,		/* only use fchain field */
	INPUT			/* fchain  */
};
struct iorec	*_actfile[MAXFILES] = {
	INPUT,
	OUTPUT,
	ERR
};

/*
 * standard files
 */
char		_inwin, _outwin, _errwin;
struct iorechd	input = {
	&_inwin,		/* fileptr */
	0,			/* lcount  */
	0x7fffffff,		/* llimit  */
	&_iob[0],		/* fbuf    */
	OUTPUT,			/* fchain  */
	STDLVL,			/* flev    */
	"standard input",	/* pfname  */
	FTEXT | FREAD | SYNC,	/* funit   */
	0,			/* fblk    */
	1			/* fsize   */
};
struct iorechd	output = {
	&_outwin,		/* fileptr */
	0,			/* lcount  */
	0x7fffffff,		/* llimit  */
	&_iob[1],		/* fbuf    */
	ERR,			/* fchain  */
	STDLVL,			/* flev    */
	"standard output",	/* pfname  */
	FTEXT | FWRITE | EOFF,	/* funit   */
	1,			/* fblk    */
	1			/* fsize   */
};
struct iorechd	_err = {
	&_errwin,		/* fileptr */
	0,			/* lcount  */
	0x7fffffff,		/* llimit  */
	&_iob[2],		/* fbuf    */
	FILNIL,			/* fchain  */
	STDLVL,			/* flev    */
	"Message file",		/* pfname  */
	FTEXT | FWRITE | EOFF,	/* funit   */
	2,			/* fblk    */
	1			/* fsize   */
};

/*
 * Px profile array
 */
#ifdef PROFILE
long _profcnts[NUMOPS];
#endif PROFILE

/*
 * debugging variables
 */
#ifdef DEBUG
char opc[10];
long opcptr = 9;
#endif DEBUG

interpreter(base)
	char *base;
{
	union progcntr pc;		/* interpreted program cntr */
	register char *vpc;		/* register used for "pc" */
	struct iorec *curfile;		/* active file */
	register struct stack *stp;	/* active stack frame ptr */
	/*
	 * the following variables are used as scratch
	 */
	double td, td1;
	register long tl, tl1, tl2;
	long *tlp;
	short *tsp, *tsp1;
	register char *tcp;
	char *tcp1;
	struct stack *tstp;
	struct formalrtn *tfp;
	union progcntr tpc;
	struct iorec **ip;

	/*
	 * necessary only on systems which do not initialize
	 * memory to zero
	 */
	for (ip = &_actfile[3]; ip < &_actfile[MAXFILES]; *ip++ = FILNIL)
		/* void */;
	/*
	 * set up global environment, then ``call'' the main program
	 */
	_display.frame[0].locvars = pushsp(2 * sizeof(struct iorec *));
	_display.frame[0].locvars += 8;	/* local offsets are negative */
	*(struct iorec **)(_display.frame[0].locvars - 4) = OUTPUT;
	*(struct iorec **)(_display.frame[0].locvars - 8) = INPUT;
	stp = (struct stack *)pushsp(sizeof(struct stack));
	_dp = &_display.frame[0];
	pc.cp = base;
	for(;;) {
#		ifdef DEBUG
		if (++opcptr == 10)
			opcptr = 0;
		opc[opcptr] = *pc.ucp;
#		endif DEBUG
#		ifdef PROFILE
		_profcnts[*pc.ucp]++;
#		endif PROFILE
		switch (*pc.ucp++) {
		default:
			panic(PBADOP);
			continue;
		case O_NODUMP:
			_nodump = TRUE;
			/* and fall through */
		case O_BEG:
			_dp += 1;		/* enter local scope */
			stp->odisp = *_dp;	/* save old display value */
			tl = *pc.ucp++;		/* tl = name size */
			stp->entry = pc.hdrp;	/* pointer to entry info */
			tl1 = pc.hdrp->framesze;/* tl1 = size of frame */
			_lino = pc.hdrp->offset;
			_runtst = pc.hdrp->tests;
			disableovrflo();
			if (_runtst)
				enableovrflo();
			pc.cp += tl;		/* skip over proc hdr info */
			stp->file = curfile;	/* save active file */
			tcp = pushsp(tl1);	/* tcp = new top of stack */
			blkclr(tl1, tcp);	/* zero stack frame */
			tcp += tl1;		/* offsets of locals are neg */
			_dp->locvars = tcp;	/* set new display pointer */
			_dp->stp = stp;
			stp->tos = pushsp(0);	/* set top of stack pointer */
			continue;
		case O_END:
			PCLOSE(_dp->locvars);	/* flush & close local files */
			stp = _dp->stp;
			curfile = stp->file;	/* restore old active file */
			*_dp = stp->odisp;	/* restore old display entry */
			if (_dp == &_display.frame[1])
				return;		/* exiting main proc ??? */
			_lino = stp->lino;	/* restore lino, pc, dp */
			pc.cp = stp->pc.cp;
			_dp = stp->dp;
			_runtst = stp->entry->tests;
			disableovrflo();
			if (_runtst)
				enableovrflo();
			popsp(stp->entry->framesze +	/* pop local vars */
			      sizeof(struct stack) +	/* pop stack frame */
			      stp->entry->nargs);	/* pop parms */
			continue;
		case O_CALL:
			tl = *pc.cp++;
			tcp = base + *pc.lp++;/* calc new entry point */
			tcp += sizeof(short);
			tcp = base + *(long *)tcp;
			stp = (struct stack *)pushsp(sizeof(struct stack));
			stp->lino = _lino;	/* save lino, pc, dp */
			stp->pc.cp = pc.cp;
			stp->dp = _dp;
			_dp = &_display.frame[tl]; /* set up new display ptr */
			pc.cp = tcp;
			continue;
		case O_FCALL:
			tl = *pc.cp++;		/* tl = number of args */
			if (tl == 0)
				tl = *pc.lp++;
			tfp = (struct formalrtn *)popaddr();
			stp = (struct stack *)pushsp(sizeof(struct stack));
			stp->lino = _lino;	/* save lino, pc, dp */
			stp->pc.cp = pc.cp;
			stp->dp = _dp;
			pc.cp = tfp->entryaddr;	/* calc new entry point */
			if (_runtst) {
				tpc.sp = pc.sp + 1;
				tl -= tpc.hdrp->nargs;
				if (tl != 0) {
					if (tl > 0)
						tl += sizeof(int) - 1;
					else
						tl -= sizeof(int) - 1;
					ERROR(ENARGS, tl / sizeof(int));
				}
			}
			_dp = &_display.frame[tfp->cbn];/* new display ptr */
			blkcpy(sizeof(struct disp) * tfp->cbn,
				&_display.frame[1], &tfp->disp[tfp->cbn]);
			blkcpy(sizeof(struct disp) * tfp->cbn,
				&tfp->disp[0], &_display.frame[1]);
			continue;
		case O_FRTN:
			tl = *pc.cp++;		/* tl = size of return obj */
			if (tl == 0)
				tl = *pc.usp++;
			tcp = pushsp(0);
			tfp = *(struct formalrtn **)(tcp + tl);
			blkcpy(tl, tcp, tcp + sizeof(struct formalrtn *));
			popsp(sizeof(struct formalrtn *));
			blkcpy(sizeof(struct disp) * tfp->cbn,
				&tfp->disp[tfp->cbn], &_display.frame[1]);
			continue;
		case O_FSAV:
			tfp = (struct formalrtn *)popaddr();
			tfp->cbn = *pc.cp++;	/* blk number of routine */
			tcp = base + *pc.lp++;/* calc new entry point */
			tcp += sizeof(short);
			tfp->entryaddr = base + *(long *)tcp;
			blkcpy(sizeof(struct disp) * tfp->cbn,
				&_display.frame[1], &tfp->disp[0]);
			pushaddr(tfp);
			continue;
		case O_SDUP2:
			pc.cp++;
			tl = pop2();
			push2(tl);
			push2(tl);
			continue;
		case O_SDUP4:
			pc.cp++;
			tl = pop4();
			push4(tl);
			push4(tl);
			continue;
		case O_TRA:
			pc.cp++;
			pc.cp += *pc.sp;
			continue;
		case O_TRA4:
			pc.cp++;
			pc.cp = base + *pc.lp;
			continue;
		case O_GOTO:
			tstp = _display.frame[*pc.cp++].stp; /* ptr to
								exit frame */
			pc.cp = base + *pc.lp;
			stp = _dp->stp;
			while (tstp != stp) {
				if (_dp == &_display.frame[1])
					ERROR(EGOTO); /* exiting prog ??? */
				PCLOSE(_dp->locvars); /* close local files */
				curfile = stp->file;  /* restore active file */
				*_dp = stp->odisp;    /* old display entry */
				_dp = stp->dp;	      /* restore dp */
				stp = _dp->stp;
			}
			/* pop locals, stack frame, parms, and return values */
			popsp(stp->tos - pushsp(0));
			continue;
		case O_LINO:
			if (_dp->stp->tos != pushsp(0))
				panic(PSTKNEMP);
			_lino = *pc.cp++;	/* set line number */
			if (_lino == 0)
				_lino = *pc.sp++;
			LINO();			/* inc statement count */
			continue;
		case O_PUSH:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.usp++;
			tl = (-tl + 1) & ~1;
			tcp = pushsp(tl);
			blkclr(tl, tcp);
			continue;
		case O_IF:
			pc.cp++;
			if (pop2()) {
				pc.sp++;
				continue;
			}
			pc.cp += *pc.sp;
			continue;
		case O_REL2:
			tl = pop2();
			tl1 = pop2();
			goto cmplong;
		case O_REL24:
			tl = pop2();
			tl1 = pop4();
			goto cmplong;
		case O_REL42:
			tl = pop4();
			tl1 = pop2();
			goto cmplong;
		case O_REL4:
			tl = pop4();
			tl1 = pop4();
		cmplong:
			tl2 = *pc.cp++;
			switch (tl2) {
			case releq:
				push2(tl1 == tl);
				continue;
			case relne:
				push2(tl1 != tl);
				continue;
			case rellt:
				push2(tl1 < tl);
				continue;
			case relgt:
				push2(tl1 > tl);
				continue;
			case relle:
				push2(tl1 <= tl);
				continue;
			case relge:
				push2(tl1 >= tl);
				continue;
			default:
				panic(PSYSTEM);
				continue;
			}
		case O_RELG:
			tl2 = *pc.cp++;		/* tc has jump opcode */
			tl = *pc.usp++;		/* tl has comparison length */
			tl1 = (tl + 1) & ~1;	/* tl1 has arg stack length */
			tcp = pushsp(0);	/* tcp pts to first arg */
			switch (tl2) {
			case releq:
				tl = RELEQ(tl, tcp + tl1, tcp);
				break;
			case relne:
				tl = RELNE(tl, tcp + tl1, tcp);
				break;
			case rellt:
				tl = RELSLT(tl, tcp + tl1, tcp);
				break;
			case relgt:
				tl = RELSGT(tl, tcp + tl1, tcp);
				break;
			case relle:
				tl = RELSLE(tl, tcp + tl1, tcp);
				break;
			case relge:
				tl = RELSGE(tl, tcp + tl1, tcp);
				break;
			default:
				panic(PSYSTEM);
				break;
			}
			popsp(tl1 << 1);
			push2(tl);
			continue;
		case O_RELT:
			tl2 = *pc.cp++;		/* tc has jump opcode */
			tl1 = *pc.usp++;	/* tl1 has comparison length */
			tcp = pushsp(0);	/* tcp pts to first arg */
			switch (tl2) {
			case releq:
				tl = RELEQ(tl1, tcp + tl1, tcp);
				break;
			case relne:
				tl = RELNE(tl1, tcp + tl1, tcp);
				break;
			case rellt:
				tl = RELTLT(tl1, tcp + tl1, tcp);
				break;
			case relgt:
				tl = RELTGT(tl1, tcp + tl1, tcp);
				break;
			case relle:
				tl = RELTLE(tl1, tcp + tl1, tcp);
				break;
			case relge:
				tl = RELTGE(tl1, tcp + tl1, tcp);
				break;
			default:
				panic(PSYSTEM);
				break;
			}
			popsp(tl1 << 1);
			push2(tl);
			continue;
		case O_REL28:
			td = pop2();
			td1 = pop8();
			goto cmpdbl;
		case O_REL48:
			td = pop4();
			td1 = pop8();
			goto cmpdbl;
		case O_REL82:
			td = pop8();
			td1 = pop2();
			goto cmpdbl;
		case O_REL84:
			td = pop8();
			td1 = pop4();
			goto cmpdbl;
		case O_REL8:
			td = pop8();
			td1 = pop8();
		cmpdbl:
			switch (*pc.cp++) {
			case releq:
				push2(td1 == td);
				continue;
			case relne:
				push2(td1 != td);
				continue;
			case rellt:
				push2(td1 < td);
				continue;
			case relgt:
				push2(td1 > td);
				continue;
			case relle:
				push2(td1 <= td);
				continue;
			case relge:
				push2(td1 >= td);
				continue;
			default:
				panic(PSYSTEM);
				continue;
			}
		case O_AND:
			pc.cp++;
			push2(pop2() & pop2());
			continue;
		case O_OR:
			pc.cp++;
			push2(pop2() | pop2());
			continue;
		case O_NOT:
			pc.cp++;
			push2(pop2() ^ 1);
			continue;
		case O_AS2:
			pc.cp++;
			tl = pop2();
			*(short *)popaddr() = tl;
			continue;
		case O_AS4:
			pc.cp++;
			tl = pop4();
			*(long *)popaddr() = tl;
			continue;
		case O_AS24:
			pc.cp++;
			tl = pop2();
			*(long *)popaddr() = tl;
			continue;
		case O_AS42:
			pc.cp++;
			tl = pop4();
			*(short *)popaddr() = tl;
			continue;
		case O_AS21:
			pc.cp++;
			tl = pop2();
			*popaddr() = tl;
			continue;
		case O_AS41:
			pc.cp++;
			tl = pop4();
			*popaddr() = tl;
			continue;
		case O_AS28:
			pc.cp++;
			tl = pop2();
			*(double *)popaddr() = tl;
			continue;
		case O_AS48:
			pc.cp++;
			tl = pop4();
			*(double *)popaddr() = tl;
			continue;
		case O_AS8:
			pc.cp++;
			td = pop8();
			*(double *)popaddr() = td;
			continue;
		case O_AS:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.usp++;
			tl1 = (tl + 1) & ~1;
			tcp = pushsp(0);
			blkcpy(tl, tcp, *(char **)(tcp + tl1));
			popsp(tl1 + sizeof(char *));
			continue;
		case O_INX2P2:
			tl = *pc.cp++;		/* tl has shift amount */
			tl1 = (pop2() - *pc.sp++) << tl;
			pushaddr(popaddr() + tl1);
			continue;
		case O_INX4P2:
			tl = *pc.cp++;		/* tl has shift amount */
			tl1 = (pop4() - *pc.sp++) << tl;
			pushaddr(popaddr() + tl1);
			continue;
		case O_INX2:
			tl = *pc.cp++;		/* tl has element size */
			if (tl == 0)
				tl = *pc.usp++;
			tl1 = pop2();		/* index */
			tl2 = *pc.sp++;
			pushaddr(popaddr() + (tl1 - tl2) * tl);
			tl = *pc.usp++;
			if (_runtst)
				SUBSC(tl1, tl2, tl); /* range check */
			continue;
		case O_INX4:
			tl = *pc.cp++;		/* tl has element size */
			if (tl == 0)
				tl = *pc.usp++;
			tl1 = pop4();		/* index */
			tl2 = *pc.sp++;
			pushaddr(popaddr() + (tl1 - tl2) * tl);
			tl = *pc.usp++;
			if (_runtst)
				SUBSC(tl1, tl2, tl); /* range check */
			continue;
		case O_OFF:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.usp++;
			push4(pop4() + tl);
			continue;
		case O_NIL:
			pc.cp++;
			NIL();
			continue;
		case O_ADD2:
			pc.cp++;
			push4(pop2() + pop2());
			continue;
		case O_ADD4:
			pc.cp++;
			push4(pop4() + pop4());
			continue;
		case O_ADD24:
			pc.cp++;
			tl = pop2();
			push4(pop4() + tl);
			continue;
		case O_ADD42:
			pc.cp++;
			tl = pop4();
			push4(pop2() + tl);
			continue;
		case O_ADD28:
			pc.cp++;
			tl = pop2();
			push8(pop8() + tl);
			continue;
		case O_ADD48:
			pc.cp++;
			tl = pop4();
			push8(pop8() + tl);
			continue;
		case O_ADD82:
			pc.cp++;
			td = pop8();
			push8(pop2() + td);
			continue;
		case O_ADD84:
			pc.cp++;
			td = pop8();
			push8(pop4() + td);
			continue;
		case O_SUB2:
			pc.cp++;
			tl = pop2();
			push4(pop2() - tl);
			continue;
		case O_SUB4:
			pc.cp++;
			tl = pop4();
			push4(pop4() - tl);
			continue;
		case O_SUB24:
			pc.cp++;
			tl = pop2();
			push4(pop4() - tl);
			continue;
		case O_SUB42:
			pc.cp++;
			tl = pop4();
			push4(pop2() - tl);
			continue;
		case O_SUB28:
			pc.cp++;
			tl = pop2();
			push8(pop8() - tl);
			continue;
		case O_SUB48:
			pc.cp++;
			tl = pop4();
			push8(pop8() - tl);
			continue;
		case O_SUB82:
			pc.cp++;
			td = pop8();
			push8(pop2() - td);
			continue;
		case O_SUB84:
			pc.cp++;
			td = pop8();
			push8(pop4() - td);
			continue;
		case O_MUL2:
			pc.cp++;
			push4(pop2() * pop2());
			continue;
		case O_MUL4:
			pc.cp++;
			push4(pop4() * pop4());
			continue;
		case O_MUL24:
			pc.cp++;
			tl = pop2();
			push4(pop4() * tl);
			continue;
		case O_MUL42:
			pc.cp++;
			tl = pop4();
			push4(pop2() * tl);
			continue;
		case O_MUL28:
			pc.cp++;
			tl = pop2();
			push8(pop8() * tl);
			continue;
		case O_MUL48:
			pc.cp++;
			tl = pop4();
			push8(pop8() * tl);
			continue;
		case O_MUL82:
			pc.cp++;
			td = pop8();
			push8(pop2() * td);
			continue;
		case O_MUL84:
			pc.cp++;
			td = pop8();
			push8(pop4() * td);
			continue;
		case O_ABS2:
		case O_ABS4:
			pc.cp++;
			tl = pop4();
			push4(tl >= 0 ? tl : -tl);
			continue;
		case O_ABS8:
			pc.cp++;
			td = pop8();
			push8(td >= 0.0 ? td : -td);
			continue;
		case O_NEG2:
			pc.cp++;
			push4(-pop2());
			continue;
		case O_NEG4:
			pc.cp++;
			push4(-pop4());
			continue;
		case O_NEG8:
			pc.cp++;
			push8(-pop8());
			continue;
		case O_DIV2:
			pc.cp++;
			tl = pop2();
			push4(pop2() / tl);
			continue;
		case O_DIV4:
			pc.cp++;
			tl = pop4();
			push4(pop4() / tl);
			continue;
		case O_DIV24:
			pc.cp++;
			tl = pop2();
			push4(pop4() / tl);
			continue;
		case O_DIV42:
			pc.cp++;
			tl = pop4();
			push4(pop2() / tl);
			continue;
		case O_MOD2:
			pc.cp++;
			tl = pop2();
			push4(pop2() % tl);
			continue;
		case O_MOD4:
			pc.cp++;
			tl = pop4();
			push4(pop4() % tl);
			continue;
		case O_MOD24:
			pc.cp++;
			tl = pop2();
			push4(pop4() % tl);
			continue;
		case O_MOD42:
			pc.cp++;
			tl = pop4();
			push4(pop2() % tl);
			continue;
		case O_ADD8:
			pc.cp++;
			push8(pop8() + pop8());
			continue;
		case O_SUB8:
			pc.cp++;
			td = pop8();
			push8(pop8() - td);
			continue;
		case O_MUL8:
			pc.cp++;
			push8(pop8() * pop8());
			continue;
		case O_DVD8:
			pc.cp++;
			td = pop8();
			push8(pop8() / td);
			continue;
		case O_STOI:
			pc.cp++;
			push4(pop2());
			continue;
		case O_STOD:
			pc.cp++;
			td = pop2();
			push8(td);
			continue;
		case O_ITOD:
			pc.cp++;
			td = pop4();
			push8(td);
			continue;
		case O_ITOS:
			pc.cp++;
			push2(pop4());
			continue;
		case O_DVD2:
			pc.cp++;
			td = pop2();
			push8(pop2() / td);
			continue;
		case O_DVD4:
			pc.cp++;
			td = pop4();
			push8(pop4() / td);
			continue;
		case O_DVD24:
			pc.cp++;
			td = pop2();
			push8(pop4() / td);
			continue;
		case O_DVD42:
			pc.cp++;
			td = pop4();
			push8(pop2() / td);
			continue;
		case O_DVD28:
			pc.cp++;
			td = pop2();
			push8(pop8() / td);
			continue;
		case O_DVD48:
			pc.cp++;
			td = pop4();
			push8(pop8() / td);
			continue;
		case O_DVD82:
			pc.cp++;
			td = pop8();
			push8(pop2() / td);
			continue;
		case O_DVD84:
			pc.cp++;
			td = pop8();
			push8(pop4() / td);
			continue;
		case O_RV1:
			tcp = _display.raw[*pc.ucp++];
			push2(*(tcp + *pc.sp++));
			continue;
		case O_RV14:
			tcp = _display.raw[*pc.ucp++];
			push4(*(tcp + *pc.sp++));
			continue;
		case O_RV2:
			tcp = _display.raw[*pc.ucp++];
			push2(*(short *)(tcp + *pc.sp++));
			continue;
		case O_RV24:
			tcp = _display.raw[*pc.ucp++];
			push4(*(short *)(tcp + *pc.sp++));
			continue;
		case O_RV4:
			tcp = _display.raw[*pc.ucp++];
			push4(*(long *)(tcp + *pc.sp++));
			continue;
		case O_RV8:
			tcp = _display.raw[*pc.ucp++];
			push8(*(double *)(tcp + *pc.sp++));
			continue;
		case O_RV:
			tcp = _display.raw[*pc.ucp++];
			tcp += *pc.sp++;
			tl = *pc.usp++;
			tcp1 = pushsp(tl);
			blkcpy(tl, tcp, tcp1);
			continue;
		case O_LV:
			tcp = _display.raw[*pc.ucp++];
			pushaddr(tcp + *pc.sp++);
			continue;
		case O_LRV1:
			tcp = _display.raw[*pc.ucp++];
			push2(*(tcp + *pc.lp++));
			continue;
		case O_LRV14:
			tcp = _display.raw[*pc.ucp++];
			push4(*(tcp + *pc.lp++));
			continue;
		case O_LRV2:
			tcp = _display.raw[*pc.ucp++];
			push2(*(short *)(tcp + *pc.lp++));
			continue;
		case O_LRV24:
			tcp = _display.raw[*pc.ucp++];
			push4(*(short *)(tcp + *pc.lp++));
			continue;
		case O_LRV4:
			tcp = _display.raw[*pc.ucp++];
			push4(*(long *)(tcp + *pc.lp++));
			continue;
		case O_LRV8:
			tcp = _display.raw[*pc.ucp++];
			push8(*(double *)(tcp + *pc.lp++));
			continue;
		case O_LRV:
			tcp = _display.raw[*pc.ucp++];
			tcp += *pc.lp++;
			tl = *pc.usp++;
			tcp1 = pushsp(tl);
			blkcpy(tl, tcp, tcp1);
			continue;
		case O_LLV:
			tcp = _display.raw[*pc.ucp++];
			pushaddr(tcp + *pc.lp++);
			continue;
		case O_IND1:
			pc.cp++;
			push2(*popaddr());
			continue;
		case O_IND14:
			pc.cp++;
			push4(*popaddr());
			continue;
		case O_IND2:
			pc.cp++;
			push2(*(short *)(popaddr()));
			continue;
		case O_IND24:
			pc.cp++;
			push4(*(short *)(popaddr()));
			continue;
		case O_IND4:
			pc.cp++;
			push4(*(long *)(popaddr()));
			continue;
		case O_IND8:
			pc.cp++;
			push8(*(double *)(popaddr()));
			continue;
		case O_IND:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.usp++;
			tcp = popaddr();
			tcp1 = pushsp((tl + 1) & ~1);
			blkcpy(tl, tcp, tcp1);
			continue;
		case O_CON1:
			push2(*pc.cp++);
			continue;
		case O_CON14:
			push4(*pc.cp++);
			continue;
		case O_CON2:
			pc.cp++;
			push2(*pc.sp++);
			continue;
		case O_CON24:
			pc.cp++;
			push4(*pc.sp++);
			continue;
		case O_CON4:
			pc.cp++;
			push4(*pc.lp++);
			continue;
		case O_CON8:
			pc.cp++;
			push8(*pc.dp++);
			continue;
		case O_CON:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.usp++;
			tl = (tl + 1) & ~1;
			tcp = pushsp(tl);
			blkcpy(tl, pc.cp, tcp);
			pc.cp += tl;
			continue;
		case O_LVCON:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.usp++;
			tl = (tl + 1) & ~1;
			pushaddr(pc.cp);
			pc.cp += tl;
			continue;
		case O_RANG2:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.sp++;
			tl1 = pop2();
			push2(RANG4(tl1, tl, *pc.sp++));
			continue;
		case O_RANG42:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.sp++;
			tl1 = pop4();
			push4(RANG4(tl1, tl, *pc.sp++));
			continue;
		case O_RSNG2:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.sp++;
			tl1 = pop2();
			push2(RSNG4(tl1, tl));
			continue;
		case O_RSNG42:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.sp++;
			tl1 = pop4();
			push4(RSNG4(tl1, tl));
			continue;
		case O_RANG4:
			pc.cp++;
			tl = *pc.lp++;
			tl1 = pop4();
			push4(RANG4(tl1, tl, *pc.lp++));
			continue;
		case O_RANG24:
			pc.cp++;
			tl = *pc.lp++;
			tl1 = pop2();
			push2(RANG4(tl1, tl, *pc.lp++));
			continue;
		case O_RSNG4:
			pc.cp++;
			tl = pop4();
			push4(RSNG4(tl, *pc.lp++));
			continue;
		case O_RSNG24:
			pc.cp++;
			tl = pop2();
			push2(RSNG4(tl, *pc.lp++));
			continue;
		case O_STLIM:
			pc.cp++;
			STLIM();
			popargs(1);
			continue;
		case O_LLIMIT:
			pc.cp++;
			LLIMIT();
			popargs(2);
			continue;
		case O_BUFF:
			BUFF(*pc.cp++);
			continue;
		case O_HALT:
			pc.cp++;
			panic(PHALT);
			continue;
		case O_PXPBUF:
			pc.cp++;
			_cntrs = *pc.lp++;
			_rtns = *pc.lp++;
			_pcpcount = (long *)calloc(_cntrs + 1, sizeof(long));
			continue;
		case O_COUNT:
			pc.cp++;
			_pcpcount[*pc.usp++]++;
			continue;
		case O_CASE1OP:
			tl = *pc.cp++;		/* tl = number of cases */
			if (tl == 0)
				tl = *pc.usp++;
			tsp = pc.sp + tl;	/* ptr to end of jump table */
			tcp = (char *)tsp;	/* tcp = ptr to case values */
			tl1 = pop2();		/* tl1 = element to find */
			for(; tl > 0; tl--)	/* look for element */
				if (tl1 == *tcp++)
					break;
			if (tl == 0)		/* default case => error */
				ERROR(ECASE, tl2);
			pc.cp += *(tsp - tl);
			continue;
		case O_CASE2OP:
			tl = *pc.cp++;		/* tl = number of cases */
			if (tl == 0)
				tl = *pc.usp++;
			tsp = pc.sp + tl;	/* ptr to end of jump table */
			tsp1 = tsp;		/* tsp1 = ptr to case values */
			tl1 = (unsigned short)pop2();/* tl1 = element to find */
			for(; tl > 0; tl--)	/* look for element */
				if (tl1 == *tsp1++)
					break;
			if (tl == 0)		/* default case => error */
				ERROR(ECASE, tl2);
			pc.cp += *(tsp - tl);
			continue;
		case O_CASE4OP:
			tl = *pc.cp++;		/* tl = number of cases */
			if (tl == 0)
				tl = *pc.usp++;
			tsp = pc.sp + tl;	/* ptr to end of jump table */
			tlp = (long *)tsp;	/* tlp = ptr to case values */
			tl1 = pop4();		/* tl1 = element to find */
			for(; tl > 0; tl--)	/* look for element */
				if (tl1 == *tlp++)
					break;
			if (tl == 0)		/* default case => error */
				ERROR(ECASE, tl2);
			pc.cp += *(tsp - tl);
			continue;
		case O_ADDT:
			tl = *pc.cp++;		/* tl has comparison length */
			if (tl == 0)
				tl = *pc.usp++;
			tcp = pushsp(0);	/* tcp pts to first arg */
			ADDT(tcp + tl, tcp + tl, tcp, tl >> 2);
			popsp(tl);
			continue;
		case O_SUBT:
			tl = *pc.cp++;		/* tl has comparison length */
			if (tl == 0)
				tl = *pc.usp++;
			tcp = pushsp(0);	/* tcp pts to first arg */
			SUBT(tcp + tl, tcp + tl, tcp, tl >> 2);
			popsp(tl);
			continue;
		case O_MULT:
			tl = *pc.cp++;		/* tl has comparison length */
			if (tl == 0)
				tl = *pc.usp++;
			tcp = pushsp(0);	/* tcp pts to first arg */
			MULT(tcp + tl, tcp + tl, tcp, tl >> 2);
			popsp(tl);
			continue;
		case O_INCT:
			tl = *pc.cp++;		/* tl has number of args */
			if (tl == 0)
				tl = *pc.usp++;
			tl1 = INCT();
			popargs(tl);
			push2(tl1);
			continue;
		case O_CTTOT:
			tl = *pc.cp++;		/* tl has number of args */
			if (tl == 0)
				tl = *pc.usp++;
			tl1 = tl * sizeof(long);
			tcp = pushsp(0) + tl1;	/* tcp pts to result space */
			CTTOT(tcp);
			popargs(tl);
			continue;
		case O_CARD:
			tl = *pc.cp++;		/* tl has comparison length */
			if (tl == 0)
				tl = *pc.usp++;
			tcp = pushsp(0);	/* tcp pts to set */
			tl1 = CARD(tcp, tl);
			popsp(tl);
			push2(tl1);
			continue;
		case O_IN:
			tl = *pc.cp++;		/* tl has comparison length */
			if (tl == 0)
				tl = *pc.usp++;
			tl1 = pop4();		/* tl1 is the element */
			tcp = pushsp(0);	/* tcp pts to set */
			tl2 = *pc.usp++;	/* lower bound */
			tl1 = IN(tl1, tl2, *pc.usp++, tcp);
			popsp(tl);
			push2(tl1);
			continue;
		case O_ASRT:
			pc.cp++;
			ASRT(pop2(), "");
			continue;
		case O_FOR1U:
			pc.cp++;
			tcp = (char *)pop4();	/* tcp = ptr to index var */
			if (*tcp < pop4()) {	/* still going up */
				tl = *tcp + 1;	/* inc index var */
				tl1 = *pc.sp++;	/* index lower bound */
				tl2 = *pc.sp++;	/* index upper bound */
				if (_runtst)
					RANG4(tl, tl1, tl2);
				*tcp = tl;	/* update index var */
				pc.cp += *pc.sp;/* return to top of loop */
				continue;
			}
			pc.sp += 3;		/* else fall through */
			continue;
		case O_FOR2U:
			pc.cp++;
			tsp = (short *)pop4();	/* tsp = ptr to index var */
			if (*tsp < pop4()) {	/* still going up */
				tl = *tsp + 1;	/* inc index var */
				tl1 = *pc.sp++;	/* index lower bound */
				tl2 = *pc.sp++;	/* index upper bound */
				if (_runtst)
					RANG4(tl, tl1, tl2);
				*tsp = tl;	/* update index var */
				pc.cp += *pc.sp;/* return to top of loop */
				continue;
			}
			pc.sp += 3;		/* else fall through */
			continue;
		case O_FOR4U:
			pc.cp++;
			tlp = (long *)pop4();	/* tlp = ptr to index var */
			if (*tlp < pop4()) {	/* still going up */
				tl = *tlp + 1;	/* inc index var */
				tl1 = *pc.lp++;	/* index lower bound */
				tl2 = *pc.lp++;	/* index upper bound */
				if (_runtst)
					RANG4(tl, tl1, tl2);
				*tlp = tl;	/* update index var */
				pc.cp += *pc.sp;/* return to top of loop */
				continue;
			}
			pc.sp += 5;		/* else fall through */
			continue;
		case O_FOR1D:
			pc.cp++;
			tcp = (char *)pop4();	/* tcp = ptr to index var */
			if (*tcp > pop4()) {	/* still going down */
				tl = *tcp - 1;	/* inc index var */
				tl1 = *pc.sp++;	/* index lower bound */
				tl2 = *pc.sp++;	/* index upper bound */
				if (_runtst)
					RANG4(tl, tl1, tl2);
				*tcp = tl;	/* update index var */
				pc.cp += *pc.sp;/* return to top of loop */
				continue;
			}
			pc.sp += 3;		/* else fall through */
			continue;
		case O_FOR2D:
			pc.cp++;
			tsp = (short *)pop4();	/* tsp = ptr to index var */
			if (*tsp > pop4()) {	/* still going down */
				tl = *tsp - 1;	/* inc index var */
				tl1 = *pc.sp++;	/* index lower bound */
				tl2 = *pc.sp++;	/* index upper bound */
				if (_runtst)
					RANG4(tl, tl1, tl2);
				*tsp = tl;	/* update index var */
				pc.cp += *pc.sp;/* return to top of loop */
				continue;
			}
			pc.sp += 3;		/* else fall through */
			continue;
		case O_FOR4D:
			pc.cp++;
			tlp = (long *)pop4();	/* tlp = ptr to index var */
			if (*tlp > pop4()) {	/* still going down */
				tl = *tlp - 1;	/* inc index var */
				tl1 = *pc.lp++;	/* index lower bound */
				tl2 = *pc.lp++;	/* index upper bound */
				if (_runtst)
					RANG4(tl, tl1, tl2);
				*tlp = tl;	/* update index var */
				pc.cp += *pc.sp;/* return to top of loop */
				continue;
			}
			pc.sp += 5;		/* else fall through */
			continue;
		case O_READE:
			pc.cp++;
			push2(READE(curfile, base + *pc.lp++));
			continue;
		case O_READ4:
			pc.cp++;
			push4(READ4(curfile));
			continue;
		case O_READC:
			pc.cp++;
			push2(READC(curfile));
			continue;
		case O_READ8:
			pc.cp++;
			push8(READ8(curfile));
			continue;
		case O_READLN:
			pc.cp++;
			READLN(curfile);
			continue;
		case O_EOF:
			pc.cp++;
			push2(TEOF(popaddr()));
			continue;
		case O_EOLN:
			pc.cp++;
			push2(TEOLN(popaddr()));
			continue;
		case O_WRITEC:
			pc.cp++;
			if (_runtst) {
				WRITEC(curfile);
				popargs(2);
				continue;
			}
			fputc();
			popargs(2);
			continue;
		case O_WRITES:
			pc.cp++;
			if (_runtst) {
				WRITES(curfile);
				popargs(4);
				continue;
			}
			fwrite();
			popargs(4);
			continue;
		case O_WRITEF:
			if (_runtst) {
				WRITEF(curfile);
				popargs(*pc.cp++);
				continue;
			}
			fprintf();
			popargs(*pc.cp++);
			continue;
		case O_WRITLN:
			pc.cp++;
			if (_runtst) {
				WRITLN(curfile);
				continue;
			}
			fputc('\n', ACTFILE(curfile));
			continue;
		case O_PAGE:
			pc.cp++;
			if (_runtst) {
				PAGE(curfile);
				continue;
			}
			fputc('^L', ACTFILE(curfile));
			continue;
		case O_NAM:
			pc.cp++;
			tl = pop4();
			pushaddr(NAM(tl, base + *pc.lp++));
			continue;
		case O_MAX:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.usp++;
			tl1 = pop4();
			if (_runtst) {
				push4(MAX(tl1, tl, *pc.usp++));
				continue;
			}
			tl1 -= tl;
			tl = *pc.usp++;
			push4(tl1 > tl ? tl1 : tl);
			continue;
		case O_MIN:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.usp++;
			tl1 = pop4();
			push4(tl1 < tl ? tl1 : tl);
			continue;
		case O_UNIT:
			pc.cp++;
			curfile = UNIT(popaddr());
			continue;
		case O_UNITINP:
			pc.cp++;
			curfile = INPUT;
			continue;
		case O_UNITOUT:
			pc.cp++;
			curfile = OUTPUT;
			continue;
		case O_MESSAGE:
			pc.cp++;
			PFLUSH();
			curfile = ERR;
			continue;
		case O_PUT:
			pc.cp++;
			PUT(curfile);
			continue;
		case O_GET:
			pc.cp++;
			GET(curfile);
			continue;
		case O_FNIL:
			pc.cp++;
			pushaddr(FNIL(popaddr()));
			continue;
		case O_DEFNAME:
			pc.cp++;
			DEFNAME();
			popargs(4);
			continue;
		case O_RESET:
			pc.cp++;
			RESET();
			popargs(4);
			continue;
		case O_REWRITE:
			pc.cp++;
			REWRITE();
			popargs(4);
			continue;
		case O_FILE:
			pc.cp++;
			pushaddr(ACTFILE(curfile));
			continue;
		case O_REMOVE:
			pc.cp++;
			REMOVE();
			popargs(2);
			continue;
		case O_FLUSH:
			pc.cp++;
			FLUSH();
			popargs(1);
			continue;
		case O_PACK:
			pc.cp++;
			PACK();
			popargs(7);
			continue;
		case O_UNPACK:
			pc.cp++;
			UNPACK();
			popargs(7);
			continue;
		case O_ARGC:
			pc.cp++;
			push4(_argc);
			continue;
		case O_ARGV:
			tl = *pc.cp++;		/* tl = size of char array */
			if (tl == 0)
				tl = *pc.usp++;
			tcp = popaddr();	/* tcp = addr of char array */
			tl1 = pop4();		/* tl1 = argv subscript */
			ARGV(tl1, tcp, tl);
			continue;
		case O_CLCK:
			pc.cp++;
			push4(CLCK());
			continue;
		case O_WCLCK:
			pc.cp++;
			push4(time(0));
			continue;
		case O_SCLCK:
			pc.cp++;
			push4(SCLCK());
			continue;
		case O_DISPOSE:
			tl = *pc.cp++;		/* tl = size being disposed */
			if (tl == 0)
				tl = *pc.usp++;
			tcp = popaddr();	/* ptr to ptr being disposed */
			DISPOSE(tcp, tl);
			*(char **)tcp = (char *)0;
			continue;
		case O_NEW:
			tl = *pc.cp++;		/* tl = size being new'ed */
			if (tl == 0)
				tl = *pc.usp++;
			tcp = popaddr();	/* ptr to ptr being new'ed */
			if (_runtst) {
				NEWZ(tcp, tl);
				continue;
			}
			NEW(tcp, tl);
			continue;
		case O_DATE:
			pc.cp++;
			DATE(popaddr());
			continue;
		case O_TIME:
			pc.cp++;
			TIME(popaddr());
			continue;
		case O_UNDEF:
			pc.cp++;
			pop8();
			push2(0);
			continue;
		case O_ATAN:
			pc.cp++;
			push8(atan(pop8()));
			continue;
		case O_COS:
			pc.cp++;
			push8(cos(pop8()));
			continue;
		case O_EXP:
			pc.cp++;
			push8(exp(pop8()));
			continue;
		case O_LN:
			pc.cp++;
			if (_runtst) {
				push8(LN(pop8()));
				continue;
			}
			push8(log(pop8()));
			continue;
		case O_SIN:
			pc.cp++;
			push8(sin(pop8()));
			continue;
		case O_SQRT:
			pc.cp++;
			if (_runtst) {
				push8(SQRT(pop8()));
				continue;
			}
			push8(sqrt(pop8()));
			continue;
		case O_CHR2:
		case O_CHR4:
			pc.cp++;
			if (_runtst) {
				push2(CHR(pop4()));
				continue;
			}
			push2(pop4());
			continue;
		case O_ODD2:
		case O_ODD4:
			pc.cp++;
			push2(pop4() & 1);
			continue;
		case O_SUCC2:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.sp++;
			tl1 = pop4();
			if (_runtst) {
				push2(SUCC(tl1, tl, *pc.sp++));
				continue;
			}
			push2(tl1 + 1);
			pc.sp++;
			continue;
		case O_SUCC24:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.sp++;
			tl1 = pop4();
			if (_runtst) {
				push4(SUCC(tl1, tl, *pc.sp++));
				continue;
			}
			push4(tl1 + 1);
			pc.sp++;
			continue;
		case O_SUCC4:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.lp++;
			tl1 = pop4();
			if (_runtst) {
				push4(SUCC(tl1, tl, *pc.lp++));
				continue;
			}
			push4(tl1 + 1);
			pc.lp++;
			continue;
		case O_PRED2:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.sp++;
			tl1 = pop4();
			if (_runtst) {
				push2(PRED(tl1, tl, *pc.sp++));
				continue;
			}
			push2(tl1 - 1);
			pc.sp++;
			continue;
		case O_PRED24:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.sp++;
			tl1 = pop4();
			if (_runtst) {
				push4(PRED(tl1, tl, *pc.sp++));
				continue;
			}
			push4(tl1 - 1);
			pc.sp++;
			continue;
		case O_PRED4:
			tl = *pc.cp++;
			if (tl == 0)
				tl = *pc.lp++;
			tl1 = pop4();
			if (_runtst) {
				push4(PRED(tl1, tl, *pc.lp++));
				continue;
			}
			push4(tl1 - 1);
			pc.lp++;
			continue;
		case O_SEED:
			pc.cp++;
			push4(SEED(pop4()));
			continue;
		case O_RANDOM:
			pc.cp++;
			push8(RANDOM(pop8()));
			continue;
		case O_EXPO:
			pc.cp++;
			push4(EXPO(pop8()));
			continue;
		case O_SQR2:
		case O_SQR4:
			pc.cp++;
			tl = pop4();
			push4(tl * tl);
			continue;
		case O_SQR8:
			pc.cp++;
			td = pop8();
			push8(td * td);
			continue;
		case O_ROUND:
			pc.cp++;
			push4(ROUND(pop8()));
			continue;
		case O_TRUNC:
			pc.cp++;
			push4(TRUNC(pop8()));
			continue;
		}
	}
}
