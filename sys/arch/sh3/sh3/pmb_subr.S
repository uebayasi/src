
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

#include <sh3/asm.h>

__KERNEL_RCSID(0, "$NetBSD$")

/*
	void sh4a_pmb_entry_set_self (uint32_t aa0, uint32_t da0, uint32_t aa1,
	uint32_t da1)
*/
NENTRY (sh4a_pmb_entry_set_self)
	mov.l	r8,	@-r15
	mov.l	r9,	@-r15
	mov.l	r10,	@-r15
	mov.l	.L_MMUCR,	r8
	mov	#4,		r9	/* MMUCR.TI*/
	mov.l	.L_PMB_AA0,	r0
	mov.l	.L_PMB_DA0,	r1
	mov.l	.L_PMB_AA1,	r2
	mov.l	.L_PMB_DA1,	r3
	mov.l	r4,	@r0
	mov.l	r5,	@r1
	mov.l	r6,	@r2
	mov.l	r7,	@r3
	/* MMUCR.TI = 1*/
	mov.l	@r8,	r10
	or	r9,	r10
	mov.l	r10,	@r8
	/* Sync resrouce.*/
	icbi	@r0
	mov.l	@r15+,	r10
	mov.l	@r15+,	r9
	rts
	 mov.l	@r15+,	r8
	.align	2
.L_MMUCR:
	.long	0xff000010
.L_PMB_AA0:
	.long	0xf6100000
.L_PMB_AA1:
	.long	0xf6100100
.L_PMB_DA0:
	.long	0xf7100000
.L_PMB_DA1:
	.long	0xf7100100
