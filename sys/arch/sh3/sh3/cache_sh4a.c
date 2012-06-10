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

#include "opt_cache.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <sh3/cache.h>
#include <sh3/cache_sh4a.h>
#ifdef SH4A_EXT_ADDR32
#include <sh3/mmu_sh4a.h>	//PMB
#endif
#include <sh3/vmparam.h>

#ifndef CPU_SYNC_RESOURCE	//XXX
#define	CPU_SYNC_RESOURCE() __asm volatile ("icbi @%0" :: "r"(0x89000000))
#endif

void
sh4a_cache_config(void)
{
	sh4a_cache_disable();
	sh4a_cache_enable(TRUE);	// write-back
//	sh4a_cache_enable(FALSE);	// write-thru
	uint32_t r = *SH4A_CCR;

	sh_cache_unified = FALSE;
	sh_cache_enable_icache = r & CCR_ICE;
	sh_cache_enable_dcache = r & CCR_OCE;
	sh_cache_ways = SH4A_CACHE_WAY;
	sh_cache_line_size = SH4A_CACHE_LINESZ;
	sh_cache_alias_mask =
	    (SH4A_DCACHE_SIZE / SH4A_CACHE_WAY - 1) & ~PAGE_MASK;
	sh_cache_prefer_mask = SH4A_DCACHE_SIZE / SH4A_CACHE_WAY - 1;
	sh_cache_write_through_p0_u0_p3 = r & CCR_WT;
#ifdef SH4A_EXT_ADDR32
	sh_cache_write_through_p1 = sh4a_pmb_region_write_thru(0);
#else
	sh_cache_write_through_p1 = !(r & CCR_CB);
#endif
	sh_cache_write_through = FALSE;//XXX
	sh_cache_ram_mode = FALSE;
	sh_cache_index_mode_icache = FALSE;
	sh_cache_index_mode_dcache = FALSE;

	sh_cache_size_dcache = SH4A_DCACHE_SIZE;
	sh_cache_size_icache = SH4A_ICACHE_SIZE;

	sh_cache_ops._icache_sync_all		= sh4a_icache_sync_all;
	sh_cache_ops._icache_sync_range		= sh4a_icache_sync_range;
	sh_cache_ops._icache_sync_range_index	= sh4a_icache_sync_range_index;

	sh_cache_ops._dcache_wbinv_all		= sh4a_dcache_wbinv_all;
	sh_cache_ops._dcache_wbinv_range	= sh4a_dcache_wbinv_range;
	sh_cache_ops._dcache_wbinv_range_index	= sh4a_dcache_wbinv_range_index;
	sh_cache_ops._dcache_inv_range		= sh4a_dcache_inv_range;
	sh_cache_ops._dcache_wb_range		= sh4a_dcache_wb_range;
}

void _cpu_run_P2(void);
void _cpu_run_P1(void);

void
sh4a_cache_enable(bool write_back)
{
  // CCR modifications must only be made by program in the P2(uncached) area.
  _cpu_run_P2();

  // Reset cache. invalidate all. CACHED DATA IS NOT WRITE-BACKED.
   *SH4A_CCR |= (CCR_ICI | CCR_OCI);

  // I/D-cache 4way, use I-cache way-prediction.
  *SH4A_RAMCR = 0;

  // Enable I/D cache
  *SH4A_CCR = CCR_ICE | CCR_OCE;
#ifndef SH4A_EXT_ADDR32	// SH4A_EXT_ADDR32 uses PMB entry WT,C bit.
  // P0,U0,P3 write-back, P1 write-back.
  if (write_back)
	  *SH4A_CCR |= CCR_CB;
  else
	  *SH4A_CCR |= CCR_WT;
#endif
  // After CCR has been updated, execute the ICBI instruction for any address.
  CPU_SYNC_RESOURCE();
  // Return to P1(cacheable) again.
  _cpu_run_P1();
}

void
sh4a_cache_disable(void)
{
  // CCR modifications must only be made by program in the P2(uncached) area.
  _cpu_run_P2();

  // Before invalidate, write-back all.
  sh4a_dcache_wbinv_all();

  // Reset cache. invalidate all.
   *SH4A_CCR |= (CCR_ICI | CCR_OCI);

  // Disable I/D cache
   *SH4A_CCR &= ~(CCR_ICE | CCR_OCE);

  // After CCR has been updated, execute the ICBI instruction for any address.
  CPU_SYNC_RESOURCE();
  // Return to P1(cacheable) again.
  _cpu_run_P1();
}

void
sh4a_icache_sync_all(void)
{
//  printf("%s\n", __FUNCTION__);

  // Memory-mapped cache array must be accessed in the P2 area program.
  _cpu_run_P2();
  int entry;
  uint32_t addr;

  // D-cache writeback invalidate.
  sh4a_dcache_wbinv_all();	// Index ops.

  // I-cache invalidate
  for (entry = 0; entry < SH4A_CACHE_ENTRY; entry++)
    {
      addr = SH4A_CCIA | (entry << CCDA_ENTRY_SHIFT);
      // non-associate. clear valid bit.
      *(volatile uint32_t *)(addr | 0) &= ~CCIA_V;
      *(volatile uint32_t *)(addr | (1 << CCIA_WAY_SHIFT)) &= ~CCIA_V;
      *(volatile uint32_t *)(addr | (2 << CCIA_WAY_SHIFT)) &= ~CCIA_V;
      *(volatile uint32_t *)(addr | (3 << CCIA_WAY_SHIFT)) &= ~CCIA_V;
    }
  CPU_SYNC_RESOURCE();
  _cpu_run_P1();
}

void
sh4a_icache_sync_range(vaddr_t va, vsize_t sz)
{
//  printf("%s\n", __FUNCTION__);

  // round/truncate with cache line.
  vsize_t eva = ROUND_CACHELINE_32(va + sz);
  va = TRUNC_CACHELINE_32(va);

  sh4a_dcache_wbinv_range(va, sz);

  while (va < eva)
    {
      // Hit invalidate. this may occur TLB miss.
      __asm volatile ("icbi @%0" :: "r"(va));
      va += SH4A_CACHE_LINESZ;	// next cache line.
    }
}

void
sh4a_icache_sync_range_index(vaddr_t va, vsize_t sz)
{
//  printf("%s\n", __FUNCTION__);

  _cpu_run_P2();
  vsize_t eva = ROUND_CACHELINE_32(va + sz);
  va = TRUNC_CACHELINE_32(va);

  if (sz > SH4A_ICACHE_SIZE)
    {
      sh4a_icache_sync_all();
      return;
    }

  sh4a_dcache_wbinv_range_index(va, sz);

  while (va < eva)
    {
      uint32_t addr = SH4A_CCIA | (va & (CCIA_ENTRY_MASK << CCIA_ENTRY_SHIFT));
      *(volatile uint32_t *)(addr | 0) &= ~CCIA_V;
      *(volatile uint32_t *)(addr | (1 << CCIA_WAY_SHIFT)) &= ~CCIA_V;
      *(volatile uint32_t *)(addr | (2 << CCIA_WAY_SHIFT)) &= ~CCIA_V;
      *(volatile uint32_t *)(addr | (3 << CCIA_WAY_SHIFT)) &= ~CCIA_V;
      va += SH4A_CACHE_LINESZ;
    }
  CPU_SYNC_RESOURCE();
  _cpu_run_P1();
}

void
sh4a_dcache_wbinv_all(void)
{
//  printf("%s\n", __FUNCTION__);

  _cpu_run_P2();
  int entry;
  uint32_t addr;

  // D-cache writeback invalidate.
  for (entry = 0; entry < SH4A_CACHE_ENTRY; entry++)
    {
      // non-associate. clear valid, dirty bit.
      addr = SH4A_CCDA | (entry << CCDA_ENTRY_SHIFT);
      // flush all way.
      *(volatile uint32_t *)(addr | 0) &= ~(CCDA_U | CCDA_V);
      *(volatile uint32_t *)(addr | (1 << CCDA_WAY_SHIFT)) &= ~(CCDA_U | CCDA_V);
      *(volatile uint32_t *)(addr | (2 << CCDA_WAY_SHIFT)) &= ~(CCDA_U | CCDA_V);
      *(volatile uint32_t *)(addr | (3 << CCDA_WAY_SHIFT)) &= ~(CCDA_U | CCDA_V);
    }
  CPU_SYNC_RESOURCE();
  _cpu_run_P1();
}

void
sh4a_dcache_wbinv_range(vaddr_t va, vsize_t sz)
{
//  printf("%s\n", __FUNCTION__);

  if (sz > SH4A_DCACHE_SIZE)
    {
      sh4a_dcache_wbinv_all();	// Index ops.
      return;
    }

  vsize_t eva = ROUND_CACHELINE_32(va + sz);
  va = TRUNC_CACHELINE_32(va);

  while (va < eva)
    {
      // Hit write-back-invalidate. this may occur TLB miss.
      __asm volatile ("ocbp @%0" :: "r"(va));
      va += SH4A_CACHE_LINESZ;	// next cache line.
    }
  __asm volatile ("synco");
}

void
sh4a_dcache_inv_range(vaddr_t va, vsize_t sz)
{
  vsize_t eva = ROUND_CACHELINE_32(va + sz);
  va = TRUNC_CACHELINE_32(va);

  while (va < eva)
    {
      // Hit invalidate. this may occur TLB miss.
#if 0
	    __asm volatile ("ocbi @%0" :: "r"(va));	//XXX SH4A bug
#else
      __asm volatile ("ocbp @%0" :: "r"(va));
#endif
      va += SH4A_CACHE_LINESZ;	// next cache line.
    }
  __asm volatile ("synco");
}

void
sh4a_dcache_wb_range(vaddr_t va, vsize_t sz)
{
//  printf("%s\n", __FUNCTION__);

  vsize_t eva = ROUND_CACHELINE_32(va + sz);
  va = TRUNC_CACHELINE_32(va);

  while (va < eva)
    {
      // Hit write-back. this may occur TLB miss.
      __asm volatile ("ocbwb @%0" :: "r"(va));
      va += SH4A_CACHE_LINESZ;	// next cache line.
    }
  __asm volatile ("synco");
}

void
sh4a_dcache_wbinv_range_index(vaddr_t va, vsize_t sz)
{
//  printf("%s\n", __FUNCTION__);

  _cpu_run_P2();
  vsize_t eva = ROUND_CACHELINE_32(va + sz);
  va = TRUNC_CACHELINE_32(va);

  if (sz > SH4A_DCACHE_SIZE)
    {
      sh4a_dcache_wbinv_all();	// Index ops.
      return;
    }

  while (va < eva)
    {
      uint32_t addr = SH4A_CCDA | (va & (CCIA_ENTRY_MASK << CCIA_ENTRY_SHIFT));
      *(volatile uint32_t *)(addr | 0) &= ~(CCDA_U | CCDA_V);
      *(volatile uint32_t *)(addr | (1 << CCIA_WAY_SHIFT)) &= ~(CCDA_U | CCDA_V);
      *(volatile uint32_t *)(addr | (2 << CCIA_WAY_SHIFT)) &= ~(CCDA_U | CCDA_V);
      *(volatile uint32_t *)(addr | (3 << CCIA_WAY_SHIFT)) &= ~(CCDA_U | CCDA_V);
      va += SH4A_CACHE_LINESZ;
    }
  CPU_SYNC_RESOURCE();
  _cpu_run_P1();
}

#include "opt_ddb.h"
#include <ddb/db_output.h>
#ifdef DDB
#define	printf	db_printf
void
sh4a_dcache_array_dump(vaddr_t va, vsize_t sz)
{
  _cpu_run_P2();
  vsize_t eva = ROUND_CACHELINE_32(va + sz);
  va = TRUNC_CACHELINE_32(va);

  size_t j, k;

  while (va < eva)
    {
      uint32_t entry = va & (CCDD_ENTRY_MASK << CCDD_ENTRY_SHIFT);
      uint32_t d0 = SH4A_CCDD | entry;
      uint32_t a0 = SH4A_CCDA | entry;
      printf ("[Entry %lx](%x)----------------------------------------------\n",
	       (va >> CCDD_ENTRY_SHIFT) & CCDD_ENTRY_MASK, entry);
      for (k = 0; k < SH4A_CACHE_WAY; k++)
	{
	  uint32_t way = k << CCDD_WAY_SHIFT;
	  uint32_t d1 = d0 | way;
	  uint32_t bit = *(volatile uint32_t *)(a0 | way);
	  printf ("way %d tag=%x (%c|%c)\n", k,
		   bit & (CCDA_TAG_MASK << CCDA_TAG_SHIFT),
		   bit & CCDA_U ? 'D' : '-',
		   bit & CCDA_V ? 'V' : '-');

	  // Dump cache line.
	  for (j = 0; j < SH4A_CACHE_LINESZ / sizeof (int32_t); j++)
	    {
	      uint32_t line = j << CCDD_LINE_SHIFT;
	      printf ("%x ",  *(volatile uint32_t *)(d1 | line));
	    }
	  printf ("\n");
	}
      va += SH4A_CACHE_LINESZ;
    }

  CPU_SYNC_RESOURCE();
  _cpu_run_P1();
}


#endif
