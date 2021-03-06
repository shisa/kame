/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/geom/geom_ctl.c,v 1.5 2002/11/06 20:05:15 phk Exp $
 */

#include "opt_geom.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/stdint.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <geom/geom.h>
#include <geom/geom_int.h>

static g_access_t g_ctl_access;
static g_start_t g_ctl_start;
static void g_ctl_init(void);
static d_ioctl_t g_ctl_ioctl;

struct g_class g_ctl_class = {
	"GEOMCTL",
	NULL,
	NULL,
	G_CLASS_INITIALIZER
};

DECLARE_GEOM_CLASS_INIT(g_ctl_class, g_ctl, g_ctl_init);

/*
 * We cannot do create our geom.ctl geom/provider in g_ctl_init() because
 * the event thread has to finish adding our class first and that doesn't
 * happen until later.  We know however, that the events are processed in
 * FIFO order, so scheduling g_ctl_init2() with g_call_me() is safe.
 */

static void
g_ctl_init2(void *p __unused)
{
	struct g_geom *gp;
	struct g_provider *pp;

	g_topology_assert();
	gp = g_new_geomf(&g_ctl_class, "geom.ctl");
	gp->start = g_ctl_start;
	gp->access = g_ctl_access;
	pp = g_new_providerf(gp, "%s", gp->name);
	pp->sectorsize = 512;
	g_error_provider(pp, 0);
}

static void
g_ctl_init(void)
{
	mtx_unlock(&Giant);
	g_add_class(&g_ctl_class);
	g_call_me(g_ctl_init2, NULL);
	mtx_lock(&Giant);
}

/*
 * We allow any kind of access.  Access control is handled at the devfs
 * level.
 */

static int
g_ctl_access(struct g_provider *pp, int r, int w, int e)
{
	int error;

	g_trace(G_T_ACCESS, "g_ctl_access(%s, %d, %d, %d)",
	    pp->name, r, w, e);

	g_topology_assert();
	error = 0;
	return (error);
}

static void
g_ctl_start(struct bio *bp)
{
	struct g_ioctl *gio;
	int error;

	switch(bp->bio_cmd) {
	case BIO_DELETE:
	case BIO_READ:
	case BIO_WRITE:
		error = EOPNOTSUPP;
		break;
	case BIO_GETATTR:
	case BIO_SETATTR:
		if (strcmp(bp->bio_attribute, "GEOM::ioctl") ||
		    bp->bio_length != sizeof *gio) {
			error = EOPNOTSUPP;
			break;
		}
		gio = (struct g_ioctl *)bp->bio_data;
		gio->func = g_ctl_ioctl;
		error = EDIRIOCTL;
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	g_io_deliver(bp, error);
	return;
}

/*
 * All the stuff above is really just needed to get to the stuff below
 */

static int
g_ctl_ioctl_configgeom(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct geomconfiggeom *gcp;
	struct g_configargs ga;
	int error;

	error = 0;
	bzero(&ga, sizeof ga);
	gcp = (struct geomconfiggeom *)data;
	ga.class = g_idclass(&gcp->class);
	if (ga.class == NULL)
		return (EINVAL);
	if (ga.class->config == NULL)
		return (EOPNOTSUPP);
	ga.geom = g_idgeom(&gcp->geom);
	ga.provider = g_idprovider(&gcp->provider);
	ga.len = gcp->len;
	if (gcp->len > 64 * 1024)
		return (EINVAL);
	else if (gcp->len == 0) {
		ga.ptr = NULL;
	} else {
		ga.ptr = g_malloc(gcp->len, M_WAITOK);
		error = copyin(gcp->ptr, ga.ptr, gcp->len);
		if (error) {
			g_free(ga.ptr);
			return (error);
		}
	}
	ga.flag = gcp->flag;
	error = ga.class->config(&ga);
	if (gcp->len != 0)
		copyout(ga.ptr, gcp->ptr, gcp->len);	/* Ignore error */
	gcp->class.u.id = (uintptr_t)ga.class;
	gcp->class.len = 0;
	gcp->geom.u.id = (uintptr_t)ga.geom;
	gcp->geom.len = 0;
	gcp->provider.u.id = (uintptr_t)ga.provider;
	gcp->provider.len = 0;
	return(error);
}

static int
g_ctl_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int error;

	DROP_GIANT();
	g_topology_lock();
	switch(cmd) {
	case GEOMCONFIGGEOM:
		error = g_ctl_ioctl_configgeom(dev, cmd, data, fflag, td);
		break;
	default:
		error = ENOTTY;
		break;
	}
	g_topology_unlock();
	PICKUP_GIANT();
	return (error);

}
