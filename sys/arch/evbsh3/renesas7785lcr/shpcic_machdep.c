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

/*
 * Machine-specific functions for PCI autoconfiguration.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/extent.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciconf.h>

#include <sys/bus.h>
#include <machine/intr.h>
#include <machine/pci_machdep.h>

static int	shpcic_match(device_t, cfdata_t, void *);
static void	shpcic_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(shpcic, 0,
    shpcic_match, shpcic_attach, NULL, NULL);

static int
shpcic_match(device_t parent, cfdata_t cf, void *aux)
{

	return shpcic_match_common(parent, cf, aux);
}

static void
shpcic_attach(device_t parent, device_t self, void *aux)
{

	shpcic_attach_common(parent, self, aux, SH7780PCIC_FLAGS_NO_RESET);
}

bus_space_tag_t
shpcic_get_bus_io_tag(void)
{
	extern struct _bus_space renesas7785lcr_pci_bus_io;

	return &renesas7785lcr_pci_bus_io;
}

bus_space_tag_t
shpcic_get_bus_mem_tag(void)
{
	extern struct _bus_space renesas7785lcr_pci_bus_mem;

	return &renesas7785lcr_pci_bus_mem;
}

bus_dma_tag_t
shpcic_get_bus_dma_tag(void)
{
	extern struct _bus_dma_tag evbsh3_bus_dma;

	return &evbsh3_bus_dma;
}

void
evbsh3_pci_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{

	/* Nothing to do */
}

int
evbsh3_pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int pin = pa->pa_intrpin;
	int line = pa->pa_intrline;

	if (pin == 0) {
		/* No IRQ used. */
		goto bad;
	}

	if (pin > 4) {
		printf("pci_intr_map: bad interrupt pin %d\n", pin);
		goto bad;
	}

	if (line == 255) {
		printf("pci_intr_map: no mapping for pin %c\n", '@' + pin);
		goto bad;
	}

	*ihp = line;
	return 0;

bad:
	*ihp = -1;
	return 1;
}

const char *
evbsh3_pci_intr_string(void *v, pci_intr_handle_t ih)
{
	static char irqstr[8];		/* 4 + 2 + NULL + sanity */

	sprintf(irqstr, "irq %d", ih);

	return (irqstr);
}

const struct evcnt *
evbsh3_pci_intr_evcnt(void *v, pci_intr_handle_t ih)
{

	/* XXX for now, no evcnt parent reported */
	return (NULL);
}

void *
evbsh3_pci_intr_establish(void *v, pci_intr_handle_t ih, int level,
    int (*ih_fun)(void *), void *ih_arg)
{
	int evtcode;

	evtcode = 0xa20 + (ih << 5);
	return intc_intr_establish(evtcode, IST_LEVEL, level, ih_fun, ih_arg);
}

void
evbsh3_pci_intr_disestablish(void *v, void *cookie)
{

	intc_intr_disestablish(cookie);
}

void
evbsh3_pci_conf_interrupt(void *v, int bus, int dev, int pin, int swiz,
    int *iline)
{
	static const int irq[4] = { 0, 1, 2, 3 };

	*iline = -1;
	if ((dev >= 0 && dev <= 3) && (pin >= 1 && pin <= 4)) {
		*iline = irq[(dev + pin - 1) & 3];
	}
}

int
evbsh3_pci_conf_hook(void *v, int bus, int dev, int func, pcireg_t id)
{

	return PCI_CONF_ALL & ~PCI_CONF_MAP_ROM;
}

/*
 * shpcic bus space
 */
struct _bus_space renesas7785lcr_pci_bus_io =
{
	.bs_cookie = NULL,

	.bs_map = shpcic_iomem_map,
	.bs_unmap = shpcic_iomem_unmap,
	.bs_subregion = shpcic_iomem_subregion,

	.bs_alloc = shpcic_iomem_alloc,
	.bs_free = shpcic_iomem_free,

	.bs_mmap = shpcic_iomem_mmap,

	.bs_r_1 = shpcic_io_read_1,
	.bs_r_2 = shpcic_io_read_2,
	.bs_r_4 = shpcic_io_read_4,

	.bs_rm_1 = shpcic_io_read_multi_1,
	.bs_rm_2 = shpcic_io_read_multi_2,
	.bs_rm_4 = shpcic_io_read_multi_4,

	.bs_rr_1 = shpcic_io_read_region_1,
	.bs_rr_2 = shpcic_io_read_region_2,
	.bs_rr_4 = shpcic_io_read_region_4,

	.bs_w_1 = shpcic_io_write_1,
	.bs_w_2 = shpcic_io_write_2,
	.bs_w_4 = shpcic_io_write_4,

	.bs_wm_1 = shpcic_io_write_multi_1,
	.bs_wm_2 = shpcic_io_write_multi_2,
	.bs_wm_4 = shpcic_io_write_multi_4,

	.bs_wr_1 = shpcic_io_write_region_1,
	.bs_wr_2 = shpcic_io_write_region_2,
	.bs_wr_4 = shpcic_io_write_region_4,

	.bs_sm_1 = shpcic_io_set_multi_1,
	.bs_sm_2 = shpcic_io_set_multi_2,
	.bs_sm_4 = shpcic_io_set_multi_4,

	.bs_sr_1 = shpcic_io_set_region_1,
	.bs_sr_2 = shpcic_io_set_region_2,
	.bs_sr_4 = shpcic_io_set_region_4,

	.bs_c_1 = shpcic_io_copy_region_1,
	.bs_c_2 = shpcic_io_copy_region_2,
	.bs_c_4 = shpcic_io_copy_region_4,
};

struct _bus_space renesas7785lcr_pci_bus_mem =
{
	.bs_cookie = NULL,

	.bs_map = shpcic_iomem_map,
	.bs_unmap = shpcic_iomem_unmap,
	.bs_subregion = shpcic_iomem_subregion,

	.bs_alloc = shpcic_iomem_alloc,
	.bs_free = shpcic_iomem_free,

	.bs_mmap = shpcic_iomem_mmap,

	.bs_r_1 = shpcic_mem_read_1,
	.bs_r_2 = shpcic_mem_read_2,
	.bs_r_4 = shpcic_mem_read_4,

	.bs_rm_1 = shpcic_mem_read_multi_1,
	.bs_rm_2 = shpcic_mem_read_multi_2,
	.bs_rm_4 = shpcic_mem_read_multi_4,

	.bs_rr_1 = shpcic_mem_read_region_1,
	.bs_rr_2 = shpcic_mem_read_region_2,
	.bs_rr_4 = shpcic_mem_read_region_4,

	.bs_w_1 = shpcic_mem_write_1,
	.bs_w_2 = shpcic_mem_write_2,
	.bs_w_4 = shpcic_mem_write_4,

	.bs_wm_1 = shpcic_mem_write_multi_1,
	.bs_wm_2 = shpcic_mem_write_multi_2,
	.bs_wm_4 = shpcic_mem_write_multi_4,

	.bs_wr_1 = shpcic_mem_write_region_1,
	.bs_wr_2 = shpcic_mem_write_region_2,
	.bs_wr_4 = shpcic_mem_write_region_4,

	.bs_sm_1 = shpcic_mem_set_multi_1,
	.bs_sm_2 = shpcic_mem_set_multi_2,
	.bs_sm_4 = shpcic_mem_set_multi_4,

	.bs_sr_1 = shpcic_mem_set_region_1,
	.bs_sr_2 = shpcic_mem_set_region_2,
	.bs_sr_4 = shpcic_mem_set_region_4,

	.bs_c_1 = shpcic_mem_copy_region_1,
	.bs_c_2 = shpcic_mem_copy_region_2,
	.bs_c_4 = shpcic_mem_copy_region_4,
};
