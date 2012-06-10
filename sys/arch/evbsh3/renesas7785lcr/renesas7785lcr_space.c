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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <uvm/uvm_extern.h>

#include <sh3/bscreg.h>
#include <sh3/devreg.h>
#include <sh3/mmu.h>
#include <sh3/pmap.h>
#include <sh3/pte.h>

#include <machine/cpu.h>

static int _iomem_map(void *v, bus_addr_t bpa, bus_size_t size, int flags,
    bus_space_handle_t *bshp);
static void _iomem_unmap(void *v, bus_space_handle_t bsh, bus_size_t size);
static int _iomem_subregion(void *v, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp);
static int _iomem_alloc(void *v, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp);
static void _iomem_free(void *v, bus_space_handle_t bsh, bus_size_t size);

static int _iomem_add_mapping(bus_addr_t, bus_size_t, bus_space_handle_t *);

static int
_iomem_add_mapping(bus_addr_t bpa, bus_size_t size, bus_space_handle_t *bshp)
{
	u_long pa, endpa;
	vaddr_t va;

	pa = sh3_trunc_page(bpa);
	endpa = sh3_round_page(bpa + size);

#ifdef DIAGNOSTIC
	if (endpa <= pa)
		panic("_iomem_add_mapping: overflow");
#endif

	va = uvm_km_alloc(kernel_map, endpa - pa, 0, UVM_KMF_VAONLY);
	if (va == 0) {
		printf("_iomem_add_mapping: nomem\n");
		return (ENOMEM);
	}

	*bshp = (bus_space_handle_t)(va + (bpa & PGOFSET));

	for (; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE) {
		pmap_kenter_pa(va, pa, VM_PROT_READ | VM_PROT_WRITE, 0);
	}

	return (0);
}

static int
_iomem_map(void *v, bus_addr_t bpa, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
#ifndef SH4A_EXT_ADDR32
	bus_addr_t addr = SH3_PHYS_TO_P2SEG(bpa);
#endif

	KASSERT((bpa & SH3_PHYS_MASK) == bpa);

#ifndef SH4A_EXT_ADDR32
	if (bpa < 0x14000000 || bpa >= 0x1c000000) {
		/* CS0,1,2,3,4,7 */
		*bshp = (bus_space_handle_t) addr;
		return (0);
	}
#endif

	/* CS5,6 */
	return _iomem_add_mapping(bpa, size, bshp);
}

static void
_iomem_unmap(void *v, bus_space_handle_t bsh, bus_size_t size)
{
	u_long va, endva;
	bus_addr_t bpa;

#ifndef SH4A_EXT_ADDR32
	if (bsh >= SH3_P2SEG_BASE && bsh <= SH3_P2SEG_END) {
		/* maybe CS0,1,2,3,4,7 */
		return;
	}
#endif

	/* CS5,6 */
	va = sh3_trunc_page(bsh);
	endva = sh3_round_page(bsh + size);

#ifdef DIAGNOSTIC
	if (endva <= va)
		panic("_io_unmap: overflow");
#endif

	pmap_extract(pmap_kernel(), va, &bpa);
	bpa += bsh & PGOFSET;

	pmap_kremove(va, endva - va);

	/*
	 * Free the kernel virtual mapping.
	 */
	uvm_km_free(kernel_map, va, endva - va, UVM_KMF_VAONLY);
}

static int
_iomem_subregion(void *v, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{

	*nbshp = bsh + offset;

	return (0);
}

static int
_iomem_alloc(void *v, bus_addr_t rstart, bus_addr_t rend,
    bus_size_t size, bus_size_t alignment, bus_size_t boundary, int flags,
    bus_addr_t *bpap, bus_space_handle_t *bshp)
{

	*bshp = *bpap = rstart;

	return (0);
}

static void
_iomem_free(void *v, bus_space_handle_t bsh, bus_size_t size)
{

	_iomem_unmap(v, bsh, size);
}

static paddr_t
_iomem_mmap(void *v, bus_space_handle_t bsh, off_t off, int prot, int flags)
{

	return (paddr_t)-1;
}

/*
 * on-board I/O bus space read/write
 */
static uint8_t _iomem_read_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset);
static uint16_t _iomem_read_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset);
static uint32_t _iomem_read_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset);
static void _iomem_read_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t *addr, bus_size_t count);
static void _iomem_read_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t *addr, bus_size_t count);
static void _iomem_read_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t *addr, bus_size_t count);
static void _iomem_read_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t *addr, bus_size_t count);
static void _iomem_read_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t *addr, bus_size_t count);
static void _iomem_read_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t *addr, bus_size_t count);
static void _iomem_write_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t value);
static void _iomem_write_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t value);
static void _iomem_write_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t value);
static void _iomem_write_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint8_t *addr, bus_size_t count);
static void _iomem_write_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint16_t *addr, bus_size_t count);
static void _iomem_write_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint32_t *addr, bus_size_t count);
static void _iomem_write_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint8_t *addr, bus_size_t count);
static void _iomem_write_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint16_t *addr, bus_size_t count);
static void _iomem_write_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint32_t *addr, bus_size_t count);
static void _iomem_set_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t val, bus_size_t count);
static void _iomem_set_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t val, bus_size_t count);
static void _iomem_set_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t val, bus_size_t count);
static void _iomem_set_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t val, bus_size_t count);
static void _iomem_set_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t val, bus_size_t count);
static void _iomem_set_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t val, bus_size_t count);
static void _iomem_copy_region_1(void *v, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t count);
static void _iomem_copy_region_2(void *v, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t count);
static void _iomem_copy_region_4(void *v, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t count);

struct _bus_space renesas7785lcr_bus_iomem =
{
	.bs_cookie = NULL,

	.bs_map = _iomem_map,
	.bs_unmap = _iomem_unmap,
	.bs_subregion = _iomem_subregion,

	.bs_alloc = _iomem_alloc,
	.bs_free = _iomem_free,

	.bs_mmap = _iomem_mmap,

	.bs_r_1 = _iomem_read_1,
	.bs_r_2 = _iomem_read_2,
	.bs_r_4 = _iomem_read_4,

	.bs_rm_1 = _iomem_read_multi_1,
	.bs_rm_2 = _iomem_read_multi_2,
	.bs_rm_4 = _iomem_read_multi_4,

	.bs_rr_1 = _iomem_read_region_1,
	.bs_rr_2 = _iomem_read_region_2,
	.bs_rr_4 = _iomem_read_region_4,

	.bs_rs_1 = _iomem_read_1,
	.bs_rs_2 = _iomem_read_2,
	.bs_rs_4 = _iomem_read_4,

	.bs_rms_1 = _iomem_read_multi_1,
	.bs_rms_2 = _iomem_read_multi_2,
	.bs_rms_4 = _iomem_read_multi_4,

	.bs_rrs_1 = _iomem_read_region_1,
	.bs_rrs_2 = _iomem_read_region_2,
	.bs_rrs_4 = _iomem_read_region_4,

	.bs_w_1 = _iomem_write_1,
	.bs_w_2 = _iomem_write_2,
	.bs_w_4 = _iomem_write_4,

	.bs_wm_1 = _iomem_write_multi_1,
	.bs_wm_2 = _iomem_write_multi_2,
	.bs_wm_4 = _iomem_write_multi_4,

	.bs_wr_1 = _iomem_write_region_1,
	.bs_wr_2 = _iomem_write_region_2,
	.bs_wr_4 = _iomem_write_region_4,

	.bs_ws_1 = _iomem_write_1,
	.bs_ws_2 = _iomem_write_2,
	.bs_ws_4 = _iomem_write_4,

	.bs_wms_1 = _iomem_write_multi_1,
	.bs_wms_2 = _iomem_write_multi_2,
	.bs_wms_4 = _iomem_write_multi_4,

	.bs_wrs_1 = _iomem_write_region_1,
	.bs_wrs_2 = _iomem_write_region_2,
	.bs_wrs_4 = _iomem_write_region_4,

	.bs_sm_1 = _iomem_set_multi_1,
	.bs_sm_2 = _iomem_set_multi_2,
	.bs_sm_4 = _iomem_set_multi_4,

	.bs_sr_1 = _iomem_set_region_1,
	.bs_sr_2 = _iomem_set_region_2,
	.bs_sr_4 = _iomem_set_region_4,

	.bs_c_1 = _iomem_copy_region_1,
	.bs_c_2 = _iomem_copy_region_2,
	.bs_c_4 = _iomem_copy_region_4,
};

/* read */
static uint8_t
_iomem_read_1(void *v, bus_space_handle_t bsh, bus_size_t offset)
{

	return *(volatile uint8_t *)(bsh + offset);
}

static uint16_t
_iomem_read_2(void *v, bus_space_handle_t bsh, bus_size_t offset)
{

	return (*(volatile uint16_t *)(bsh + offset));
}

static uint32_t
_iomem_read_4(void *v, bus_space_handle_t bsh, bus_size_t offset)
{

	return (*(volatile uint32_t *)(bsh + offset));
}

static void
_iomem_read_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t *addr, bus_size_t count)
{
	volatile uint8_t *p = (void *)(bsh + offset);

	while (count--) {
		*addr++ = *p;
	}
}

static void
_iomem_read_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t *addr, bus_size_t count)
{
	volatile uint16_t *src = (void *)(bsh + offset);
	volatile uint16_t *dest = (void *)addr;

	while (count--) {
		*dest++ = *src;
	}
}

static void
_iomem_read_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t *addr, bus_size_t count)
{
	volatile uint32_t *src = (void *)(bsh + offset);
	volatile uint32_t *dest = (void *)addr;

	while (count--) {
		*dest++ = *src;
	}
}

static void
_iomem_read_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t *addr, bus_size_t count)
{
	volatile uint8_t *p = (void *)(bsh + offset);

	while (count--) {
		*addr++ = *p++;
	}
}

static void
_iomem_read_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t *addr, bus_size_t count)
{
	volatile uint16_t *p = (void *)(bsh + offset);

	while (count--) {
		*addr++ = *p++;
	}
}

static void
_iomem_read_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t *addr, bus_size_t count)
{
	volatile uint32_t *p = (void *)(bsh + offset);

	while (count--) {
		*addr++ = *p++;
	}
}

/* write */
static void
_iomem_write_1(void *v, bus_space_handle_t bsh, bus_size_t offset,
    uint8_t value)
{

	*(volatile uint8_t *)(bsh + offset) = value;
}

static void
_iomem_write_2(void *v, bus_space_handle_t bsh, bus_size_t offset,
    uint16_t value)
{

	*(volatile uint16_t *)(bsh + offset) = value;
}

static void
_iomem_write_4(void *v, bus_space_handle_t bsh, bus_size_t offset,
    uint32_t value)
{

	*(volatile uint32_t *)(bsh + offset) = value;
}

static void
_iomem_write_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint8_t *addr, bus_size_t count)
{
	volatile uint8_t *p = (void *)(bsh + offset);

	while (count--) {
		*p = *addr++;
	}
}

static void
_iomem_write_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint16_t *addr, bus_size_t count)
{
	volatile uint16_t *dest = (void *)(bsh + offset);
	volatile const uint16_t *src = (const void *)addr;

	while (count--) {
		*dest = *src++;
	}
}

static void
_iomem_write_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint32_t *addr, bus_size_t count)
{
	volatile uint32_t *dest = (void *)(bsh + offset);
	volatile const uint32_t *src = (const void *)addr;

	while (count--) {
		*dest = *src++;
	}
}

static void
_iomem_write_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint8_t *addr, bus_size_t count)
{
	volatile uint8_t *p = (void *)(bsh + offset);

	while (count--) {
		*p++ = *addr++;
	}
}

static void
_iomem_write_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint16_t *addr, bus_size_t count)
{
	volatile uint16_t *p = (void *)(bsh + offset);

	while (count--) {
		*p++ = *addr++;
	}
}

static void
_iomem_write_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, const uint32_t *addr, bus_size_t count)
{
	volatile uint32_t *p = (void *)(bsh + offset);

	while (count--) {
		*p++ = *addr++;
	}
}

static void
_iomem_set_multi_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t val, bus_size_t count)
{
	volatile uint8_t *p = (void *)(bsh + offset);

	while (count--) {
		*p = val;
	}
}

static void
_iomem_set_multi_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t val, bus_size_t count)
{
	volatile uint16_t *dest = (void *)(bsh + offset);

	while (count--) {
		*dest = val;
	}
}

static void
_iomem_set_multi_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t val, bus_size_t count)
{
	volatile uint32_t *dest = (void *)(bsh + offset);

	while (count--) {
		*dest = val;
	}
}

static void
_iomem_set_region_1(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint8_t val, bus_size_t count)
{
	volatile uint8_t *addr = (void *)(bsh + offset);

	while (count--) {
		*addr++ = val;
	}
}

static void
_iomem_set_region_2(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint16_t val, bus_size_t count)
{
	volatile uint16_t *dest = (void *)(bsh + offset);

	while (count--) {
		*dest++ = val;
	}
}

static void
_iomem_set_region_4(void *v, bus_space_handle_t bsh,
    bus_size_t offset, uint32_t val, bus_size_t count)
{
	volatile uint32_t *dest = (void *)(bsh + offset);

	while (count--) {
		*dest++ = val;
	}
}

static void
_iomem_copy_region_1(void *v, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t count)
{
	volatile uint8_t *addr1 = (void *)(h1 + o1);
	volatile uint8_t *addr2 = (void *)(h2 + o2);

	if (addr1 >= addr2) {	/* src after dest: copy forward */
		while (count--) {
			*addr2++ = *addr1++;
		}
	} else {		/* dest after src: copy backwards */
		addr1 += count - 1;
		addr2 += count - 1;
		while (count--) {
			*addr2-- = *addr1--;
		}
	}
}

static void
_iomem_copy_region_2(void *v, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t count)
{
	volatile uint16_t *addr1 = (void *)(h1 + o1);
	volatile uint16_t *addr2 = (void *)(h2 + o2);

	if (addr1 >= addr2) {	/* src after dest: copy forward */
		while (count--) {
			*addr2++ = *addr1++;
		}
	} else {		/* dest after src: copy backwards */
		addr1 += count - 1;
		addr2 += count - 1;
		while (count--) {
			*addr2-- = *addr1--;
		}
	}
}

static void
_iomem_copy_region_4(void *v, bus_space_handle_t h1,
    bus_size_t o1, bus_space_handle_t h2, bus_size_t o2, bus_size_t count)
{
	volatile uint32_t *addr1 = (void *)(h1 + o1);
	volatile uint32_t *addr2 = (void *)(h2 + o2);

	if (addr1 >= addr2) {	/* src after dest: copy forward */
		while (count--) {
			*addr2++ = *addr1++;
		}
	} else {		/* dest after src: copy backwards */
		addr1 += count - 1;
		addr2 += count - 1;
		while (count--) {
			*addr2-- = *addr1--;
		}
	}
}
