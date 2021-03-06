/* $Id: kftxx.c,v 1.4 1998/11/15 18:25:16 dfr Exp $ */
/* $NetBSD: kftxx.c,v 1.9 1998/05/14 00:01:32 thorpej Exp $ */

/*
 * Copyright (c) 1997 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * KFTIA and KFTHA Bus Adapter Node for I/O hoses
 * found on AlphaServer 8200 and 8400 systems.
 *
 * i.e., handler for all TLSB I/O nodes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/rpb.h>

#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/tlsbvar.h>
#include <alpha/tlsb/kftxxreg.h>
#include <alpha/tlsb/kftxxvar.h>

struct kft_softc {
	int		sc_node;	/* TLSB node */
	u_int16_t	sc_dtype;	/* device type */
};

/*
 * Instance variables for kft devices.
 */
struct kft_device {
	char *		kd_name;	/*  name */
	int		kd_node;	/* node number */
	u_int16_t	kd_dtype;	/* device type */
	u_int16_t	kd_hosenum;	/* hose number */
};

#define KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))

static devclass_t kft_devclass;

/*
 * Device methods
 */
static int kft_probe(device_t dev);
static void kft_print_child(device_t dev, device_t child);
static int kft_read_ivar(device_t dev, device_t child, int which, u_long *result);;

static device_method_t kft_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		kft_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	kft_print_child),
	DEVMETHOD(bus_read_ivar,	kft_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t kft_driver = {
	"kft",
	kft_methods,
	DRIVER_TYPE_MISC,
	1,			/* no softc */
};

static int
kft_probe(device_t dev)
{
	struct kft_softc *sc = (struct kft_softc *) device_get_softc(dev);
	struct kft_device* kd;
	int hoseno;

	if (!TLDEV_ISIOPORT(tlsb_get_dtype(dev)))
		return ENXIO;

	sc->sc_node = tlsb_get_node(dev);
	sc->sc_dtype = tlsb_get_dtype(dev);

	for (hoseno = 0; hoseno < MAXHOSE; hoseno++) {
		u_int32_t value =
			TLSB_GET_NODEREG(sc->sc_node, KFT_IDPNSEX(hoseno));
		if (value & 0x0E000000) {
			printf("%s%d: Hose %d IDPNSE has %x\n",
			       device_get_name(dev), device_get_unit(dev),
			       hoseno, value);
			continue;
		}
		if ((value & 0x1) != 0x0) {
			printf("%s%d: Hose %d has a Bad Cable (0x%x)\n",
			       device_get_name(dev), device_get_unit(dev),
			       hoseno, value);
			continue;
		}
		if ((value & 0x6) != 0x6) {
			if (value)
				printf("%s%d: Hose %d is missing PWROK (0x%x)\n",
				       device_get_name(dev), device_get_unit(dev),
				       hoseno, value);
			continue;
		}

		kd = (struct kft_device*) malloc(sizeof(struct kft_device),
						 M_DEVBUF, M_NOWAIT);
		if (!kd) continue;

		kd->kd_name = "dwlpx";
		kd->kd_node = sc->sc_node;
		kd->kd_dtype = sc->sc_dtype;
		kd->kd_hosenum = hoseno;
		device_add_child(dev, kd->kd_name, -1, kd);
	}

	return 0;
}

static void
kft_print_child(device_t bus, device_t dev)
{
	struct kft_device *kd = (struct kft_device*) device_get_ivars(dev);

	printf(" at %s%d hose %d",
	       device_get_name(bus), device_get_unit(bus),
	       kd->kd_hosenum);
}

static int
kft_read_ivar(device_t bus, device_t dev,
	      int index, u_long* result)
{
	struct kft_device *kd = (struct kft_device*) device_get_ivars(dev);

	switch (index) {
	case KFT_IVAR_NAME:
		*result = (u_long) kd->kd_name;
		return 0;

	case KFT_IVAR_NODE:
		*result = (u_long) kd->kd_node;
		return 0;

	case KFT_IVAR_DTYPE:
		*result = (u_long) kd->kd_dtype;
		return 0;

	case KFT_IVAR_HOSENUM:
		*result = (u_long) kd->kd_hosenum;
		return 0;

	default:
		return ENOENT;
	}
}

DRIVER_MODULE(kft, tlsb, kft_driver, kft_devclass, 0, 0);
