/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)isa.c	7.2 (Berkeley) 5/13/91
 *	$Id: intr_machdep.c,v 1.19 1999/04/21 07:26:27 peter Exp $
 */
/*
 * This file contains an aggregated module marked:
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 * See the notice for details.
 */

#include "opt_auto_eoi.h"

#include <sys/param.h>
#ifndef SMP
#include <machine/lock.h>
#endif
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/interrupt.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#include <machine/segments.h>
#include <sys/bus.h> 

#if defined(APIC_IO)
#include <machine/smp.h>
#include <machine/smptests.h>			/** FAST_HI */
#endif /* APIC_IO */
#include <i386/isa/isa_device.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <pc98/pc98/epsonio.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/icu.h>

#include <isa/isavar.h>
#include <i386/isa/intr_machdep.h>
#include <sys/interrupt.h>
#ifdef APIC_IO
#include <machine/clock.h>
#endif

/* XXX should be in suitable include files */
#ifdef PC98
#define	ICU_IMR_OFFSET		2		/* IO_ICU{1,2} + 2 */
#define	ICU_SLAVEID			7
#else
#define	ICU_IMR_OFFSET		1		/* IO_ICU{1,2} + 1 */
#define	ICU_SLAVEID			2
#endif

#ifdef APIC_IO
/*
 * This is to accommodate "mixed-mode" programming for 
 * motherboards that don't connect the 8254 to the IO APIC.
 */
#define	AUTO_EOI_1	1
#endif

#define	NR_INTRNAMES	(1 + ICU_LEN + 2 * ICU_LEN)

u_long	*intr_countp[ICU_LEN];
inthand2_t *intr_handler[ICU_LEN];
u_int	intr_mask[ICU_LEN];
static u_int*	intr_mptr[ICU_LEN];
void	*intr_unit[ICU_LEN];

static inthand_t *fastintr[ICU_LEN] = {
	&IDTVEC(fastintr0), &IDTVEC(fastintr1),
	&IDTVEC(fastintr2), &IDTVEC(fastintr3),
	&IDTVEC(fastintr4), &IDTVEC(fastintr5),
	&IDTVEC(fastintr6), &IDTVEC(fastintr7),
	&IDTVEC(fastintr8), &IDTVEC(fastintr9),
	&IDTVEC(fastintr10), &IDTVEC(fastintr11),
	&IDTVEC(fastintr12), &IDTVEC(fastintr13),
	&IDTVEC(fastintr14), &IDTVEC(fastintr15)
#if defined(APIC_IO)
	, &IDTVEC(fastintr16), &IDTVEC(fastintr17),
	&IDTVEC(fastintr18), &IDTVEC(fastintr19),
	&IDTVEC(fastintr20), &IDTVEC(fastintr21),
	&IDTVEC(fastintr22), &IDTVEC(fastintr23)
#endif /* APIC_IO */
};

static inthand_t *slowintr[ICU_LEN] = {
	&IDTVEC(intr0), &IDTVEC(intr1), &IDTVEC(intr2), &IDTVEC(intr3),
	&IDTVEC(intr4), &IDTVEC(intr5), &IDTVEC(intr6), &IDTVEC(intr7),
	&IDTVEC(intr8), &IDTVEC(intr9), &IDTVEC(intr10), &IDTVEC(intr11),
	&IDTVEC(intr12), &IDTVEC(intr13), &IDTVEC(intr14), &IDTVEC(intr15)
#if defined(APIC_IO)
	, &IDTVEC(intr16), &IDTVEC(intr17), &IDTVEC(intr18), &IDTVEC(intr19),
	&IDTVEC(intr20), &IDTVEC(intr21), &IDTVEC(intr22), &IDTVEC(intr23)
#endif /* APIC_IO */
};

static inthand2_t isa_strayintr;

#ifdef PC98
#define NMI_PARITY 0x04
#define NMI_EPARITY 0x02
#else
#define NMI_PARITY (1 << 7)
#define NMI_IOCHAN (1 << 6)
#define ENMI_WATCHDOG (1 << 7)
#define ENMI_BUSTIMER (1 << 6)
#define ENMI_IOSTATUS (1 << 5)
#endif

/*
 * Handle a NMI, possibly a machine check.
 * return true to panic system, false to ignore.
 */
int
isa_nmi(cd)
	int cd;
{
#ifdef PC98
 	int port = inb(0x33);
	if (epson_machine_id == 0x20)
		epson_outb(0xc16, epson_inb(0xc16) | 0x1);
	if (port & NMI_PARITY) {
		panic("BASE RAM parity error, likely hardware failure.");
	} else if (port & NMI_EPARITY) {
		panic("EXTENDED RAM parity error, likely hardware failure.");
	} else {
		printf("\nNMI Resume ??\n");
		return(0);
	}
#else /* IBM-PC */
	int isa_port = inb(0x61);
	int eisa_port = inb(0x461);

	if (isa_port & NMI_PARITY)
		panic("RAM parity error, likely hardware failure.");

	if (isa_port & NMI_IOCHAN)
		panic("I/O channel check, likely hardware failure.");

	/*
	 * On a real EISA machine, this will never happen.  However it can
	 * happen on ISA machines which implement XT style floating point
	 * error handling (very rare).  Save them from a meaningless panic.
	 */
	if (eisa_port == 0xff)
		return(0);

	if (eisa_port & ENMI_WATCHDOG)
		panic("EISA watchdog timer expired, likely hardware failure.");

	if (eisa_port & ENMI_BUSTIMER)
		panic("EISA bus timeout, likely hardware failure.");

	if (eisa_port & ENMI_IOSTATUS)
		panic("EISA I/O port status error.");

	printf("\nNMI ISA %x, EISA %x\n", isa_port, eisa_port);
	return(0);
#endif
}

/*
 * Fill in default interrupt table (in case of spuruious interrupt
 * during configuration of kernel, setup interrupt control unit
 */
void
isa_defaultirq()
{
	int i;

	/* icu vectors */
	for (i = 0; i < ICU_LEN; i++)
		icu_unset(i, (inthand2_t *)NULL);

	/* initialize 8259's */
	outb(IO_ICU1, 0x11);		/* reset; program device, four bytes */

	outb(IO_ICU1+ICU_IMR_OFFSET, NRSVIDT);	/* starting at this vector index */
	outb(IO_ICU1+ICU_IMR_OFFSET, IRQ_SLAVE);		/* slave on line 7 */
#ifdef PC98
#ifdef AUTO_EOI_1
	outb(IO_ICU1+ICU_IMR_OFFSET, 0x1f);		/* (master) auto EOI, 8086 mode */
#else
	outb(IO_ICU1+ICU_IMR_OFFSET, 0x1d);		/* (master) 8086 mode */
#endif
#else /* IBM-PC */
#ifdef AUTO_EOI_1
	outb(IO_ICU1+ICU_IMR_OFFSET, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU1+ICU_IMR_OFFSET, 1);		/* 8086 mode */
#endif
#endif /* PC98 */
	outb(IO_ICU1+ICU_IMR_OFFSET, 0xff);		/* leave interrupts masked */
	outb(IO_ICU1, 0x0a);		/* default to IRR on read */
#ifndef PC98
	outb(IO_ICU1, 0xc0 | (3 - 1));	/* pri order 3-7, 0-2 (com2 first) */
#endif /* !PC98 */

	outb(IO_ICU2, 0x11);		/* reset; program device, four bytes */
	outb(IO_ICU2+ICU_IMR_OFFSET, NRSVIDT+8); /* staring at this vector index */
	outb(IO_ICU2+ICU_IMR_OFFSET, ICU_SLAVEID);         /* my slave id is 7 */
#ifdef PC98
	outb(IO_ICU2+ICU_IMR_OFFSET,9);              /* 8086 mode */
#else /* IBM-PC */
#ifdef AUTO_EOI_2
	outb(IO_ICU2+ICU_IMR_OFFSET, 2 | 1);		/* auto EOI, 8086 mode */
#else
	outb(IO_ICU2+ICU_IMR_OFFSET,1);		/* 8086 mode */
#endif
#endif /* PC98 */
	outb(IO_ICU2+ICU_IMR_OFFSET, 0xff);          /* leave interrupts masked */
	outb(IO_ICU2, 0x0a);		/* default to IRR on read */
}

/*
 * Caught a stray interrupt, notify
 */
static void
isa_strayintr(vcookiep)
	void *vcookiep;
{
	int intr = (void **)vcookiep - &intr_unit[0];

	/* DON'T BOTHER FOR NOW! */
	/* for some reason, we get bursts of intr #7, even if not enabled! */
	/*
	 * Well the reason you got bursts of intr #7 is because someone
	 * raised an interrupt line and dropped it before the 8259 could
	 * prioritize it.  This is documented in the intel data book.  This
	 * means you have BAD hardware!  I have changed this so that only
	 * the first 5 get logged, then it quits logging them, and puts
	 * out a special message. rgrimes 3/25/1993
	 */
	/*
	 * XXX TODO print a different message for #7 if it is for a
	 * glitch.  Glitches can be distinguished from real #7's by
	 * testing that the in-service bit is _not_ set.  The test
	 * must be done before sending an EOI so it can't be done if
	 * we are using AUTO_EOI_1.
	 */
	if (intrcnt[1 + intr] <= 5)
		log(LOG_ERR, "stray irq %d\n", intr);
	if (intrcnt[1 + intr] == 5)
		log(LOG_CRIT,
		    "too many stray irq %d's; not logging any more\n", intr);
}

/*
 * Return a bitmap of the current interrupt requests.  This is 8259-specific
 * and is only suitable for use at probe time.
 */
intrmask_t
isa_irq_pending()
{
	u_char irr1;
	u_char irr2;

	irr1 = inb(IO_ICU1);
	irr2 = inb(IO_ICU2);
	return ((irr2 << 8) | irr1);
}

int
update_intr_masks(void)
{
	int intr, n=0;
	u_int mask,*maskptr;

	for (intr=0; intr < ICU_LEN; intr ++) {
#if defined(APIC_IO)
		/* no 8259 SLAVE to ignore */
#else
		if (intr==ICU_SLAVEID) continue;	/* ignore 8259 SLAVE output */
#endif /* APIC_IO */
		maskptr = intr_mptr[intr];
		if (!maskptr)
			continue;
		*maskptr |= 1 << intr;
		mask = *maskptr;
		if (mask != intr_mask[intr]) {
#if 0
			printf ("intr_mask[%2d] old=%08x new=%08x ptr=%p.\n",
				intr, intr_mask[intr], mask, maskptr);
#endif
			intr_mask[intr]=mask;
			n++;
		}

	}
	return (n);
}

static void
update_intrname(int intr, char *name)
{
	char buf[32];
	char *cp;
	int name_index, off, strayintr;

	/*
	 * Initialise strings for bitbucket and stray interrupt counters.
	 * These have statically allocated indices 0 and 1 through ICU_LEN.
	 */
	if (intrnames[0] == '\0') {
		off = sprintf(intrnames, "???") + 1;
		for (strayintr = 0; strayintr < ICU_LEN; strayintr++)
			off += sprintf(intrnames + off, "stray irq%d",
			    strayintr) + 1;
	}

	if (name == NULL)
		name = "???";
	if (snprintf(buf, sizeof(buf), "%s irq%d", name, intr) >= sizeof(buf))
		goto use_bitbucket;

	/*
	 * Search for `buf' in `intrnames'.  In the usual case when it is
	 * not found, append it to the end if there is enough space (the \0
	 * terminator for the previous string, if any, becomes a separator).
	 */
	for (cp = intrnames, name_index = 0;
	    cp != eintrnames && name_index < NR_INTRNAMES;
	    cp += strlen(cp) + 1, name_index++) {
		if (*cp == '\0') {
			if (strlen(buf) >= eintrnames - cp)
				break;
			strcpy(cp, buf);
			goto found;
		}
		if (strcmp(cp, buf) == 0)
			goto found;
	}

use_bitbucket:
	printf("update_intrname: counting %s irq%d as %s\n", name, intr,
	    intrnames);
	name_index = 0;
found:
	intr_countp[intr] = &intrcnt[name_index];
}

int
icu_setup(int intr, inthand2_t *handler, void *arg, u_int *maskptr, int flags)
{
#ifdef FAST_HI
	int		select;		/* the select register is 8 bits */
	int		vector;
	u_int32_t	value;		/* the window register is 32 bits */
#endif /* FAST_HI */
	u_long	ef;
	u_int	mask = (maskptr ? *maskptr : 0);

#if defined(APIC_IO)
	if ((u_int)intr >= ICU_LEN)	/* no 8259 SLAVE to ignore */
#else
	if ((u_int)intr >= ICU_LEN || intr == ICU_SLAVEID)
#endif /* APIC_IO */
	if (intr_handler[intr] != isa_strayintr)
		return (EBUSY);

	ef = read_eflags();
	disable_intr();
	intr_handler[intr] = handler;
	intr_mptr[intr] = maskptr;
	intr_mask[intr] = mask | (1 << intr);
	intr_unit[intr] = arg;
#ifdef FAST_HI
	if (flags & INTR_FAST) {
		vector = TPR_FAST_INTS + intr;
		setidt(vector, fastintr[intr],
		       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	}
	else {
		vector = TPR_SLOW_INTS + intr;
#ifdef APIC_INTR_REORDER
#ifdef APIC_INTR_HIGHPRI_CLOCK
		/* XXX: Hack (kludge?) for more accurate clock. */
		if (intr == apic_8254_intr || intr == 8) {
			vector = TPR_FAST_INTS + intr;
		}
#endif
#endif
		setidt(vector, slowintr[intr],
		       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	}
#ifdef APIC_INTR_REORDER
	set_lapic_isrloc(intr, vector);
#endif
	/*
	 * Reprogram the vector in the IO APIC.
	 */
	if (int_to_apicintpin[intr].ioapic >= 0) {
		select = int_to_apicintpin[intr].redirindex;
		value = io_apic_read(int_to_apicintpin[intr].ioapic, 
				     select) & ~IOART_INTVEC;
		io_apic_write(int_to_apicintpin[intr].ioapic, 
			      select, value | vector);
	}
#else
	setidt(ICU_OFFSET + intr,
	       flags & INTR_FAST ? fastintr[intr] : slowintr[intr],
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif /* FAST_HI */
	INTREN(1 << intr);
	MPINTR_UNLOCK();
	write_eflags(ef);
	return (0);
}

void
register_imask(dvp, mask)
	struct isa_device *dvp;
	u_int	mask;
{
	if (dvp->id_alive && dvp->id_irq) {
		int	intr;

		intr = ffs(dvp->id_irq) - 1;
		intr_mask[intr] = mask | (1 <<intr);
	}
	(void) update_intr_masks();
}

int
icu_unset(intr, handler)
	int	intr;
	inthand2_t *handler;
{
	u_long	ef;

	if ((u_int)intr >= ICU_LEN || handler != intr_handler[intr])
		return (EINVAL);

	INTRDIS(1 << intr);
	ef = read_eflags();
	disable_intr();
	intr_countp[intr] = &intrcnt[1 + intr];
	intr_handler[intr] = isa_strayintr;
	intr_mptr[intr] = NULL;
	intr_mask[intr] = HWI_MASK | SWI_MASK;
	intr_unit[intr] = &intr_unit[intr];
#ifdef FAST_HI_XXX
	/* XXX how do I re-create dvp here? */
	setidt(flags & INTR_FAST ? TPR_FAST_INTS + intr : TPR_SLOW_INTS + intr,
	    slowintr[intr], SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#else /* FAST_HI */
#ifdef APIC_INTR_REORDER
	set_lapic_isrloc(intr, ICU_OFFSET + intr);
#endif
	setidt(ICU_OFFSET + intr, slowintr[intr], SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
#endif /* FAST_HI */
	MPINTR_UNLOCK();
	write_eflags(ef);
	return (0);
}

/* The following notice applies beyond this point in the file */

/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *
 * $Id: intr_machdep.c,v 1.19 1999/04/21 07:26:27 peter Exp $
 *
 */

typedef struct intrec {
	intrmask_t	mask;
	inthand2_t	*handler;
	void		*argument;
	struct intrec	*next;
	char		*name;
	int		intr;
	intrmask_t	*maskptr;
	int		flags;
} intrec;

static intrec *intreclist_head[ICU_LEN];

typedef struct isarec {
	int		id_unit;
	ointhand2_t	*id_handler;
} isarec;

static isarec *isareclist[ICU_LEN];

/*
 * The interrupt multiplexer calls each of the handlers in turn,
 * and applies the associated interrupt mask to "cpl", which is
 * defined as a ".long" in /sys/i386/isa/ipl.s
 */

static void
intr_mux(void *arg)
{
	intrec *p = arg;
	int oldspl;

	while (p != NULL) {
		oldspl = splq(p->mask);
		p->handler(p->argument);
		splx(oldspl);
		p = p->next;
	}
}

static void
isa_intr_wrap(void *cookie)
{
	isarec *irec = (isarec *)cookie;

	irec->id_handler(irec->id_unit);
}

static intrec*
find_idesc(unsigned *maskptr, int irq)
{
	intrec *p = intreclist_head[irq];

	while (p && p->maskptr != maskptr)
		p = p->next;

	return (p);
}

static intrec**
find_pred(intrec *idesc, int irq)
{
	intrec **pp = &intreclist_head[irq];
	intrec *p = *pp;

	while (p != idesc) {
		if (p == NULL)
			return (NULL);
		pp = &p->next;
		p = *pp;
	}
	return (pp);
}

/*
 * Both the low level handler and the shared interrupt multiplexer
 * block out further interrupts as set in the handlers "mask", while
 * the handler is running. In fact *maskptr should be used for this
 * purpose, but since this requires one more pointer dereference on
 * each interrupt, we rather bother update "mask" whenever *maskptr
 * changes. The function "update_masks" should be called **after**
 * all manipulation of the linked list of interrupt handlers hung
 * off of intrdec_head[irq] is complete, since the chain of handlers
 * will both determine the *maskptr values and the instances of mask
 * that are fixed. This function should be called with the irq for
 * which a new handler has been add blocked, since the masks may not
 * yet know about the use of this irq for a device of a certain class.
 */

static void
update_mux_masks(void)
{
	int irq;
	for (irq = 0; irq < ICU_LEN; irq++) {
		intrec *idesc = intreclist_head[irq];
		while (idesc != NULL) {
			if (idesc->maskptr != NULL) {
				/* our copy of *maskptr may be stale, refresh */
				idesc->mask = *idesc->maskptr;
			}
			idesc = idesc->next;
		}
	}
}

static void
update_masks(intrmask_t *maskptr, int irq)
{
	intrmask_t mask = 1 << irq;

	if (maskptr == NULL)
		return;

	if (find_idesc(maskptr, irq) == NULL) {
		/* no reference to this maskptr was found in this irq's chain */
		if ((*maskptr & mask) == 0)
			return;
		/* the irq was included in the classes mask, remove it */
		INTRUNMASK(*maskptr, mask);
	} else {
		/* a reference to this maskptr was found in this irq's chain */
		if ((*maskptr & mask) != 0)
			return;
		/* put the irq into the classes mask */
		INTRMASK(*maskptr, mask);
	}
	/* we need to update all values in the intr_mask[irq] array */
	update_intr_masks();
	/* update mask in chains of the interrupt multiplex handler as well */
	update_mux_masks();
}

/*
 * Add interrupt handler to linked list hung off of intreclist_head[irq]
 * and install shared interrupt multiplex handler, if necessary
 */

static int
add_intrdesc(intrec *idesc)
{
	int irq = idesc->intr;

	intrec *head = intreclist_head[irq];

	if (head == NULL) {
		/* first handler for this irq, just install it */
		if (icu_setup(irq, idesc->handler, idesc->argument, 
			      idesc->maskptr, idesc->flags) != 0)
			return (-1);

		update_intrname(irq, idesc->name);
		/* keep reference */
		intreclist_head[irq] = idesc;
	} else {
		if ((idesc->flags & INTR_EXCL) != 0
		    || (head->flags & INTR_EXCL) != 0) {
			/*
			 * can't append new handler, if either list head or
			 * new handler do not allow interrupts to be shared
			 */
			if (bootverbose)
				printf("\tdevice combination doesn't support "
				       "shared irq%d\n", irq);
			return (-1);
		}
		if (head->next == NULL) {
			/*
			 * second handler for this irq, replace device driver's
			 * handler by shared interrupt multiplexer function
			 */
			icu_unset(irq, head->handler);
			if (icu_setup(irq, intr_mux, head, 0, 0) != 0)
				return (-1);
			if (bootverbose)
				printf("\tusing shared irq%d.\n", irq);
			update_intrname(irq, "mux");
		}
		/* just append to the end of the chain */
		while (head->next != NULL)
			head = head->next;
		head->next = idesc;
	}
	update_masks(idesc->maskptr, irq);
	return (0);
}

/*
 * Create and activate an interrupt handler descriptor data structure.
 *
 * The dev_instance pointer is required for resource management, and will
 * only be passed through to resource_claim().
 *
 * There will be functions that derive a driver and unit name from a
 * dev_instance variable, and those functions will be used to maintain the
 * interrupt counter label array referenced by systat and vmstat to report
 * device interrupt rates (->update_intrlabels).
 *
 * Add the interrupt handler descriptor data structure created by an
 * earlier call of create_intr() to the linked list for its irq and
 * adjust the interrupt masks if necessary.
 */

intrec *
inthand_add(const char *name, int irq, inthand2_t handler, void *arg,
	     intrmask_t *maskptr, int flags)
{
	intrec *idesc;
	int errcode = -1;
	intrmask_t oldspl;

	if (ICU_LEN > 8 * sizeof *maskptr) {
		printf("create_intr: ICU_LEN of %d too high for %d bit intrmask\n",
		       ICU_LEN, 8 * sizeof *maskptr);
		return (NULL);
	}
	if ((unsigned)irq >= ICU_LEN) {
		printf("create_intr: requested irq%d too high, limit is %d\n",
		       irq, ICU_LEN -1);
		return (NULL);
	}

	idesc = malloc(sizeof *idesc, M_DEVBUF, M_WAITOK);
	if (idesc == NULL)
		return NULL;
	bzero(idesc, sizeof *idesc);

	if (name == NULL)
		name = "???";
	idesc->name     = malloc(strlen(name) + 1, M_DEVBUF, M_WAITOK);
	if (idesc->name == NULL) {
		free(idesc, M_DEVBUF);
		return NULL;
	}
	strcpy(idesc->name, name);

	idesc->handler  = handler;
	idesc->argument = arg;
	idesc->maskptr  = maskptr;
	idesc->intr     = irq;
	idesc->flags    = flags;

	/* block this irq */
	oldspl = splq(1 << irq);

	/* add irq to class selected by maskptr */
	errcode = add_intrdesc(idesc);
	splx(oldspl);

	if (errcode != 0) {
		if (bootverbose)
			printf("\tintr_connect(irq%d) failed, result=%d\n", 
			       irq, errcode);
		free(idesc->name, M_DEVBUF);
		free(idesc, M_DEVBUF);
		idesc = NULL;
	}

	return (idesc);
}

/*
 * Deactivate and remove the interrupt handler descriptor data connected
 * created by an earlier call of intr_connect() from the linked list and
 * adjust theinterrupt masks if necessary.
 *
 * Return the memory held by the interrupt handler descriptor data structure
 * to the system. Make sure, the handler is not actively used anymore, before.
 */

int
inthand_remove(intrec *idesc)
{
	intrec **hook, *head;
	int irq;
	int errcode = 0;
	intrmask_t oldspl;

	if (idesc == NULL)
		return (-1);

	irq = idesc->intr;

	/* find pointer that keeps the reference to this interrupt descriptor */
	hook = find_pred(idesc, irq);
	if (hook == NULL)
		return (-1);

	/* make copy of original list head, the line after may overwrite it */
	head = intreclist_head[irq];

	/* unlink: make predecessor point to idesc->next instead of to idesc */
	*hook = idesc->next;

	/* now check whether the element we removed was the list head */
	if (idesc == head) {

		oldspl = splq(1 << irq);

		/* we want to remove the list head, which was known to intr_mux */
		icu_unset(irq, intr_mux);

		/* check whether the new list head is the only element on list */
		head = intreclist_head[irq];
		if (head != NULL) {
			if (head->next != NULL) {
				/* install the multiplex handler with new list head as argument */
				errcode = icu_setup(irq, intr_mux, head, 0, 0);
				if (errcode == 0)
					update_intrname(irq, NULL);
			} else {
				/* install the one remaining handler for this irq */
				errcode = icu_setup(irq, head->handler,
						    head->argument,
						    head->maskptr, head->flags);
				if (errcode == 0)
					update_intrname(irq, head->name);
			}
		}
		splx(oldspl);
	}
	update_masks(idesc->maskptr, irq);

	free(idesc, M_DEVBUF);
	return (0);
}

/*
 * Emulate the register_intr() call previously defined as low level function.
 * That function (now icu_setup()) may no longer be directly called, since 
 * a conflict between an ISA and PCI interrupt might go by unnocticed, else.
 */

int
register_intr(int intr, int device_id, u_int flags,
	      ointhand2_t handler, u_int *maskptr, int unit)
{
	intrec *idesc;
	isarec *irec;

	irec = malloc(sizeof *irec, M_DEVBUF, M_WAITOK);
	if (irec == NULL)
		return NULL;
	bzero(irec, sizeof *irec);
	irec->id_unit = device_id;
	irec->id_handler = handler;

	flags |= INTR_EXCL;
	idesc = inthand_add("old", intr, isa_intr_wrap, irec, maskptr, flags);
	if (idesc == NULL) {
		free(irec, M_DEVBUF);
		return -1;
	}
	isareclist[intr] = irec;
	return 0;
}

/*
 * Emulate the old unregister_intr() low level function. 
 * Make sure there is just one interrupt, that it was 
 * registered as non-shared, and that the handlers match.
 */

int
unregister_intr(int intr, ointhand2_t handler)
{
	intrec *p = intreclist_head[intr];

	if (p != NULL && (p->flags & INTR_EXCL) != 0 &&
	    p->handler == isa_intr_wrap && isareclist[intr] != NULL &&
	    isareclist[intr]->id_handler == handler) {
		free(isareclist[intr], M_DEVBUF);
		isareclist[intr] = NULL;
		return (inthand_remove(p));
	}
	return (EINVAL);
}
