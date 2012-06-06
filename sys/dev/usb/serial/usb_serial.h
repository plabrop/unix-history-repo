/*	$NetBSD: ucomvar.h,v 1.9 2001/01/23 21:56:17 augustss Exp $	*/
/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2001-2002, Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
 * All rights reserved.
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
 */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _USB_SERIAL_H_
#define	_USB_SERIAL_H_

#include <sys/tty.h>
#include <sys/serial.h>
#include <sys/fcntl.h>
#include <sys/termios.h>

/* Module interface related macros */
#define	UCOM_MODVER	1

#define	UCOM_MINVER	1
#define	UCOM_PREFVER	UCOM_MODVER
#define	UCOM_MAXVER	1

struct usb_device;
struct ucom_softc;
struct usb_device_request;
struct thread;

/*
 * NOTE: There is no guarantee that "ucom_cfg_close()" will
 * be called after "ucom_cfg_open()" if the device is detached
 * while it is open!
 */
struct ucom_callback {
	void    (*ucom_cfg_get_status) (struct ucom_softc *, uint8_t *plsr, uint8_t *pmsr);
	void    (*ucom_cfg_set_dtr) (struct ucom_softc *, uint8_t);
	void    (*ucom_cfg_set_rts) (struct ucom_softc *, uint8_t);
	void    (*ucom_cfg_set_break) (struct ucom_softc *, uint8_t);
	void    (*ucom_cfg_set_ring) (struct ucom_softc *, uint8_t);
	void    (*ucom_cfg_param) (struct ucom_softc *, struct termios *);
	void    (*ucom_cfg_open) (struct ucom_softc *);
	void    (*ucom_cfg_close) (struct ucom_softc *);
	int     (*ucom_pre_open) (struct ucom_softc *);
	int     (*ucom_pre_param) (struct ucom_softc *, struct termios *);
	int     (*ucom_ioctl) (struct ucom_softc *, uint32_t, caddr_t, int, struct thread *);
	void    (*ucom_start_read) (struct ucom_softc *);
	void    (*ucom_stop_read) (struct ucom_softc *);
	void    (*ucom_start_write) (struct ucom_softc *);
	void    (*ucom_stop_write) (struct ucom_softc *);
	void    (*ucom_tty_name) (struct ucom_softc *, char *pbuf, uint16_t buflen, uint16_t unit, uint16_t subunit);
	void    (*ucom_poll) (struct ucom_softc *);
};

/* Line status register */
#define	ULSR_RCV_FIFO	0x80
#define	ULSR_TSRE	0x40		/* Transmitter empty: byte sent */
#define	ULSR_TXRDY	0x20		/* Transmitter buffer empty */
#define	ULSR_BI		0x10		/* Break detected */
#define	ULSR_FE		0x08		/* Framing error: bad stop bit */
#define	ULSR_PE		0x04		/* Parity error */
#define	ULSR_OE		0x02		/* Overrun, lost incoming byte */
#define	ULSR_RXRDY	0x01		/* Byte ready in Receive Buffer */
#define	ULSR_RCV_MASK	0x1f		/* Mask for incoming data or error */

struct ucom_cfg_task {
	struct usb_proc_msg hdr;
	struct ucom_softc *sc;
};

struct ucom_param_task {
	struct usb_proc_msg hdr;
	struct ucom_softc *sc;
	struct termios termios_copy;
};

struct ucom_super_softc {
	struct usb_process sc_tq;
	uint32_t sc_unit;
	uint32_t sc_subunits;
};

struct ucom_softc {
	/*
	 * NOTE: To avoid loosing level change information we use two
	 * tasks instead of one for all commands.
	 *
	 * Level changes are transitions like:
	 *
	 * ON->OFF
	 * OFF->ON
	 * OPEN->CLOSE
	 * CLOSE->OPEN
	 */
	struct ucom_cfg_task	sc_start_task[2];
	struct ucom_cfg_task	sc_open_task[2];
	struct ucom_cfg_task	sc_close_task[2];
	struct ucom_cfg_task	sc_line_state_task[2];
	struct ucom_cfg_task	sc_status_task[2];
	struct ucom_param_task	sc_param_task[2];
	struct cv sc_cv;
	/* Used to set "UCOM_FLAG_GP_DATA" flag: */
	struct usb_proc_msg	*sc_last_start_xfer;
	const struct ucom_callback *sc_callback;
	struct ucom_super_softc *sc_super;
	struct tty *sc_tty;
	struct mtx *sc_mtx;
	void   *sc_parent;
	int sc_subunit;
	uint16_t sc_portno;
	uint16_t sc_flag;
#define	UCOM_FLAG_RTS_IFLOW	0x01	/* use RTS input flow control */
#define	UCOM_FLAG_GONE		0x02	/* the device is gone */
#define	UCOM_FLAG_ATTACHED	0x04	/* set if attached */
#define	UCOM_FLAG_GP_DATA	0x08	/* set if get and put data is possible */
#define	UCOM_FLAG_LL_READY	0x20	/* set if low layer is ready */
#define	UCOM_FLAG_HL_READY	0x40	/* set if high layer is ready */
#define	UCOM_FLAG_CONSOLE	0x80	/* set if device is a console */
	uint8_t	sc_lsr;
	uint8_t	sc_msr;
	uint8_t	sc_mcr;
	uint8_t	sc_ttyfreed;		/* set when TTY has been freed */
	/* programmed line state bits */
	uint8_t sc_pls_set;		/* set bits */
	uint8_t sc_pls_clr;		/* cleared bits */
	uint8_t sc_pls_curr;		/* last state */
#define	UCOM_LS_DTR	0x01
#define	UCOM_LS_RTS	0x02
#define	UCOM_LS_BREAK	0x04
#define	UCOM_LS_RING	0x08
};

#define	ucom_cfg_do_request(udev,com,req,ptr,flags,timo) \
    usbd_do_request_proc(udev,&(com)->sc_super->sc_tq,req,ptr,flags,NULL,timo)

int	ucom_attach(struct ucom_super_softc *,
	    struct ucom_softc *, int, void *,
	    const struct ucom_callback *callback, struct mtx *);
void	ucom_detach(struct ucom_super_softc *, struct ucom_softc *);
void	ucom_set_pnpinfo_usb(struct ucom_super_softc *, device_t);
void	ucom_status_change(struct ucom_softc *);
uint8_t	ucom_get_data(struct ucom_softc *, struct usb_page_cache *,
	    uint32_t, uint32_t, uint32_t *);
void	ucom_put_data(struct ucom_softc *, struct usb_page_cache *,
	    uint32_t, uint32_t);
uint8_t	ucom_cfg_is_gone(struct ucom_softc *);
#endif					/* _USB_SERIAL_H_ */
