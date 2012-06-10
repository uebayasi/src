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
// SH7785 Timer Unit.

#ifndef _TMU_H_
#define	_TMU_H_
// Timer Start Register (0,1,2 common)
#define	TSTR0	((volatile uint8_t *)0xffd80004)
#define	 TSTR_STR0	0x1
#define	 TSTR_STR1	0x2
#define	 TSTR_STR2	0x4

// Channel 0
// Timer Constant Register
#define	TCOR0	((volatile uint32_t *)0xffd80008)
// Timer Counter
#define	TCNT0	((volatile uint32_t *)0xffd8000c)
// Timer Control Register
#define	TCR0	((volatile uint16_t *)0xffd80010)
#define	 TCR2_ICPF	0x200	// Input Captuer Interrupt Flag
#define	 TCR_UNF	0x100	// Under flow flag.
#define	 TCR2_ICPE1	0x80	// Input Captuer Control
#define	 TCR2_ICPE0	0x40
#define	  TCPE_DISABLE	0
#define	  TCPE_ENABLE	TCR2_ICPE1
#define	  TCPE_INTR	(TCR2_ICPE0 | TCR2_ICPE1)
#define	 TCR_UNIE	0x20	// Undef flow Interrupt Enable
#define	 TCR_CKEG1	0x10	// Clock Edge Select
#define	 TCR_CKEG0	0x8
#define	  CKEG_RISING_EDGE	0
#define	  CKEG_FALLING_EDGE	TCR_CKEG0
#define	  CKEG_BOTH_EDGE	TCR_CKEG1
#define	 TCR_TPSC2	0x4	// Timer Prescaler
#define	 TCR_TPSC1	0x2
#define	 TCR_TPSC0	0x1
#define	  TPSC_PCK_4	0
#define	  TPSC_PCK_16	TCR_TPSC0
#define	  TPSC_PCK_64	TCR_TPSC1
#define	  TPSC_PCK_256	(TCR_TPSC0 | TCR_TPSC1)
#define	  TPSC_PCK_1024	TCR_TPSC2
// External clock (Channel 0,1,2 only)
#define	  TPSC_PCK_EXT	(TCR_TPSC0 | TCR_TPSC1 | TCR_TPSC2)

// Channel 1
#define	TCOR1	((volatile uint32_t *)0xffd80014)
#define	TCNT1	((volatile uint32_t *)0xffd80018)
#define	TCR1	((volatile uint16_t *)0xffd8001c)
// Channel 2
#define	TCOR2	((volatile uint32_t *)0xffd80020)
#define	TCNT2	((volatile uint32_t *)0xffd80024)
#define	TCR2	((volatile uint16_t *)0xffd80028)
// Input Capture Register
#define	TCPR2	((volatile uint32_t *)0xffd8002c)	//RO

// Timer Start Register (3,4,5 common)
#define	TSTR1	((volatile uint8_t *)0xffdc0004)
#define	 TSTR_STR3	0x1
#define	 TSTR_STR4	0x2
#define	 TSTR_STR5	0x4

// Channel 3
#define	TCOR3	((volatile uint32_t *)0xffdc0008)
#define	TCNT3	((volatile uint32_t *)0xffdc000c)
#define	TCR3	((volatile uint16_t *)0xffdc0010)
// Channel 4
#define	TCOR4	((volatile uint32_t *)0xffdc0014)
#define	TCNT4	((volatile uint32_t *)0xffdc0018)
#define	TCR4	((volatile uint16_t *)0xffdc001c)
// Channel 5
#define	TCOR5	((volatile uint32_t *)0xffdc0020)
#define	TCNT5	((volatile uint32_t *)0xffdc0024)
#define	TCR5	((volatile uint16_t *)0xffdc0028)

#define	T0_BASE	0xffd80008
#define	T1_BASE	0xffd80014
#define	T2_BASE	0xffd80020
#define	T3_BASE	0xffdc0008
#define	T4_BASE	0xffdc0014
#define	T5_BASE	0xffdc0020

#define	T_COR	0
#define	T_CNT	4
#define	T_CR	8

#endif
