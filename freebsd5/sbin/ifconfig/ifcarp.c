/*	$FreeBSD: src/sbin/ifconfig/ifcarp.c,v 1.2.2.1 2005/03/21 16:05:33 glebius Exp $ */
/*	from $OpenBSD: ifconfig.c,v 1.82 2003/10/19 05:43:35 mcbride Exp $ */

/*
 * Copyright (c) 2002 Michael Shalayeff. All rights reserved.
 * Copyright (c) 2003 Ryan McBride. All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <stdlib.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/ip_carp.h>
#include <net/route.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ifconfig.h"

static const char *carp_states[] = { CARP_STATES };

void
carp_status(int s, struct rt_addrinfo *info __unused)
{
	const char *state;
	struct carpreq carpr;

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		return;

	if (carpr.carpr_vhid > 0) {
		if (carpr.carpr_state > CARP_MAXSTATE)
			state = "<UNKNOWN>";
		else
			state = carp_states[carpr.carpr_state];

		printf("\tcarp: %s vhid %d advbase %d advskew %d\n",
		    state, carpr.carpr_vhid, carpr.carpr_advbase,
		    carpr.carpr_advskew);
	}

        return;

}

void
setcarp_passwd(const char *val, int d, int s, const struct afswtch *afp)
{
	struct carpreq carpr;

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	/* XXX Should hash the password into the key here, perhaps? */
	strlcpy(carpr.carpr_key, val, CARP_KEY_LEN);

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");

	return;
}

void
setcarp_vhid(const char *val, int d, int s, const struct afswtch *afp)
{
	int vhid;
	struct carpreq carpr;

	vhid = atoi(val);

	if (vhid <= 0)
		errx(1, "vhid must be greater than 0");

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	carpr.carpr_vhid = vhid;

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");

	return;
}

void
setcarp_advskew(const char *val, int d, int s, const struct afswtch *afp)
{
	int advskew;
	struct carpreq carpr;

	advskew = atoi(val);

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	carpr.carpr_advskew = advskew;

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");

	return;
}

void
setcarp_advbase(const char *val, int d, int s, const struct afswtch *afp)
{
	int advbase;
	struct carpreq carpr;

	advbase = atoi(val);

	memset((char *)&carpr, 0, sizeof(struct carpreq));
	ifr.ifr_data = (caddr_t)&carpr;

	if (ioctl(s, SIOCGVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCGVH");

	carpr.carpr_advbase = advbase;

	if (ioctl(s, SIOCSVH, (caddr_t)&ifr) == -1)
		err(1, "SIOCSVH");

	return;
}
