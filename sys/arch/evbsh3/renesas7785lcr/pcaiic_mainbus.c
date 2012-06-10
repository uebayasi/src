/*	$NetBSD$	*/

/*
 * Copyright (c) 2010 NONAKA Kimihiro <nonaka@netbsd.org>
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <dev/i2c/i2cvar.h>

#include <dev/ic/pca9564var.h>

#include <arch/evbsh3/renesas7785lcr/reg.h>

static int	pcaiic_mainbus_match(device_t, cfdata_t, void *);
static void	pcaiic_mainbus_attach(device_t, device_t, void *);

static uint8_t	pcaiic_read_byte(device_t, int reg);
static void	pcaiic_write_byte(device_t, int reg, uint8_t val);

struct pcaiic_mainbus_softc {
	struct pca9564_softc sc_pca;
};

CFATTACH_DECL_NEW(pcaiic, sizeof(struct pcaiic_mainbus_softc),
	pcaiic_mainbus_match, pcaiic_mainbus_attach, NULL, NULL);

static int
pcaiic_mainbus_match(device_t parent, cfdata_t match, void *aux)
{

	return 1;
}

static void
pcaiic_mainbus_attach(device_t parent, device_t self, void *aux)
{
	extern struct _bus_space renesas7785lcr_bus_iomem;
	struct pcaiic_mainbus_softc *psc = device_private(self);
	struct pca9564_softc *sc = &psc->sc_pca;

	sc->sc_dev = self;
	sc->sc_iot = &renesas7785lcr_bus_iomem;

	if (bus_space_map(sc->sc_iot, PCA9564_ADDRESS, 4, 0, &sc->sc_ioh)) {
		aprint_error(": can't map register space\n");
               	return;
	}

	sc->sc_ios.read_byte = pcaiic_read_byte;
	sc->sc_ios.write_byte = pcaiic_write_byte;

	sc->sc_i2c_clock = 59000;	/* 59kHz */

	pca9564_attach(sc);
}

static uint8_t
pcaiic_read_byte(device_t dev, int reg)
{
	struct pcaiic_mainbus_softc *psc = device_private(dev);
	struct pca9564_softc *sc = &psc->sc_pca;

	return bus_space_read_1(sc->sc_iot, sc->sc_ioh, reg);
}

static void
pcaiic_write_byte(device_t dev, int reg, uint8_t val)
{
	struct pcaiic_mainbus_softc *psc = device_private(dev);
	struct pca9564_softc *sc = &psc->sc_pca;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, reg, val);
}
