SRCDIR=	${.CURDIR}/../../kame/rtsold

PROG=	rtsold
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c rtsock.c

CFLAGS+=-DINET6 -DHAVE_GETIFADDRS -DUSE_RTSOCK
LDADD+=	-L${.CURDIR}/../../lib/libinet6 -L${.CURDIR}/../../lib/libinet6/obj \
	-L/usr/local/v6/lib -linet6
DPADD+=	${.CURDIR}/../../lib/libinet6/libinet6.a \
	${.CURDIR}/../../lib/libinet6/obj/libinet6.a \
	/usr/local/v6/lib/libinet6.a

MAN8=	rtsold.8
MLINKS=	rtsold.8 rtsol.8

.PATH:	${SRCDIR}

.include <bsd.prog.mk>
