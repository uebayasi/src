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

// SH7785 INTC

#ifndef _INTC_H_
#define	_INTC_H_

#include <sys/evcnt.h>

#define	ICR0		((volatile uint32_t *)0xffd00000)
#define	INTPRI		((volatile uint32_t *)0xffd00010)
#define	ICR1		((volatile uint32_t *)0xffd0001c)
#define	INTREQ		((volatile uint32_t *)0xffd00024)
#define	INTMSK0		((volatile uint32_t *)0xffd00044)
#define	INTMSK1		((volatile uint32_t *)0xffd00048)
#define	INTMSKCLR0	((volatile uint32_t *)0xffd00064)
#define	INTMSKCLR1	((volatile uint32_t *)0xffd00068)
#define	INTMSK2		((volatile uint32_t *)0xffd40080)
#define	INTMSKCLR2	((volatile uint32_t *)0xffd40084)
#define	NMIFCR		((volatile uint32_t *)0xffd000c0)

#define	USERIMASK	((volatile uint32_t *)0xffd30000)

#define	INT2MSKR	((volatile uint32_t *)0xffd40038)
#define	INT2MSKCR	((volatile uint32_t *)0xffd4003c)

#define	INT2PRI0	((volatile uint32_t *)0xffd40000)
#define	INT2PRI1	((volatile uint32_t *)0xffd40004)
#define	INT2PRI2	((volatile uint32_t *)0xffd40008)
#define	INT2PRI3	((volatile uint32_t *)0xffd4000c)
#define	INT2PRI4	((volatile uint32_t *)0xffd40010)
#define	INT2PRI5	((volatile uint32_t *)0xffd40014)
#define	INT2PRI6	((volatile uint32_t *)0xffd40018)
#define	INT2PRI7	((volatile uint32_t *)0xffd4001c)
#define	INT2PRI8	((volatile uint32_t *)0xffd40020)
#define	INT2PRI9	((volatile uint32_t *)0xffd40024)

#define	INT2A0		((volatile uint32_t *)0xffd40030)
#define	INT2A1		((volatile uint32_t *)0xffd40034)
#define	INT2B0		((volatile uint32_t *)0xffd40040)
#define	INT2B1		((volatile uint32_t *)0xffd40044)
#define	INT2B2		((volatile uint32_t *)0xffd40048)
#define	INT2B3		((volatile uint32_t *)0xffd4004c)
#define	INT2B4		((volatile uint32_t *)0xffd40050)
#define	INT2B5		((volatile uint32_t *)0xffd40054)
#define	INT2B6		((volatile uint32_t *)0xffd40058)
#define	INT2B7		((volatile uint32_t *)0xffd4005c)

#define	INT2GPIC	((volatile uint32_t *)0xffd40090)

#define	EXPEVT		((volatile uint32_t *)0xff000024)
#define	INTEVT		((volatile uint32_t *)0xff000028)

__BEGIN_DECLS
typedef int (intr_handler_t)(void *);
struct intr_table {
	intr_handler_t *func;
	void *arg;
	int level;
	struct evcnt evcnt;
};
extern struct intr_table intr_table[];
__END_DECLS
#endif
