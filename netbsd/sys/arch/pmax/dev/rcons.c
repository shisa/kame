/*	$NetBSD: rcons.c,v 1.21 1999/01/28 10:35:53 jonathan Exp $	*/

/*
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ted Lemon.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "rasterconsole.h"
#if NRASTERCONSOLE > 0

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <machine/conf.h>
#include <sys/vnode.h>

#include <pmax/dev/sccreg.h>
#include <pmax/pmax/kn01.h>
#include <pmax/pmax/kn02.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/maxine.h>
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/asic.h>
#include <pmax/pmax/turbochannel.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/dev/rconsvar.h>
#include <dev/cons.h>

#include <sys/device.h>
#include <machine/fbio.h>
#include <dev/rcons/rcons.h>
#include <dev/rcons/rcons_subr.h>
#include <dev/rcons/raster.h>
#include <machine/fbvar.h>

#include <machine/pmioctl.h>
#include <pmax/dev/fbreg.h>
#include <pmax/dev/lk201var.h>



/*
 * Console I/O is redirected to the appropriate device, either a screen and
 * keyboard, a serial port, or the "virtual" console.
 */

extern struct tty *constty;	/* virtual console output device */
struct tty *fbconstty;		/* Frame buffer console tty... */
struct tty rcons_tty [NRASTERCONSOLE];	/* Console tty struct... */

struct	vnode *cn_in_devvp;	/* vnode for underlying input device. */
dev_t	cn_in_dev = NODEV;	/* console input device. */

char rcons_maxcols [20];

void	rcons_connect __P((struct fbinfo *info));
void	rasterconsoleattach __P((int n));
void	rcons_vputc __P ((dev_t dev, int c));


void	rconsreset __P((struct tty *tp, int rw));
void	rcons_input __P((dev_t dev, int ic));

#ifdef notyet
void		rconsstart		__P((struct tty *));
static void	rcons_later __P((void*));
#endif


/*
 * rcons_connect is called by fbconnect when the first frame buffer is
 * attached.   That frame buffer will always be the console frame buffer.
 */
void
rcons_connect (info)
	struct fbinfo *info;
{
	static struct rconsole rc;
	static int row, col;

	fbconstty = &rcons_tty [0];
	fbconstty->t_dev = makedev(85, 0);	/* /dev/console */
	fbconstty->t_ispeed = fbconstty->t_ospeed = TTYDEF_SPEED;
	fbconstty->t_param = (int (*)(struct tty *, struct termios *))nullop;

	/*
	 * Connect the console geometry...
	 * the part that rcons.h says
	 * "This section must be filled in by the framebugger device"
	 */
	rc.rc_width = info->fi_type.fb_width;
	rc.rc_height = info->fi_type.fb_height;
	rc.rc_depth = info->fi_type.fb_depth;
	rc.rc_pixels =info->fi_pixels;
	rc.rc_linebytes = info->fi_linebytes;
#define HW_FONT_WIDTH 8
#define HW_FONT_HEIGHT 15
	rc.rc_maxrow = rc.rc_height / HW_FONT_HEIGHT;
	rc.rc_maxcol = 80;
	rc.rc_bell = lk_bell;

	/* Initialize the state information. */
	rc.rc_bits = 0;
	rc.rc_ringing = 0;
	rc.rc_belldepth = 0;
	rc.rc_scroll = 0;
	rc.rc_p0 = 0;
	rc.rc_p1 = 0;

	/* The following two items may optionally be left zero */
	rc.rc_row = &row;
	rc.rc_col = &col;

	/* Initialize the rastercons font to something suitable for a qvss */
	rcons_font(&rc);


	row = (rc.rc_height / HW_FONT_HEIGHT) - 1;
	col = 0;

	rcons_init (&rc);


	rc.rc_xorigin = 0;
	rc.rc_yorigin = 0;
}

/* 
 * Hack around the rcons putchar interface not taking a dev_t.
 */
void
rcons_vputc(dev, c)
	dev_t dev;
	int c;
{
	/*
	 * Call the pointer-to-function that rcons_init tried to give us,
	 * discarding the dev_t argument.
	 */
	extern void rcons_cnputc __P((int c));
	rcons_cnputc(c);
}

/*
 * Set up the console device to take input from `dev'
 * and send output via rcons.
 */
void
rcons_indev(cn)
	struct consdev *cn;
{
 	register int s;

	s = spltty();

	/* Send any subsequent console calls to this cn_tab to rcons. */
	cn->cn_dev = makedev (RCONSDEV, 0);	

	/* fixup for signature mismatch. */
	cn->cn_putc = rcons_vputc;
	splx(s);
}

/*
 * Pseudo-device attach routine.  rcons doesn't need to be a pseudo
 * device, and isn't on a sparc; this is a useful point to set up
 * the vnode, clean up pmax console initialization, and set
 * the initial tty size.
 */
/* ARGSUSED */
void
rasterconsoleattach (n)
	int n;
{
	register struct tty *tp = &rcons_tty [0];

#ifdef notyet
	int status;
#endif


	/* Set up the tty queues now... */
	clalloc(&tp->t_rawq, 1024, 1);
	clalloc(&tp->t_canq, 1024, 1);
	/* output queue doesn't need quoting */
	clalloc(&tp->t_outq, 1024, 0);
#ifdef DEBUG
	printf("rconsattach: %d raster consoles\n", n);
#endif

#ifdef notyet /* ugly console input on pmaxes */

	/* Try to set up the input device... */
	if (cn_in_dev != NODEV && cn_in_devvp == NULLVP) {
		/* try to get a reference on its vnode, but fail silently */
		cdevvp(cn_in_dev, &cn_in_devvp);
		status = ((*cdevsw[major(cn_in_dev)].d_open)
			  (cn_in_dev, O_NONBLOCK, S_IFCHR, curproc)); /* XXX */
		if (status)
			printf ("rconsattach: input device open failed: %d\n",
			        status);
	}
	/* Now the input side has been opened cleanly, we can dispense
	 * with any special-case console input hacks, and point the
	 * console device at rcons.
	 */
#endif
	
}

/* ARGSUSED */
int
rconsopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp = &rcons_tty [0];
 	/*static int firstopen = 1;*/
	int status;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/*
		 * Leave baud rate alone!
		 */
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_state = TS_ISOPEN | TS_CARR_ON;
		(void)(*tp->t_param)(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);

	status = (*linesw[tp->t_line].l_open)(dev, tp);
	return status;
}

/* ARGSUSED */
int
rconsclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp = &rcons_tty [0];

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);

	return (0);
}

/* ARGSUSED */
int
rconsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp = &rcons_tty [0];

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

/* ARGSUSED */
int
rconswrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp;

	tp = &rcons_tty [0];
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
rconstty(dev)
        dev_t dev;
{
        register struct tty *tp = &rcons_tty [0];
        return (tp);
}

int
rconsioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct tty *tp;
	int error;

	tp = &rcons_tty [0];
	if ((error = linesw[tp->t_line].l_ioctl(tp, cmd, data, flag, p)) >= 0)
		return (error);
	if ((error = ttioctl(tp, cmd, data, flag, p)) >= 0)
		return (error);
	return (ENOTTY);
}

/* ARGSUSED */
void
rconsstop(tp, rw)
	struct tty *tp;
	int rw;
{

}

/*ARGSUSED*/
void
rconsreset(tp, rw)
	struct tty *tp;
	int rw;
{

}


int
rconspoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	return (ttpoll(dev, events, p));
}

/*ARGSUSED*/
int
rconsmmap (dev, off, prot)
	dev_t dev;
	 int off;
	 int prot;
{

	return -1;
}


#ifdef notyet
void
rconsstart(tp)
	struct tty *tp;
{
	struct clist *cl;
	register int s;

	s = spltty();
	if (tp->t_state & (TS_BUSY|TS_TTSTOP|TS_TIMEOUT))
		goto out;

	cl = &tp->t_outq;
	if (cl->c_cc) {
		tp->t_state |= TS_BUSY;
		if (0 /* hardware prio */) {
			/* called at level zero - update screen now. */
			(void) splsoftclock();
			/*kd_putfb(tp);*/
			(void) spltty();
			tp->t_state &= ~TS_BUSY;
		} else {
			/* called at interrupt level - do it later */
			timeout(rcons_later, (void*)tp, 0);
		}
	}
	if (cl->c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)cl);
		}
		selwakeup(&tp->t_wsel);
	}
out:
	splx(s);
}


/*
 * Timeout function to do delayed writes to the screen.
 * Called at splsoftclock when requested by rconssstart.
 */
static void
rcons_later(tpaddr)
	void *tpaddr;
{
	struct tty *tp = tpaddr;
	register int s;

#if 0
	/*XXX*/printf("rcons_later\n");
#endif

	s = spltty();
	(*(fbconstty->t_oproc)) (tp);	/* XXX */

	tp->t_state &= ~TS_BUSY;
	(*linesw[tp->t_line].l_start)(tp);
	splx(s);
}
#endif	/* notyet */


/*
 * Our "interrupt" routine for input.
 * Called by the  keyboard device driver at spltty when it has
 * input for rcons.
 */
void
rcons_input (dev, ic)
	dev_t dev;
	int ic;
{
	struct tty *tp;
	int unit = minor (dev);

#ifdef RCONS_DEBUG
	printf("rcons_input: unit %d gives %c on \n", unit, ic);
#endif

	if (unit > NRASTERCONSOLE)
		return;
	tp = &rcons_tty [unit];
	if (!(tp -> t_state & TS_ISOPEN)) {
		return;
	}
	(*linesw [tp -> t_line].l_rint)(ic, tp);
}
#endif /* NRASTERCONSOLE > 0 */
