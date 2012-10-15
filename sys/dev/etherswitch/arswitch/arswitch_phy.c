/*-
 * Copyright (c) 2011-2012 Stefan Bethke.
 * Copyright (c) 2012 Adrian Chadd.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/etherswitch/mdio.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/arswitch/arswitchreg.h>
#include <dev/etherswitch/arswitch/arswitchvar.h>

#include <dev/etherswitch/arswitch/arswitch_reg.h>
#include <dev/etherswitch/arswitch/arswitch_phy.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

#if	defined(DEBUG)
static SYSCTL_NODE(_debug, OID_AUTO, arswitch, CTLFLAG_RD, 0, "arswitch");
#endif

/*
 * access PHYs integrated into the switch chip through the switch's MDIO
 * control register.
 */
int
arswitch_readphy(device_t dev, int phy, int reg)
{
	struct arswitch_softc *sc;
	uint32_t data = 0, ctrl;
	int err, timeout;

	sc = device_get_softc(dev);
	ARSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);

	if (phy < 0 || phy >= 32)
		return (ENXIO);
	if (reg < 0 || reg >= 32)
		return (ENXIO);

	ARSWITCH_LOCK(sc);
	err = arswitch_writereg_msb(dev, AR8X16_REG_MDIO_CTRL,
	    AR8X16_MDIO_CTRL_BUSY | AR8X16_MDIO_CTRL_MASTER_EN |
	    AR8X16_MDIO_CTRL_CMD_READ |
	    (phy << AR8X16_MDIO_CTRL_PHY_ADDR_SHIFT) |
	    (reg << AR8X16_MDIO_CTRL_REG_ADDR_SHIFT));
	DEVERR(dev, err, "arswitch_readphy()=%d: phy=%d.%02x\n", phy, reg);
	if (err != 0)
		goto fail;
	for (timeout = 100; timeout--; ) {
		ctrl = arswitch_readreg_msb(dev, AR8X16_REG_MDIO_CTRL);
		if ((ctrl & AR8X16_MDIO_CTRL_BUSY) == 0)
			break;
	}
	if (timeout < 0)
		goto fail;
	data = arswitch_readreg_lsb(dev, AR8X16_REG_MDIO_CTRL) &
	    AR8X16_MDIO_CTRL_DATA_MASK;
	ARSWITCH_UNLOCK(sc);
	return (data);

fail:
	ARSWITCH_UNLOCK(sc);
	return (-1);
}

int
arswitch_writephy(device_t dev, int phy, int reg, int data)
{
	struct arswitch_softc *sc;
	uint32_t ctrl;
	int err, timeout;

	sc = device_get_softc(dev);
	ARSWITCH_LOCK_ASSERT(sc, MA_NOTOWNED);

	if (reg < 0 || reg >= 32)
		return (ENXIO);

	ARSWITCH_LOCK(sc);
	err = arswitch_writereg_lsb(dev, AR8X16_REG_MDIO_CTRL,
	    (data & AR8X16_MDIO_CTRL_DATA_MASK));
	if (err != 0)
		goto out;
	err = arswitch_writereg_msb(dev, AR8X16_REG_MDIO_CTRL,
	    AR8X16_MDIO_CTRL_BUSY |
	    AR8X16_MDIO_CTRL_MASTER_EN |
	    AR8X16_MDIO_CTRL_CMD_WRITE |
	    (phy << AR8X16_MDIO_CTRL_PHY_ADDR_SHIFT) |
	    (reg << AR8X16_MDIO_CTRL_REG_ADDR_SHIFT));
	if (err != 0)
		goto out;
	for (timeout = 100; timeout--; ) {
		ctrl = arswitch_readreg(dev, AR8X16_REG_MDIO_CTRL);
		if ((ctrl & AR8X16_MDIO_CTRL_BUSY) == 0)
			break;
	}
	if (timeout < 0)
		err = EIO;
out:
	DEVERR(dev, err, "arswitch_writephy()=%d: phy=%d.%02x\n", phy, reg);
	ARSWITCH_UNLOCK(sc);
	return (err);
}
