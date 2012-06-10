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

#include "opt_sh4a_cpumode.h"
#include "opt_sh4a_input_clock.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/systm.h>
#include <sh3/7785/cpg.h>


static const int cpg_freq[] = { 0, 18 * 8, 9 * 8, 6 * 8, 9 * 4,
				3 * 8, 9 * 2, 2 * 8, 3 * 4, 9, 8, 3 * 2 };

#define	CPG_FREQ_SCALE	(SH4A_CPUMODE >= 16 ? 8/*mode 16-19*/ : 4/*mode 0-3*/)

void
cpg_dump(void)
{
	static const char *clk [] = {
	    "P", "DU", "GDTA", "DDR", "B", "SH", "U", "I"
	};
	static const int div[] = {
	    0, 2, 4, 6, 8, 12, 16, 18, 24, 32, 36, 48, -1, -1, -1, -1
	};
	uint32_t r = *FRQMR1;
	int i;

	for (i = 0; i < 8; i++) {
		int j = (r >> (i * 4)) & 0xf;
		printf("%sck 1/%d ", clk[i], div[j]);
		if (div[j] > 0)
			printf("%dMHz\n",
			    SH4A_INPUT_CLOCK / CPG_FREQ_SCALE *
			    cpg_freq[j]/ 1000000 + 1);
		else
			printf("Disabled.\n");
	}
}

uint32_t
cpg_pck(void)
{
	uint32_t r = *FRQMR1;

	uint32_t h = SH4A_INPUT_CLOCK /
	    CPG_FREQ_SCALE * cpg_freq[(r >> PFC_SHIFT) & 0xf];

	return h;
}

uint32_t
cpg_ick(void)
{
	uint32_t r = *FRQMR1;

	uint32_t h = SH4A_INPUT_CLOCK /
	    CPG_FREQ_SCALE * cpg_freq[(r >> IFC_SHIFT) & 0xf];

	return h;
}
