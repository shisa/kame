# New ports collection makefile for:	vnc
# Version required:     3.3.2r3 + IPv6
# Date created:		24 February 1998
# Whom:			itojun@itojun.org
#
# $Id: Makefile,v 1.1 1999/08/09 01:37:43 itojun Exp $
#

DISTNAME=       	vnc-3.3.2r3_unixsrc
PKGNAME=        	vnc-3.3.2r3
CATEGORIES=		net x11
MASTER_SITES=   	http://www.uk.research.att.com/vnc/dist/
DISTFILES=		vnc-3.3.2r3_unixsrc.tgz \
			vnc-3.3.2_doc.tgz

PATCH_SITES=		ftp://ftp.kame.net/pub/kame/misc/
PATCHFILES=		vnc-3.3.2r3-v6-19990105.diff.gz
PATCH_DIST_STRIP=	-p1

MAINTAINER=	 	itojun@itojun.org

WRKSRC=			${WRKDIR}/vnc_unixsrc

USE_PERL5=		YES

EXTRACT_ONLY=		vnc-3.3.2r3_unixsrc.tgz
IGNOREFILES=		${DISTFILES}

NO_WRKSUBDIR=		YES

USE_IMAKE=		YES
NO_INSTALL_MANPAGES=	YES

ALL_TARGET=    		World

# We have to frob a few things, and we want our own permissions, so it's easier
# to do the install ourselves.
post-install:
	${INSTALL_PROGRAM} ${WRKDIR}/Xvnc/programs/Xserver/Xvnc ${PREFIX}/bin/Xvnc
	${INSTALL_PROGRAM} ${WRKDIR}/vncviewer/vncviewer ${PREFIX}/bin/vncviewer
	${INSTALL_PROGRAM} ${WRKDIR}/vncpasswd/vncpasswd ${PREFIX}/bin/vncpasswd
	${SED} -e s%/usr/local/vnc/classes%${PREFIX}/share/vnc/classes% \
		< ${WRKDIR}/vncserver \
		> ${WRKDIR}/vncserver.local
	${INSTALL_SCRIPT} ${WRKDIR}/vncserver.local ${PREFIX}/bin/vncserver
	${LN} ${PREFIX}/bin/vncserver ${PREFIX}/bin/vncserver6
# The documentation comes with bogus ownerships; this is a little leaky security-wise
	${MKDIR} ${PREFIX}/share/doc/vnc
	tar -C ${PREFIX}/share/doc/vnc -xzf ${DISTDIR}/vnc-3.3.2_doc.tgz
	chown -R ${SHAREOWN}:${SHAREGRP} ${PREFIX}/share/doc/vnc
# Go install Java classes
	${MKDIR} ${PREFIX}/share/vnc
	${CP} -R ${WRKDIR}/classes ${PREFIX}/share/vnc
.include <bsd.port.mk>
