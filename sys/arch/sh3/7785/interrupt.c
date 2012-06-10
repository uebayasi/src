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

// SH7785 INTC module.

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/intr.h>
#include <sys/cpu.h>

#include <sh3/exception.h>
#include <sh3/7785/intc.h>

static void _intc_intr_enable(int, int);
#if 0
static void _intc_intr_disable(int);
#endif
static intr_handler_t null_intr;

struct intr_table intr_table[512];	// INTEVT: 0x1c0-0xfe0 -> 56-508

static const char *intr_names[] = {
	"0x000", "0x020", "0x040", "0x060", "0x080", "0x0a0", "0x0c0", "0x0e0",
	"0x100", "0x120", "0x140", "0x160", "0x180", "0x1a0", "0x1c0", "0x1e0",
	"0x200", "0x220", "0x240", "0x260", "0x280", "0x2a0", "0x2c0", "0x2e0",
	"0x300", "0x320", "0x340", "0x360", "0x380", "0x3a0", "0x3c0", "0x3e0",
	"0x400", "0x420", "0x440", "0x460", "0x480", "0x4a0", "0x4c0", "0x4e0",
	"0x500", "0x520", "0x540", "0x560", "TUNI0", "TUNI1", "TUNI2", "0x5e0",
	"0x600", "0x620", "0x640", "0x660", "0x680", "0x6a0", "0x6c0", "0x6e0",
	"0x700", "0x720", "0x740", "0x760", "ERI1",  "RXI1",  "BRI1",  "TXI1",
	"0x800", "0x820", "0x840", "0x860", "0x880", "0x8a0", "0x8c0", "0x8e0",
	"0x900", "0x920", "0x940", "0x960", "0x980", "0x9a0", "0x9c0", "0x9e0",
	"PCIC0", "PCIC1", "PCIC2", "PCIC3", "PCIC4", "0xaa0", "0xac0", "0xae0",
	"0xb00", "0xb20", "0xb40", "0xb60", "0xb80", "0xba0", "0xbc0", "0xbe0",
	"0xc00", "0xc20", "0xc40", "0xc60", "0xc80", "0xca0", "0xcc0", "0xce0",
	"0xd00", "0xd20", "0xd40", "0xd60", "0xd80", "0xda0", "0xdc0", "0xde0",
	"0xe00", "0xe20", "0xe40", "0xe60", "0xe80", "0xea0", "0xec0", "0xee0",
	"0xf00", "0xf20", "0xf40", "0xf60", "0xf80", "0xfa0", "0xfc0", "0xfe0",
};

void
intc_init(void)
{
	size_t i;

	for (i = 0; i < __arraycount(intr_table); i++)
		intr_table[i].func = null_intr;
}

void *
intc_intr_establish(int intevt, int trigger, int level, int (*ih_func)(void *),
    void *ih_arg)
{
	struct intr_table *p = intr_table + (intevt >> 3);

	p->func = ih_func;
	p->arg = ih_arg;
	p->level = level;
	evcnt_attach_dynamic(&p->evcnt, EVCNT_TYPE_INTR, NULL, "intc",
	    intr_names[intevt >> 5]);
	_intc_intr_enable(intevt, level);

	return p;
}

void
intc_intr_disestablish(void *arg)
{
	struct intr_table *p = (struct intr_table *)arg;

	evcnt_detach(&p->evcnt);
	p->func = null_intr;
	p->arg = NULL;
}

static void
_intc_intr_enable(int intevt, int level)
{
	int pri = (level == IPL_HIGH) ? 0x1f : (level << 1);

	switch (intevt) {
	case 0x580:	// TMU0 TUNI0
		*INT2MSKCR = 1;	// unmask TMU0-2
		// Set priority.
		*INT2PRI0 |= pri << 24;
		break;
	case 0x5a0:	// TMU1 TUNI1
		*INT2MSKCR = 1;	// unmask TMU0-2
		*INT2PRI0 |= pri << 16;
		break;
	case 0x5c0:	// TMU2 TUNI2
		*INT2MSKCR = 1;	// unmask TMU0-2
		*INT2PRI0 |= pri << 8;
		break;
	case 0x780:	// SCIF-ch1 ERI1
		*INT2MSKCR = 1 << 3;
		*INT2PRI2 |= pri << 16;
		break;
	case 0x7a0:	// SCIF-ch1 RXI1
		*INT2MSKCR = 1 << 3;
		*INT2PRI2 |= pri << 16;
		break;
	case 0x7c0:	// SCIF-ch1 BRI1
		*INT2MSKCR = 1 << 3;
		*INT2PRI2 |= pri << 16;
		break;
	case 0x7e0:	// SCIF-ch1 TXI1
		*INT2MSKCR = 1 << 3;
		*INT2PRI2 |= pri << 16;
		break;
	case 0xa00:	// PCISERR: PCIC(0)
		*INT2MSKCR = 1 << 14;
		*INT2PRI5 |= pri << 8;
		break;
	case 0xa20:	// PCIINTA: PCIC(1)
		*INT2MSKCR = 1 << 15;
		*INT2PRI5 |= pri;
		break;
	case 0xa40:	// PCIINTB: PCIC(2)
		*INT2MSKCR = 1 << 16;
		*INT2PRI6 |= pri << 24;
		break;
	case 0xa60:	// PCIINTC: PCIC(3)
		*INT2MSKCR = 1 << 17;
		*INT2PRI6 |= pri << 16;
		break;
	case 0xa80:	// PCIINTD: PCIC(4)
		*INT2MSKCR = 1 << 18;
		*INT2PRI6 |= pri << 8;
		break;
	case 0xaa0:	// PCIERR: PCIC(5)
		*INT2MSKCR = 1 << 19;
		*INT2PRI6 |= pri;
		break;
	case 0xac0:	// PCIPWD[1-3]: PCIC(5)
		*INT2MSKCR = 1 << 19;
		*INT2PRI6 |= pri;
		break;
	case 0xae0:	// PCIPWD0: PCIC(5)
		*INT2MSKCR = 1 << 19;
		*INT2PRI6 |= pri;
		break;
	}
}

#if 0
static void
_intc_intr_disable(int intevt)
{

	switch (intevt) {
	case 0x580:	// TMU0 TUNI0
		*INT2PRI0 &= ~0x1f000000;
		break;
	case 0x5a0:	// TMU1 TUNI1
		*INT2PRI0 &= ~0x001f0000;
		break;
	case 0x5c0:	// TMU2 TUNI2
		*INT2PRI0 &= ~0x00001f00;
		break;
	case 0xa00:	// PCISERR: PCIC(0)
		*INT2PRI5 &= ~0x00001f00;
		break;
	case 0xa20:	// PCIINTA: PCIC(1)
		*INT2PRI5 &= ~0x0000001f;
		break;
	case 0xa40:	// PCIINTB: PCIC(2)
		*INT2PRI6 &= ~0x1f000000;
		break;
	case 0xa60:	// PCIINTC: PCIC(3)
		*INT2PRI6 &= ~0x001f0000;
		break;
	case 0xa80:	// PCIINTD: PCIC(4)
		*INT2PRI6 &= ~0x00001f00;
		break;
	case 0xaa0:	// PCIERR: PCIC(5)
		*INT2PRI6 &= ~0x0000001f;
		break;
	case 0xac0:	// PCIPWD[1-3]: PCIC(5)
		*INT2PRI6 &= ~0x0000001f;
		break;
	case 0xae0:	// PCIPWD0: PCIC(5)
		*INT2PRI6 &= ~0x0000001f;
		break;
	}
}
#endif

bool
cpu_intr_p(void)
{

	return curcpu()->ci_idepth >= 0;
}

static int
null_intr(void *ctx)
{
	uint32_t intevt = *INTEVT;

	printf("%x %x\n", intevt, intevt >> 3);
	panic("interrupt handler missing");
	return 0;
}
