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

#include "opt_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciconf.h>
#include <dev/pci/pcidevs.h>

#include <sh3/bscreg.h>
#include <sh3/cache.h>
#include <sh3/exception.h>
#include <sh3/pcicreg.h>

#include <machine/autoconf.h>
#include <sys/bus.h>
#include <machine/intr.h>
#include <machine/pci_machdep.h>

#if defined(DEBUG) && !defined(SHPCIC_DEBUG)
#define SHPCIC_DEBUG 0
#endif
#if defined(SHPCIC_DEBUG)
int shpcic_debug = SHPCIC_DEBUG + 0;
#define	DPRINTF(arg)	if (shpcic_debug) printf arg
#else
#define	DPRINTF(arg)
#endif

#define	PCI_MODE1_ENABLE	0x80000000UL

#define	PCI_MEM_ADDR32		0xc0000000UL
#define	PCI_MEM_SIZE32		512

extern struct cfdriver shpcic_cd;

int
shpcic_match_common(device_t parent, cfdata_t cf, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	pcireg_t id;

	if (strcmp(ma->ma_name, shpcic_cd.cd_name) != 0)
		return 0;

	switch (cpu_product) {
	case CPU_PRODUCT_7780:
	case CPU_PRODUCT_7785:
		break;

	default:
		return 0;
	}

	_reg_write_4(SH7780_PCIECR, PCIECR_ENBL);

	id = _reg_read_4(SH7780_PCIVID);

	switch (PCI_VENDOR(id)) {
	case PCI_VENDOR_RENESAS:
		break;

	default:
		goto fail;
	}

	switch (PCI_PRODUCT(id)) {
	case PCI_PRODUCT_RENESAS_SH7780:
	case PCI_PRODUCT_RENESAS_SH7785:
		break;

	default:
		goto fail;
	}

	_reg_write_4(SH7780_PCIECR, 0);
	return 1;

fail:
	_reg_write_4(SH7780_PCIECR, 0);
	return 0;
}

void
shpcic_attach_common(device_t parent, device_t self, void *aux, uint flags)
{
	struct pcibus_attach_args pba;
#ifdef PCI_NETBSD_CONFIGURE
	struct extent *ioext, *memext;
	paddr_t memstart, memsize;
#endif

	aprint_naive("\n");
	aprint_normal("\n");

	_reg_write_4(SH7780_PCIECR, PCIECR_ENBL);

	if ((flags & SH7780PCIC_FLAGS_NO_RESET) == 0) {
		/* Initialize PCIC */
		_reg_write_4(SH7780_PCICR, PCICR_BASE | PCICR_RSTCTL);
		delay(100 * 1000);
		_reg_write_4(SH7780_PCICR, PCICR_BASE);
	}

	/* Enable I/O, memory, bus-master */
	_reg_write_4(SH7780_PCICMD, PCI_COMMAND_IO_ENABLE
	                            | PCI_COMMAND_MEM_ENABLE
	                            | PCI_COMMAND_MASTER_ENABLE
	                            | PCI_COMMAND_PARITY_ENABLE);

	/* Class: Host-Bridge */
	_reg_write_4(SH7780_PCIRID,
	    PCI_CLASS_CODE(PCI_CLASS_BRIDGE, PCI_SUBCLASS_BRIDGE_HOST, 0x00));

#define	PCILSR(s)	(((((s) << 20) - 1) & 0xfff00000) | 1)
#if defined(SH4A_EXT_ADDR32)
	/* set PCI local address 0 */
	_reg_write_4(SH7780_PCILSR0, PCILSR(512));
	_reg_write_4(SH7780_PCILAR0, 0x40000000);
	_reg_write_4(SH7780_PCIMBAR0, 0x40000000);

	/* set PCI local address 1 */
	_reg_write_4(SH7780_PCILSR1, 0);
	_reg_write_4(SH7780_PCILAR1, 0);
	_reg_write_4(SH7780_PCIMBAR1, 0);
#else
	/* set PCI local address 0 */
	_reg_write_4(SH7780_PCILSR0, PCILSR(128));
	_reg_write_4(SH7780_PCILAR0, 0xa8000000);
	_reg_write_4(SH7780_PCIMBAR0, 0xa8000000);

	/* set PCI local address 1 */
	_reg_write_4(SH7780_PCILSR1, PCILSR(128));
	_reg_write_4(SH7780_PCILAR1, 0xa8000000);
	_reg_write_4(SH7780_PCIMBAR1, 0x88000000);
#endif
#undef	PCILSR

	_reg_write_4(SH7780_PCIMBR0, SH7780_PCIC_MEM);
	_reg_write_4(SH7780_PCIMBMR0, (SH7780_PCIC_MEM_SIZE - 1) & 0xfc0000);

	_reg_write_4(SH7780_PCIMBR1, 0x10000000);
	_reg_write_4(SH7780_PCIMBMR1, ((64 << 20) - 1) & 0x3fc0000);

	_reg_write_4(SH7780_PCIMBR2, PCI_MEM_ADDR32);
	_reg_write_4(SH7780_PCIMBMR2, ((PCI_MEM_SIZE32 << 20) - 1) & 0x1ffc0000);

	/* set PCI I/O, memory base address */
	_reg_write_4(SH7780_PCIIOBR, SH7780_PCIC_IO);
	_reg_write_4(SH7780_PCIIOBMR, (SH7780_PCIC_IO_SIZE - 1) & 0x1c0000);

	/* disable snoop */
	_reg_write_4(SH7780_PCICSCR0, 0);
	_reg_write_4(SH7780_PCICSCR1, 0);
	_reg_write_4(SH7780_PCICSAR0, 0);
	_reg_write_4(SH7780_PCICSAR1, 0);

//	sh7780pcic_fixup_hook();

	/* Initialize done. */
	_reg_write_4(SH7780_PCICR, PCICR_BASE
	                           | PCICR_PFCS
	                           | PCICR_FTO
	                           | PCICR_PFE
	                           | PCICR_BMABT
	                           | PCICR_CFINIT);

#ifdef PCI_NETBSD_CONFIGURE
	/* configure PCI bus */
	ioext  = extent_create("pciio",
	    SH7780_PCIC_IO, SH7780_PCIC_IO + SH7780_PCIC_IO_SIZE - 1,
	    NULL, 0, EX_NOWAIT);
#if defined(SH4A_EXT_ADDR32)
	memstart = PCI_MEM_ADDR32;
	memsize = PCI_MEM_SIZE32 * 1024 * 1024;
#else
	memstart = SH7780_PCIC_MEM;
	memsize = SH7780_PCIC_MEM_SIZE;
#endif
	memext = extent_create("pcimem", memstart, memstart + memsize - 1,
	    NULL, 0, EX_NOWAIT);

	pci_configure_bus(NULL, ioext, memext, NULL, 0, sh_cache_line_size);

	extent_destroy(ioext);
	extent_destroy(memext);
#endif

	/* PCI bus */
	memset(&pba, 0, sizeof(pba));
	pba.pba_iot = shpcic_get_bus_io_tag();
	pba.pba_memt = shpcic_get_bus_mem_tag();
	pba.pba_dmat = shpcic_get_bus_dma_tag();
	pba.pba_dmat64 = NULL;
	pba.pba_pc = NULL;
	pba.pba_bus = 0;
	pba.pba_bridgetag = NULL;
	pba.pba_flags = PCI_FLAGS_IO_OKAY | PCI_FLAGS_MEM_OKAY;
	config_found(self, &pba, NULL);
}

int
shpcic_bus_maxdevs(void *v, int busno)
{

	/*
	 * Bus number is irrelevant.  Configuration Mechanism 1 is in
	 * use, can have devices 0-32 (i.e. the `normal' range).
	 */
	return 32;
}

pcitag_t
shpcic_make_tag(void *v, int bus, int device, int function)
{
	pcitag_t tag;

	if (bus >= 256 || device >= 32 || function >= 8)
		panic("pci_make_tag: bad request");

	tag = PCI_MODE1_ENABLE |
		    (bus << 16) | (device << 11) | (function << 8);

	return tag;
}

void
shpcic_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp, int *fp)
{

	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x7;
}

pcireg_t
shpcic_conf_read(void *v, pcitag_t tag, int reg)
{
	pcireg_t data;
	int s;

	s = splhigh();
	_reg_write_4(SH7780_PCIPAR, tag | reg);
	data = _reg_read_4(SH7780_PCIPDR);
	splx(s);

	return data;
}

void
shpcic_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	int s;

	s = splhigh();
	_reg_write_4(SH7780_PCIPAR, tag | reg);
	_reg_write_4(SH7780_PCIPDR, data);
	splx(s);
}

void *
shpcic_intr_establish(int evtcode, int (*ih_func)(void *), void *ih_arg)
{

	return intc_intr_establish(evtcode, IST_LEVEL, IPL_BIO, ih_func, ih_arg); /*XXXNONAKA*/
}

void
shpcic_intr_disestablish(void *ih)
{

	intc_intr_disestablish(ih);
}

/*
 * shpcic bus space
 */
int
shpcic_iomem_map(void *v, bus_addr_t bpa, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{

	*bshp = (bus_space_handle_t)bpa;

	return (0);
}

void
shpcic_iomem_unmap(void *v, bus_space_handle_t bsh, bus_size_t size)
{

	/* Nothing to do */
}

int
shpcic_iomem_subregion(void *v, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{

	*nbshp = bsh + offset;

	return (0);
}

int
shpcic_iomem_alloc(void *v, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	*bshp = *bpap = rstart;

	return (0);
}

void
shpcic_iomem_free(void *v, bus_space_handle_t bsh, bus_size_t size)
{

	/* Nothing to do */
}

paddr_t
shpcic_iomem_mmap(void *v, bus_addr_t addr, off_t off, int prot, int flags)
{

	return (paddr_t)-1;
}

/*
 * shpcic bus space io/mem read/write
 */
/* read */
static inline uint8_t __shpcic_io_read_1(bus_space_handle_t bsh,
    bus_size_t offset);
static inline uint16_t __shpcic_io_read_2(bus_space_handle_t bsh,
    bus_size_t offset);
static inline uint32_t __shpcic_io_read_4(bus_space_handle_t bsh,
    bus_size_t offset);
static inline uint8_t __shpcic_mem_read_1(bus_space_handle_t bsh,
    bus_size_t offset);
static inline uint16_t __shpcic_mem_read_2(bus_space_handle_t bsh,
    bus_size_t offset);
static inline uint32_t __shpcic_mem_read_4(bus_space_handle_t bsh,
    bus_size_t offset);

static inline uint8_t
__shpcic_io_read_1(bus_space_handle_t bsh, bus_size_t offset)
{
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_IO_MASK;

	return *(volatile uint8_t *)(SH7780_PCIC_IO + adr);
}

static inline uint16_t
__shpcic_io_read_2(bus_space_handle_t bsh, bus_size_t offset)
{
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_IO_MASK;

	return *(volatile uint16_t *)(SH7780_PCIC_IO + adr);
}

static inline uint32_t
__shpcic_io_read_4(bus_space_handle_t bsh, bus_size_t offset)
{
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_IO_MASK;

	return *(volatile uint32_t *)(SH7780_PCIC_IO + adr);
}

static inline uint8_t
__shpcic_mem_read_1(bus_space_handle_t bsh, bus_size_t offset)
{
#if defined(SH4A_EXT_ADDR32)
	u_long adr = (u_long)(bsh + offset);

	return *(volatile uint8_t *)(PCI_MEM_ADDR32 + adr);
#else
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_MEM_MASK;

	return *(volatile uint8_t *)(SH7780_PCIC_MEM + adr);
#endif
}

static inline uint16_t
__shpcic_mem_read_2(bus_space_handle_t bsh, bus_size_t offset)
{
#if defined(SH4A_EXT_ADDR32)
	u_long adr = (u_long)(bsh + offset);

	return *(volatile uint16_t *)(PCI_MEM_ADDR32 + adr);
#else
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_MEM_MASK;

	return *(volatile uint16_t *)(SH7780_PCIC_MEM + adr);
#endif
}

static inline uint32_t
__shpcic_mem_read_4(bus_space_handle_t bsh, bus_size_t offset)
{
#if defined(SH4A_EXT_ADDR32)
	u_long adr = (u_long)(bsh + offset);

	return *(volatile uint32_t *)(PCI_MEM_ADDR32 + adr);
#else
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_MEM_MASK;

	return *(volatile uint32_t *)(SH7780_PCIC_MEM + adr);
#endif
}

/*
 * read single
 */
uint8_t
shpcic_io_read_1(void *v, bus_space_handle_t bsh, bus_size_t offset)
{
	uint8_t value;

	value = __shpcic_io_read_1(bsh, offset);

	return value;
}

uint16_t
shpcic_io_read_2(void *v, bus_space_handle_t bsh, bus_size_t offset)
{
	uint16_t value;

	value = __shpcic_io_read_2(bsh, offset);

	return value;
}

uint32_t
shpcic_io_read_4(void *v, bus_space_handle_t bsh, bus_size_t offset)
{
	uint32_t value;

	value = __shpcic_io_read_4(bsh, offset);

	return value;
}

uint8_t
shpcic_mem_read_1(void *v, bus_space_handle_t bsh, bus_size_t offset)
{
	uint8_t value;

	value = __shpcic_mem_read_1(bsh, offset);

	return value;
}

uint16_t
shpcic_mem_read_2(void *v, bus_space_handle_t bsh, bus_size_t offset)
{
	uint16_t value;

	value = __shpcic_mem_read_2(bsh, offset);

	return value;
}

uint32_t
shpcic_mem_read_4(void *v, bus_space_handle_t bsh, bus_size_t offset)
{
	uint32_t value;

	value = __shpcic_mem_read_4(bsh, offset);

	return value;
}

/*
 * read multi
 */
void
shpcic_io_read_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_io_read_1(bsh, offset);
	}
}

void
shpcic_io_read_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_io_read_2(bsh, offset);
	}
}

void
shpcic_io_read_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_io_read_4(bsh, offset);
	}
}

void
shpcic_mem_read_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_mem_read_1(bsh, offset);
	}
}

void
shpcic_mem_read_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_mem_read_2(bsh, offset);
	}
}

void
shpcic_mem_read_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_mem_read_4(bsh, offset);
	}
}

/*
 *
 * read region
 */
void
shpcic_io_read_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_io_read_1(bsh, offset);
		offset += 1;
	}
}

void
shpcic_io_read_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_io_read_2(bsh, offset);
		offset += 2;
	}
}

void
shpcic_io_read_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_io_read_4(bsh, offset);
		offset += 4;
	}
}

void
shpcic_mem_read_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_mem_read_1(bsh, offset);
		offset += 1;
	}
}

void
shpcic_mem_read_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_mem_read_2(bsh, offset);
		offset += 2;
	}
}

void
shpcic_mem_read_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t *addr, bus_size_t count)
{

	while (count--) {
		*addr++ = __shpcic_mem_read_4(bsh, offset);
		offset += 4;
	}
}

/* write */
static inline void __shpcic_io_write_1(bus_space_handle_t bsh,
    bus_size_t offset, uint8_t value);
static inline void __shpcic_io_write_2(bus_space_handle_t bsh,
    bus_size_t offset, uint16_t value);
static inline void __shpcic_io_write_4(bus_space_handle_t bsh,
    bus_size_t offset, uint32_t value);
static inline void __shpcic_mem_write_1(bus_space_handle_t bsh,
    bus_size_t offset, uint8_t value);
static inline void __shpcic_mem_write_2(bus_space_handle_t bsh,
    bus_size_t offset, uint16_t value);
static inline void __shpcic_mem_write_4(bus_space_handle_t bsh,
    bus_size_t offset, uint32_t value);

static inline void
__shpcic_io_write_1(bus_space_handle_t bsh, bus_size_t offset,
    uint8_t value)
{
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_IO_MASK;

	*(volatile uint8_t *)(SH7780_PCIC_IO + adr) = value;
}

static inline void
__shpcic_io_write_2(bus_space_handle_t bsh, bus_size_t offset,
    uint16_t value)
{
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_IO_MASK;

	*(volatile uint16_t *)(SH7780_PCIC_IO + adr) = value;
}

static inline void
__shpcic_io_write_4(bus_space_handle_t bsh, bus_size_t offset,
    uint32_t value)
{
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_IO_MASK;

	*(volatile uint32_t *)(SH7780_PCIC_IO + adr) = value;
}

static inline void
__shpcic_mem_write_1(bus_space_handle_t bsh, bus_size_t offset,
    uint8_t value)
{
#if defined(SH4A_EXT_ADDR32)
	u_long adr = (u_long)(bsh + offset);

	*(volatile uint8_t *)(PCI_MEM_ADDR32 + adr) = value;
#else
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_MEM_MASK;

	*(volatile uint8_t *)(SH7780_PCIC_MEM + adr) = value;
#endif
}

static inline void
__shpcic_mem_write_2(bus_space_handle_t bsh, bus_size_t offset,
    uint16_t value)
{
#if defined(SH4A_EXT_ADDR32)
	u_long adr = (u_long)(bsh + offset);

	*(volatile uint16_t *)(PCI_MEM_ADDR32 + adr) = value;
#else
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_MEM_MASK;

	*(volatile uint16_t *)(SH7780_PCIC_MEM + adr) = value;
#endif
}

static inline void
__shpcic_mem_write_4(bus_space_handle_t bsh, bus_size_t offset,
    uint32_t value)
{
#if defined(SH4A_EXT_ADDR32)
	u_long adr = (u_long)(bsh + offset);

	*(volatile uint32_t *)(PCI_MEM_ADDR32 + adr) = value;
#else
	u_long adr = (u_long)(bsh + offset) & SH7780_PCIC_MEM_MASK;

	*(volatile uint32_t *)(SH7780_PCIC_MEM + adr) = value;
#endif
}

/*
 * write single
 */
void
shpcic_io_write_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t value)
{

	__shpcic_io_write_1(bsh, offset, value);
}

void
shpcic_io_write_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t value)
{

	__shpcic_io_write_2(bsh, offset, value);
}

void
shpcic_io_write_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t value)
{

	__shpcic_io_write_4(bsh, offset, value);
}

void
shpcic_mem_write_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t value)
{

	__shpcic_mem_write_1(bsh, offset, value);
}

void
shpcic_mem_write_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t value)
{

	__shpcic_mem_write_2(bsh, offset, value);
}

void
shpcic_mem_write_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t value)
{

	__shpcic_mem_write_4(bsh, offset, value);
}

/*
 * write multi
 */
void
shpcic_io_write_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint8_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_1(bsh, offset, *addr++);
	}
}

void
shpcic_io_write_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint16_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_2(bsh, offset, *addr++);
	}
}

void
shpcic_io_write_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint32_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_4(bsh, offset, *addr++);
	}
}

void
shpcic_mem_write_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint8_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_1(bsh, offset, *addr++);
	}
}

void
shpcic_mem_write_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint16_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_2(bsh, offset, *addr++);
	}
}

void
shpcic_mem_write_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint32_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_4(bsh, offset, *addr++);
	}
}

/*
 * write region
 */
void
shpcic_io_write_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint8_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_1(bsh, offset, *addr++);
		offset += 1;
	}
}

void
shpcic_io_write_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint16_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_2(bsh, offset, *addr++);
		offset += 2;
	}
}

void
shpcic_io_write_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint32_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_4(bsh, offset, *addr++);
		offset += 4;
	}
}

void
shpcic_mem_write_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint8_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_1(bsh, offset, *addr++);
		offset += 1;
	}
}

void
shpcic_mem_write_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint16_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_2(bsh, offset, *addr++);
		offset += 2;
	}
}

void
shpcic_mem_write_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint32_t *addr, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_4(bsh, offset, *addr++);
		offset += 4;
	}
}

/*
 * set multi
 */
void
shpcic_io_set_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_1(bsh, offset, value);
	}
}

void
shpcic_io_set_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_2(bsh, offset, value);
	}
}

void
shpcic_io_set_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_4(bsh, offset, value);
	}
}

void
shpcic_mem_set_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_1(bsh, offset, value);
	}
}

void
shpcic_mem_set_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_2(bsh, offset, value);
	}
}

void
shpcic_mem_set_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_4(bsh, offset, value);
	}
}

/*
 * set region
 */
void
shpcic_io_set_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_1(bsh, offset, value);
		offset += 1;
	}
}

void
shpcic_io_set_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_2(bsh, offset, value);
		offset += 2;
	}
}

void
shpcic_io_set_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_io_write_4(bsh, offset, value);
		offset += 4;
	}
}

void
shpcic_mem_set_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_1(bsh, offset, value);
		offset += 1;
	}
}

void
shpcic_mem_set_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_2(bsh, offset, value);
		offset += 2;
	}
}

void
shpcic_mem_set_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t value, bus_size_t count)
{

	while (count--) {
		__shpcic_mem_write_4(bsh, offset, value);
		offset += 4;
	}
}

/*
 * copy region
 */
void
shpcic_io_copy_region_1(void *v, bus_space_handle_t bsh1,
    bus_size_t off1, bus_space_handle_t bsh2, bus_size_t off2, bus_size_t count)
{
	u_long addr1 = bsh1 + off1;
	u_long addr2 = bsh2 + off2;
	uint8_t value;

	if (addr1 >= addr2) {	/* src after dest: copy forward */
		while (count--) {
			value = __shpcic_io_read_1(bsh1, off1);
			__shpcic_io_write_1(bsh2, off2, value);
			off1 += 1;
			off2 += 1;
		}
	} else {		/* dest after src: copy backwards */
		off1 += (count - 1) * 1;
		off2 += (count - 1) * 1;
		while (count--) {
			value = __shpcic_io_read_1(bsh1, off1);
			__shpcic_io_write_1(bsh2, off2, value);
			off1 -= 1;
			off2 -= 1;
		}
	}
}

void
shpcic_io_copy_region_2(void *v, bus_space_handle_t bsh1,
    bus_size_t off1, bus_space_handle_t bsh2, bus_size_t off2, bus_size_t count)
{
	u_long addr1 = bsh1 + off1;
	u_long addr2 = bsh2 + off2;
	uint16_t value;

	if (addr1 >= addr2) {	/* src after dest: copy forward */
		while (count--) {
			value = __shpcic_io_read_2(bsh1, off1);
			__shpcic_io_write_2(bsh2, off2, value);
			off1 += 2;
			off2 += 2;
		}
	} else {		/* dest after src: copy backwards */
		off1 += (count - 1) * 2;
		off2 += (count - 1) * 2;
		while (count--) {
			value = __shpcic_io_read_2(bsh1, off1);
			__shpcic_io_write_2(bsh2, off2, value);
			off1 -= 2;
			off2 -= 2;
		}
	}
}

void
shpcic_io_copy_region_4(void *v, bus_space_handle_t bsh1,
    bus_size_t off1, bus_space_handle_t bsh2, bus_size_t off2, bus_size_t count)
{
	u_long addr1 = bsh1 + off1;
	u_long addr2 = bsh2 + off2;
	uint32_t value;

	if (addr1 >= addr2) {	/* src after dest: copy forward */
		while (count--) {
			value = __shpcic_io_read_4(bsh1, off1);
			__shpcic_io_write_4(bsh2, off2, value);
			off1 += 4;
			off2 += 4;
		}
	} else {		/* dest after src: copy backwards */
		off1 += (count - 1) * 4;
		off2 += (count - 1) * 4;
		while (count--) {
			value = __shpcic_io_read_4(bsh1, off1);
			__shpcic_io_write_4(bsh2, off2, value);
			off1 -= 4;
			off2 -= 4;
		}
	}
}

void
shpcic_mem_copy_region_1(void *v, bus_space_handle_t bsh1,
    bus_size_t off1, bus_space_handle_t bsh2, bus_size_t off2, bus_size_t count)
{
	u_long addr1 = bsh1 + off1;
	u_long addr2 = bsh2 + off2;
	uint8_t value;

	if (addr1 >= addr2) {	/* src after dest: copy forward */
		while (count--) {
			value = __shpcic_mem_read_1(bsh1, off1);
			__shpcic_mem_write_1(bsh2, off2, value);
			off1 += 1;
			off2 += 1;
		}
	} else {		/* dest after src: copy backwards */
		off1 += (count - 1) * 1;
		off2 += (count - 1) * 1;
		while (count--) {
			value = __shpcic_mem_read_1(bsh1, off1);
			__shpcic_mem_write_1(bsh2, off2, value);
			off1 -= 1;
			off2 -= 1;
		}
	}
}

void
shpcic_mem_copy_region_2(void *v, bus_space_handle_t bsh1,
    bus_size_t off1, bus_space_handle_t bsh2, bus_size_t off2, bus_size_t count)
{
	u_long addr1 = bsh1 + off1;
	u_long addr2 = bsh2 + off2;
	uint16_t value;

	if (addr1 >= addr2) {	/* src after dest: copy forward */
		while (count--) {
			value = __shpcic_mem_read_2(bsh1, off1);
			__shpcic_mem_write_2(bsh2, off2, value);
			off1 += 2;
			off2 += 2;
		}
	} else {		/* dest after src: copy backwards */
		off1 += (count - 1) * 2;
		off2 += (count - 1) * 2;
		while (count--) {
			value = __shpcic_mem_read_2(bsh1, off1);
			__shpcic_mem_write_2(bsh2, off2, value);
			off1 -= 2;
			off2 -= 2;
		}
	}
}

void
shpcic_mem_copy_region_4(void *v, bus_space_handle_t bsh1,
    bus_size_t off1, bus_space_handle_t bsh2, bus_size_t off2, bus_size_t count)
{
	u_long addr1 = bsh1 + off1;
	u_long addr2 = bsh2 + off2;
	uint32_t value;

	if (addr1 >= addr2) {	/* src after dest: copy forward */
		while (count--) {
			value = __shpcic_mem_read_4(bsh1, off1);
			__shpcic_mem_write_4(bsh2, off2, value);
			off1 += 4;
			off2 += 4;
		}
	} else {		/* dest after src: copy backwards */
		off1 += (count - 1) * 4;
		off2 += (count - 1) * 4;
		while (count--) {
			value = __shpcic_mem_read_4(bsh1, off1);
			__shpcic_mem_write_4(bsh2, off2, value);
			off1 -= 4;
			off2 -= 4;
		}
	}
}
