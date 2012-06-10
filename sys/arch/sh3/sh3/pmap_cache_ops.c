/*	$NetBSD$	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pool.h>
#include <sys/msgbuf.h>

#include <uvm/uvm.h>

#include <sh3/mmu.h>
#include <sh3/cache.h>
#include <sh3/pmap_cache_ops.h>

#ifdef DEBUG
#define	STATIC
#else
#define	STATIC	static
#endif

cache_func_t *current_dcache_inv;
cache_func_t *noncurrent_dcache_inv;
cache_func_t *current_dcache_wb;
cache_func_t *noncurrent_dcache_wb;
cache_func_t *current_dcache_wbinv;
cache_func_t *noncurrent_dcache_wbinv;

cache_func_t *current_icache_inv;
cache_func_t *noncurrent_icache_inv;

STATIC void cache_nop(vaddr_t va, vsize_t sz);

void
cache_nop(vaddr_t va, vsize_t sz)
{
}

void
__pmap_cache_ops_init(void)
{
	// I-cache
	if (SH_HAS_UNIFIED_CACHE) {
		current_icache_inv = cache_nop;
		noncurrent_icache_inv = cache_nop;
	} else {
		current_icache_inv = sh_cache_ops._icache_sync_range;
		noncurrent_icache_inv =
		    sh_cache_ops._icache_sync_range_index;
	}

	// D-cache

	// Invalidate
	if (SH_HAS_WRITEBACK_CACHE) {
		printf ("writeback ");
		if (SH_HAS_VIRTUAL_ALIAS) {
			printf ("virtual alias ");
			current_dcache_inv = sh_cache_ops._dcache_inv_range;
			if (sh_cache_ways > 1) {
				printf("way=%d\n", sh_cache_ways);
				noncurrent_dcache_inv =
				    sh_cache_ops._dcache_wbinv_range_index;
			} else {
				printf ("direct map");
#if 0
				noncurrent_dcache_inv =
				    sh_cache_ops._dcache_inv_range_index;
#else
				noncurrent_dcache_inv =
				    sh_cache_ops._dcache_wbinv_range_index;
#endif
			}
		} else {
			current_dcache_inv = cache_nop;
			noncurrent_dcache_inv = cache_nop;
		}
	} else {
		current_dcache_inv = cache_nop;
		noncurrent_dcache_inv = cache_nop;
	}

	// Writeback
	if (SH_HAS_WRITEBACK_CACHE) {
		current_dcache_wb = sh_cache_ops._dcache_wb_range;
#if 0
		noncurrent_dcache_wb = sh_cache_ops._dcache_wb_range_index;
#else
		noncurrent_dcache_wb = sh_cache_ops._dcache_wbinv_range_index;
#endif
	} else {
		current_dcache_wb = cache_nop;
		noncurrent_dcache_wb = cache_nop;
	}

	// Writeback Invalidate
	if (SH_HAS_WRITEBACK_CACHE) {
		current_dcache_wbinv = sh_cache_ops._dcache_wbinv_range;
		noncurrent_dcache_wbinv = sh_cache_ops._dcache_wbinv_range_index;
	} else {
		current_dcache_wbinv = sh_cache_ops._dcache_inv_range;
#if 0
		noncurrent_dcache_wbinv = sh_cache_ops._dcache_inv_range_index;
#else
		noncurrent_dcache_wbinv = sh_cache_ops._dcache_wbinv_range_index;
#endif
	}
	printf("\n");
}

void __pmap_pg_dump(struct vm_page *pg)
{
	struct vm_page_md *pvh;
	struct pv_entry *pv;
//	int current_asid = sh_tlb_get_asid();
	printf("%s: %p\n", __FUNCTION__, pg);
	pvh = &pg->mdpage;
	SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
		printf("%p: %p-%lx\n", pg, pv->pv_pmap, pv->pv_va);
	}
}

void __pmap_icache_inv_va(struct pmap *pmap, vaddr_t va)
{
	int current_asid = sh_tlb_get_asid();

	if (pmap == pmap_kernel() || pmap->pm_asid == current_asid)
		current_icache_inv(va, PAGE_SIZE);
	else
		noncurrent_icache_inv(va, PAGE_SIZE);
}


void __pmap_dcache_inv_va(struct pmap *pmap, vaddr_t va)
{
	int current_asid = sh_tlb_get_asid();

	if (pmap == pmap_kernel() || pmap->pm_asid == current_asid)
		current_dcache_inv(va, PAGE_SIZE);
	else
		noncurrent_dcache_inv(va, PAGE_SIZE);
}

void __pmap_dcache_wb_va(struct pmap *pmap, vaddr_t va)
{
	int current_asid = sh_tlb_get_asid();

	if (pmap == pmap_kernel() || pmap->pm_asid == current_asid)
		current_dcache_wb(va, PAGE_SIZE);
	else
		noncurrent_dcache_wb(va, PAGE_SIZE);
}

void __pmap_dcache_wbinv_va(struct pmap *pmap, vaddr_t va)
{
	int current_asid = sh_tlb_get_asid();

	if (pmap == pmap_kernel() || pmap->pm_asid == current_asid)
		current_dcache_wbinv(va, PAGE_SIZE);
	else
		noncurrent_dcache_wbinv(va, PAGE_SIZE);
}

extern int __tlb_exception_cnt;
extern int __tlb_exception_debug;
paddr_t sh4a_mmu_utlb_lookup_by_va(vaddr_t);
void sh4a_dcache_array_dump(vaddr_t, vsize_t);

void __pmap_dcache_inv_pg(struct vm_page *pg)
{
	struct vm_page_md *pvh;
	struct pv_entry *pv;
	struct pmap *pmap;
	int current_asid = sh_tlb_get_asid();
	pvh = &pg->mdpage;
//	int s = splvm();
	SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
		pmap = pv->pv_pmap;
//XXXNONAKA		paddr_t pa =  sh4a_mmu_utlb_lookup_by_va(pv->pv_va);
//		sh4a_dcache_array_dump(pv->pv_va, PAGE_SIZE);
//XXXNONAKA		printf("%s: ASID%d<->%d %p,va:%lx pa:%lx ", __FUNCTION__, current_asid, pmap->pm_asid, pmap, pv->pv_va, pa);
		if (pmap == pmap_kernel() || pmap->pm_asid == current_asid) {
			printf("ohayo0 %d\n", __tlb_exception_cnt);
			__tlb_exception_debug = 1;
			current_dcache_inv(pv->pv_va, PAGE_SIZE);
			printf("ohayo1 %d\n", __tlb_exception_cnt);
			__tlb_exception_debug = 0;
		} else {
			noncurrent_dcache_inv(pv->pv_va, PAGE_SIZE);
		}
	}
//	splx(s);
}

void __pmap_dcache_wb_pg(struct vm_page *pg)
{
	struct vm_page_md *pvh;
	struct pv_entry *pv;
	struct pmap *pmap;
	int current_asid = sh_tlb_get_asid();

	pvh = &pg->mdpage;
	SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
		pmap = pv->pv_pmap;
		if (pmap == pmap_kernel() || pmap->pm_asid == current_asid)
			current_dcache_wb(pv->pv_va, PAGE_SIZE);
		else
			noncurrent_dcache_wb(pv->pv_va, PAGE_SIZE);
	}
}

void __pmap_dcache_wbinv_pg(struct vm_page *pg)
{
	struct vm_page_md *pvh;
	struct pv_entry *pv;
	struct pmap *pmap;
	int current_asid = sh_tlb_get_asid();

	pvh = &pg->mdpage;
	SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
		pmap = pv->pv_pmap;
//XXXNONAKA		printf("%s: ASID%d<->%d %p,va:%lx pa:%lx ", __FUNCTION__, current_asid, pmap->pm_asid, pmap, pv->pv_va, sh4a_mmu_utlb_lookup_by_va (pv->pv_va));
		if (pmap == pmap_kernel() || pmap->pm_asid == current_asid) {
//			printf("ohayo-0 %d\n", __tlb_exception_cnt);
			current_dcache_wbinv(pv->pv_va, PAGE_SIZE);
//			printf("ohayo-1 %d\n", __tlb_exception_cnt);
		} else {
			noncurrent_dcache_wbinv(pv->pv_va, PAGE_SIZE);
		}
	}
}
