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

// SH7785 clock module. SH7785 don't have RTC module.

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/timetc.h>

#include <sh3/clock.h>
#include <sh3/7785/cpg.h>
#include <sh3/7785/tmu.h>
#include <sh3/7785/intc.h>
#include <sh3/devreg.h>

#ifndef HZ
#define	HZ	64
#endif

static vaddr_t timer_base[] = { T0_BASE, T1_BASE, T2_BASE,
				T3_BASE, T4_BASE, T5_BASE };
enum timer_counter_match {
	COUNTER_STOP,
	COUNTER_RESET,
};

enum timer_counter_unit {
	COUNTER_USEC,
	COUNTER_MSEC,
	COUNTER_RAW,
};

enum timer_channel {
	TIMER0,
	TIMER1,
	TIMER2,
	TIMER3,
	TIMER4,
	TIMER5,
};

enum timer_cmd {
	TIMER_START,
	TIMER_STOP,
	TIMER_BUSY,
};

static bool timer_start(int, uint32_t, enum timer_counter_unit,
    enum timer_counter_match, bool);
static bool tmu_start(int, enum timer_cmd);
static intr_handler_t tmu0_intr;

static u_int sh_timecounter_get(struct timecounter *);
static uint32_t udelay_param;

void
sh_clock_init(int flags)
{

	udelay_param = cpg_pck () / 1000000 / 4;
}


int
sh_clock_get_pclock(void)
{

	return cpg_pck();
}

int
sh_clock_get_cpuclock(void)
{

	return cpg_ick();
}

u_int
sh_timecounter_get(struct timecounter *tc)
{

	return 0xffffffff - *TCNT1;
}

void
setstatclockrate(int newhz)
{
}


void
cpu_initclocks(void)
{
	static struct timecounter __tc;	// don't allocate stack.

	/* Set global variables. */
	hz = HZ;
	tick = 1000000 / hz;

	// Hard clock.
	timer_start(TIMER0, 1000000 / HZ, COUNTER_USEC, COUNTER_RESET, TRUE);
//XXX	timer_start(TIMER0, 1000, COUNTER_MSEC, COUNTER_RESET, TRUE);
	intc_intr_establish(0x580, IST_LEVEL, IPL_CLOCK, tmu0_intr, 0);
//	intc_intr_enable(0x580);

	// Freerunning counter timercounter(9)
	timer_start(TIMER1, 0xffffffff, COUNTER_RAW, COUNTER_RESET, FALSE);

	__tc.tc_get_timecount = sh_timecounter_get;
	__tc.tc_frequency = cpg_pck() / 4;
	__tc.tc_name = "tmu0_pclock_4";
	__tc.tc_quality = 0;
	__tc.tc_counter_mask = 0xffffffff;
	tc_init(&__tc);
}

void
delay(int n)
{
	int s = _cpu_intr_suspend();

	*TSTR1 &= ~TSTR_STR5;	// Stop TMU5
	*TCNT5 = n * udelay_param;
	*TCOR5 = 0;
	*TCR5 = TPSC_PCK_4;
	*TSTR1 |= TSTR_STR5;	// Start TMU5
	while ((*TCR5 & TCR_UNF) == 0)
		;
	*TSTR1 &= ~TSTR_STR5;	// Stop TMU5

	_cpu_intr_resume(s);
}

int
tmu0_intr(void *arg)
{
//	printf("tmu0_intr\n");
	*TCR0 &= ~TCR_UNF;	// Clear underflow.
	hardclock(arg);

	return 0;
}

static inline void
__reg_write_4(enum timer_channel ch, uint32_t offset, uint32_t val)
{
	*(volatile uint32_t *)(timer_base[ch] + offset) = val;
}

static inline uint32_t
__reg_read_4(enum timer_channel ch, uint32_t offset)
{
	return *(volatile uint32_t *)(timer_base[ch] + offset);
}

static inline void
__reg_write_2(enum timer_channel ch, uint32_t offset, uint32_t val)
{
	*(volatile uint16_t *)(timer_base[ch] + offset) = val;
}

static inline uint16_t
__reg_read_2(enum timer_channel ch, uint32_t offset)
{
	return *(volatile uint16_t *)(timer_base[ch] + offset);
}

bool
timer_start(int channel, uint32_t interval,
    enum timer_counter_unit counter_unit,
    enum timer_counter_match counter_conf, bool interrupt)
{
	extern uint32_t udelay_param;
	uint16_t r;

	tmu_start(channel, TIMER_STOP);

	if (counter_unit != COUNTER_RAW) {
		if (counter_unit == COUNTER_MSEC)
			interval *= 1000;
		interval *= udelay_param;
	}
	__reg_write_4(channel, T_CNT, interval);
	__reg_write_4(channel, T_COR,
	    counter_conf == COUNTER_STOP ? 0 : interval);

	r = TPSC_PCK_4;
	if (interrupt)
		r |= TCR_UNIE;	// Underflow interrupt.
	__reg_write_2(channel, T_CR, r);

	return tmu_start(channel, TIMER_START);
}

bool
tmu_start(int channel, enum timer_cmd cmd)
{
	volatile uint8_t *start_reg;
	uint8_t bit;

	if (channel < 3) {
		start_reg = TSTR0;
		bit = 1 << channel;
	} else {
		start_reg = TSTR1;
		bit = 1 << (channel - 3);
	}

	switch (cmd)
	{
	case TIMER_START:
		*start_reg |= bit;
		return true;
	case TIMER_STOP:
		*start_reg &= ~bit;
		return true;
	case TIMER_BUSY:
		return *start_reg & bit;
	}

	return false;
}
