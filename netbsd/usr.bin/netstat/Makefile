#	$NetBSD: Makefile,v 1.22.2.1 2004/05/10 15:02:23 tron Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/12/93

.include <bsd.own.mk>

PROG=	netstat
SRCS=	atalk.c fast_ipsec.c if.c inet.c inet6.c ipsec.c iso.c \
	main.c mbuf.c mroute.c mroute6.c ns.c route.c tp_astring.c \
	unix.c
.PATH:	${NETBSDSRCDIR}/sys/netiso
BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}
CPPFLAGS+= -DINET6 -DIPSEC
CPPFLAGS+= -DDCCP

.if exists(/usr/local/v6/lib/libinet6.a)
LDADD+=	-L${.CURDIR}/../../lib/libinet6 -linet6
DPADD+=	${.CURDIR}/../../lib/libinet6/libinet6.a
.endif

# shared ipsec.c
CPPFLAGS+=	-I${.CURDIR}
.PATH: ${.CURDIR}/../../kame/netstat

.include <bsd.prog.mk>
