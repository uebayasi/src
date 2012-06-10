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
// SH7785 Clock Pulse Generator

#ifndef _CPG_H_
#define	_CPG_H_
// Frequency Control Register
#define	FRQCR0		((volatile uint32_t *)0xffc80000)
#define	 FRQCR0_CODE	0xcf000000
#define	 FRQCR0_FRQE	0x1	// Frequency change sequence enable

#define	FRQCR1		((volatile uint32_t *)0xffc80004)
// CPU (Ick)
#define	 IFC_SHIFT	28
#define	 IFC3		0x80000000
#define	 IFC2		0x40000000
#define	 IFC1		0x20000000
#define	 IFC0		0x10000000
#define	  IFC_2		IFC0		// 1/2
#define	  IFC_4		IFC1		// 1/4
#define	  IFC_6		(IFC0 | IFC1)	// 1/6
// RAM (Uck)
#define	 UFC_SHIFT	24
#define	 UFC3		0x08000000
#define	 UFC2		0x04000000
#define	 UFC1		0x02000000
#define	 UFC0		0x01000000
#define	  UFC_4		UFC1		// 1/4
#define	  UFC_6		(UFC0 | UFC1)	// 1/6
// SuperHyway (SHck)
#define	 SFC_SHIFT	20
#define	 SFC3		0x00800000
#define	 SFC2		0x00400000
#define	 SFC1		0x00200000
#define	 SFC0		0x00100000
#define	  SFC_4		SFC1		// 1/4
#define	  SFC_6		(SFC0 | SFC1)	// 1/6
// Bus (Bck)
#define	 BFC_SHIFT	16
#define	 BFC3		0x00080000
#define	 BFC2		0x00040000
#define	 BFC1		0x00020000
#define	 BFC0		0x00010000
#define	  BFC_12	(BFC2 | BFC0)
#define	  BFC_16	(BFC2 | BFC1)
#define	  BFC_18	(BFC2 | BFC1 | BFC0)
#define	  BFC_24	BFC3
#define	  BFC_32	(BFC3 | BFC0)
#define	  BFC_36	(BFC3 | BFC1)
#define	  BFC_48	(BFC3 | BFC1 | BFC0)
// DDR (DDRck)
#define	 MFC_SHIFT	12
#define	 MFC3		0x00008000
#define	 MFC2		0x00004000
#define	 MFC1		0x00002000
#define	 MFC0		0x00001000
#define	  MFC_4		MFC1
#define	  MFC_6		(MFC1 | MFC0)
// GDTA (GAck)
#define	 S2FC_SHIFT	8
#define	 S2FC3		0x00000800
#define	 S2FC2		0x00000400
#define	 S2FC1		0x00000200
#define	 S2FC0		0x00000100
#define	  S2FC_8	S2FC2
#define	  S2FC_12	(S2FC2 | S2FC0)
// DU (DUck)
#define	 S3FC_SHIFT	4
#define	 S3FC3		0x00000080
#define	 S3FC2		0x00000040
#define	 S3FC1		0x00000020
#define	 S3FC0		0x00000010
#define	  S3FC_8	S3FC2
#define	  S3FC_12	(S3FC2 | S3FC0)
#define	  S3FC_16	(S3FC2 | S3FC1)
#define	  S3FC_18	(S3FC2 | S3FC1 | S3FC0)
#define	  S3FC_24	S3FC3
#define	  S3FC_32	(S3FC3 | S3FC0)
#define	  S3FC_36	(S3FC3 | S3FC1)
#define	  S3FC_48	(S3FC3 | S3FC1 | S3FC0)
// Peripheral (Pck)
#define	 PFC_SHIFT	0
#define	 PFC3		0x00000008
#define	 PFC2		0x00000004
#define	 PFC1		0x00000002
#define	 PFC0		0x00000001
#define	  PFC_8		PFC2
#define	  PFC_24	PFC3
#define	  PFC_32	(PFC3 | PFC0)
#define	  PFC_36	(PFC3 | PFC1)
#define	  PFC_48	(PFC3 | PFC1 | PFC0)

// Frequency Display Register
#define	FRQMR1		((volatile uint32_t *)0xffc80014)
// Sleep Control Register
#define	SLPCR		((volatile uint32_t *)0xffc80020)
// PLL Control Register
#define	PLLCR		((volatile uint32_t *)0xffc80024)
#define	 PLLCR_CKOFF	0x2	// CLKOUT output enable.

// Stanby Control Regsiter
#define	MSTPCR0		((volatile uint32_t *)0xffc80030)
#define	MSTPCR1		((volatile uint32_t *)0xffc80034)
// Stanby Display Register
#define	MSTPMR		((volatile uint32_t *)0xffc80044)

__BEGIN_DECLS
uint32_t cpg_pck(void);
uint32_t cpg_ick(void);
void cpg_dump(void);
__END_DECLS
#endif
