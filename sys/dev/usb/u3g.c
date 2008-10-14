/*
 * Copyright (c) 2008 AnyWi Technologies
 * Author: Andrea Guzzo <aguzzo@anywi.com>
 * * based on uark.c 1.1 2006/08/14 08:30:22 jsg *
 * * parts from ubsa.c 183348 2008-09-25 12:00:56Z phk *
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/ucomvar.h>

#include "usbdevs.h"

#ifdef U3G_DEBUG
#define DPRINTFN(n, x)	do { if (u3gdebug > (n)) printf x; } while (0)
int	u3gtebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define U3G_BUFSIZ		1024
#define U3G_MAXPORTS		4
#define U3G_CONFIG_INDEX	0

struct u3g_softc {
	struct ucom_softc	sc_ucom[U3G_MAXPORTS];;
	device_t		sc_dev;
	usbd_device_handle	sc_udev;
	u_int16_t		sc_flags;
	u_char			sc_msr;
	u_char			sc_lsr;
	u_char			numports;

	usbd_interface_handle	sc_intr_iface;	/* interrupt interface */
#ifdef U3G_DEBUG
	int			sc_intr_number;	/* interrupt number */
	usbd_pipe_handle	sc_intr_pipe;	/* interrupt pipe */
	u_char			*sc_intr_buf;	/* interrupt buffer */
#endif
	int			sc_isize;
};

struct ucom_callback u3g_callback = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};


/*
 * Various supported device vendors/products.
 */
struct u3g_dev_type_s {
	struct usb_devno	u3g_dev;
	u_int16_t		u3g_flags;
#define U3GFL_NONE		0x0000
#define U3GFL_HUAWEI_INIT	0x0001		/* Send USB command to reset iface to ucom mode */
};

static const struct u3g_dev_type_s u3g_devs[] = {
	/* OEM: Option */
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3G },		U3GFL_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3GQUAD },		U3GFL_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GT3GPLUS },		U3GFL_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_GTMAX36 },		U3GFL_NONE },
	{{ USB_VENDOR_OPTION, USB_PRODUCT_OPTION_VODAFONEMC3G },	U3GFL_NONE },
	/* OEM: Qualcomm, Inc. */
	{{ USB_VENDOR_QUALCOMMINC, USB_PRODUCT_QUALCOMMINC_CDMA_MSM },	U3GFL_NONE },
	/* OEM: Huawei */
	/* Handled separately. Do not add!
	{{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_MOBILE },		U3GFL_NONE },
	{{ USB_VENDOR_HUAWEI, USB_PRODUCT_HUAWEI_E220 },		U3GFL_NONE },
	 */
	/* OEM: Novatel */
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_CDMA_MODEM },	U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_ES620 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_MC950D },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U720 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U727 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U740_2 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_U870 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V620 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V640 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V720 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_V740 },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_X950D },		U3GFL_NONE },
	{{ USB_VENDOR_NOVATEL, USB_PRODUCT_NOVATEL_XU870 },		U3GFL_NONE },
	{{ USB_VENDOR_DELL,    USB_PRODUCT_DELL_U740 },			U3GFL_NONE },
};
#define u3g_lookup(v, p) ((const struct u3g_dev_type_s *)usb_lookup(u3g_devs, v, p))

#ifdef U3G_DEBUG
static void
u3g_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct u3g_softc *sc = (struct u3g_softc *)priv;
	device_printf(sc->sc_dev, "Interrupt callback\n");
}
#endif

static int
u3g_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usb_interface_descriptor_t *id;
	const struct u3g_dev_type_s *u3g_dev_type;

	if (uaa->vendor == USB_VENDOR_HUAWEI) {
		if (uaa->iface) {
			/* We attach to the interface instead of the device as
			 * some devices have a built-in SD card reader on the
			 * second interface. If the interface class of the
			 * first interface is no longer mass storage it has
			 * changed appearance and we should attach it.
			 */
			id = usbd_get_interface_descriptor(uaa->iface);
			if (id && id->bInterfaceClass == UICLASS_VENDOR)
				return (UMATCH_VENDOR_PRODUCT_CONF_IFACE);
		}
		return UMATCH_NONE;
	}

	u3g_dev_type = u3g_lookup(uaa->vendor, uaa->product);
	if (u3g_dev_type)
		return UMATCH_VENDOR_PRODUCT;

	return UMATCH_NONE;
}

static int
u3g_attach(device_t self)
{
	struct u3g_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	int i, n; 
	usb_config_descriptor_t *cd;
	struct ucom_softc *ucom = NULL;
	char devnamefmt[32];

	sc->sc_dev = self;
#ifdef U3G_DEBUG
	sc->sc_intr_number = -1;
	sc->sc_intr_pipe = NULL;
#endif
	/* Move the device into the configured state. */
	error = usbd_set_config_index(dev, U3G_CONFIG_INDEX, 1);
	if (error) {
		device_printf(self, "failed to set configuration: %s\n",
			      usbd_errstr(error));
		goto bad;
	}

	/* get the config descriptor */
	cd = usbd_get_config_descriptor(dev);

	if (cd == NULL) {
		device_printf(self, "failed to get configuration descriptor\n");
		goto bad;
	}

	sc->sc_udev = dev;
	sc->numports = (cd->bNumInterface <= U3G_MAXPORTS)?cd->bNumInterface:U3G_MAXPORTS;
	for ( i = 0; i < sc->numports; i++ ) {
		ucom = &sc->sc_ucom[i];

		ucom->sc_dev = self;
		ucom->sc_udev = dev;
		error = usbd_device2interface_handle(dev, i, &iface);
		if (error) {
			device_printf(ucom->sc_dev,
				"failed to get interface, err=%s\n",
			usbd_errstr(error));
			ucom->sc_dying = 1;
			goto bad;
		}
		id = usbd_get_interface_descriptor(iface);
		ucom->sc_iface = iface;

		ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
		for (n = 0; n < id->bNumEndpoints; n++) {
			ed = usbd_interface2endpoint_descriptor(iface, n);
			if (ed == NULL) {
				device_printf(ucom->sc_dev,
					      "could not read endpoint descriptor\n");
				goto bad;
			}
			if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
			    && UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
				ucom->sc_bulkin_no = ed->bEndpointAddress;
			else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT
				 && UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
				ucom->sc_bulkout_no = ed->bEndpointAddress;
		}
		if (ucom->sc_bulkin_no == -1 || ucom->sc_bulkout_no == -1) {
			device_printf(ucom->sc_dev, "missing endpoint\n");
			goto bad;
		}
		ucom->sc_parent = sc;
		ucom->sc_ibufsize = U3G_BUFSIZ;
		ucom->sc_obufsize = U3G_BUFSIZ;
		ucom->sc_ibufsizepad = U3G_BUFSIZ;
		ucom->sc_opkthdrlen = 0;

		ucom->sc_callback = &u3g_callback;

		sprintf(devnamefmt,"U%d.%%d", device_get_unit(self));
		DPRINTF(("u3g: in=0x%x out=0x%x, devname=%s\n",
			 ucom->sc_bulkin_no, ucom->sc_bulkout_no, devnamefmt));
#if __FreeBSD_version < 800000
		ucom_attach_tty(ucom, TS_CALLOUT, devnamefmt, i);
#else
		ucom_attach_tty(ucom, devnamefmt, i);
#endif
	}
#ifdef U3G_DEBUG
	if (sc->sc_intr_number != -1 && sc->sc_intr_pipe == NULL) {
		sc->sc_intr_buf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
		error = usbd_open_pipe_intr(sc->sc_intr_iface,
					    sc->sc_intr_number,
					    USBD_SHORT_XFER_OK,
					    &sc->sc_intr_pipe,
					    sc,
					    sc->sc_intr_buf,
					    sc->sc_isize,
					    u3g_intr,
					    100);
		if (error) {
			device_printf(self,
				      "cannot open interrupt pipe (addr %d)\n",
				      sc->sc_intr_number);
			goto bad;
		}
	}
#endif
	device_printf(self, "configured %d serial ports (/dev/cuaU%d.X)\n",
		      sc->numports, device_get_unit(self));

	return 0;

bad:
	DPRINTF(("u3g_attach: ATTACH ERROR\n"));
	ucom->sc_dying = 1;
	return ENXIO;
}

static int
u3g_detach(device_t self)
{
	struct u3g_softc *sc = device_get_softc(self);
	int rv = 0;
	int i;

	DPRINTF(("u3g_detach: sc=%p\n", sc));

	for (i = 0; i < sc->numports; i++) {
		if(sc->sc_ucom[i].sc_udev) {
			sc->sc_ucom[i].sc_dying = 1;
			rv = ucom_detach(&sc->sc_ucom[i]);
			if(rv != 0) {
				device_printf(self, "Can't deallocat port %d\n", i);
				return rv;
			}
		}
	}

#ifdef U3G_DEBUG
	if (sc->sc_intr_pipe != NULL) {
		int err = usbd_abort_pipe(sc->sc_intr_pipe);
		if (err)
			device_printf(self,
				"abort interrupt pipe failed: %s\n",
				usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_intr_pipe);
		if (err)
			device_printf(self,
				      "close interrupt pipe failed: %s\n",
				      usbd_errstr(err));
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}
#endif

	return 0;
}

static device_method_t u3g_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, u3g_match),
	DEVMETHOD(device_attach, u3g_attach),
	DEVMETHOD(device_detach, u3g_detach),

	{ 0, 0 }
};

static driver_t u3g_driver = {
	"ucom",
	u3g_methods,
	sizeof (struct u3g_softc)
};

DRIVER_MODULE(u3g, uhub, u3g_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(u3g, usb, 1, 1, 1);
MODULE_DEPEND(u3g, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);

/*******************************************************************
 ****** Stub driver to hide devices that need to reinitialise ******
 *******************************************************************/

static void
u3gstub_huawei_init(usbd_device_handle dev)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, UHF_PORT_SUSPEND);
	USETW(req.wLength, 0);

	(void) usbd_do_request(dev, &req, 0);		/* ignore any error */
}

static int
u3gstub_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usb_device_descriptor_t *dd = usbd_get_device_descriptor(uaa->device);
	usb_interface_descriptor_t *id;

	/* These are 3G modem devices (E220, Mobile, etc.) with auto-install
	 * flash disks for Windows/MacOSX through the first interface.
	 */
	if (uaa->vendor == USB_VENDOR_HUAWEI) {
		/* The Huawei device presents itself as a umass device with
		 * Windows drivers on it. After installation of the driver, it
		 * reinits into a 3G serial device.
		 */
		if (uaa->iface) {
			id = usbd_get_interface_descriptor(uaa->iface);
			if (id && id->bInterfaceNumber == 0 && id->bInterfaceClass == UICLASS_MASS) {
				u3gstub_huawei_init(uaa->device);
				return (UMATCH_VENDOR_PRODUCT_CONF_IFACE);
			}
		}
	}

	if (UGETW(dd->idVendor) == USB_VENDOR_QUALCOMMINC
	    || UGETW(dd->idVendor) == USB_VENDOR_NOVATEL) {
		/* Device by these vendors will automatically reappear as a
		 * ucom device if ignored (or if sent an eject command).
		 */
		return UMATCH_VENDOR_PRODUCT;
	}

	return UMATCH_NONE;
}

static int
u3gstub_attach(device_t self)
{
#if 0
	if (!bootverbose)
		device_quiet(self);
#endif

	return 0;
}

static int
u3gstub_detach(device_t self)
{
	return 0;
}

static device_method_t u3gstub_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, u3gstub_match),
	DEVMETHOD(device_attach, u3gstub_attach),
	DEVMETHOD(device_detach, u3gstub_detach),

	{ 0, 0 }
};

static driver_t u3gstub_driver = {
	"u3gstub",
	u3gstub_methods,
	0
};

DRIVER_MODULE(u3gstub, uhub, u3gstub_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(u3gstub, usb, 1, 1, 1);
