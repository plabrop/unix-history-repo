/*
 * Copyright (c) 2001 Gary Jennejohn. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *      i4b_ifpi2 - Fritz!Card PCI Version 2 for split layers
 *      ------------------------------------------
 *
 *	$Id$
 *
 * $FreeBSD$
 *
 *      last edit-date: [Fri Jun  2 14:53:31 2000]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_IFPI2_EXT_H_
#define _I4B_IFPI2_EXT_H_

#include <i4b/include/i4b_l3l4.h>

void ifpi2_set_linktab(int , int , drvr_link_t * );
isdn_link_t *ifpi2_ret_linktab(int , int );

int ifpi2_ph_data_req(int , struct mbuf *, int );
int ifpi2_ph_activate_req(int );
int ifpi2_mph_command_req(int , int , void *);

void ifpi2_isacsx_irq(struct l1_softc *, int );
void ifpi2_isacsx_l1_cmd(struct l1_softc *, int );
int ifpi2_isacsx_init(struct l1_softc *);

void ifpi2_recover(struct l1_softc *);
char * ifpi2_printstate(struct l1_softc *);
void ifpi2_next_state(struct l1_softc *, int );

#define IFPI2_MAXUNIT 4
extern struct l1_softc *ifpi2_scp[IFPI2_MAXUNIT];

/* the ISACSX has 2 mask registers of interest - cannot use ISAC_IMASK */
extern unsigned char isacsx_imaskd;
extern unsigned char isacsx_imask;

#endif /* _I4B_IFPI2_EXT_H_ */
