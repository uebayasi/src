
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

// Physical Address Space. (SH4A_EXT_ADDR32)

#ifdef SH4A_EXT_ADDR32	//32bit physical addrres mode.
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sh3/mmu_sh4a.h>

static int pmb_page_size[] = { 16, 64, 128, 512 };

struct {
    uint32_t aa;
    uint32_t da;
} pmb [] = {
    // P1
    { 0x80000000 | PMB_AA_V,
#if 1
      0x00000000 | PMB_DA_V | PMB_DA_C | PMB_DA_SZ_512M },
#else
      0x00000000 | PMB_DA_V | PMB_DA_C | PMB_DA_WT | PMB_DA_SZ_512M },// write-thru test.
#endif
    // P2
    { 0xa0000000 | PMB_AA_V,
      0x00000000 | PMB_DA_V | PMB_DA_SZ_512M },
};

void
sh4a_pmb_setup()
{
  // Change PMB setting P1/P2
  sh4a_pmb_entry_set_self (pmb[0].aa, pmb[0].da, pmb[1].aa, pmb[1].da);

  // Additional PMB.
  size_t i;
  for (i = 2; i < sizeof pmb / sizeof (pmb[0]); i++)
    {
      if (!sh4a_pmb_align_check (pmb[i].aa, pmb[i].da))
	continue;
      sh4a_pmb_entry_set (i, pmb[i].aa, pmb[i].da);
      *SH4A_MMUCR |= MMUCR_TI;	// Invalidate TLB
      CPU_SYNC_RESOURCE ();	// For MMUCR
    }
}

bool
sh4a_pmb_region_write_thru (int region)
{

	return pmb[region].da & PMB_DA_WT;
}

bool
sh4a_pmb_align_check (uint32_t aa, uint32_t da)
{
  int pgmb = pmb_page_size[_PMB_SZ_INDEX (da)];
  uint32_t mask = pgmb * 1024 * 1024 - 1;
  uint32_t vpn = aa & PMB_AA_VPN;
  uint32_t ppn = da & PMB_DA_PPN;
  bool vpn_ok = !(vpn & mask);
  bool ppn_ok = !(ppn & mask);

  printf ("PMB page size %dMB mask=%x VPN:%x%s PPN:%x%s\n", pgmb, mask,
      vpn, vpn_ok ? "" : "***NG***", ppn, ppn_ok ? "" : "***NG***");

  return vpn_ok && ppn_ok;
}

void
sh4a_pmb_entry_get (int entry, uint32_t *aa, uint32_t *da)
{
  uint32_t e = entry << PMB_AA_E_SHIFT;

  *aa = *(volatile uint32_t *)(PMB_AA | e);
  *da = *(volatile uint32_t *)(PMB_DA | e);
}

void
sh4a_pmb_entry_set (int entry, uint32_t aa, uint32_t da)
{
  uint32_t e = entry << PMB_AA_E_SHIFT;

  aa &= PMB_AA_VALID_BIT;
  da &= PMB_DA_VALID_BIT;
  // Valid bit is mapped to both AA and DA.
  aa |= PMB_AA_V;
  da &= ~PMB_DA_V;
  *(volatile uint32_t *)(PMB_DA | e) = da;
  *(volatile uint32_t *)(PMB_AA | e) = aa;
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/db_output.h>
#define	printf	db_printf
void sh4a_pmb_entry_dump (uint32_t, uint32_t);

void
sh4a_pmb_entry_dump (uint32_t aa, uint32_t da)
{

  if (!(aa & PMB_AA_V))
    return;

  printf ("VPN:%x (%c) ", aa & PMB_AA_VPN, aa & PMB_AA_V ? 'V' : '-');
  printf ("PPN:%x (%c) ", da & PMB_DA_PPN, da & PMB_DA_V ? 'V' : '-');
  printf ("%dMB, ", pmb_page_size[_PMB_SZ_INDEX (da)]);
  printf ("%sbufferd write, ", da & PMB_DA_UB ? "un" : "");
  printf ("%scacheable, ", da & PMB_DA_C ? "" : "un");
  printf ("write-%s, ", da & PMB_DA_WT ? "thru" : "back");

  printf ("\n");
}

void
sh4a_pmb_dump ()
{
  uint32_t r;
  int entry;
  int i;

  r = *PASCR;
  if (!(r & PASCR_SE))
    {
      // These are effectable for 29bit mode.
      printf ("Buffered write setting: |");
      for (i = 0; i < 8; i++)
	printf ("%c", r & (1 << i) ? 'x' : '-');
      printf ("| x:unbuffered write.\n");
    }

  for (entry = 0; entry < PMB_ENTRY; entry++)
    {
      uint32_t aa, da;
      sh4a_pmb_entry_get (entry, &aa, &da);
      sh4a_pmb_entry_dump (aa, da);
    }
}
#endif
#endif // SH4A_EXT_ADDR32
