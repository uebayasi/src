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

#include <sh3/asm.h>

__KERNEL_RCSID(0, "$NetBSD$")

NENTRY(start)
#if 0
1:	mov.l	.L_led, r0
	mov	#0x4,	r1
	mov.l	r1,	@r0
#if 0
	bra	1b
	 nop
#endif
#endif

	/* Change BANK0 */
	stc	sr,	r1
	mov.l	.L_SR_RB0, r0
	and	r0,	r1
	ldc	r1,	sr

	mov.l	.L_machine_startup, r0
	mov.l	.L_stack_start, r15
	jmp	@r0
	 nop

	/* NOTREACHED */
	.align	2
#if 0
.L_led:
	.long	0xa4000008
#endif
.L_machine_startup:
	.long	machine_startup
.L_stack_start:
	.long	0x89000000
.L_SR_RB0:
	.long	0xdfffffff

/*XXX*/
NENTRY(_cpu_run_P2)
	sts	pr,	r0
	mov.l	.L_P2,	r1
	or	r1,	r0
	lds	r0,	pr
	rts
	 nop

.L_P2:	.long	0x20000000

NENTRY(_cpu_run_P1)
	sts	pr,	r0
	mov.l	.L_P1,	r1
	and	r1,	r0
	lds	r0,	pr
	rts
	 nop

.L_P1:	.long	0xdfffffff
