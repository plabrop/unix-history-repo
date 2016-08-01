/*-
 * Copyright (c) 2011,2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
 *
 * Developed by Damjan Marion <damjan.marion@gmail.com>
 *
 * Based on OMAP4 GIC code by Ben Gray
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ARM_GIC_H_
#define _ARM_GIC_H_

#define GIC_DEBUG_SPURIOUS

#define	GIC_FIRST_SGI		 0	/* Irqs 0-15 are SGIs/IPIs. */
#define	GIC_LAST_SGI		15
#define	GIC_FIRST_PPI		16	/* Irqs 16-31 are private (per */
#define	GIC_LAST_PPI		31	/* core) peripheral interrupts. */
#define	GIC_FIRST_SPI		32	/* Irqs 32+ are shared peripherals. */

#ifdef INTRNG
struct arm_gic_range {
	uint64_t bus;
	uint64_t host;
	uint64_t size;
};
#endif

struct arm_gic_softc {
	device_t		gic_dev;
#ifdef INTRNG
	void *			gic_intrhand;
	struct gic_irqsrc *	gic_irqs;
#endif
	struct resource *	gic_res[3];
	bus_space_tag_t		gic_c_bst;
	bus_space_tag_t		gic_d_bst;
	bus_space_handle_t	gic_c_bsh;
	bus_space_handle_t	gic_d_bsh;
	uint8_t			ver;
	struct mtx		mutex;
	uint32_t		nirqs;
	uint32_t		typer;
#ifdef GIC_DEBUG_SPURIOUS
	uint32_t		last_irq[MAXCPU];
#endif

#ifdef INTRNG
	/* FDT child data */
	pcell_t			addr_cells;
	pcell_t			size_cells;
	int			nranges;
	struct arm_gic_range *	ranges;
#endif
};

DECLARE_CLASS(arm_gic_driver);

#ifdef INTRNG
struct arm_gicv2m_softc {
	struct resource	*sc_mem;
	struct mtx	sc_mutex;
	uintptr_t	sc_xref;
	u_int		sc_spi_start;
	u_int		sc_spi_end;
	u_int		sc_spi_count;
};

DECLARE_CLASS(arm_gicv2m_driver);
#endif

int arm_gic_attach(device_t);
int arm_gic_detach(device_t);
int arm_gicv2m_attach(device_t);
int arm_gic_intr(void *);

#endif /* _ARM_GIC_H_ */
