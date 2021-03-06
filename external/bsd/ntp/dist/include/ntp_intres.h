/*	$NetBSD: ntp_intres.h,v 1.1.1.1 2012/01/31 21:23:13 kardel Exp $	*/

#ifndef NTP_INTRES_H
#define NTP_INTRES_H

/*
 * Some systems do not support fork() and don't have an alternate
 * threads implementation of ntp_intres.  Such systems are limited
 * to using numeric IP addresses.
 */
#if defined(VMS) || defined (SYS_VXWORKS) || \
    (!defined(HAVE_WORKING_FORK) && !defined(SYS_WINNT))
#define NO_INTRES
#endif

#endif	/* !defined(NTP_INTRES_H) */
