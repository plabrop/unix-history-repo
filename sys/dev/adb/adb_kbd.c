/*-
 * Copyright (C) 2008 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kbio.h>
#include <sys/condvar.h>
#include <sys/callout.h>
#include <sys/kernel.h>

#include <machine/bus.h>

#include "opt_kbd.h"
#include <dev/kbd/kbdreg.h>
#include <dev/kbd/kbdtables.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "adb.h"

#define KBD_DRIVER_NAME "akbd"

#define AKBD_EMULATE_ATKBD 1

static int adb_kbd_probe(device_t dev);
static int adb_kbd_attach(device_t dev);
static int adb_kbd_detach(device_t dev);
static void akbd_repeat(void *xsc);

static u_int adb_kbd_receive_packet(device_t dev, u_char status, 
	u_char command, u_char reg, int len, u_char *data);

struct adb_kbd_softc {
	keyboard_t sc_kbd;

	device_t sc_dev;
	struct mtx sc_mutex;
	struct cv  sc_cv;

	int sc_mode;
	int sc_state;

	int have_led_control;

	uint8_t buffer[8];
	volatile int buffers;

	struct callout sc_repeater;
	int sc_repeatstart;
	int sc_repeatcontinue;
	uint8_t last_press;
};

static device_method_t adb_kbd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         adb_kbd_probe),
        DEVMETHOD(device_attach,        adb_kbd_attach),
        DEVMETHOD(device_detach,        adb_kbd_detach),
        DEVMETHOD(device_shutdown,      bus_generic_shutdown),
        DEVMETHOD(device_suspend,       bus_generic_suspend),
        DEVMETHOD(device_resume,        bus_generic_resume),

	/* ADB interface */
	DEVMETHOD(adb_receive_packet,	adb_kbd_receive_packet),

	{ 0, 0 }
};

static driver_t adb_kbd_driver = {
	"akbd",
	adb_kbd_methods,
	sizeof(struct adb_kbd_softc),
};

static devclass_t adb_kbd_devclass;

DRIVER_MODULE(akbd, adb, adb_kbd_driver, adb_kbd_devclass, 0, 0);

static const uint8_t adb_to_at_scancode_map[128] = { 30, 31, 32, 33, 35, 34, 
	44, 45, 46, 47, 0, 48, 16, 17, 18, 19, 21, 20, 2, 3, 4, 5, 7, 6, 13, 
	10, 8, 12, 9, 11, 27, 24, 22, 26, 23, 25, 28, 38, 36, 40, 37, 39, 43, 
	51, 53, 49, 50, 52, 15, 57, 41, 14, 0, 1, 29, 0, 42, 58, 56, 97, 98, 
	100, 95, 0, 0, 83, 0, 55, 0, 78, 0, 69, 0, 0, 0, 91, 89, 0, 74, 13, 0, 
	0, 82, 79, 80, 81, 75, 76, 77, 71, 0, 72, 73, 0, 0, 0, 63, 64, 65, 61, 
	66, 67, 0, 87, 0, 105, 0, 70, 0, 68, 0, 88, 0, 107, 102, 94, 96, 103, 
	62, 99, 60, 101, 59, 54, 93, 90, 0, 0 };

/* keyboard driver declaration */
static int              akbd_configure(int flags);
static kbd_probe_t      akbd_probe;
static kbd_init_t       akbd_init;
static kbd_term_t       akbd_term;
static kbd_intr_t       akbd_interrupt;
static kbd_test_if_t    akbd_test_if;
static kbd_enable_t     akbd_enable;
static kbd_disable_t    akbd_disable;
static kbd_read_t       akbd_read;
static kbd_check_t      akbd_check;
static kbd_read_char_t  akbd_read_char;
static kbd_check_char_t akbd_check_char;
static kbd_ioctl_t      akbd_ioctl;
static kbd_lock_t       akbd_lock;
static kbd_clear_state_t akbd_clear_state;
static kbd_get_state_t  akbd_get_state;
static kbd_set_state_t  akbd_set_state;
static kbd_poll_mode_t  akbd_poll;

keyboard_switch_t akbdsw = {
        akbd_probe,
        akbd_init,
        akbd_term,
        akbd_interrupt,
        akbd_test_if,
        akbd_enable,
        akbd_disable,
        akbd_read,
        akbd_check,
        akbd_read_char,
        akbd_check_char,
        akbd_ioctl,
        akbd_lock,
        akbd_clear_state,
        akbd_get_state,
        akbd_set_state,
        genkbd_get_fkeystr,
        akbd_poll,
        genkbd_diag,
};

KEYBOARD_DRIVER(akbd, akbdsw, akbd_configure);

static int 
adb_kbd_probe(device_t dev) 
{
	uint8_t type;

	type = adb_get_device_type(dev);

	if (type != ADB_DEVICE_KEYBOARD)
		return (ENXIO);

	switch(adb_get_device_handler(dev)) {
	case 1:
		device_set_desc(dev,"Apple Standard Keyboard");
		break;
	case 2:
		device_set_desc(dev,"Apple Extended Keyboard");
		break;
	case 4:
		device_set_desc(dev,"Apple ISO Keyboard");
		break;
	case 5:
		device_set_desc(dev,"Apple Extended ISO Keyboard");
		break;
	case 8:
		device_set_desc(dev,"Apple Keyboard II");
		break;
	case 9:
		device_set_desc(dev,"Apple ISO Keyboard II");
		break;
	case 12:
		device_set_desc(dev,"PowerBook Keyboard");
		break;
	case 13:
		device_set_desc(dev,"PowerBook ISO Keyboard");
		break;
	case 24:
		device_set_desc(dev,"PowerBook Extended Keyboard");
		break;
	case 27:
		device_set_desc(dev,"Apple Design Keyboard");
		break;
	case 195:
		device_set_desc(dev,"PowerBook G3 Keyboard");
		break;
	case 196:
		device_set_desc(dev,"iBook Keyboard");
		break;
	default:
		device_set_desc(dev,"ADB Keyboard");
		break;
	}

	return (0);
}

static int
ms_to_ticks(int ms)
{
	if (hz > 1000)
		return ms*(hz/1000);

	return ms/(1000/hz);
}
	
static int 
adb_kbd_attach(device_t dev) 
{
	struct adb_kbd_softc *sc;
	keyboard_switch_t *sw;

	sw = kbd_get_switch(KBD_DRIVER_NAME);
	if (sw == NULL) {
		return ENXIO;
	}

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_mode = K_RAW;
	sc->sc_state = 0;
	sc->have_led_control = 0;
	sc->buffers = 0;

	/* Try stepping forward to the extended keyboard protocol */
	adb_set_device_handler(dev,3);

	mtx_init(&sc->sc_mutex,KBD_DRIVER_NAME,MTX_DEF,0);
	cv_init(&sc->sc_cv,KBD_DRIVER_NAME);
	callout_init(&sc->sc_repeater, 0);

#ifdef AKBD_EMULATE_ATKBD
	kbd_init_struct(&sc->sc_kbd, KBD_DRIVER_NAME, KB_101, 0, 0, 0, 0);
	kbd_set_maps(&sc->sc_kbd, &key_map, &accent_map, fkey_tab,
            sizeof(fkey_tab) / sizeof(fkey_tab[0]));
#else
	#error ADB raw mode not implemented
#endif

	KBD_FOUND_DEVICE(&sc->sc_kbd);
	KBD_PROBE_DONE(&sc->sc_kbd);
	KBD_INIT_DONE(&sc->sc_kbd);
	KBD_CONFIG_DONE(&sc->sc_kbd);

	(*sw->enable)(&sc->sc_kbd);

	kbd_register(&sc->sc_kbd);

#ifdef KBD_INSTALL_CDEV
	if (kbd_attach(&sc->sc_kbd)) {
		adb_kbd_detach(dev);
		return ENXIO;
	}
#endif

	adb_set_autopoll(dev,1);

	/* Check (asynchronously) if we can read out the LED state from 
	   this keyboard by reading the key state register */
	adb_send_packet(dev,ADB_COMMAND_TALK,2,0,NULL);

	return (0);
}

static int 
adb_kbd_detach(device_t dev) 
{
	struct adb_kbd_softc *sc;
	keyboard_t *kbd;

	sc = device_get_softc(dev);

	adb_set_autopoll(dev,0);
	callout_stop(&sc->sc_repeater);

	mtx_lock(&sc->sc_mutex);

	kbd = kbd_get_keyboard(kbd_find_keyboard(KBD_DRIVER_NAME,
	          device_get_unit(dev)));

	kbdd_disable(kbd);

#ifdef KBD_INSTALL_CDEV
	kbd_detach(kbd);
#endif

	kbdd_term(kbd);

	mtx_unlock(&sc->sc_mutex);

	mtx_destroy(&sc->sc_mutex);
	cv_destroy(&sc->sc_cv);

	return (0);
}

static u_int 
adb_kbd_receive_packet(device_t dev, u_char status, 
    u_char command, u_char reg, int len, u_char *data)
{
	struct adb_kbd_softc *sc;

	sc = device_get_softc(dev);

	if (command != ADB_COMMAND_TALK)
		return 0;

	if (reg == 2 && len == 2) {
		sc->have_led_control = 1;
		return 0;
	}

	if (reg != 0 || len != 2)
		return (0);

	mtx_lock(&sc->sc_mutex);
		if ((data[0] & 0x7f) == 57 && sc->buffers < 7) {
			/* Fake the down/up cycle for caps lock */
			sc->buffer[sc->buffers++] = data[0] & 0x7f;
			sc->buffer[sc->buffers++] = (data[0] & 0x7f) | (1 << 7);
		} else {
			sc->buffer[sc->buffers++] = data[0];
		}

		if (sc->buffer[sc->buffers-1] < 0xff)
			sc->last_press = sc->buffer[sc->buffers-1];

		if ((data[1] & 0x7f) == 57 && sc->buffers < 7) {
			/* Fake the down/up cycle for caps lock */
			sc->buffer[sc->buffers++] = data[1] & 0x7f;
			sc->buffer[sc->buffers++] = (data[1] & 0x7f) | (1 << 7);
		} else {
			sc->buffer[sc->buffers++] = data[1];
		}

		if (sc->buffer[sc->buffers-1] < 0xff)
			sc->last_press = sc->buffer[sc->buffers-1];

		/* Stop any existing key repeating */
		callout_stop(&sc->sc_repeater);

		/* Schedule a repeat callback on keydown */
		if (!(sc->last_press & (1 << 7))) {
			callout_reset(&sc->sc_repeater, 
			    ms_to_ticks(sc->sc_kbd.kb_delay1), akbd_repeat, sc);
		}
	mtx_unlock(&sc->sc_mutex);

	cv_broadcast(&sc->sc_cv);

	if (KBD_IS_ACTIVE(&sc->sc_kbd) && KBD_IS_BUSY(&sc->sc_kbd)) {
		sc->sc_kbd.kb_callback.kc_func(&sc->sc_kbd,
			 KBDIO_KEYINPUT, sc->sc_kbd.kb_callback.kc_arg);
	}

	return (0);
}

static void
akbd_repeat(void *xsc) {
	struct adb_kbd_softc *sc = xsc;
	int notify_kbd = 0;

	/* Fake an up/down key repeat so long as we have the
	   free buffers */
	mtx_lock(&sc->sc_mutex);
		if (sc->buffers < 7) {
			sc->buffer[sc->buffers++] = sc->last_press | (1 << 7);
			sc->buffer[sc->buffers++] = sc->last_press;

			notify_kbd = 1;
		}
	mtx_unlock(&sc->sc_mutex);

	if (notify_kbd && KBD_IS_ACTIVE(&sc->sc_kbd) 
	    && KBD_IS_BUSY(&sc->sc_kbd)) {
		sc->sc_kbd.kb_callback.kc_func(&sc->sc_kbd,
		    KBDIO_KEYINPUT, sc->sc_kbd.kb_callback.kc_arg);
	}

	/* Reschedule the callout */
	callout_reset(&sc->sc_repeater, ms_to_ticks(sc->sc_kbd.kb_delay2),
	    akbd_repeat, sc);
}
	
static int 
akbd_configure(int flags) 
{
	return 0;
}

static int 
akbd_probe(int unit, void *arg, int flags) 
{
	return 0;
}

static int 
akbd_init(int unit, keyboard_t **kbdp, void *arg, int flags) 
{
	return 0;
}

static int 
akbd_term(keyboard_t *kbd) 
{
	return 0;
}

static int 
akbd_interrupt(keyboard_t *kbd, void *arg) 
{
	return 0;
}

static int 
akbd_test_if(keyboard_t *kbd) 
{
	return 0;
}

static int 
akbd_enable(keyboard_t *kbd) 
{
	KBD_ACTIVATE(kbd);
	return (0);
}

static int 
akbd_disable(keyboard_t *kbd) 
{
	struct adb_kbd_softc *sc;
	sc = (struct adb_kbd_softc *)(kbd);

	callout_stop(&sc->sc_repeater);
	KBD_DEACTIVATE(kbd);
	return (0);
}

static int 
akbd_read(keyboard_t *kbd, int wait) 
{
	return (0);
}

static int 
akbd_check(keyboard_t *kbd) 
{
	struct adb_kbd_softc *sc;

	if (!KBD_IS_ACTIVE(kbd))
		return (FALSE);

	sc = (struct adb_kbd_softc *)(kbd);

	mtx_lock(&sc->sc_mutex);
		if (sc->buffers > 0) {
			mtx_unlock(&sc->sc_mutex);
			return (TRUE); 
		}
	mtx_unlock(&sc->sc_mutex);

	return (FALSE);
}

static u_int 
akbd_read_char(keyboard_t *kbd, int wait) 
{
	struct adb_kbd_softc *sc;
	uint8_t adb_code, final_scancode;
	int i;

	sc = (struct adb_kbd_softc *)(kbd);

	mtx_lock(&sc->sc_mutex);
		if (!sc->buffers && wait)
			cv_wait(&sc->sc_cv,&sc->sc_mutex);

		if (!sc->buffers) {
			mtx_unlock(&sc->sc_mutex);
			return (0);
		}

		adb_code = sc->buffer[0];

		for (i = 1; i < sc->buffers; i++)
			sc->buffer[i-1] = sc->buffer[i];

		sc->buffers--;
	mtx_unlock(&sc->sc_mutex);

	#ifdef AKBD_EMULATE_ATKBD
		final_scancode = adb_to_at_scancode_map[adb_code & 0x7f];
		final_scancode |= adb_code & 0x80;
	#else
		final_scancode = adb_code;
	#endif

	return (final_scancode);
}

static int 
akbd_check_char(keyboard_t *kbd) 
{
	if (!KBD_IS_ACTIVE(kbd))
		return (FALSE);

	return (akbd_check(kbd));
}

static int
set_typematic(keyboard_t *kbd, int code)
{
	/* These numbers are in microseconds, so convert to ticks */

	static int delays[] = { 250, 500, 750, 1000 };
	static int rates[] = {  34,  38,  42,  46,  50,  55,  59,  63,
				68,  76,  84,  92, 100, 110, 118, 126,
				136, 152, 168, 184, 200, 220, 236, 252,
				272, 304, 336, 368, 400, 440, 472, 504 };
		
	if (code & ~0x7f)
		return EINVAL;
	kbd->kb_delay1 = delays[(code >> 5) & 3];
	kbd->kb_delay2 = rates[code & 0x1f];
	return 0;
}

static int akbd_ioctl(keyboard_t *kbd, u_long cmd, caddr_t data)
{
	struct adb_kbd_softc *sc;
	uint16_t r2;
	int error;

	sc = (struct adb_kbd_softc *)(kbd);
	error = 0;

	switch (cmd) {
	case KDGKBMODE:
		*(int *)data = sc->sc_mode;
		break;
	case KDSKBMODE:
		switch (*(int *)data) {
		case K_XLATE:
			if (sc->sc_mode != K_XLATE) {
				/* make lock key state and LED state match */
				sc->sc_state &= ~LOCK_MASK;
				sc->sc_state |= KBD_LED_VAL(kbd);
			}
			/* FALLTHROUGH */
		case K_RAW:
		case K_CODE:
			if (sc->sc_mode != *(int *)data) 
				sc->sc_mode = *(int *)data;
			break;
		default:
			error = EINVAL;
			break;
		}

		break;

	case KDGETLED:
		*(int *)data = KBD_LED_VAL(kbd);
		break;

	case KDSKBSTATE:
		if (*(int *)data & ~LOCK_MASK) {
			error = EINVAL;
			break;
		}
		sc->sc_state &= ~LOCK_MASK;
		sc->sc_state |= *(int *)data;

		/* FALLTHROUGH */

	case KDSETLED:
		KBD_LED_VAL(kbd) = *(int *)data;
	
		if (!sc->have_led_control)
			break;

		r2 = (~0 & 0x04) | 3;

		if (*(int *)data & NLKED)
			r2 &= ~1;
		if (*(int *)data & CLKED)
			r2 &= ~2;
		if (*(int *)data & SLKED)
			r2 &= ~4;

		adb_send_packet(sc->sc_dev,ADB_COMMAND_LISTEN,2,
			sizeof(uint16_t),(u_char *)&r2);
		
		break;

	case KDGKBSTATE:
		*(int *)data = sc->sc_state & LOCK_MASK;
		break;

	case KDSETREPEAT:
		if (!KBD_HAS_DEVICE(kbd))
			return 0;
		if (((int *)data)[1] < 0)
			return EINVAL;
		if (((int *)data)[0] < 0)
			return EINVAL;
		else if (((int *)data)[0] == 0)  /* fastest possible value */
			kbd->kb_delay1 = 200;
		else
			kbd->kb_delay1 = ((int *)data)[0];
		kbd->kb_delay2 = ((int *)data)[1];

		break;

	case KDSETRAD:
		error = set_typematic(kbd, *(int *)data);
		break;

	case PIO_KEYMAP:
	case PIO_KEYMAPENT:
	case PIO_DEADKEYMAP:
	default:
		return (genkbd_commonioctl(kbd, cmd, data));
	}

	return (error);
}

static int akbd_lock(keyboard_t *kbd, int lock)
{
	return (0);
}

static void akbd_clear_state(keyboard_t *kbd)
{
}

static int akbd_get_state(keyboard_t *kbd, void *buf, size_t len)
{
	return (0);
}

static int akbd_set_state(keyboard_t *kbd, void *buf, size_t len)
{
	return (0);
}

static int akbd_poll(keyboard_t *kbd, int on) 
{
	return (0);
}

static int
akbd_modevent(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		kbd_add_driver(&akbd_kbd_driver);
		break;

	case MOD_UNLOAD:
		kbd_delete_driver(&akbd_kbd_driver);
		break;

	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

DEV_MODULE(akbd, akbd_modevent, NULL);

