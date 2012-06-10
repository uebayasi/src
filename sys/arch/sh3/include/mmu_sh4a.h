
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

// SH4A: SH7785
// 64entry full-assciative
#ifndef _SH7785_MMU_H_
#define	_SH7785_MMU_H_
// SH7780 don't have TLB-extended mode.

// Page Table Entry Hi Register
#define	SH4A_PTEH	((volatile uint32_t *)0xff000000)
#define	 PTEH_VPN	0xfffffc00	// Virtual Page Number
#define	 PTEH_ASID	0x000000ff	// Address Space ID
#define	 PTEH_VALID_BITS	(PTEH_VPN | PTEH_ASID)

// Page Table Entry Lo Register (LDTLB)
#define	SH4A_PTEL	((volatile uint32_t *)0xff000004)
#define	 PTEL_V		0x100		// Valid
#ifdef SH4A_EXT_ADDR32
#define	 PTEL_PPN	0xfffffc00	// Pysical Page Number
#define	 PTEL_UB	0x200		// Unbuffered write.
#else
#define	 PTEL_PPN	0x1ffffc00	// 29bit
#endif

#ifndef SH4A_EXT_MMU	// Extended Mode, use Data Array 2.
#define	 PTEL_SZ1	0x80		// Page size
#define	 PTEL_PR1	0x40		// Protection key
#define	 PTEL_PR0	0x20
#define	 PTEL_SZ0	0x10
#define		PTEL_SZ_1K		0
#define		PTEL_SZ_4K		PTEL_SZ0
#define		PTEL_SZ_64K		PTEL_SZ1
#define		PTEL_SZ_1M		(PTEL_SZ0 | PTEL_SZ1)
#define		PTEL_PR_P_RDONLY	0
#define		PTEL_PR_P_RW		PTEL_PR0
#define		PTEL_PR_U_RDONLY	PTEL_PR1
#define		PTEL_PR_U_RW		(PTEL_PR0 | PTEL_PR1)
#endif	//SH4A_EXT_MMU
#define	 PTEL_C		0x8		// Cacheable
#define	 PTEL_D		0x4		// Dirty
#define	 PTEL_SH	0x2		// Shared page (igonre ASID)
#define	 PTEL_WT	0x1		// Write-thru. (0: Write-back)

#ifdef SH4A_EXT_ADDR32	// UB bit is added.
#ifdef SH4A_EXT_MMU	// page size and protection is setted by PTEA.
#define	 PTEL_VALID_BITS	(PTEL_PPN|PTEL_V|PTEL_UB|PTEL_C|PTEL_D|PTEL_SH|PTEL_WT)
#else
#define	 PTEL_VALID_BITS	(PTEL_PPN|PTEL_V|PTEL_UB|PTEL_C|PTEL_D|PTEL_SH|PTEL_WT | PTEL_SZ1|PTEL_SZ0|PTEL_PR1|PTEL_PR0)
#endif
#else	//!SH4A_EXT_ADDR32
#ifdef SH4A_EXT_MMU
#define	 PTEL_VALID_BITS	(PTEL_PPN|PTEL_V|PTEL_C|PTEL_D|PTEL_SH|PTEL_WT)
#else
#define	 PTEL_VALID_BITS	(PTEL_PPN|PTEL_V|PTEL_C|PTEL_D|PTEL_SH|PTEL_WT | PTEL_SZ1|PTEL_SZ0|PTEL_PR1|PTEL_PR0)
#endif
#endif

// Translation Table Base Register. (user register)
#define	SH4A_TTB	((volatile uint32_t *)0xff000008)

// TLB Exception Address Register.
#define	SH4A_TEA	((volatile uint32_t *)0xff00000c)

// MMU Control Register.
#define	SH4A_MMUCR	((volatile uint32_t *)0xff000010)
#define	 MMUCR_LRUI_MASK	0x3f	// ITLB LRU
#define	 MMUCR_LRUT_SHIFT	26
#define	 MMUCR_URB_MASK		0x3f	// UTLB Replace Boundary.
#define	 MMUCR_URB_SHIFT	18
#define	 MMUCR_URC_MASK		0x3f	// UTLB Replace Counter.
#define	 MMUCR_URC_SHIFT	10
#define	 MMUCR_SQMD		0x200	// Store Queue Mode. (1: priv only)
// Single Virtual Memory/ Multiple Virtual Memory mode switch
#define	 MMUCR_SV		0x100	// 1: Single virtual
#define	 MMUCR_ME		0x80	// TLB Extended mode (SH7785)
#define	 MMUCR_TI		0x4	// TLB Invalidate bit
#define	 MMUCR_AT		0x1	// Address Translation Enable bit.

#ifdef SH4A_EXT_MMU
// Page Table Entry Assistance Register. (SH7785)
// SH7780 don't have this register.
#define	SH4A_PTEA	((volatile uint32_t *)0xff000034)
//	Page Control Infomation. for TLB Extended mode; MMUCR.ME = 1
#define	 PTEA_EPR		(0x3f << 8)
#define	 PTEA_EPR_SHIFT		8
#define	 PTEA_ESZ		(0xf << 4)
#define	 PTEA_ESZ_SHIFT		4
#define	 PTEA_EPR_P_R		0x2000
#define	 PTEA_EPR_P_W		0x1000
#define	 PTEA_EPR_P_X		0x800
#define	 PTEA_EPR_U_R		0x400
#define	 PTEA_EPR_U_W		0x200
#define	 PTEA_EPR_U_X		0x100

#define	 PTEA_ESZ_1K		0
#define	 PTEA_ESZ_4K		0x10
#define	 PTEA_ESZ_8K		0x20
#define	 PTEA_ESZ_64K		0x40
#define	 PTEA_ESZ_256K		0x50
#define	 PTEA_ESZ_1M		0x70
#define	 PTEA_ESZ_4M		0x80
#define	 PTEA_ESZ_64M		0xc0
#endif	// SH4A_EXT_MMU

//////////////////////////////////////////////////////////////////////
// Physical Address Space Control Register. (reset 0x0)
#define	PASCR	((volatile uint32_t *)0xff000070)
#ifdef SH4A_EXT_ADDR32
//	16entry Privilege-area Mapping Buffer.
#define	PMB_ENTRY	16

#define	PMB_AA		0xf6100000
#define	 PMB_AA_E_MASK		0xf
#define	 PMB_AA_E_SHIFT		8
#define	 PMB_AA_V		0x100
#define	 PMB_AA_VPN		0xff000000
#define	 PMB_AA_VPN_MASK	0xff
#define	 PMB_AA_VPN_SHIFT	24

#define	 PMB_AA_VALID_BIT	(PMB_AA_VPN|PMB_AA_V)

#define	PMB_DA		0xf7100000
#define	 PMB_DA_E_MASK	0xf
#define	 PMB_DA_E_SHIFT	8

#define	 PMB_DA_PPN		0xff000000
#define	 PMB_DA_PPN_MASK	0xff
#define	 PMB_DA_PPN_SHIFT	24

#define	 PMB_DA_SZ_MASK	0x3	// Page size

#define	 PMB_DA_WT		0x1	// Write-thru.
#define	 PMB_DA_C		0x8	// Cacheable.
#define	 PMB_DA_V		0x100	// Valid
#define	 PMB_DA_UB		0x200	// Buffered write.
#define	 PMB_DA_SZ0		0x10
#define	 PMB_DA_SZ1		0x80
#define	 PMB_DA_SZ_16M		0
#define	 PMB_DA_SZ_64M		PMB_DA_SZ0
#define	 PMB_DA_SZ_128M		PMB_DA_SZ1
#define	 PMB_DA_SZ_512M		(PMB_DA_SZ0 | PMB_DA_SZ1)
#define	 _PMB_SZ_INDEX(x)	(((da & PMB_DA_SZ0) >> 4) | \
				 ((da & PMB_DA_SZ1) >> 6))

#define	 PMB_DA_VALID_BIT	(PMB_DA_PPN|PMB_DA_WT|PMB_DA_C|PMB_DA_V| \
				 PMB_DA_SZ0|PMB_DA_SZ1|PMB_DA_UB)


// 0: 29bit physical address, 1: 32bit physical address.
#define	 PASCR_SE	0x80000000
#else	// 29bit address.
// 0: Buffered write, 1: Unbuffered write.
// When 32bit address exetended mode, these bits disabled.
#define	 PASCR_UB0	0x01
#define	 PASCR_UB1	0x02
#define	 PASCR_UB2	0x04
#define	 PASCR_UB3	0x08
#define	 PASCR_UB4	0x10
#define	 PASCR_UB5	0x20
#define	 PASCR_UB6	0x40
#endif	// SH4A_EXT_ADDR32
#define	 PASCR_UB7	0x80


//////////////////////////////////////////////////////////////////////
// Instruction Re-Fetch Inhibit Control Register.
#define	IRMCR	((volatile uint32_t *)0xff000078)
// MMUCR, PACCR, CCR, RAMCR, PTEH
#define	 IRMCR_R2	0x10
// 0xff200000-0xff2fffff
#define	 IRMCR_R1	0x08
// LDTLB
#define	 IRMCR_LT	0x04
// Memory-maped TLB
#define	 IRMCR_MT	0x02
// Memory-maped CACHE
#define	 IRMCR_MC	0x01

//////////////////////////////////////////////////////////////////////
// Memory mapped TLB
// ITLB 4entry
// UTLB 64entry
//
// ITLB Address Array
#define	SH4A_ITLB_AA		0xf2000000
#define	SH4A_ITLB_ENTRY		4
#define	 ITLB_AA_E_MASK		0x3	// Entry
#define	 ITLB_AA_E_SHIFT	8
#define	 ITLB_AA_VPN		0xfffffc00
#define	 ITLB_AA_V		0x100	// Valid
#define	 ITLB_AA_ASID		0xff
// ITLB Data Array
#define	SH4A_ITLB_DA1		0xf3000000
#define	 ITLB_DA1_E_MASK	0x3	// Entry
#define	 ITLB_DA1_E_SHIFT	8
#ifdef SH4A_EXT_ADDR32
#define	 ITLB_DA1_PPN		0xfffffc00	// 32bit
#else
#define	 ITLB_DA1_PPN		0x1ffffc00	// 29bit
#endif
#define	 ITLB_DA1_V		0x100	// Valid.
#define	 ITLB_DA1_C		0x8	// Cacheable.
#define	 ITLB_DA1_SH		0x2	// Shared page.
#ifndef SH4A_EXT_MMU	// TLB Extended mode uses 'ITLB Data Array 2'
#define	 ITLB_PR		0x40	// User/Privilege
#define	 ITLB_SZ0		0x10
#define	 ITLB_SZ1		0x80
#define		ITLB_SZ_1K	0
#define		ITLB_SZ_4K	ITLB_SZ0
#define		ITLB_SZ_64K	ITLB_SZ1
#define		ITLB_SZ_1M	(ITLB_SZ0 | ITLB_SZ1)
#endif	//!SH4A_EXT_MMU

// UTLB Address Array
#define	SH4A_UTLB_AA		0xf6000000
#define	SH4A_UTLB_ENTRY		64
#define	 UTLB_AA_E_MASK		0x3f	// Entry
#define	 UTLB_AA_E_SHIFT	8
#define	 UTLB_AA_A		0x80	// Associate bit
#define	 UTLB_AA_VPN		0xfffffc00
#define	 UTLB_AA_D		0x200	// Dirty
#define	 UTLB_AA_V		0x100	// Valid
#define	 UTLB_AA_ASID		0xff	// Address Space ID.

// UTLB Data Array
#define	SH4A_UTLB_DA1		0xf7000000
#define	 UTLB_DA1_E_MASK	0x3f	// Entry
#define	 UTLB_DA1_E_SHIFT	8
#define	 UTLB_DA1_V		0x100	// Valid
#ifdef SH4A_EXT_ADDR32
#define	 UTLB_DA1_PPN		0xfffffc00	// 32bit
#define	 UTLB_DA1_UB		0x200	// UnBuffered write.
#else
#define	 UTLB_DA1_PPN		0x1ffffc00	// 29bit
#endif
#ifndef SH4A_EXT_MMU
#define	 UTLB_PR0		0x20
#define	 UTLB_PR1		0x40
#define	 UTLB_SZ0		0x10
#define	 UTLB_SZ1		0x80
#define		UTLB_SZ_1K		0
#define		UTLB_SZ_4K		UTLB_SZ0
#define		UTLB_SZ_64K		UTLB_SZ1
#define		UTLB_SZ_1M		(UTLB_SZ0 | UTLB_SZ1)
#define		UTLB_PR_P_RDONLY	0
#define		UTLB_PR_P_RW		UTLB_PR0
#define		UTLB_PR_U_RDONLY	UTLB_PR1
#define		UTLB_PR_U_RW		(UTLB_PR0 | UTLB_PR1)
#endif	//!SH4A_EXT_MMU
#define	 UTLB_DA1_C		0x8	// Cacheable.
#define	 UTLB_DA1_D		0x4
#define	 UTLB_DA1_SH		0x2	// Shared page.
#define	 UTLB_DA1_WT		0x1	// Write Thru.

#ifdef SH4A_EXT_MMU
#define	SH4A_ITLB_DA2		0xf3800000
#define	 ITLB_DA2_E_MASK	0x3	// Entry
#define	 ITLB_DA2_E_SHIFT	8

#define	 ITLB_DA2_VALID_MASK	0x2df0
#define	 ITLB_DA2_EPR_P_R	0x2000
#define	 ITLB_DA2_EPR_P_X	0x800
#define	 ITLB_DA2_EPR_U_R	0x400
#define	 ITLB_DA2_EPR_U_X	0x100
#define	 ITLB_DA2_ESZ_1K	0
#define	 ITLB_DA2_ESZ_4K	0x10
#define	 ITLB_DA2_ESZ_8K	0x20
#define	 ITLB_DA2_ESZ_64K	0x40
#define	 ITLB_DA2_ESZ_256K	0x50
#define	 ITLB_DA2_ESZ_1M	0x70
#define	 ITLB_DA2_ESZ_4M	0x80
#define	 ITLB_DA2_ESZ_64M	0xc0

#define	SH4A_UTLB_DA2		0xf7800000
#define	 UTLB_DA2_E_MASK	0x3f	// Entry
#define	 UTLB_DA2_E_SHIFT	8

#define	 UTLB_DA2_VALID_MASK	0x3ff0
#define	 UTLB_DA2_EPR_P_R	0x2000
#define	 UTLB_DA2_EPR_P_W	0x1000
#define	 UTLB_DA2_EPR_P_X	0x800
#define	 UTLB_DA2_EPR_U_R	0x400
#define	 UTLB_DA2_EPR_U_W	0x200
#define	 UTLB_DA2_EPR_U_X	0x100
#define	 UTLB_DA2_ESZ_1K	0
#define	 UTLB_DA2_ESZ_4K	0x10
#define	 UTLB_DA2_ESZ_8K	0x20
#define	 UTLB_DA2_ESZ_64K	0x40
#define	 UTLB_DA2_ESZ_256K	0x50
#define	 UTLB_DA2_ESZ_1M	0x70
#define	 UTLB_DA2_ESZ_4M	0x80
#define	 UTLB_DA2_ESZ_64M	0xc0
#endif

__BEGIN_DECLS
enum mmu_cache
{
  UNCACHED,
  WRITEBACK,
  WRITETHRU,
};
enum mmu_page_size
  {
    SH4A_PG_1K,
    SH4A_PG_4K,
    SH4A_PG_64K,
    SH4A_PG_1M,
#ifdef SH4A_EXT_MMU
    SH4A_PG_8K,
    SH4A_PG_256K,
    SH4A_PG_4M,
    SH4A_PG_64M,
#endif
  };
enum mmu_protect
  {
    P_RDONLY,
    P_RW,
    U_RDONLY,
    U_RW,
  };

struct tlb_entry
{

  vaddr_t vpn;
  paddr_t ppn;
  int asid;
  enum mmu_cache cache;
  enum mmu_page_size size;
  enum mmu_protect protect;
  bool shared;
};

void sh4a_mmu_information (void);

#ifdef SH4A_EXT_ADDR32
void sh4a_pmb_setup (void);
bool sh4a_pmb_region_write_thru (int);
void sh4a_pmb_dump (void);
void sh4a_pmb_entry_get (int, uint32_t *, uint32_t *);
void sh4a_pmb_entry_set (int, uint32_t, uint32_t);
void sh4a_pmb_entry_set_self (uint32_t, uint32_t, uint32_t, uint32_t);
bool sh4a_pmb_align_check (uint32_t, uint32_t);
#endif

void sh4a_mmu_itlb_dump (void);
void sh4a_mmu_utlb_dump (void);
void sh4a_mmu_dump (void);

void sh4a_mmu_start (void);
void sh4a_tlb_clear_all (void);
void sh4a_tlb_wired_entry (struct tlb_entry *);
// TLB entry helper functions.
void sh4a_tlb_entry_pagesize (int,  uint32_t *, uint32_t *);
void sh4a_tlb_entry_protect (int,  uint32_t *, uint32_t *);
void sh4a_tlb_entry_cache (int,  uint32_t *, uint32_t *);


// ASID
void sh4a_mmu_asid_set (uint32_t);
uint32_t sh4a_mmu_asid_get (void);
// Wired entry.
void sh4a_mmu_wired_set (int);
uint32_t sh4a_mmu_wired_get (void);

// TLB entry couter.
void sh4a_tlb_counter_incr (void);
void sh4a_tlb_counter_set (uint32_t);
uint32_t sh4a_tlb_counter_get (void);

// Software bits.
void sh4a_mmu_decode_swbits (uint32_t, uint32_t *, uint32_t *);
uint32_t sh4a_mmu_encode_swbits (enum mmu_page_size, enum mmu_protect, uint32_t);

void sh4a_tlb_invalidate_addr (int, vaddr_t);
void sh4a_tlb_invalidate_asid (int);
void sh4a_tlb_invalidate_all (void);
void sh4a_tlb_update(int, vaddr_t, uint32_t);


int sh4a_wired_counter_alloc (vaddr_t);
void sh4a_wired_counter_dealloc (vaddr_t);
void sh4a_tlb_update_wired(int, vaddr_t, uint32_t);


__END_DECLS

#define	CPU_SYNC_RESOURCE() __asm volatile ("icbi @%0" :: "r"(0x89000000))
#endif
