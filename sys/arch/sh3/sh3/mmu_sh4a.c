/* $NetBSD$	 */

/*-
 * Copyright (c) 2009 UCHIYAMA Yasushi.  All rights reserved.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Memory Management Unit

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <sh3/pte.h>		/* NetBSD/sh3 specific PTE */
#include <sh3/mmu.h>
#include <sh3/mmu_sh4a.h>
#include <sh3/cache_sh4a.h>
#include <sh3/vmparam.h>	// PAGE_SIZE

#define	STATIC	static
void            tlb_entry_print(uint32_t, uint32_t, uint32_t, int);
#ifdef DEBUG
void            sh4a_mmu_utlb_check(void);
#endif
paddr_t         sh4a_mmu_utlb_lookup_by_va(vaddr_t);

static int      mmu_paddr;
static int      mmu_mode;
#define	SPECIAL_WIRED	4

void
sh4a_mmu_start(void)
{
	uint32_t r;
#ifdef DEBUG
	sh4a_mmu_dump();
#endif
	sh4a_tlb_clear_all();
#ifdef DEBUG
	sh4a_mmu_dump();
#endif
#ifdef SH4A_EXT_ADDR32
	*PASCR = PASCR_SE;
	mmu_paddr = 32;
#else
	// CPU doesn 't wait for the end of writing bus access and starts
	// next bus access
	*PASCR = 0;
	mmu_paddr = 29;
#endif

	// Don 't re-fetch all.
	// *IRMCR = IRMCR_R2 | IRMCR_R1 | IRMCR_LT | IRMCR_MT | IRMCR_MC;

	// Re-fetch. (slow)
	*IRMCR = 0;

	// Set ASID
	*SH4A_PTEH = 0;

	// Invalidate all entries.and start Address translation.
	r = MMUCR_TI | MMUCR_AT;
#ifdef SH4A_EXT_MMU
	r |= MMUCR_ME;
	mmu_mode = 1;
#endif
	r |= MMUCR_SQMD;
	// Store queue

	*SH4A_MMUCR = r;

#ifdef NetBSD
	sh4a_mmu_wired_set(UPAGES + SPECIAL_WIRED);
#endif

	CPU_SYNC_RESOURCE();
}

void
sh4a_mmu_information(void)
{
	printf("cpu0: %dbit physical address mode. ", mmu_paddr);
	printf("TLB-%s mode.\n", mmu_mode ? "extended" : "compatible");
#if defined(SH4A_EXT_ADDR32) && defined(DEBUG)
	sh4a_pmb_dump();
#endif
}

void
sh4a_mmu_asid_set(uint32_t acid)
{

	*SH4A_PTEH = acid & PTEH_ASID;
}

uint32_t
sh4a_mmu_asid_get(void)
{

	return *SH4A_PTEH & PTEH_ASID;
}

void
sh4a_tlb_counter_set(uint32_t n)
{

	*SH4A_MMUCR &= ~(MMUCR_URC_MASK << MMUCR_URC_SHIFT);
	*SH4A_MMUCR |= (n << MMUCR_URC_SHIFT);
}

uint32_t
sh4a_tlb_counter_get(void)
{

	return (*SH4A_MMUCR >> MMUCR_URC_SHIFT) & MMUCR_URC_MASK;
}

void
sh4a_mmu_wired_set(int n)
{
	uint32_t limit = (SH4A_UTLB_ENTRY - n) & MMUCR_URB_MASK;

	// If current counter is wired area, reset.
	if (sh4a_tlb_counter_get() >= limit)
		sh4a_tlb_counter_set(0);
	// Set coutner limit.
	*SH4A_MMUCR &= ~(MMUCR_URB_MASK << MMUCR_URB_SHIFT);
	*SH4A_MMUCR |= (limit << MMUCR_URB_SHIFT);

	printf("wired entry: %d-%d\n", limit, SH4A_UTLB_ENTRY - 1);
}

uint32_t
sh4a_mmu_wired_get(void)
{
	// Return
	// # of wired entry. if URC == URB, URC is setted 0.
	uint32_t urb = (*SH4A_MMUCR >> MMUCR_URB_SHIFT) & MMUCR_URB_MASK;

	return urb == 0 ? 0 : SH4A_UTLB_ENTRY - urb;
	// urb == 0:no wired entry.
}

void
sh4a_tlb_clear_all(void)
{
	uint32_t entry, e;

	for (entry = 0; entry < SH4A_ITLB_ENTRY; entry++) {
		e = entry << ITLB_AA_E_SHIFT;
		*(volatile uint32_t *)(SH4A_ITLB_AA | e) = 0;
		*(volatile uint32_t *)(SH4A_ITLB_DA1 | e) = 0;
#ifdef SH4A_EXT_MMU
		*(volatile uint32_t *)(SH4A_ITLB_DA2 | e) = 0;
#endif
	}
	for (entry = 0; entry < SH4A_UTLB_ENTRY; entry++) {
		e = entry << UTLB_AA_E_SHIFT;
		*(volatile uint32_t *)(SH4A_UTLB_AA | e) = 0;
		*(volatile uint32_t *)(SH4A_UTLB_DA1 | e) = 0;
#ifdef SH4A_EXT_MMU
		*(volatile uint32_t *)(SH4A_UTLB_DA2 | e) = 0;
#endif
	}
}

void
sh4a_tlb_wired_entry(struct tlb_entry * e)
{
	uint32_t        pteh, ptel, ptea;
#ifdef DEBUG
	uint32_t        pgsz_table[] = {0x400, 0x1000, 0x1000, 0x100000, 0x2000, 0x40000,
	0x400000, 0x4000000};
	uint32_t        pgsz_mask = pgsz_table[e->size] - 1;
	assert(!(e->vpn & pgsz_mask) && !(e->ppn & pgsz_mask));
#endif
	// Set Physical addr - Virtual addr.
	ptel = e->ppn & PTEL_PPN;
	pteh = e->vpn & PTEH_VPN;
	ptea = 0;

	// Shared page don 't bother ASID.
	if (e->shared)
		ptel |= PTEL_SH;
	else
		pteh |= e->asid;
	// Setup Protect, Cache, Pagesize.
	sh4a_tlb_entry_pagesize(e->size, &ptel, &ptea);
	sh4a_tlb_entry_protect(e->protect, &ptel, &ptea);
	sh4a_tlb_entry_cache(e->cache, &ptel, &ptea);

	// Enable this entry.
	ptel |= (PTEL_V | PTEL_D);
	// Prepare new wired entry.
	uint32_t n = sh4a_mmu_wired_get() + 1;

	// Set URC for LDTLB
	sh4a_tlb_counter_set(SH4A_UTLB_ENTRY - n);
	*SH4A_PTEH = pteh;
	*SH4A_PTEL = ptel;
#ifdef SH4A_EXT_MMU
	*SH4A_PTEA = ptea;
#endif
	__asm volatile("ldtlb");
	// Make this wired entry.
	sh4a_mmu_wired_set(n);
	CPU_SYNC_RESOURCE();
#ifdef DEBUG
	// Valid bit is maped to both address array and data array, but
	// tlb_entry_print check address array only.XXX
	tlb_entry_print(pteh | UTLB_AA_V, ptel, ptea, 0);
#endif
}

// Invalidate TLB entry by address.
void
sh4a_tlb_invalidate_addr(int asid, vaddr_t va)
{
	int s = _cpu_intr_suspend();
	// Save current ASID
	uint32_t oasid = sh4a_mmu_asid_get();
	bool save_asid = asid != oasid;

	// Set ASID for associative write
	if (save_asid)
		sh4a_mmu_asid_set(asid);

	// Associative write UTLB.ITLB is also invalidated.
	// SH4A can access by a program in the P1 / P2.no need to change P2.
	*(volatile uint32_t *)(SH4A_UTLB_AA | UTLB_AA_A) = va & PTEH_VPN;

	// Restore ASID
	if (save_asid)
		sh4a_mmu_asid_set(oasid);
	_cpu_intr_resume(s);
}

// Invalidate TLB entry by ASID.
void
sh4a_tlb_invalidate_asid(int asid)
{
	int s = _cpu_intr_suspend();
	// Don 't thread switch here.
	volatile uint32_t *a;
	uint32_t entry;
	// I assume asid is not 0. so wired entry is not matched.
	KASSERT(asid != 0);
	for (entry = 0; entry < SH4A_UTLB_ENTRY; entry++) {
		a = (volatile uint32_t *)(SH4A_UTLB_AA | (entry << UTLB_AA_E_SHIFT));
		if ((*a & UTLB_AA_ASID) == (uint32_t) asid)
			*a = 0;
	}

	for (entry = 0; entry < SH4A_ITLB_ENTRY; entry++) {
		a = (volatile uint32_t *)(SH4A_ITLB_AA | (entry << ITLB_AA_E_SHIFT));
		if ((*a & ITLB_AA_ASID) == (uint32_t) asid)
			*a = 0;
		// Drop valid bit.
	}

	_cpu_intr_resume(s);
}

// Invalidate TLB entry exclude wired entry.
void
sh4a_tlb_invalidate_all(void)
{
	int s = _cpu_intr_suspend();
	volatile uint32_t *a;
	uint32_t limit, entry;

	limit = SH4A_UTLB_ENTRY - sh4a_mmu_wired_get();
	for (entry = 0; entry < limit; entry++) {
		*(volatile uint32_t *)(SH4A_UTLB_AA | (entry << UTLB_AA_E_SHIFT)) = 0;
	}

	for (entry = 0; entry < SH4A_ITLB_ENTRY; entry++) {
		a = (volatile uint32_t *)(SH4A_ITLB_AA | (entry << ITLB_AA_E_SHIFT));
		*a = 0;
	}

	_cpu_intr_resume(s);
}

void
sh4a_tlb_entry_pagesize(int szidx, uint32_t *ptel __unused,
    uint32_t *ptea __unused)
{
#ifdef SH4A_EXT_MMU
	uint32_t pgsz_bits[] = {
		PTEA_ESZ_1K, PTEA_ESZ_4K, PTEA_ESZ_64K, PTEA_ESZ_1M,
		PTEA_ESZ_8K, PTEA_ESZ_256K, PTEA_ESZ_4M, PTEA_ESZ_64M
	};
	*ptea |= pgsz_bits[szidx];
#else
	uint32_t pgsz_bits[] = {
		PTEL_SZ_1K, PTEL_SZ_4K, PTEL_SZ_64K, PTEL_SZ_1M
	};
	*ptel |= pgsz_bits[szidx];
#endif
}

void
sh4a_tlb_entry_protect(int pridx, uint32_t *ptel __unused,
    uint32_t *ptea __unused)
{
#ifdef SH4A_EXT_MMU
	uint32_t pr_bits[] = {
		PTEA_EPR_P_R |                PTEA_EPR_P_X,

		PTEA_EPR_P_R | PTEA_EPR_P_W | PTEA_EPR_P_X,

		PTEA_EPR_P_R | PTEA_EPR_P_W | PTEA_EPR_P_X |
		PTEA_EPR_U_R |                PTEA_EPR_U_X,

		PTEA_EPR_P_R | PTEA_EPR_P_W | PTEA_EPR_P_X |
		PTEA_EPR_U_R | PTEA_EPR_U_W | PTEA_EPR_U_X
	};
	*ptea |= pr_bits[pridx];
#else
	uint32_t pr_bits[] = {
		PTEL_PR_P_RDONLY, PTEL_PR_P_RW, PTEL_PR_U_RDONLY, PTEL_PR_U_RW
	};
	*ptel |= pr_bits[pridx];
#endif
}

void
sh4a_tlb_entry_cache(int cidx, uint32_t *ptel, uint32_t *ptea __unused)
{
	uint32_t cache_bits[] = {0, PTEL_C, PTEL_C | PTEL_WT};

	*ptel |= cache_bits[cidx];
}

uint32_t
sh4a_mmu_encode_swbits(
	enum mmu_page_size sz,	// 8 types.3 bit
	enum mmu_protect pr,	// 4 types.2 bit
	uint32_t opt __unused)
{
	uint32_t pte = 0;

	pte |= (sz & 1) << 4;
	pte |= (sz & 2) << 6;
#ifdef SH4A_EXT_MMU
	pte |= (sz & 4) << 6;
#endif
	pte |= pr << 5;

	return pte;
}

struct {
	int inuse;
	vaddr_t va;
} wired_map[1024];

int
sh4a_wired_counter_alloc(vaddr_t va)
{
	int i;

	for (i = 0; i < __arraycount(wired_map); i++) {
		if (!wired_map[i].inuse) {
			wired_map[i].inuse = TRUE;
			wired_map[i].va = va;
			printf("%s wired %d alloc\n", __FUNCTION__, i);
			return 0;
			//return SH4A_UTLB_ENTRY - UPAGES - SPECIAL_WIRED + i;
		}
	}

	return 0;
}

void
sh4a_wired_counter_dealloc(vaddr_t va)
{
	int i, j;

	for (i = j = 0; i < __arraycount(wired_map); i++) {
		bool wired = wired_map[i].inuse;
		if (wired) {
			j++;
			if (wired_map[i].va == va) {
				wired_map[i].inuse = FALSE;
				printf("%s wired %d dealloc\n", __FUNCTION__, i);
			}
		}
	}
	printf("tolal %d wired page\n", j);
}

// Load new TLB entry.
void
sh4a_tlb_update_wired(int asid, vaddr_t va, uint32_t pte)
{
	//printf("%s %d %lx %x\n", __FUNCTION__, asid, va, pte);

	int s = _cpu_intr_suspend();
	va &= ~PGOFSET;

	// Save current ASID
	uint32_t oasid = sh4a_mmu_asid_get();
	bool save_asid = asid != oasid;

	// Load new entry.
	uint32_t vpn = va & PTEH_VPN;

	uint32_t pteh = (vpn | asid) & PTEH_VALID_BITS;
	*SH4A_PTEH = pteh;
	uint32_t ptel = pte & PG_HW_BITS;
	// Remove software bits
	*SH4A_PTEL = ptel;

	int cnt = sh4a_tlb_counter_get();
	// Set wired entry.
	sh4a_tlb_counter_set(sh4a_wired_counter_alloc(va));
	__asm volatile("ldtlb");
	CPU_SYNC_RESOURCE();
	// Restore counter.
	sh4a_tlb_counter_set(cnt);

	// Restore ASID
	if (save_asid)
		sh4a_mmu_asid_set(oasid);
	//sh4a_mmu_dump();
	_cpu_intr_resume(s);
}

// Load new TLB entry.
void
sh4a_tlb_update(int asid, vaddr_t va, uint32_t pte)
{
	//printf("%s %d %lx %x\n", __FUNCTION__, asid, va, pte);

	int s = _cpu_intr_suspend();
	va &= ~PGOFSET;

	// Save current ASID
	uint32_t oasid = sh4a_mmu_asid_get();
	bool save_asid = asid != oasid;
	if (save_asid)
		sh4a_mmu_asid_set(asid);

	// Invalidate old entry.
	sh4a_tlb_invalidate_addr(asid, va);

	// Load new entry.
	uint32_t vpn = va & PTEH_VPN;

	uint32_t pteh = (vpn | asid) & PTEH_VALID_BITS;
	*SH4A_PTEH = pteh;
#ifdef SH4A_EXT_MMU
#error notyet
	uint32_t ptel, ptea;
	pte = pte;
	ptel = 0;
	ptea = 0;
	//sh4a_mmu_decode_swbits(pte, &ptel, &ptea);
	*SH4A_PTEL = ptel;
	*SH4A_PTEA = ptea;
#else
#ifdef NetBSD
	uint32_t ptel = pte & PG_HW_BITS;
	// Remove software bits
#else
	uint32_t ptel = pte & PTEL_VALID_BITS;
#endif
	*SH4A_PTEL = ptel;
#endif

	int cnt = sh4a_tlb_counter_get() + 1;
	if (cnt > (60 - SPECIAL_WIRED))
		cnt = 0;
	sh4a_tlb_counter_set(cnt);

	__asm volatile("ldtlb");
	CPU_SYNC_RESOURCE();

	// Restore ASID
	if (save_asid)
		sh4a_mmu_asid_set(oasid);
#ifdef DEBUG
	sh4a_mmu_utlb_check();
#endif
	//sh4a_mmu_dump();
	_cpu_intr_resume(s);
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/db_output.h>
#define	printf	db_printf
void
sh4a_mmu_dump(void)
{

	printf("MMUCR: %08x ", *SH4A_MMUCR);
	printf("PTEH: %08x ", *SH4A_PTEH);
	printf("PTEL: %08x ", *SH4A_PTEL);
	printf("TTB: %08x ", *SH4A_TTB);
	printf("TEA: %08x ", *SH4A_TEA);
#ifdef SH4A_EXT_MMU
	printf("PTEA: %08x", *SH4A_TEA);
#endif
	printf("\n");
	printf("wired entry:%d\n", sh4a_mmu_wired_get());

	sh4a_mmu_itlb_dump();
	sh4a_mmu_utlb_dump();
#if defined(SH4A_EXT_ADDR32) && defined(DEBUG)
	printf("--------------------PMB--------------------\n");
	uint32_t        r = *PASCR;
	printf("paddr %dbit mode.\n", r & PASCR_SE ? 32 : 29);
	sh4a_pmb_dump();
#endif
}

void
sh4a_mmu_itlb_dump(void)
{
	uint32_t entry, e, aa, da1, da2;

	printf("--------------------ITLB--------------------\n");
	for (entry = 0; entry < SH4A_ITLB_ENTRY; entry++) {
		e = entry << ITLB_AA_E_SHIFT;
		aa = *(volatile uint32_t *)(SH4A_ITLB_AA | e);
		da1 = *(volatile uint32_t *)(SH4A_ITLB_DA1 | e);
#ifdef SH4A_EXT_MMU
		da2 = *(volatile uint32_t *)(SH4A_ITLB_DA2 | e);
#else
		da2 = 0;
#endif
		tlb_entry_print(aa, da1, da2, entry);
	}
}

void
sh4a_mmu_utlb_dump(void)
{
	uint32_t entry, e, aa, da1, da2;

	printf("--------------------UTLB--------------------\n");
	for (entry = 0; entry < SH4A_UTLB_ENTRY; entry++) {
		e = entry << UTLB_AA_E_SHIFT;
		aa = *(volatile uint32_t *)(SH4A_UTLB_AA | e);
		da1 = *(volatile uint32_t *)(SH4A_UTLB_DA1 | e);
#ifdef SH4A_EXT_MMU
		da2 = *(volatile uint32_t *)(SH4A_UTLB_DA2 | e);
#else
		da2 = 0;
#endif
		tlb_entry_print(aa, da1, da2, entry);
	}
}

vaddr_t utlb_vaddr[SH4A_UTLB_ENTRY];

paddr_t
sh4a_mmu_utlb_lookup_by_va(vaddr_t va)
{
	uint32_t entry, e, aa, da1;
	paddr_t pa = 0;
	va &= UTLB_AA_VPN;

	for (entry = 0; entry < SH4A_UTLB_ENTRY; entry++) {
		e = entry << UTLB_AA_E_SHIFT;
		aa = *(volatile uint32_t *)(SH4A_UTLB_AA | e);
		if ((aa & UTLB_AA_VPN) == va) {
			da1 = *(volatile uint32_t *)(SH4A_UTLB_DA1 | e);
			pa = da1 & UTLB_DA1_PPN;
			tlb_entry_print(aa, da1, 0, entry);
			return pa;
		}
	}
	printf("no mapping\n");

	return pa;
}

void
sh4a_mmu_utlb_check(void)
{
	uint32_t entry, e, aa;

	for (entry = 0; entry < SH4A_UTLB_ENTRY; entry++) {
		e = entry << UTLB_AA_E_SHIFT;
		aa = *(volatile uint32_t *)(SH4A_UTLB_AA | e);
		utlb_vaddr[entry] = aa & UTLB_AA_V ?
		    (aa & (UTLB_AA_VPN | UTLB_AA_ASID)) : 0xffffffff;
	}

	int i, j;
	uint32_t a0, a1;
	for (i = 0; i < SH4A_UTLB_ENTRY; i++) {
		a0 = utlb_vaddr[i];
		if (a0 == 0xffffffff)
			continue;
		for (j = 0; j < i; j++) {
			a1 = utlb_vaddr[j];
			if (a1 == 0xffffffff)
				continue;
			if (a0 == a1) {
				printf("TLB double hit %x\n", a0);
				sh4a_mmu_utlb_dump();
				while (/* CONSTCOND */1)
					continue;
			}
		}
	}
}

void
tlb_entry_print(uint32_t aa, uint32_t da1, uint32_t da2, int n)
{
	uint32_t vpn = aa & UTLB_AA_VPN;
	bool pmb = (vpn & 0xc0000000) == 0x80000000;
	int sz;
	int valid = aa & UTLB_AA_V;

	if (!valid)
		return;

	printf("(%c|%c) ", aa & UTLB_AA_D ? 'D' : '-', valid ? 'V' : '-');
	const char *pmb_pgsz[] = {
		" 16M", " 64M", "128M", "512M"
	};
#ifdef SH4A_EXT_MMU
	const char *pgsz[] = {
		"  1K", "  4K", "  8K", "-XX-",
		" 64K", "256K", "-XX-", "  1M",
		"  4M", "-XX-", "-XX-", "-XX-",
		" 64M", "-XX-", "-XX-", "-XX-"
	};
	if (pmb)
		sz = ((da1 & PMB_DA_SZ0) >> 4) | ((da1 & PMB_DA_SZ1) >> 6);
	else
		sz = (da2 >> 4) & 0xf;
	if (!pmb) {
		printf("[%c%c%c][%c%c%c] ",
		       da2 & UTLB_DA2_EPR_P_R ? 'r' : '-',
		       da2 & UTLB_DA2_EPR_P_W ? 'w' : '-',
		       da2 & UTLB_DA2_EPR_P_X ? 'x' : '-',
		       da2 & UTLB_DA2_EPR_U_R ? 'r' : '-',
		       da2 & UTLB_DA2_EPR_U_W ? 'w' : '-',
		       da2 & UTLB_DA2_EPR_U_X ? 'x' : '-');
	}
#else
	const char *pgsz[] = {
		"  1K", "  4K", " 64K", "  1M"
	};
	sz = ((da1 & UTLB_SZ0) >> 4) | ((da1 & UTLB_SZ1) >> 6);

	if (!pmb) {
		int pr = (da1 >> 5) & 0x3;
		const char *pr_pat[] = {"[r-][--]",
			"[rw][--]",
			"[rw][r-]",
		"[rw][rw]"};
		printf("%s ", pr_pat[pr]);
	}
#endif
	printf("%s ", pmb ? pmb_pgsz[sz] : pgsz[sz]);

#ifdef SH4A_EXT_ADDR32
	printf("%s ", da1 & UTLB_DA1_UB ? "UB" : "--");
#endif
	if (da1 & UTLB_DA1_C) {
		printf("W%c ", da1 & UTLB_DA1_WT ? 'T' : 'B');
	} else {
		printf("-- ");
	}
	bool shared_page = da1 & UTLB_DA1_SH;
	if (!pmb)
		printf("%s ", shared_page ? "SH" : "--");
	printf("VPN:%08x PPN:%08x ", aa & UTLB_AA_VPN, da1 & UTLB_DA1_PPN);
	if (pmb)
		printf("PMB ");
	else if (!shared_page)
		printf("ASID:%x ", aa & UTLB_AA_ASID);

	printf("[%d]\n", n);
}
#endif	/* DEBUG */
