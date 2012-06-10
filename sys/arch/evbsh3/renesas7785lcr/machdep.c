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

#include "opt_ddb.h"
#include "opt_kgdb.h"

#include <sys/param.h>
#include <sys/systm.h>
//#include <sys/user.h>
#include <sys/reboot.h>
#include <sys/lwp.h>
#include <uvm/uvm_extern.h>

#include <dev/cons.h>
#include <sh3/intr.h>
#include <sh3/7785/intc.h>
#include <sh3/pcb.h>
#include <evbsh3/renesas7785lcr/reg.h>

#include <dev/cons.h>

char machine[] = MACHINE;
char machine_arch[] = MACHINE_ARCH;

void led(uint8_t);
uint8_t dip_switch(void);

void machine_init(void);
void machine_startup(void) __dead;
void main(void) __dead;

void
machine_init(void)
{

	/* Nothing to do */
}

void
machine_startup(void)
{
	extern char edata[], end[];

	/* Clear bss. */
	memset(edata, 0, end - edata);

	consinit();

	/* Initialize Cache, MMU, Exception handler. */
	sh_cpu_init(CPU_ARCH_SH4A, CPU_PRODUCT_7785);

	/* Load memory to UVM */
	struct {
		int kernel;
		paddr_t addr;
		psize_t size;
		int list;
	} ram_region [] = {
#ifdef SH4A_EXT_ADDR32
		/* 32-bit mode */
		// Load all memory.
		{ FALSE, 0x08000000 + 0x1000, 0x1000000 - 0x1000,
		  VM_FREELIST_P1ACCESS },//SKIP VBR area 0x08fff000
		{ TRUE,  0x09000000, 0x07000000, VM_FREELIST_P1ACCESS },
#if 1
#if 1
		{ FALSE, 0x40000000, 0x08000000, VM_FREELIST_DEFAULT },
		{ FALSE, 0x50000000, 0x10000000, VM_FREELIST_DEFAULT },
#else
		{ FALSE, 0x40000000, 0x01000000, VM_FREELIST_DEFAULT },
		{ FALSE, 0x50000000, 0x0b007000, VM_FREELIST_DEFAULT },
#endif
#endif
#else
		/* 29-bit mode */

		// Load memory mapped P1/P2.
		//{ FALSE, 0x08000000 + 0x1000, 0x01000000 - 0x1000,
		{ FALSE, 0x08000000 + 0x1000, 0x00400000 - 0x1000,//4MB
		  VM_FREELIST_DEFAULT },//SKIP VBR XXX see sh3_machdep.c

		// DEFTEXTADDR=0x89000000
		//{ TRUE,  0x09000000, 0x07000000, VM_FREELIST_DEFAULT },
#endif
	}, *p;
	vaddr_t kernend = atop(round_page(SH3_P1SEG_TO_PHYS(end)));

	physmem = 0;
	int i;
	for (i = 0, p = ram_region;
	    i < sizeof ram_region / sizeof(ram_region[0]); i++, p++) {
		uvm_page_physload(atop(p->addr), atop(p->addr + p->size),
		    p->kernel ? kernend : atop(p->addr), atop(p->addr + p->size),
		    p->list);
		physmem += atop(p->size);
		printf ("load %lx-%lx\n", p->addr, p->addr + p->size);
	}

	// Initialize lwp0 u-area. (construct the 1st switch frame).
	sh_proc0_init();

	/* Initialize pmap and start to address translation */
	pmap_bootstrap();

//	pmap_growkernel(VM_MAX_KERNEL_ADDRESS);//XXX

//XXXUEBS
extern int boothowto;
boothowto |= RB_SINGLE;

{
	struct pcb *pcb = lwp_getpcb(&lwp0);
	struct switchframe *sf = &pcb->pcb_sf;
	// Jump to main(). use lwp0 kernel stack.
	__asm volatile (
		"jmp	@%0;"
		"mov	%1, r15"
		:: "r"(main),"r"(sf->sf_r7_bank));
}
	// NOTREACHED
}

void
consinit(void)
{
	static int initted;

	if (initted++)
		return;

	cninit();
}

void
cpu_startup(void)
{

	sh_startup();
	printf("Renesas evaluation board R0P7785LC0011RL\n");
}

void
cpu_reboot(int howto, char *bootstr)
{
	if (howto & RB_HALT) {
		*POFCR = 1;	// Shutdown board.
	} else {
		// Reset board peripherals.
		*LOCALCR = 1;
		delay(100);	// at least 10us.
		cpu_reset();
	}
	while (/*CONSTCOND*/1)
		;
	// NOTREACHED
}

// Called from sh_vector_interrupt()
void
intc_intr(int ssr, int spc, int ssp)
{
//XXXNONAKA	static uint8_t heartbeat;
	uint32_t intevc = *INTEVT;
	struct intr_table *handler = intr_table + (intevc >> 3);

	handler->evcnt.ev_count++;
//	printf ("%d<=\n", handler->level);
	if (intevc == 0x580) {	//TMU0 TUNI0
		struct clockframe cf;
		cf.spc = spc;
		cf.ssr = ssr;
		cf.ssp = ssp;
		handler->func(&cf);
	} else {

//		printf("device\n");
		handler->func(handler->arg);
//		printf("device done\n");
	}

//XXXNONAKA	led(heartbeat ^= 1);
}

// Renesas evaluation board R0P7785LC0011RL specific functions.
uint8_t
dip_switch(void)
{

	return *SWSR;
}

void
led(uint8_t r)
{

	*LEDCR = r;
}
