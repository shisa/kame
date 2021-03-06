/*
 * APM (Advanced Power Management) BIOS Device Driver
 *
 * Copyright (c) 1994 UKAI, Fumitoshi.
 * Copyright (c) 1994-1995 by HOSOKAWA, Tatsumi <hosokawa@jp.FreeBSD.org>
 * Copyright (c) 1996 Nate Williams <nate@FreeBSD.org>
 * Copyright (c) 1997 Poul-Henning Kamp <phk@FreeBSD.org>
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 *
 * Sep, 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 *
 *	$FreeBSD: src/sys/i386/apm/apm.h,v 1.1.2.1 1999/10/13 19:08:29 nsayer Exp $
 */

#define APM_NEVENTS 16
#define APM_NPMEV   13

/* static data */
struct apm_softc {
	int	initialized, active, bios_busy;
	int	always_halt_cpu, slow_idle_cpu;
	int	disabled, disengaged;
 	int	standby_countdown, suspend_countdown;
	u_int	minorversion, majorversion;
	u_int	cs32_base, cs16_base, ds_base;
	u_int	cs16_limit, cs32_limit, ds_limit;
	u_int	cs_entry;
	u_int	intversion;
 	u_int	standbys, suspends;
	struct apmhook sc_suspend;
	struct apmhook sc_resume;
	struct selinfo sc_rsel;
	int	sc_flags;
	int	event_count;
	int	event_ptr;
	struct	apm_event_info event_list[APM_NEVENTS];
	u_char	event_filter[APM_NPMEV];
#ifdef DEVFS
	void 	*sc_devfs_token;
#endif
};
