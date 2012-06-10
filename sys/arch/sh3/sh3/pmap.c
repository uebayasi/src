/*	$NetBSD: pmap.c,v 1.77 2010/11/12 07:59:27 uebayasi Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pmap.c,v 1.77 2010/11/12 07:59:27 uebayasi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pool.h>
#include <sys/msgbuf.h>
#include <sys/socketvar.h>	/* XXX: for sock_loan_thresh */

#include <uvm/uvm.h>

#include <sh3/mmu.h>
#include <sh3/cache.h>
#include <sh3/pmap_cache_ops.h>

#ifdef DEBUG
#define	STATIC
#else
#define	STATIC	static
#endif

#define	__PMAP_PTP_SHIFT	22
#define	__PMAP_PTP_TRUNC(va)						\
	(((va) + (1 << __PMAP_PTP_SHIFT) - 1) & ~((1 << __PMAP_PTP_SHIFT) - 1))
#define	__PMAP_PTP_PG_N		(PAGE_SIZE / sizeof(pt_entry_t))
#define	__PMAP_PTP_INDEX(va)	(((va) >> __PMAP_PTP_SHIFT) & (__PMAP_PTP_N - 1))
#define	__PMAP_PTP_OFSET(va)	((va >> PGSHIFT) & (__PMAP_PTP_PG_N - 1))

struct pmap __pmap_kernel;
struct pmap *const kernel_pmap_ptr = &__pmap_kernel;
STATIC vaddr_t __pmap_kve;	/* VA of last kernel virtual */
// kloader uses avail_start and avail_end.
paddr_t avail_start;		/* PA of first available physical page */
paddr_t avail_end;		/* PA of last available physical page */

/* For the fast tlb miss handler */
pt_entry_t **curptd;		/* p1 va of curlwp->...->pm_ptp */

/* pmap pool */
STATIC struct pool __pmap_pmap_pool;

#define	__pmap_pv_alloc()	pool_get(&__pmap_pv_pool, PR_NOWAIT)
#define	__pmap_pv_free(pv)	pool_put(&__pmap_pv_pool, (pv))
STATIC void __pmap_pv_enter(pmap_t, struct vm_page *, vaddr_t);
STATIC void __pmap_pv_remove(pmap_t, struct vm_page *, vaddr_t);
STATIC void *__pmap_pv_page_alloc(struct pool *, int);
STATIC void __pmap_pv_page_free(struct pool *, void *);
STATIC struct pool __pmap_pv_pool;
STATIC struct pool_allocator pmap_pv_page_allocator = {
	__pmap_pv_page_alloc, __pmap_pv_page_free, 0,
};

/* ASID ops. */
STATIC int __pmap_asid_alloc(void);
STATIC void __pmap_asid_free(int);
STATIC struct {
	uint32_t map[8];
	int hint;	/* hint for next allocation */
} __pmap_asid;

/* page table entry ops. */
STATIC pt_entry_t *__pmap_pte_alloc(pmap_t, vaddr_t);

/* pmap_enter util */
STATIC bool __pmap_map_change(pmap_t, vaddr_t, paddr_t, vm_prot_t, pt_entry_t);

void
pmap_bootstrap(void)
{
	/* Steal msgbuf area */
	initmsgbuf((void *)uvm_pageboot_alloc(MSGBUFSIZE), MSGBUFSIZE);

	avail_start = ptoa(VM_PHYSMEM_PTR(0)->start);
	avail_end = ptoa(VM_PHYSMEM_PTR(vm_nphysseg - 1)->end);
	__pmap_kve = VM_MIN_KERNEL_ADDRESS;

	pmap_kernel()->pm_refcnt = 1;
	pmap_kernel()->pm_ptp = (pt_entry_t **)uvm_pageboot_alloc(PAGE_SIZE);
	memset(pmap_kernel()->pm_ptp, 0, PAGE_SIZE);

	/* Enable MMU */
	sh_mmu_start();
	/* Mask all interrupt */
	_cpu_intr_suspend();
	/* Enable exception for P3 access */
	_cpu_exception_resume(0);
}

vaddr_t
pmap_steal_memory(vsize_t size, vaddr_t *vstart, vaddr_t *vend)
{
//	printf("%s\n", __FUNCTION__);
	struct vm_physseg *bank;
	int i, j, npage;
	paddr_t pa;
	vaddr_t va;

	KDASSERT(!uvm.page_init_done);

	size = round_page(size);
	npage = atop(size);

	bank = NULL;
#ifdef SH4A_EXT_ADDR32
	// To assure uvm_pageboot_alloc allocates P1/P2 mapped area.
	for (i = 0; i < vm_nphysseg; i++, bank++) {
		bank = VM_PHYSMEM_PTR(i);
		if ((npage <= bank->avail_end - bank->avail_start) &&
		    (bank->free_list == VM_FREELIST_P1ACCESS))
			break;
	}
#else
	for (i = 0; i < vm_nphysseg; i++) {
		bank = VM_PHYSMEM_PTR(i);
		if (npage <= bank->avail_end - bank->avail_start)
			break;
	}
#endif
	KDASSERT(i != vm_nphysseg);
	KDASSERT(bank != NULL);

	/* Steal pages */
	pa = ptoa(bank->avail_start);
	bank->avail_start += npage;
	bank->start += npage;

	/* GC memory bank */
	if (bank->avail_start == bank->end) {
		/* Remove this segment from the list. */
		vm_nphysseg--;
		KDASSERT(vm_nphysseg > 0);
		for (j = i; i < vm_nphysseg; j++)
			VM_PHYSMEM_PTR_SWAP(j, j + 1);
	}

	va = SH3_PHYS_TO_P1SEG(pa);
#ifdef SH4A_EXT_ADDR32
	assert(pa < 0x20000000);
#endif
	memset((void *)va, 0, size);

	return (va);
}

vaddr_t
pmap_growkernel(vaddr_t maxkvaddr)
{
//	printf("%s %d\n", __FUNCTION__, uvm.page_init_done);
	int i, n;

	if (maxkvaddr <= __pmap_kve)
		return (__pmap_kve);

	i = __PMAP_PTP_INDEX(__pmap_kve - VM_MIN_KERNEL_ADDRESS);
	__pmap_kve = __PMAP_PTP_TRUNC(maxkvaddr);
	n = __PMAP_PTP_INDEX(__pmap_kve - VM_MIN_KERNEL_ADDRESS);

	/* Allocate page table pages */
	for (;i < n; i++) {
		if (__pmap_kernel.pm_ptp[i] != NULL)
			continue;

		if (uvm.page_init_done) {
#ifdef SH4A_EXT_ADDR32
			struct vm_page *pg = uvm_pagealloc_strat(NULL, 0, NULL,
			    UVM_PGA_USERESERVE | UVM_PGA_ZERO,
			    UVM_PGA_STRAT_ONLY, VM_FREELIST_P1ACCESS);
			assert(VM_PAGE_TO_PHYS(pg) < 0x20000000);
#else
			struct vm_page *pg = uvm_pagealloc(NULL, 0, NULL,
			    UVM_PGA_USERESERVE | UVM_PGA_ZERO);
#endif
			if (pg == NULL)
				goto error;
			__pmap_kernel.pm_ptp[i] = (pt_entry_t *)
			    SH3_PHYS_TO_P1SEG(VM_PAGE_TO_PHYS(pg));
//XXXNONAKA			printf("%s: %lx for %d\n", __FUNCTION__, VM_PAGE_TO_PHYS(pg), i);

		} else {
			pt_entry_t *ptp = (pt_entry_t *)
			    uvm_pageboot_alloc(PAGE_SIZE);
			if (ptp == NULL)
				goto error;
			__pmap_kernel.pm_ptp[i] = ptp;
			memset(ptp, 0, PAGE_SIZE);
		}
	}

	return (__pmap_kve);
 error:
	panic("pmap_growkernel: out of memory.");
	/* NOTREACHED */
}

void
pmap_virtual_space(vaddr_t *start, vaddr_t *end)
{
//	printf("%s\n", __FUNCTION__);
	*start = VM_MIN_KERNEL_ADDRESS;
	*end = VM_MAX_KERNEL_ADDRESS;
}

void
pmap_init(void)
{
//	printf("%s\n", __FUNCTION__);
	/* Initialize pmap module */
#if 0//def PMAP_MAP_POOLPAGE
	pool_init(&__pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pool_allocator_nointr, IPL_NONE);
#else
	// allocate pmap to P1.
	pool_init(&__pmap_pmap_pool, sizeof(struct pmap), 0, 0, 0, "pmappl",
	    &pmap_pv_page_allocator, IPL_NONE);
#endif
	pool_init(&__pmap_pv_pool, sizeof(struct pv_entry), 0, 0, 0, "pvpl",
	    &pmap_pv_page_allocator, IPL_NONE);
	pool_setlowat(&__pmap_pv_pool, 16);
#if defined SH4 || defined SH4A //XXX should be  SH_HAS_VIRTUAL_ALIAS
	if (SH_HAS_VIRTUAL_ALIAS) {
		/*
		 * XXX
		 * Disable sosend_loan() in src/sys/kern/uipc_socket.c
		 * on SH4 to avoid possible virtual cache aliases and
		 * unnecessary map/unmap thrashing in __pmap_pv_enter().
		 * (also see comments in __pmap_pv_enter())
		 * 
		 * Ideally, read only shared mapping won't cause aliases
		 * so __pmap_pv_enter() should handle any shared read only
		 * mappings like ARM pmap.
		 */
		sock_loan_thresh = -1;
	}
#endif
	__pmap_cache_ops_init();

}
pmap_t
pmap_create(void)
{
//	printf("%s\n", __FUNCTION__);
	pmap_t pmap;
	struct vm_page *pg;

	pmap = pool_get(&__pmap_pmap_pool, PR_WAITOK);
	memset(pmap, 0, sizeof(struct pmap));
	pmap->pm_asid = -1;
	pmap->pm_refcnt = 1;
	/* Allocate page table page holder (512 slot) */
#ifdef SH4A_EXT_ADDR32
	pg = uvm_pagealloc_strat(NULL, 0, NULL,
	    UVM_PGA_USERESERVE | UVM_PGA_ZERO,
	    UVM_PGA_STRAT_ONLY, VM_FREELIST_P1ACCESS);
	assert(VM_PAGE_TO_PHYS(pg) < 0x20000000);
#else
	pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE | UVM_PGA_ZERO);
#endif
	pmap->pm_ptp = (pt_entry_t **)SH3_PHYS_TO_P1SEG(VM_PAGE_TO_PHYS(pg));

	return (pmap);
}

void
pmap_destroy(pmap_t pmap)
{
//	printf("%s\n", __FUNCTION__);
	vaddr_t va;
	int i;

	if (--pmap->pm_refcnt > 0)
		return;

	/* Deallocate all page table page */
	for (i = 0; i < __PMAP_PTP_N; i++) {
		va = (vaddr_t)pmap->pm_ptp[i];
		if (va == 0)
			continue;
#ifdef DEBUG	/* Check no mapping exists. */
		{
			int j;
			pt_entry_t *pte = (pt_entry_t *)va;
			for (j = 0; j < __PMAP_PTP_PG_N; j++, pte++)
				KDASSERT(*pte == 0);
		}
#endif /* DEBUG */
		/* Free page table */
		uvm_pagefree(PHYS_TO_VM_PAGE(SH3_P1SEG_TO_PHYS(va)));
		/* Purge cache entry for next use of this page. */
		__pmap_dcache_inv_va(pmap_kernel(), va);
	}
	/* Deallocate page table page holder */
	va = (vaddr_t)pmap->pm_ptp; // P1
	uvm_pagefree(PHYS_TO_VM_PAGE(SH3_P1SEG_TO_PHYS(va)));
	__pmap_dcache_inv_va(pmap_kernel(), va);

	/* Free ASID */
	__pmap_asid_free(pmap->pm_asid);

	pool_put(&__pmap_pmap_pool, pmap);
}

void
pmap_reference(pmap_t pmap)
{
//	printf("%s\n", __FUNCTION__);
	pmap->pm_refcnt++;
}

void
pmap_activate(struct lwp *l)
{
//	void sh4a_tlb_invalidate_all (void);
//	sh4a_tlb_invalidate_all();

	pmap_t pmap = l->l_proc->p_vmspace->vm_map.pmap;
//	printf("%s %d\n", __FUNCTION__, pmap->pm_asid);
	if (pmap->pm_asid == -1)
		pmap->pm_asid = __pmap_asid_alloc();

	KDASSERT(pmap->pm_asid >=0 && pmap->pm_asid < 256);

	sh_tlb_set_asid(pmap->pm_asid);
	curptd = pmap->pm_ptp;
}

void
pmap_deactivate(struct lwp *l)
{
//	printf("%s\n", __FUNCTION__);
	/* Nothing to do */
}

int
pmap_enter(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot, u_int flags)
{
//	printf("%s pa=%lx va=%lx\n", __FUNCTION__, pa, va);
	struct vm_page *pg;
	struct vm_page_md *pvh;
	pt_entry_t entry, *pte;
	bool kva = (pmap == pmap_kernel());

	/* "flags" never exceed "prot" */
	KDASSERT(prot != 0 && ((flags & VM_PROT_ALL) & ~prot) == 0);

	pg = PHYS_TO_VM_PAGE(pa);
	entry = (pa & PG_PPN) | PG_4K;
	if (flags & PMAP_WIRED)
		entry |= _PG_WIRED;

	if (pg != NULL) {	/* memory-space */
		pvh = VM_PAGE_TO_MD(pg);
		entry |= PG_C;	/* always cached */

		/* Seed modified/reference tracking */
		if (flags & VM_PROT_WRITE) {
			entry |= PG_V | PG_D;
			pvh->pvh_flags |= PVH_MODIFIED | PVH_REFERENCED;
		} else if (flags & VM_PROT_ALL) {
			entry |= PG_V;
			pvh->pvh_flags |= PVH_REFERENCED;
		}

		/* Protection */
		if ((prot & VM_PROT_WRITE) && (pvh->pvh_flags & PVH_MODIFIED)) {
			if (kva)
				entry |= PG_PR_KRW | PG_SH;
			else
				entry |= PG_PR_URW;
		} else {
			/* RO or COW page */
			if (kva)
				entry |= PG_PR_KRO | PG_SH;
			else
				entry |= PG_PR_URO;
		}

		/* Check for existing mapping */
		if (__pmap_map_change(pmap, va, pa, prot, entry))
			return (0);

		/* Add to physical-virtual map list of this page */
		__pmap_pv_enter(pmap, pg, va);

	} else {	/* bus-space (always uncached map) */
		if (kva) {
			entry |= PG_V | PG_SH |
			    ((prot & VM_PROT_WRITE) ?
			    (PG_PR_KRW | PG_D) : PG_PR_KRO);
		} else {
			entry |= PG_V |
			    ((prot & VM_PROT_WRITE) ?
			    (PG_PR_URW | PG_D) : PG_PR_URO);
		}
	}

	/* Register to page table */
	if (kva)
		pte = __pmap_kpte_lookup(va);
	else {
		pte = __pmap_pte_alloc(pmap, va);
		if (pte == NULL) {
			if (flags & PMAP_CANFAIL)
				return ENOMEM;
			panic("pmap_enter: cannot allocate pte");
		}
	}

	*pte = entry;

	if (pmap->pm_asid != -1)
		sh_tlb_update(pmap->pm_asid, va, entry);

	if (prot == (VM_PROT_READ | VM_PROT_EXECUTE)) {
		__pmap_icache_inv_va(pmap, va);
	}

	if (entry & _PG_WIRED)
		pmap->pm_stats.wired_count++;
	pmap->pm_stats.resident_count++;

	return (0);
}

/*
 * bool __pmap_map_change(pmap_t pmap, vaddr_t va, paddr_t pa,
 *     vm_prot_t prot, pt_entry_t entry):
 *	Handle the situation that pmap_enter() is called to enter a
 *	mapping at a virtual address for which a mapping already
 *	exists.
 */
bool
__pmap_map_change(pmap_t pmap, vaddr_t va, paddr_t pa, vm_prot_t prot,
    pt_entry_t entry)
{
//	printf("%s\n", __FUNCTION__);
	pt_entry_t *pte, oentry;
	vaddr_t eva = va + PAGE_SIZE;

	if ((pte = __pmap_pte_lookup(pmap, va)) == NULL ||
	    ((oentry = *pte) == 0))
		return (false);		/* no mapping exists. */

	if (pa != (oentry & PG_PPN)) {
		/* Enter a mapping at a mapping to another physical page. */
		pmap_remove(pmap, va, eva);
		return (false);
	}

	/* Pre-existing mapping */

	/* Protection change. */
	if ((oentry & PG_PR_MASK) != (entry & PG_PR_MASK))
		pmap_protect(pmap, va, eva, prot);

	/* Wired change */
	if (oentry & _PG_WIRED) {
		if (!(entry & _PG_WIRED)) {
			/* wired -> unwired */
			*pte = entry;
			/* "wired" is software bits. no need to update TLB */
			pmap->pm_stats.wired_count--;
		}
#if 1//XXXnoneed
	} else if (entry & _PG_WIRED) {
		/* unwired -> wired. make sure to reflect "flags" */
		pmap_remove(pmap, va, eva);
		return (false);
#endif
	}

	return (true);	/* mapping was changed. */
}

/*
 * void __pmap_pv_enter(pmap_t pmap, struct vm_page *pg, vaddr_t vaddr):
 *	Insert physical-virtual map to vm_page.
 *	Assume pre-existed mapping is already removed.
 */
void
__pmap_pv_enter(pmap_t pmap, struct vm_page *pg, vaddr_t va)
{
//	printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%s pa=%lx, va=%lx\n",   __FUNCTION__, pa, va);
	struct vm_page_md *pvh;
	struct pv_entry *pv;
	int s;

	s = splvm();
	if (SH_HAS_VIRTUAL_ALIAS) {
		/*
		 * Remove all other mappings on this physical page
		 * which have different virtual cache indexes to
		 * avoid virtual cache aliases.
		 *
		 * XXX We should also handle shared mappings which
		 * XXX have different virtual cache indexes by
		 * XXX mapping them uncached (like arm and mips do).
		 */
 again:
		pvh = VM_PAGE_TO_MD(pg);
		SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
			if (sh_cache_indexof(va) !=
			    sh_cache_indexof(pv->pv_va)) {
				pmap_remove(pv->pv_pmap, pv->pv_va,
				    pv->pv_va + PAGE_SIZE);
				printf("VIRTUAL ALIAS FOUND\n");
				goto again;
			}
		}
	}

	/* Register pv map */
	pvh = VM_PAGE_TO_MD(pg);
	pv = __pmap_pv_alloc();
	pv->pv_pmap = pmap;
	pv->pv_va = va;

	SLIST_INSERT_HEAD(&pvh->pvh_head, pv, pv_link);
	splx(s);
}

void
pmap_remove(pmap_t pmap, vaddr_t sva, vaddr_t eva)
{
//XXXNONAKA	printf("%s: asid%d %lx-%lx\n", __FUNCTION__, pmap->pm_asid, sva, eva);
	struct vm_page *pg;
	pt_entry_t *pte, entry;
	vaddr_t va;

	KDASSERT((sva & PGOFSET) == 0);

	for (va = sva; va < eva; va += PAGE_SIZE) {
		if ((pte = __pmap_pte_lookup(pmap, va)) == NULL ||
		    (entry = *pte) == 0)
			continue;

		if ((pg = PHYS_TO_VM_PAGE(entry & PG_PPN)) != NULL)
			__pmap_pv_remove(pmap, pg, va);

		if (entry & _PG_WIRED)
			pmap->pm_stats.wired_count--;
		pmap->pm_stats.resident_count--;
		*pte = 0;

		/*
		 * When pmap->pm_asid == -1 (invalid ASID), old entry attribute
		 * to this pmap is already removed by pmap_activate().
		 */
		if (pmap->pm_asid != -1)
			sh_tlb_invalidate_addr(pmap->pm_asid, va);
	}
}

/*
 * void __pmap_pv_remove(pmap_t pmap, struct vm_page *pg, vaddr_t vaddr):
 *	Remove physical-virtual map from vm_page.
 */
void
__pmap_pv_remove(pmap_t pmap, struct vm_page *pg, vaddr_t vaddr)
{
//	printf("%s\n", __FUNCTION__);
	struct vm_page_md *pvh;
	struct pv_entry *pv;
	int s;

	s = splvm();
	pvh = VM_PAGE_TO_MD(pg);

	SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
		if (pv->pv_pmap == pmap && pv->pv_va == vaddr) {
#if 0
			if (SH_HAS_VIRTUAL_ALIAS ||
			    (SH_HAS_WRITEBACK_CACHE &&
				(pvh->pvh_flags & PVH_MODIFIED))) {
				/*
				 * Always use index ops. since I don't want to
				 * worry about address space.
				 */
				sh_dcache_wbinv_range_index
				    (pv->pv_va, PAGE_SIZE);
			}
#endif

			SLIST_REMOVE(&pvh->pvh_head, pv, pv_entry, pv_link);
			__pmap_pv_free(pv);
			break;
		}
	}
	__pmap_icache_inv_va(pmap, vaddr);

#ifdef DEBUG
	/* Check duplicated map. */
	SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
//		printf("%s: pmap %p va %lx\n", __FUNCTION__, pv->pv_pmap, pv->pv_va);
	    KDASSERT(!(pv->pv_pmap == pmap && pv->pv_va == vaddr));
	}
#endif
	splx(s);
}

void
pmap_kenter_pa(vaddr_t va, paddr_t pa, vm_prot_t prot, u_int flags)
{
//	printf("%s va=%lx pa=%lx ptpslot=%ld\n", __FUNCTION__, va, pa,
//	    __PMAP_PTP_INDEX(va-VM_MIN_KERNEL_ADDRESS));
	pt_entry_t *pte, entry;

	KDASSERT((va & PGOFSET) == 0);
	KDASSERT(va >= VM_MIN_KERNEL_ADDRESS && va < VM_MAX_KERNEL_ADDRESS);
	entry = (pa & PG_PPN) | PG_V | PG_SH | PG_4K;
	if (prot & VM_PROT_WRITE)
		entry |= (PG_PR_KRW | PG_D);
	else
		entry |= PG_PR_KRO;

	if (PHYS_TO_VM_PAGE(pa))
		entry |= PG_C;

	if ((pte = __pmap_kpte_lookup(va)) == NULL) {
		printf ("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$%s no pte\n",
		    __FUNCTION__);
	}

	KDASSERT(*pte == 0);
	*pte = entry;

	sh_tlb_update(0, va, entry);
}

void
pmap_kremove(vaddr_t va, vsize_t len)
{
//	printf("%s va=%lx len=%lx\n", __FUNCTION__, va, len);
	pt_entry_t *pte;
	vaddr_t eva = va + len;

	KDASSERT((va & PGOFSET) == 0);
	KDASSERT((len & PGOFSET) == 0);
	KDASSERT(va >= VM_MIN_KERNEL_ADDRESS && eva <= VM_MAX_KERNEL_ADDRESS);

	for (; va < eva; va += PAGE_SIZE) {
		pte = __pmap_kpte_lookup(va);
		KDASSERT(pte != NULL);
		if (*pte == 0)
			continue;

		if (PHYS_TO_VM_PAGE(*pte & PG_PPN))	// Memory.
			__pmap_dcache_wbinv_va(pmap_kernel(), va);//XXXinv??
		*pte = 0;

		sh_tlb_invalidate_addr(0, va);
	}
}

bool
pmap_extract(pmap_t pmap, vaddr_t va, paddr_t *pap)
{
//	printf("%s\n", __FUNCTION__);
	pt_entry_t *pte;

	/* handle P1 and P2 specially: va == pa */
	if (pmap == pmap_kernel() && (va >> 30) == 2) {
		if (pap != NULL)
			*pap = va & SH3_PHYS_MASK;

		return (true);
	}

	pte = __pmap_pte_lookup(pmap, va);
	if (pte == NULL || *pte == 0) {
//		printf("%s va=%lx failed\n", __FUNCTION__, va);
		return (false);
	}

	if (pap != NULL)
		*pap = (*pte & PG_PPN) | (va & PGOFSET);

	return (true);
}

void
pmap_protect(pmap_t pmap, vaddr_t sva, vaddr_t eva, vm_prot_t prot)
{
//	printf("%s\n", __FUNCTION__);
	bool kernel = pmap == pmap_kernel();
	pt_entry_t *pte, entry, protbits;
	vaddr_t va;

	sva = trunc_page(sva);

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

	switch (prot) {
	default:
		panic("pmap_protect: invalid protection mode %x", prot);
		/* NOTREACHED */
	case VM_PROT_READ:
		/* FALLTHROUGH */
	case VM_PROT_READ | VM_PROT_EXECUTE:
		protbits = kernel ? PG_PR_KRO : PG_PR_URO;
		break;
	case VM_PROT_READ | VM_PROT_WRITE:
		/* FALLTHROUGH */
	case VM_PROT_ALL:
		protbits = kernel ? PG_PR_KRW : PG_PR_URW;
		break;
	}

	for (va = sva; va < eva; va += PAGE_SIZE) {

		if (((pte = __pmap_pte_lookup(pmap, va)) == NULL) ||
		    (entry = *pte) == 0)
			continue;

		if (prot & VM_PROT_EXECUTE)
			__pmap_icache_inv_va(pmap, va);// also dcache_wbinv
		else
			__pmap_dcache_wbinv_va(pmap, va);

		entry = (entry & ~PG_PR_MASK) | protbits;
		*pte = entry;

		if (pmap->pm_asid != -1)
			sh_tlb_update(pmap->pm_asid, va, entry);
	}
}

void
pmap_page_protect(struct vm_page *pg, vm_prot_t prot)
{
//	printf("%s\n", __FUNCTION__);
	struct vm_page_md *pvh = VM_PAGE_TO_MD(pg);
	struct pv_entry *pv;
	struct pmap *pmap;
	vaddr_t va;
	int s;

	switch (prot) {
	case VM_PROT_READ | VM_PROT_WRITE:
		/* FALLTHROUGH */
	case VM_PROT_ALL:
		break;

	case VM_PROT_READ:
		/* FALLTHROUGH */
	case VM_PROT_READ | VM_PROT_EXECUTE:
		s = splvm();
		SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
			pmap = pv->pv_pmap;
			va = pv->pv_va;

			KDASSERT(pmap);
			pmap_protect(pmap, va, va + PAGE_SIZE, prot);
		}
		splx(s);
		break;

	default:
		/* Remove all */
		s = splvm();
		while ((pv = SLIST_FIRST(&pvh->pvh_head)) != NULL) {
			va = pv->pv_va;
			pmap_remove(pv->pv_pmap, va, va + PAGE_SIZE);
		}
		splx(s);
	}
}

void
pmap_unwire(pmap_t pmap, vaddr_t va)
{
//	printf("%s\n", __FUNCTION__);
	pt_entry_t *pte, entry;

	if ((pte = __pmap_pte_lookup(pmap, va)) == NULL ||
	    (entry = *pte) == 0 ||
	    (entry & _PG_WIRED) == 0)
		return;

	*pte = entry & ~_PG_WIRED;
	pmap->pm_stats.wired_count--;
}

void
pmap_procwr(struct proc	*p, vaddr_t va, size_t len)
{
//	printf("%s\n", __FUNCTION__);
#if 0
	if (!SH_HAS_UNIFIED_CACHE)
		sh_icache_sync_range_index(va, len);
#else
	if (SH_HAS_UNIFIED_CACHE)
		return;
	struct pmap *pmap = p->p_vmspace->vm_map.pmap;
	int current_asid = sh_tlb_get_asid();

	if (pmap->pm_asid == current_asid)
		sh_icache_sync_range(va, len);
	else
		sh_icache_sync_range_index(va, len);
#endif
}

void
pmap_zero_page(paddr_t phys)
{
	struct vm_page *pg;

//	printf("%s pa=%lx\n", __FUNCTION__, phys);
#ifdef SH4A_EXT_ADDR32
	if (phys < 0x20000000)
		goto maped_area;

	struct vm_page_md *pvh;
	struct pv_entry *pv;

	bool vm_mapped = FALSE;
	if ((pg = PHYS_TO_VM_PAGE(phys)) != NULL) {
		pvh = &pg->mdpage;
		// For each vm page which maps this physical page.
		SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
			sh_tlb_set_asid(pv->pv_pmap->pm_asid);
			memset ((void *)pv->pv_va, 0, PAGE_SIZE);
			sh_tlb_set_asid(0);
			vm_mapped = TRUE;
		}
	}
	if (!vm_mapped) {
		// Mapped to P0 area
		vaddr_t va = 0;
		sh_tlb_update (0, va,
		    (phys & PG_PPN) | PG_PR_KRW | PG_V | PG_D | PG_4K);
		int asid = sh_tlb_get_asid();
		sh_tlb_set_asid(0);
		memset ((void *)va, 0, PAGE_SIZE);
		sh_tlb_set_asid(asid);
		sh_tlb_invalidate_addr (0, 0);
	}
	return;
maped_area:
#endif
	if (SH_HAS_VIRTUAL_ALIAS) {	/* don't polute cache */
		/* sync cache since we access via P2. */
		if ((pg = PHYS_TO_VM_PAGE(phys))) {
			__pmap_dcache_inv_pg(pg);	//OK inv
		}
		memset((void *)SH3_PHYS_TO_P2SEG(phys), 0, PAGE_SIZE);
	} else {
		memset((void *)SH3_PHYS_TO_P1SEG(phys), 0, PAGE_SIZE);
	}
}

void
pmap_copy_page(paddr_t src, paddr_t dst)
{
//XXXNONAKA	printf("%s src:%lx, dst:%lx\n", __FUNCTION__, src, dst);
	struct vm_page *pg;
#ifdef SH4A_EXT_ADDR32
	if (src < 0x20000000 && dst < 0x20000000)
		goto maped_area;
	struct vm_page *pg_src, *pg_dst;
	vaddr_t dst_va;
	vaddr_t src_va;
	int s = _cpu_intr_suspend();
	// If mapped to virtual address, sync cache.
	if ((pg_src = PHYS_TO_VM_PAGE(src)) != NULL)
		__pmap_dcache_wb_pg(pg_src);

	if ((pg_dst = PHYS_TO_VM_PAGE(dst)) != NULL)
		__pmap_dcache_inv_pg(pg_src);//XXXinv

	src_va = 0;
	dst_va = PAGE_SIZE;
	sh_tlb_update(0, src_va,
	    (src & PG_PPN) | PG_PR_KRW | PG_V | PG_D | PG_4K);
	sh_tlb_update(0, dst_va,
	    (dst & PG_PPN) | PG_PR_KRW | PG_V | PG_D | PG_4K);
	int asid = sh_tlb_get_asid();
	sh_tlb_set_asid(0);
	memcpy((void *)dst_va, (void *)src_va, PAGE_SIZE);
	sh_tlb_set_asid(asid);
	sh_tlb_invalidate_addr (0, src_va);
	sh_tlb_invalidate_addr (0, dst_va);
	_cpu_intr_resume (s);
	return;
maped_area:
#endif
	if (SH_HAS_VIRTUAL_ALIAS) {
		/* sync cache since we access via P2. */
//XXXNONAKA		printf("%s P2\n", __FUNCTION__);
		if ((pg = PHYS_TO_VM_PAGE(dst)))
			__pmap_dcache_inv_pg(pg);
		if ((pg = PHYS_TO_VM_PAGE(src)))
			__pmap_dcache_wb_pg(pg);
		memcpy((void *)SH3_PHYS_TO_P2SEG(dst),
		    (void *)SH3_PHYS_TO_P2SEG(src), PAGE_SIZE);
	} else {
		memcpy((void *)SH3_PHYS_TO_P1SEG(dst),
		    (void *)SH3_PHYS_TO_P1SEG(src), PAGE_SIZE);
	}
//	printf("DONE***************************************%s\n", __FUNCTION__);
}

bool
pmap_is_referenced(struct vm_page *pg)
{
//	printf("%s\n", __FUNCTION__);
	struct vm_page_md *pvh = VM_PAGE_TO_MD(pg);

	return ((pvh->pvh_flags & PVH_REFERENCED) ? true : false);
}

bool
pmap_clear_reference(struct vm_page *pg)
{
//	printf("%s\n", __FUNCTION__);
	struct vm_page_md *pvh = VM_PAGE_TO_MD(pg);
	struct pv_entry *pv;
	pt_entry_t *pte;
	pmap_t pmap;
	vaddr_t va;
	int s;

	if ((pvh->pvh_flags & PVH_REFERENCED) == 0)
		return (false);

	pvh->pvh_flags &= ~PVH_REFERENCED;

	s = splvm();
	/* Restart reference bit emulation */
	SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
		pmap = pv->pv_pmap;
		va = pv->pv_va;

		if ((pte = __pmap_pte_lookup(pmap, va)) == NULL)
			continue;
		if ((*pte & PG_V) == 0)
			continue;
		*pte &= ~PG_V;

		if (pmap->pm_asid != -1)
			sh_tlb_invalidate_addr(pmap->pm_asid, va);
	}
	splx(s);

	return (true);
}

bool
pmap_is_modified(struct vm_page *pg)
{
	struct vm_page_md *pvh = VM_PAGE_TO_MD(pg);

	return ((pvh->pvh_flags & PVH_MODIFIED) ? true : false);
}

bool
pmap_clear_modify(struct vm_page *pg)
{
//	printf("%s\n", __FUNCTION__);
	struct vm_page_md *pvh = VM_PAGE_TO_MD(pg);
	struct pv_entry *pv;
	struct pmap *pmap;
	pt_entry_t *pte, entry;
	bool modified;
	vaddr_t va;
	int s;

	modified = pvh->pvh_flags & PVH_MODIFIED;
	if (!modified)
		return (false);

	pvh->pvh_flags &= ~PVH_MODIFIED;

	s = splvm();
	if (SLIST_EMPTY(&pvh->pvh_head)) {/* no map on this page */
		splx(s);
		return (true);
	}

	/* Write-back and invalidate TLB entry */
#if 0
	if (!SH_HAS_VIRTUAL_ALIAS && SH_HAS_WRITEBACK_CACHE)//XXX
		sh_dcache_wbinv_all();//XXX
#endif
// 	__pmap_dcache_wbinv_pg(pg);

	SLIST_FOREACH(pv, &pvh->pvh_head, pv_link) {
		pmap = pv->pv_pmap;
		va = pv->pv_va;
		if ((pte = __pmap_pte_lookup(pmap, va)) == NULL)
			continue;
		entry = *pte;
		if ((entry & PG_D) == 0)
			continue;

		__pmap_dcache_wbinv_va(pmap, va);

		*pte = entry & ~PG_D;
		if (pmap->pm_asid != -1)
			sh_tlb_invalidate_addr(pmap->pm_asid, va);
	}
	splx(s);

	return (true);
}

paddr_t
pmap_phys_address(paddr_t cookie)
{
//	printf("%s\n", __FUNCTION__);
	return (sh3_ptob(cookie));
}

#if defined SH4 || defined SH4A //XXX should be  SH_HAS_VIRTUAL_ALIAS
/*
 * pmap_prefer(vaddr_t foff, vaddr_t *vap)
 *
 * Find first virtual address >= *vap that doesn't cause
 * a virtual cache alias against vaddr_t foff.
 */
void
pmap_prefer(vaddr_t foff, vaddr_t *vap)
{
//	printf("%s\n", __FUNCTION__);
	vaddr_t va;

	if (*vap & 0x1f)
		printf("%s %lx\n", __FUNCTION__, *vap);
	assert (!(foff & 0x1f));
	if (SH_HAS_VIRTUAL_ALIAS) {
#if 1
		va = *vap;
		*vap = va + ((foff - va) & sh_cache_prefer_mask);
#else
		va = *vap;
		uint32_t ci0 = foff & (0xff << 5);
		uint32_t ci1 = va & (0xff << 5);
		*vap = va + (ci0 - ci1);
#endif
	}
}
#endif /* SH_HAS_VIRTUAL_ALIAS */

/*
 * pv_entry pool allocator:
 *	void *__pmap_pv_page_alloc(struct pool *pool, int flags):
 *	void __pmap_pv_page_free(struct pool *pool, void *v):
 */
void *
__pmap_pv_page_alloc(struct pool *pool, int flags)
{
//	printf("%s\n", __FUNCTION__);
	struct vm_page *pg;
#ifdef SH4A_EXT_ADDR32
	pg = uvm_pagealloc_strat(NULL, 0, NULL, UVM_PGA_USERESERVE,
	    UVM_PGA_STRAT_ONLY, VM_FREELIST_P1ACCESS);
	assert(VM_PAGE_TO_PHYS(pg) < 0x20000000);
#else
	pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE);
#endif
	if (pg == NULL)
		return (NULL);

	return ((void *)SH3_PHYS_TO_P1SEG(VM_PAGE_TO_PHYS(pg)));
}

void
__pmap_pv_page_free(struct pool *pool, void *v)
{
//	printf("%s\n", __FUNCTION__);
	vaddr_t va = (vaddr_t)v;

	/* Invalidate cache for next use of this page */
	__pmap_dcache_inv_va(pmap_kernel(), va);

	uvm_pagefree(PHYS_TO_VM_PAGE(SH3_P1SEG_TO_PHYS(va)));
}

/*
 * pt_entry_t __pmap_pte_alloc(pmap_t pmap, vaddr_t va):
 *	lookup page table entry. if found returns it, else allocate it.
 *	page table is accessed via P1.
 */
pt_entry_t *
__pmap_pte_alloc(pmap_t pmap, vaddr_t va)
{
//	printf("%s\n", __FUNCTION__);
	struct vm_page *pg;
	pt_entry_t *ptp, *pte;

	if ((pte = __pmap_pte_lookup(pmap, va)) != NULL)
		return (pte);

	/* Allocate page table (not managed page) */
#ifdef SH4A_EXT_ADDR32
	pg = uvm_pagealloc_strat(NULL, 0, NULL,
	    UVM_PGA_USERESERVE | UVM_PGA_ZERO,
	    UVM_PGA_STRAT_ONLY, VM_FREELIST_P1ACCESS);
	assert(VM_PAGE_TO_PHYS(pg) < 0x20000000);
#else
	pg = uvm_pagealloc(NULL, 0, NULL, UVM_PGA_USERESERVE | UVM_PGA_ZERO);
#endif
	if (pg == NULL)
		return NULL;

	ptp = (pt_entry_t *)SH3_PHYS_TO_P1SEG(VM_PAGE_TO_PHYS(pg));
	pmap->pm_ptp[__PMAP_PTP_INDEX(va)] = ptp;

	return (ptp + __PMAP_PTP_OFSET(va));
}

/*
 * pt_entry_t *__pmap_pte_lookup(pmap_t pmap, vaddr_t va):
 *	lookup page table entry, if not allocated, returns NULL.
 */
pt_entry_t *
__pmap_pte_lookup(pmap_t pmap, vaddr_t va)
{
////	printf("%s\n", __FUNCTION__);
	pt_entry_t *ptp;

	if (pmap == pmap_kernel())
		return (__pmap_kpte_lookup(va));

	/* Lookup page table page */
	ptp = pmap->pm_ptp[__PMAP_PTP_INDEX(va)];
	if (ptp == NULL)
		return (NULL);

	return (ptp + __PMAP_PTP_OFSET(va));
}


/*
 * pt_entry_t *__pmap_kpte_lookup(vaddr_t va):
 *	kernel virtual only version of __pmap_pte_lookup().
 */
pt_entry_t *
__pmap_kpte_lookup(vaddr_t va)
{
	pt_entry_t *ptp;
//	printf("%s va=%lx, %lx\n", __FUNCTION__, va, __PMAP_PTP_INDEX(va-VM_MIN_KERNEL_ADDRESS));
	ptp = __pmap_kernel.pm_ptp[__PMAP_PTP_INDEX(va-VM_MIN_KERNEL_ADDRESS)];
	if (ptp == NULL)
		return NULL;

//	printf("%s found %p\n", __FUNCTION__, ptp + __PMAP_PTP_OFSET(va));
	return (ptp + __PMAP_PTP_OFSET(va));
}

/*
 * bool __pmap_pte_load(pmap_t pmap, vaddr_t va, int flags):
 *	lookup page table entry, if found it, load to TLB.
 *	flags specify do emulate reference and/or modified bit or not.
 */
bool
__pmap_pte_load(pmap_t pmap, vaddr_t va, int flags)
{
//	printf("%s\n", __FUNCTION__);
	struct vm_page *pg;
	pt_entry_t *pte;
	pt_entry_t entry;

	KDASSERT((((int)va < 0) && (pmap == pmap_kernel())) ||
	    (((int)va >= 0) && (pmap != pmap_kernel())));

	/* Lookup page table entry */
	if (((pte = __pmap_pte_lookup(pmap, va)) == NULL) ||
	    ((entry = *pte) == 0))
		return (false);

	KDASSERT(va != 0);

	/* Emulate reference/modified tracking for managed page. */
	if (flags != 0 && (pg = PHYS_TO_VM_PAGE(entry & PG_PPN)) != NULL) {
		struct vm_page_md *pvh = VM_PAGE_TO_MD(pg);

		if (flags & PVH_REFERENCED) {
			pvh->pvh_flags |= PVH_REFERENCED;
			entry |= PG_V;
		}
		if (flags & PVH_MODIFIED) {
			pvh->pvh_flags |= PVH_MODIFIED;
			entry |= PG_D;
		}
		*pte = entry;
	}

	/* When pmap has valid ASID, register to TLB */
	if (pmap->pm_asid != -1)
		sh_tlb_update(pmap->pm_asid, va, entry);

	return (true);
}

/*
 * int __pmap_asid_alloc(void):
 *	Allocate new ASID. if all ASID is used, steal from other process.
 */
int
__pmap_asid_alloc(void)
{
//	printf("%s\n", __FUNCTION__);
	struct proc *p;
	int i, j, k, n, map, asid;

	/* Search free ASID */
	i = __pmap_asid.hint >> 5;
	n = i + 8;
	for (; i < n; i++) {
		k = i & 0x7;
		map = __pmap_asid.map[k];
		for (j = 0; j < 32; j++) {
			if ((map & (1 << j)) == 0 && (k + j) != 0) {
				__pmap_asid.map[k] |= (1 << j);
				__pmap_asid.hint = (k << 5) + j;
//XXXNONAKA				printf("%s: %d\n", __FUNCTION__,
//XXXNONAKA				    __pmap_asid.hint);
				return (__pmap_asid.hint);
			}
		}
	}

	/* Steal ASID */
	LIST_FOREACH(p, &allproc, p_list) {
		if ((asid = p->p_vmspace->vm_map.pmap->pm_asid) > 0) {
			pmap_t pmap = p->p_vmspace->vm_map.pmap;
			pmap->pm_asid = -1;
			__pmap_asid.hint = asid;
			/* Invalidate all old ASID entry */
			sh_tlb_invalidate_asid(pmap->pm_asid);

			return (__pmap_asid.hint);
		}
	}

	panic("No ASID allocated.");
	/* NOTREACHED */
}

/*
 * void __pmap_asid_free(int asid):
 *	Return unused ASID to pool. and remove all TLB entry of ASID.
 */
void
__pmap_asid_free(int asid)
{
	int i;

	if (asid < 1)	/* Don't invalidate kernel ASID 0 */
		return;
//XXXNONAKA	printf("%s:%d\n", __FUNCTION__, asid);
	sh_tlb_invalidate_asid(asid);
//	void  sh4a_dcache_array_dump (vaddr_t va, vsize_t sz);
//	sh4a_dcache_array_dump(0, 32*1024);


	i = asid >> 5;
	__pmap_asid.map[i] &= ~(1 << (asid - (i << 5)));
}
