/*	$NetBSD: locore.S,v 1.11 2006/10/07 01:21:47 tsutsui Exp $	*/

#include "opt_cputype.h"
#include "opt_evbsh3_boardtype.h"
#if defined SH3 || defined SH4
#include "locore_sh3_sh4.S"
#elif EVBSH3_BOARDTYPE == renesas7785lcr
#include "../renesas7785lcr/locore.S"
#else
#error
#endif
