/*	$OpenBSD: subr_prf.c,v 1.25 1999/01/11 05:12:23 millert Exp $	*/
/*	$NetBSD: subr_prf.c,v 1.45 1997/10/24 18:14:25 chuck Exp $	*/

/*-
 * Copyright (c) 1986, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)subr_prf.c	8.3 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/tty.h>
#include <sys/tprintf.h>
#include <sys/syslog.h>
#include <sys/malloc.h>

#include <dev/cons.h>

/*
 * note that stdarg.h and the ansi style va_start macro is used for both
 * ansi and traditional c complers.
 * XXX: this requires that stdarg.h define: va_alist and va_dcl
 */
#include <machine/stdarg.h>

#ifdef KGDB
#include <sys/kgdb.h>
#include <machine/cpu.h>
#endif
#ifdef DDB
#include <ddb/db_output.h>	/* db_printf, db_putchar prototypes */
extern	int db_radix;		/* XXX: for non-standard '%r' format */
#endif


/*
 * defines
 */

/* flags for kprintf */
#define TOCONS		0x01	/* to the console */
#define TOTTY		0x02	/* to the process' tty */
#define TOLOG		0x04	/* to the kernel message buffer */
#define TOBUFONLY	0x08	/* to the buffer (only) [for sprintf] */
#define TODDB		0x10	/* to ddb console */

/* max size buffer kprintf needs to print quad_t [size in base 8 + \0] */
#define KPRINTF_BUFSIZE		(sizeof(quad_t) * NBBY / 3 + 2)


/*
 * local prototypes
 */

static int	 kprintf __P((const char *, int, struct tty *, 
				char *, va_list));
static void	 putchar __P((int, int, struct tty *));


/*
 * globals
 */

struct	tty *constty;	/* pointer to console "window" tty */
int	consintr = 1;	/* ok to handle console interrupts? */
extern	int log_open;	/* subr_log: is /dev/klog open? */
const	char *panicstr; /* arg to first call to panic (used as a flag
			   to indicate that panic has already been called). */
#ifdef DDB
int	db_panic = 1;
int	db_console = 0;
#endif

/*
 * v_putc: routine to putc on virtual console
 *
 * the v_putc pointer can be used to redirect the console cnputc elsewhere
 * [e.g. to a "virtual console"].
 */

void (*v_putc) __P((int)) = cnputc;	/* start with cnputc (normal cons) */


/*
 * functions
 */

/*
 *      Partial support (the failure case) of the assertion facility
 *      commonly found in userland.
 */
void
__assert(t, f, l, e)
	const char *t, *f, *e;
	int l;
{

	panic("kernel %sassertion \"%s\" failed: file \"%s\", line %d",
		t, e, f, l);
}

/*
 * tablefull: warn that a system table is full
 */

void
tablefull(tab)
	const char *tab;
{
	log(LOG_ERR, "%s: table is full\n", tab);
}

/*
 * panic: handle an unresolvable fatal error
 *
 * prints "panic: <message>" and reboots.   if called twice (i.e. recursive
 * call) we avoid trying to sync the disk and just reboot (to avoid 
 * recursive panics).
 */

void
#ifdef __STDC__
panic(const char *fmt, ...)
#else
panic(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	static char panicbuf[512];
	int bootopt;
	va_list ap;

	bootopt = RB_AUTOBOOT | RB_DUMP;
	va_start(ap, fmt);
	if (panicstr)
		bootopt |= RB_NOSYNC;
	else {
		vsprintf(panicbuf, fmt, ap);
		panicstr = panicbuf;
	}

	printf("panic: ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);

#ifdef KGDB
	kgdb_panic();
#endif
#ifdef KADB
	if (boothowto & RB_KDB)
		kdbpanic();
#endif
#ifdef DDB
	if (db_panic)
		Debugger();
#endif
	boot(bootopt);
}

/*
 * kernel logging functions: log, logpri, addlog
 */

/*
 * log: write to the log buffer
 *
 * => will not sleep [so safe to call from interrupt]
 * => will log to console if /dev/klog isn't open
 */

void
#ifdef __STDC__
log(int level, const char *fmt, ...)
#else
log(level, fmt, va_alist)
	int level;
	char *fmt;
	va_dcl
#endif
{
	register int s;
	va_list ap;

	s = splhigh();
	logpri(level);		/* log the level first */
	va_start(ap, fmt);
	kprintf(fmt, TOLOG, NULL, NULL, ap);
	splx(s);
	va_end(ap);
	if (!log_open) {
		va_start(ap, fmt);
		kprintf(fmt, TOCONS, NULL, NULL, ap);
		va_end(ap);
	}
	logwakeup();		/* wake up anyone waiting for log msgs */
}

/*
 * logpri: log the priority level to the klog
 */

void			/* XXXCDC: should be static? */
logpri(level)
	int level;
{
	char *p;
	char snbuf[KPRINTF_BUFSIZE];

	putchar('<', TOLOG, NULL);
	sprintf(snbuf, "%d", level);
	for (p = snbuf ; *p ; p++)
		putchar(*p, TOLOG, NULL);
	putchar('>', TOLOG, NULL);
}

/*
 * addlog: add info to previous log message
 */

int
#ifdef __STDC__
addlog(const char *fmt, ...)
#else
addlog(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	register int s;
	va_list ap;

	s = splhigh();
	va_start(ap, fmt);
	kprintf(fmt, TOLOG, NULL, NULL, ap);
	splx(s);
	va_end(ap);
	if (!log_open) {
		va_start(ap, fmt);
		kprintf(fmt, TOCONS, NULL, NULL, ap);
		va_end(ap);
	}
	logwakeup();
	return(0);
}


/*
 * putchar: print a single character on console or user terminal.
 *
 * => if console, then the last MSGBUFS chars are saved in msgbuf
 *	for inspection later (e.g. dmesg/syslog)
 */
static void
putchar(c, flags, tp)
	register int c;
	int flags;
	struct tty *tp;
{
	extern int msgbufmapped;
	struct msgbuf *mbp;

	if (panicstr)
		constty = NULL;
	if ((flags & TOCONS) && tp == NULL && constty) {
		tp = constty;
		flags |= TOTTY;
	}
	if ((flags & TOTTY) && tp && tputchar(c, tp) < 0 &&
	    (flags & TOCONS) && tp == constty)
		constty = NULL;
	if ((flags & TOLOG) &&
	    c != '\0' && c != '\r' && c != 0177 && msgbufmapped) {
		mbp = msgbufp;
		if (mbp->msg_magic != MSG_MAGIC) {
			bzero((caddr_t) mbp, sizeof(*mbp));
			mbp->msg_magic = MSG_MAGIC;
		}
		mbp->msg_bufc[mbp->msg_bufx++] = c;
		if (mbp->msg_bufx < 0 || mbp->msg_bufx >= MSG_BSIZE)
			mbp->msg_bufx = 0;
	}
	if ((flags & TOCONS) && constty == NULL && c != '\0')
		(*v_putc)(c);
#ifdef DDB
	if (flags & TODDB)
		db_putchar(c);
#endif
}


/*
 * uprintf: print to the controlling tty of the current process
 *
 * => we may block if the tty queue is full
 * => no message is printed if the queue doesn't clear in a reasonable
 *	time
 */

void
#ifdef __STDC__
uprintf(const char *fmt, ...)
#else
uprintf(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	register struct proc *p = curproc;
	va_list ap;

	if (p->p_flag & P_CONTROLT && p->p_session->s_ttyvp) {
		va_start(ap, fmt);
		kprintf(fmt, TOTTY, p->p_session->s_ttyp, NULL, ap);
		va_end(ap);
	}
}

/*
 * tprintf functions: used to send messages to a specific process
 *
 * usage:
 *   get a tpr_t handle on a process "p" by using "tprintf_open(p)"
 *   use the handle when calling "tprintf"
 *   when done, do a "tprintf_close" to drop the handle
 */

/*
 * tprintf_open: get a tprintf handle on a process "p"
 *
 * => returns NULL if process can't be printed to
 */

tpr_t
tprintf_open(p)
	register struct proc *p;
{

	if (p->p_flag & P_CONTROLT && p->p_session->s_ttyvp) {
		SESSHOLD(p->p_session);
		return ((tpr_t) p->p_session);
	}
	return ((tpr_t) NULL);
}

/*
 * tprintf_close: dispose of a tprintf handle obtained with tprintf_open
 */

void
tprintf_close(sess)
	tpr_t sess;
{

	if (sess)
		SESSRELE((struct session *) sess);
}

/*
 * tprintf: given tprintf handle to a process [obtained with tprintf_open], 
 * send a message to the controlling tty for that process.
 *
 * => also sends message to /dev/klog
 */
void
#ifdef __STDC__
tprintf(tpr_t tpr, const char *fmt, ...)
#else
tprintf(tpr, fmt, va_alist)
	tpr_t tpr;
	char *fmt;
	va_dcl
#endif
{
	register struct session *sess = (struct session *)tpr;
	struct tty *tp = NULL;
	int flags = TOLOG;
	va_list ap;

	logpri(LOG_INFO);
	if (sess && sess->s_ttyvp && ttycheckoutq(sess->s_ttyp, 0)) {
		flags |= TOTTY;
		tp = sess->s_ttyp;
	}
	va_start(ap, fmt);
	kprintf(fmt, flags, tp, NULL, ap);
	va_end(ap);
	logwakeup();
}


/*
 * ttyprintf: send a message to a specific tty
 *
 * => should be used only by tty driver or anything that knows the
 *	underlying tty will not be revoked(2)'d away.  [otherwise,
 *	use tprintf]
 */
void
#ifdef __STDC__
ttyprintf(struct tty *tp, const char *fmt, ...)
#else
ttyprintf(tp, fmt, va_alist)
	struct tty *tp;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

	va_start(ap, fmt);
	kprintf(fmt, TOTTY, tp, NULL, ap);
	va_end(ap);
}

#ifdef DDB

/*
 * db_printf: printf for DDB (via db_putchar)
 */

int
#ifdef __STDC__
db_printf(const char *fmt, ...)
#else
db_printf(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = kprintf(fmt, TODDB, NULL, NULL, ap);
	va_end(ap);
	return(retval);
}

#endif /* DDB */


/*
 * normal kernel printf functions: printf, vprintf, sprintf
 */

/*
 * printf: print a message to the console and the log
 */
int
#ifdef __STDC__
printf(const char *fmt, ...)
#else
printf(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int savintr, retval;

	savintr = consintr;		/* disable interrupts */
	consintr = 0;
	va_start(ap, fmt);
	retval = kprintf(fmt, TOCONS | TOLOG, NULL, NULL, ap);
	va_end(ap);
	if (!panicstr)
		logwakeup();
	consintr = savintr;		/* reenable interrupts */
	return(retval);
}

/*
 * vprintf: print a message to the console and the log [already have a
 *	va_list]
 */

void
vprintf(fmt, ap)
	const char *fmt;
	va_list ap;
{
	int savintr;

	savintr = consintr;		/* disable interrupts */
	consintr = 0;
	kprintf(fmt, TOCONS | TOLOG, NULL, NULL, ap);
	if (!panicstr)
		logwakeup();
	consintr = savintr;		/* reenable interrupts */
}

/*
 * sprintf: print a message to a buffer
 */
int
#ifdef __STDC__
sprintf(char *buf, const char *fmt, ...)
#else
sprintf(buf, fmt, va_alist)
        char *buf;
        const char *cfmt;
        va_dcl
#endif
{
	int retval;
	va_list ap;

	va_start(ap, fmt);
	retval = kprintf(fmt, TOBUFONLY, NULL, buf, ap);
	va_end(ap);
	*(buf + retval) = 0;	/* null terminate */
	return(retval);
}

/*
 * vsprintf: print a message to the provided buffer [already have a
 *	va_list]
 */

int
vsprintf(buf, fmt, ap)
	char *buf;
	const char *fmt;
	va_list ap;
{
	int savintr;
	int len;

	savintr = consintr;		/* disable interrupts */
	consintr = 0;
	len = kprintf(fmt, TOBUFONLY, NULL, buf, ap);
	if (!panicstr)
		logwakeup();
	consintr = savintr;		/* reenable interrupts */
	buf[len] = 0;
	return (0);
}

/*
 * kprintf: scaled down version of printf(3).
 *
 * this version based on vfprintf() from libc which was derived from 
 * software contributed to Berkeley by Chris Torek.
 *
 * Two additional formats:
 *
 * The format %b is supported to decode error registers.
 * Its usage is:
 *
 *	printf("reg=%b\n", regval, "<base><arg>*");
 *
 * where <base> is the output base expressed as a control character, e.g.
 * \10 gives octal; \20 gives hex.  Each arg is a sequence of characters,
 * the first of which gives the bit number to be inspected (origin 1), and
 * the next characters (up to a control character, i.e. a character <= 32),
 * give the name of the register.  Thus:
 *
 *	kprintf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 *
 * would produce output:
 *
 *	reg=3<BITTWO,BITONE>
 *
 * The format %: passes an additional format string and argument list
 * recursively.  Its usage is:
 *
 * fn(char *fmt, ...)
 * {
 *	va_list ap;
 *	va_start(ap, fmt);
 *	printf("prefix: %: suffix\n", fmt, ap);
 *	va_end(ap);
 * }
 *
 * this is the actual printf innards
 *
 * This code is large and complicated...
 */

/*
 * macros for converting digits to letters and vice versa
 */
#define	to_digit(c)	((c) - '0')
#define is_digit(c)	((unsigned)to_digit(c) <= 9)
#define	to_char(n)	((n) + '0')

/*
 * flags used during conversion.
 */
#define	ALT		0x001		/* alternate form */
#define	HEXPREFIX	0x002		/* add 0x or 0X prefix */
#define	LADJUST		0x004		/* left adjustment */
#define	LONGDBL		0x008		/* long double; unimplemented */
#define	LONGINT		0x010		/* long integer */
#define	QUADINT		0x020		/* quad integer */
#define	SHORTINT	0x040		/* short integer */
#define	ZEROPAD		0x080		/* zero (as opposed to blank) pad */
#define FPT		0x100		/* Floating point number */

	/*
	 * To extend shorts properly, we need both signed and unsigned
	 * argument extraction methods.
	 */
#define	SARG() \
	(flags&QUADINT ? va_arg(ap, quad_t) : \
	    flags&LONGINT ? va_arg(ap, long) : \
	    flags&SHORTINT ? (long)(short)va_arg(ap, int) : \
	    (long)va_arg(ap, int))
#define	UARG() \
	(flags&QUADINT ? va_arg(ap, u_quad_t) : \
	    flags&LONGINT ? va_arg(ap, u_long) : \
	    flags&SHORTINT ? (u_long)(u_short)va_arg(ap, int) : \
	    (u_long)va_arg(ap, u_int))

#define KPRINTF_PUTCHAR(C) \
	(oflags == TOBUFONLY) ? *sbuf++ = (C) : putchar((C), oflags, tp);

int
kprintf(fmt0, oflags, tp, sbuf, ap)
	const char *fmt0;
	int oflags;
	struct tty *tp;
	char *sbuf;
	va_list ap;
{
	char *fmt;		/* format string */
	int ch;			/* character from fmt */
	int n;			/* handy integer (short term usage) */
	char *cp;		/* handy char pointer (short term usage) */
	int flags;		/* flags as above */
	int ret;		/* return value accumulator */
	int width;		/* width from format (%8d), or 0 */
	int prec;		/* precision from format (%.3d), or -1 */
	char sign;		/* sign prefix (' ', '+', '-', or \0) */

	u_quad_t _uquad;	/* integer arguments %[diouxX] */
	enum { OCT, DEC, HEX } base;/* base for [diouxX] conversion */
	int dprec;		/* a copy of prec if [diouxX], 0 otherwise */
	int realsz;		/* field size expanded by dprec */
	int size;		/* size of converted field or string */
	char *xdigs;		/* digits for [xX] conversion */
	char buf[KPRINTF_BUFSIZE]; /* space for %c, %[diouxX] */

	cp = NULL;	/* XXX: shutup gcc */
	size = 0;	/* XXX: shutup gcc */

	fmt = (char *)fmt0;
	ret = 0;

	xdigs = NULL;		/* XXX: shut up gcc warning */

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {
		while (*fmt != '%' && *fmt) {
			KPRINTF_PUTCHAR(*fmt++);
			ret++;
		}
		if (*fmt == 0)
			goto done;

		fmt++;		/* skip over '%' */

		flags = 0;
		dprec = 0;
		width = 0;
		prec = -1;
		sign = '\0';

rflag:		ch = *fmt++;
reswitch:	switch (ch) {
		/* XXX: non-standard '%:' format */
#ifndef __powerpc__
		case ':': 
			if (oflags != TOBUFONLY) {
				cp = va_arg(ap, char *);
				kprintf(cp, oflags, tp, 
					NULL, va_arg(ap, va_list));
			}
			continue;	/* no output */
#endif
		/* XXX: non-standard '%b' format */
		case 'b': {
			char *b, *z;
			int tmp;
			_uquad = va_arg(ap, int);
			b = va_arg(ap, char *);
			if (*b == 8)
				sprintf(buf, "%qo", _uquad);
			else if (*b == 10)
				sprintf(buf, "%qd", _uquad);
			else if (*b == 16)
				sprintf(buf, "%qx", _uquad);
			else
				break;
			b++;

			z = buf;
			while (*z) {
				KPRINTF_PUTCHAR(*z++);
				ret++;
			}

			if (_uquad) {
				tmp = 0;
				while ((n = *b++) != 0) {
					if (_uquad & (1 << (n - 1))) {
						KPRINTF_PUTCHAR(tmp ? ',':'<');
						ret++;
						while ((n = *b) > ' ') {
							KPRINTF_PUTCHAR(n);
							ret++;
							b++;
						}
						tmp = 1;
					} else {
						while(*b > ' ')
							b++;
					}
				}
				if (tmp) {
					KPRINTF_PUTCHAR('>');
					ret++;
				}
			}
			continue;	/* no output */
		}

#ifdef DDB
		/* XXX: non-standard '%r' format (print int in db_radix) */
		case 'r':
			if ((oflags & TODDB) == 0) 
				goto default_case;
			
			if (db_radix == 16)
				goto case_z;	/* signed hex */
			_uquad = SARG();
			if ((quad_t)_uquad < 0) {
				_uquad = -_uquad;
				sign = '-';
			}
			base = (db_radix == 8) ? OCT : DEC;
			goto number;


		/* XXX: non-standard '%z' format ("signed hex", a "hex %i")*/
		case 'z':
		case_z:
			if ((oflags & TODDB) == 0) 
				goto default_case;

			xdigs = "0123456789abcdef";
			ch = 'x';	/* the 'x' in '0x' (below) */
			_uquad = SARG();
			base = HEX;
			/* leading 0x/X only if non-zero */
			if (flags & ALT && _uquad != 0)
				flags |= HEXPREFIX;
			if ((quad_t)_uquad < 0) {
				_uquad = -_uquad;
				sign = '-';
			}
			goto number;
#endif

		case ' ':
			/*
			 * ``If the space and + flags both appear, the space
			 * flag will be ignored.''
			 *	-- ANSI X3J11
			 */
			if (!sign)
				sign = ' ';
			goto rflag;
		case '#':
			flags |= ALT;
			goto rflag;
		case '*':
			/*
			 * ``A negative field width argument is taken as a
			 * - flag followed by a positive field width.''
			 *	-- ANSI X3J11
			 * They don't exclude field widths read from args.
			 */
			if ((width = va_arg(ap, int)) >= 0)
				goto rflag;
			width = -width;
			/* FALLTHROUGH */
		case '-':
			flags |= LADJUST;
			goto rflag;
		case '+':
			sign = '+';
			goto rflag;
		case '.':
			if ((ch = *fmt++) == '*') {
				n = va_arg(ap, int);
				prec = n < 0 ? -1 : n;
				goto rflag;
			}
			n = 0;
			while (is_digit(ch)) {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			}
			prec = n < 0 ? -1 : n;
			goto reswitch;
		case '0':
			/*
			 * ``Note that 0 is taken as a flag, not as the
			 * beginning of a field width.''
			 *	-- ANSI X3J11
			 */
			flags |= ZEROPAD;
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			} while (is_digit(ch));
			width = n;
			goto reswitch;
		case 'h':
			flags |= SHORTINT;
			goto rflag;
		case 'l':
			if (*fmt == 'l') {
				fmt++;
				flags |= QUADINT;
			} else {
				flags |= LONGINT;
			}
			goto rflag;
		case 'q':
			flags |= QUADINT;
			goto rflag;
		case 'c':
			*(cp = buf) = va_arg(ap, int);
			size = 1;
			sign = '\0';
			break;
		case 'D':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'd':
		case 'i':
			_uquad = SARG();
			if ((quad_t)_uquad < 0) {
				_uquad = -_uquad;
				sign = '-';
			}
			base = DEC;
			goto number;
		case 'n':
#ifdef DDB
		/* XXX: non-standard '%n' format */
		/*
		 * XXX: HACK!   DDB wants '%n' to be a '%u' printed
		 * in db_radix format.   this should die since '%n'
		 * is already defined in standard printf to write
		 * the number of chars printed so far to the arg (which
		 * should be a pointer.
		 */
			if (oflags & TODDB) {
				if (db_radix == 16)
					ch = 'x';	/* convert to %x */
				else if (db_radix == 8)
					ch = 'o';	/* convert to %o */
				else
					ch = 'u';	/* convert to %u */

				/* ... and start again */
				goto reswitch;
			}

#endif
			if (flags & QUADINT)
				*va_arg(ap, quad_t *) = ret;
			else if (flags & LONGINT)
				*va_arg(ap, long *) = ret;
			else if (flags & SHORTINT)
				*va_arg(ap, short *) = ret;
			else
				*va_arg(ap, int *) = ret;
			continue;	/* no output */
		case 'O':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'o':
			_uquad = UARG();
			base = OCT;
			goto nosign;
		case 'p':
			/*
			 * ``The argument shall be a pointer to void.  The
			 * value of the pointer is converted to a sequence
			 * of printable characters, in an implementation-
			 * defined manner.''
			 *	-- ANSI X3J11
			 */
			/* NOSTRICT */
			_uquad = (u_long)va_arg(ap, void *);
			base = HEX;
			xdigs = "0123456789abcdef";
			flags |= HEXPREFIX;
			ch = 'x';
			goto nosign;
		case 's':
			if ((cp = va_arg(ap, char *)) == NULL)
				cp = "(null)";
			if (prec >= 0) {
				/*
				 * can't use strlen; can only look for the
				 * NUL in the first `prec' characters, and
				 * strlen() will go further.
				 */
				char *p = memchr(cp, 0, prec);

				if (p != NULL) {
					size = p - cp;
					if (size > prec)
						size = prec;
				} else
					size = prec;
			} else
				size = strlen(cp);
			sign = '\0';
			break;
		case 'U':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'u':
			_uquad = UARG();
			base = DEC;
			goto nosign;
		case 'X':
			xdigs = "0123456789ABCDEF";
			goto hex;
		case 'x':
			xdigs = "0123456789abcdef";
hex:			_uquad = UARG();
			base = HEX;
			/* leading 0x/X only if non-zero */
			if (flags & ALT && _uquad != 0)
				flags |= HEXPREFIX;

			/* unsigned conversions */
nosign:			sign = '\0';
			/*
			 * ``... diouXx conversions ... if a precision is
			 * specified, the 0 flag will be ignored.''
			 *	-- ANSI X3J11
			 */
number:			if ((dprec = prec) >= 0)
				flags &= ~ZEROPAD;

			/*
			 * ``The result of converting a zero value with an
			 * explicit precision of zero is no characters.''
			 *	-- ANSI X3J11
			 */
			cp = buf + KPRINTF_BUFSIZE;
			if (_uquad != 0 || prec != 0) {
				/*
				 * Unsigned mod is hard, and unsigned mod
				 * by a constant is easier than that by
				 * a variable; hence this switch.
				 */
				switch (base) {
				case OCT:
					do {
						*--cp = to_char(_uquad & 7);
						_uquad >>= 3;
					} while (_uquad);
					/* handle octal leading 0 */
					if (flags & ALT && *cp != '0')
						*--cp = '0';
					break;

				case DEC:
					/* many numbers are 1 digit */
					while (_uquad >= 10) {
						*--cp = to_char(_uquad % 10);
						_uquad /= 10;
					}
					*--cp = to_char(_uquad);
					break;

				case HEX:
					do {
						*--cp = xdigs[_uquad & 15];
						_uquad >>= 4;
					} while (_uquad);
					break;

				default:
					cp = "bug in kprintf: bad base";
					size = strlen(cp);
					goto skipsize;
				}
			}
			size = buf + KPRINTF_BUFSIZE - cp;
		skipsize:
			break;
		default:	/* "%?" prints ?, unless ? is NUL */
#ifdef DDB
		default_case:	/* DDB */
#endif
			if (ch == '\0')
				goto done;
			/* pretend it was %c with argument ch */
			cp = buf;
			*cp = ch;
			size = 1;
			sign = '\0';
			break;
		}

		/*
		 * All reasonable formats wind up here.  At this point, `cp'
		 * points to a string which (if not flags&LADJUST) should be
		 * padded out to `width' places.  If flags&ZEROPAD, it should
		 * first be prefixed by any sign or other prefix; otherwise,
		 * it should be blank padded before the prefix is emitted.
		 * After any left-hand padding and prefixing, emit zeroes
		 * required by a decimal [diouxX] precision, then print the
		 * string proper, then emit zeroes required by any leftover
		 * floating precision; finally, if LADJUST, pad with blanks.
		 *
		 * Compute actual size, so we know how much to pad.
		 * size excludes decimal prec; realsz includes it.
		 */
		realsz = dprec > size ? dprec : size;
		if (sign)
			realsz++;
		else if (flags & HEXPREFIX)
			realsz+= 2;

		/* right-adjusting blank padding */
		if ((flags & (LADJUST|ZEROPAD)) == 0) {
			n = width - realsz;
			while (n-- > 0)
				KPRINTF_PUTCHAR(' ');
		}

		/* prefix */
		if (sign) {
			KPRINTF_PUTCHAR(sign);
		} else if (flags & HEXPREFIX) {
			KPRINTF_PUTCHAR('0');
			KPRINTF_PUTCHAR(ch);
		}

		/* right-adjusting zero padding */
		if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD) {
			n = width - realsz;
			while (n-- > 0)
				KPRINTF_PUTCHAR('0');
		}

		/* leading zeroes from decimal precision */
		n = dprec - size;
		while (n-- > 0)
			KPRINTF_PUTCHAR('0');

		/* the string or number proper */
		while (size--)
			KPRINTF_PUTCHAR(*cp++);
		/* left-adjusting padding (always blank) */
		if (flags & LADJUST) {
			n = width - realsz;
			while (n-- > 0)
				KPRINTF_PUTCHAR(' ');
		}

		/* finally, adjust ret */
		ret += width > realsz ? width : realsz;

	}
done:
	return (ret);
	/* NOTREACHED */
}
