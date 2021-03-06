/*-
 * Copyright (c) 2000 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: hid.h,v 1.2 2001/08/22 21:05:25 matt Exp $
 * $FreeBSD: src/sys/powerpc/include/hid.h,v 1.1 2003/02/05 12:04:29 benno Exp $
 */

#ifndef _POWERPC_HID_H_
#define _POWERPC_HID_H_

/* Hardware Implementation Dependent registers for the PowerPC */

#define HID0_EMCP	0x80000000  /* Enable MCP */
#define HID0_DBP	0x40000000  /* Disable 60x bus parity generation */
#define HID0_EBA	0x20000000  /* Enable 60x bus address parity checking */
#define HID0_EBD	0x10000000  /* Enable 60x bus data parity checking */
#define HID0_BCLK	0x08000000  /* CLK_OUT clock type selection */
#define HID0_EICE	0x04000000  /* Enable ICE output */
#define HID0_TBEN	0x04000000  /* Time base enable (7450) */
#define HID0_ECLK	0x02000000  /* CLK_OUT clock type selection */
#define HID0_PAR	0x01000000  /* Disable precharge of ARTRY */
#define HID0_STEN	0x01000000  /* Software table search enable (7450) */
#define HID0_DOZE	0x00800000  /* Enable doze mode */
#define HID0_NAP	0x00400000  /* Enable nap mode */
#define HID0_SLEEP	0x00200000  /* Enable sleep mode */
#define HID0_DPM	0x00100000  /* Enable Dynamic power management */
#define HID0_RISEG	0x00080000  /* Read I-SEG */
#define HID0_BHTCLR	0x00080000  /* Clear branch history table (7450) */
#define HID0_EIEC	0x00040000  /* Enable internal error checking */
#define HID0_XAEN	0x00040000  /* Enable eXtended Addressing (7450) */
#define HID0_NHR	0x00010000  /* Not hard reset */
#define HID0_ICE	0x00008000  /* Enable i-cache */
#define HID0_DCE	0x00004000  /* Enable d-cache */
#define HID0_ILOCK	0x00002000  /* i-cache lock */
#define HID0_DLOCK	0x00001000  /* d-cache lock */
#define HID0_ICFI	0x00000800  /* i-cache flush invalidate */
#define HID0_DCFI	0x00000400  /* d-cache flush invalidate */
#define HID0_SPD	0x00000200  /* Disable speculative cache access */
#define HID0_IFEM	0x00000100  /* Enable M-bit for I-fetch */
#define HID0_SGE	0x00000080  /* Enable store gathering */
#define HID0_DCFA	0x00000040  /* Data cache flush assist */
#define HID0_BTIC	0x00000020  /* Enable BTIC */
#define HID0_ABE	0x00000008  /* Enable address broadcast */
#define HID0_BHT	0x00000004  /* Enable branch history table */
#define HID0_NOPTI	0x00000001  /* No-op the dcbt(st) */

#define HID0_BITMASK							\
    "\20"								\
    "\040EMCP\037DBP\036EBA\035EBD\034BCLK\033EICE\032ECLK\031PAR"	\
    "\030DOZE\027NAP\026SLEEP\025DPM\024RISEG\023EIEC\022res\021NHR"	\
    "\020ICE\017DCE\016ILOCK\015DLOCK\014ICFI\013DCFI\012SPD\011IFEM"	\
    "\010SGE\007DCFA\006BTIC\005FBIOB\004ABE\003BHT\002NOPDST\001NOPTI"

#define HID0_7450_BITMASK						\
    "\20"								\
    "\040EMCP\037b1\036b2\035b3\034b4\033TBEN\032b6\031STEN"		\
    "\030b8\027NAP\026SLEEP\025DPM\024b12\023BHTCLR\022XAEN\021NHR"	\
    "\020ICE\017DCE\016ILOCK\015DLOCK\014ICFI\013DCFI\012SPD\011b23"	\
    "\010SGE\007b25\006BTIC\005LRSTK\004FOLD\003BHT\002NOPDST\001NOPTI"

/*
 *  HID0 bit definitions per cpu model
 *
 * bit	603	604	750	7400	7410	7450
 *   0	EMCP	EMCP	EMCP	EMCP	EMCP	-
 *   1	-	ECP	DBP	-	-	-
 *   2	EBA	EBA	EBA	EBA	EDA	-
 *   3	EBD	EBD	EBD	EBD	EBD	-
 *   4	SBCLK	-	BCLK	BCKL	BCLK	-
 *   5	EICE	-	-	-	-	TBEN
 *   6	ECLK	-	ECLK	ECLK	ECLK	-
 *   7	PAR	PAR	PAR	PAR	PAR	STEN
 *   8	DOZE	-	DOZE	DOZE	DOZE	-
 *   9	NAP	-	NAP	NAP	NAP	NAP
 *  10	SLEEP	-	SLEEP	SLEEP	SLEEP	SLEEP
 *  11	DPM	-	DPM	DPM	DPM	DPM
 *  12	RISEG	-	-	RISEG	-	-
 *  13	-	-	-	EIEC	EIEC	BHTCLR
 *  14	-	-	-	-	-	XAEN
 *  15	-	NHR	NHR	NHR	NHR	NHR
 *  16	ICE	ICE	ICE	ICE	ICE	ICE
 *  17	DCE	DCE	DCE	DCE	DCE	DCE
 *  18	ILOCK	ILOCK	ILOCK	ILOCK	ILOCK	ILOCK
 *  19	DLOCK	DLOCK	DLOCK	DLOCK	DLOCK	DLOCK
 *  20	ICFI	ICFI	ICFI	ICFI	ICFI	ICFI
 *  21	DCFI	DCFI	DCFI	DCFI	DCFI	DCFI
 *  22	-	-	SPD	SPD	SPG	SPD
 *  23	-	-	IFEM	IFTT	IFTT	-
 *  24	-	SIE	SGE	SGE	SGE	SGE
 *  25	-	-	DCFA	DCFA	DCFA	-
 *  26	-	-	BTIC	BTIC	BTIC	BTIC
 *  27	FBIOB	-	-	-	-	LRSTK
 *  28	-	-	ABE	-	-	FOLD
 *  29	-	BHT	BHT	BHT	BHT	BHT
 *  30	-	-	-	NOPDST	NOPDST	NOPDST
 *  31	NOOPTI	-	NOOPTI	NOPTI	NOPTI	NOPTI
 *
 *  604: ECP = Enable cache parity checking
 *  604: SIE = Serial instruction execution disable
 * 7450: TBEN = Time Base Enable
 * 7450: STEN = Software table lookup enable
 * 7450: BHTCLR = Branch history clear
 * 7450: LRSTK = Link Register Stack Enable
 * 7450: FOLD = Branch folding enable
 */

#endif /* _POWERPC_HID_H_ */
