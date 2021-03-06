/*	$OpenBSD: locore.s,v 1.19 1998/09/06 20:10:53 millert Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: locore.s 1.66 92/12/22$
 *
 *	@(#)locore.s	8.6 (Berkeley) 5/27/94
 */

#include "assym.h"
#include "ksyms.h"
#include <machine/asm.h>
#include <machine/prom.h>

/*
 * Temporary stack for a variety of purposes.
 * Try and make this the first thing is the data segment so it
 * is page aligned.  Note that if we overflow here, we run into
 * our text segment.
 */
	.data
	.space	NBPG
	.globl	tmpstk
tmpstk:

#define	RELOC(var, ar) \
	lea	var,ar

/*
 * Initialization
 *
 * The bootstrap loader loads us in starting at 0, and VBR is non-zero.
 * On entry, args on stack are boot device, boot filename, console unit,
 * boot flags (howto), boot device name, filesystem type name.
 */
	.comm	_lowram,4
	.comm	_esym,4
	.comm	_emini,4
	.comm	_smini,4
	.comm	_needprom,4
	.comm	_promvbr,4
	.comm	_promcall,4

	.text
	.globl	_edata
	.globl	_etext,_end
	.globl	start
	.globl	_kernel_text
_kernel_text:
start:
	movw	#PSL_HIGHIPL,sr		| no interrupts
	movl	#0,a5			| RAM starts at 0
	movl	sp@(4), d7		| get boothowto
	movl	sp@(8), d6		| get bootaddr
	movl	sp@(12),d5		| get bootctrllun
	movl	sp@(16),d4		| get bootdevlun
	movl	sp@(20),d3		| get bootpart
	movl	sp@(24),d2		| get esyms
	/* note: d2-d7 in use */

	RELOC(tmpstk, a0)
	movl	a0,sp			| give ourselves a temporary stack

	RELOC(_edata, a0)		| clear out BSS
	movl	#_end-4,d0		| (must be <= 256 kB)
	subl	#_edata,d0
	lsrl	#2,d0
1:	clrl	a0@+
	dbra	d0,1b

	movc	vbr,d0			| save prom's trap #15 vector
	RELOC(_promvbr, a0)
	movl	d0, a0@
	RELOC(_esym, a0)
	movl	d2,a0@			| store end of symbol table
	/* note: d2 now free, d3-d7 still in use */
	RELOC(_lowram, a0)
	movl	a5,a0@			| store start of physical memory

	clrl	sp@-
	trap	#15
	.short	MVMEPROM_GETBRDID
	movl	sp@+, a1

	movl	#SIZEOF_MVMEPROM_BRDID, d0	| copy to local variables
	RELOC(_brdid, a0)
1:	movb	a1@+, a0@+
	subql	#1, d0
	bne	1b

	clrl	d0
	RELOC(_brdid, a1)
	movw	a1@(MVMEPROM_BRDID_MODEL), d0
	RELOC(_cputyp, a0)
	movl	d0, a0@			| init _cputyp

#ifdef MVME147
	cmpw	#CPU_147, d0
	beq	is147
#endif

#ifdef MVME162
	cmpw	#CPU_162, d0
	beq	is162
#endif

#ifdef MVME167
	cmpw	#CPU_166, d0
	beq	is167
	cmpw	#CPU_167, d0
	beq	is167
#endif

#ifdef MVME177
	cmpw	#CPU_177, d0
	beq	is177
#endif
	
	.data
notsup:	.ascii	"kernel does not support this model."
notsupend:
	.even
	.text

	| first we bitch, then we die.
	movl	#notsupend, sp@-
	movl	#notsup, sp@-
	trap	#15
	.short	MVMEPROM_OUTSTRCRLF
	addql	#8,sp

	trap	#15
	.short	MVMEPROM_EXIT		| return to m68kbug
	/*NOTREACHED */

#ifdef MVME147
is147:
	RELOC(_mmutype, a0)		| no, we have 68030
	movl	#MMU_68030,a0@		| set to reflect 68030 PMMU

	RELOC(_cputype, a0)		| no, we have 68030
	movl	#CPU_68030,a0@		| set to reflect 68030 CPU

	movl	#CACHE_OFF,d0
	movc	d0,cacr			| clear and disable on-chip cache(s)

	movb	#0, 0xfffe1026		| XXX serial interrupt off
	movb	#0, 0xfffe1018		| XXX timer 1 off
	movb	#0, 0xfffe1028		| XXX ethernet off

	movl	#0xfffe0000, a0		| mvme147 nvram base
	| move nvram component of etheraddr (only last 3 bytes)
	RELOC(_myea, a1)
	movw	a0@(NVRAM_147_ETHER+0), a1@(3+0)
	movb	a0@(NVRAM_147_ETHER+2), a1@(3+2)
	movl	a0@(NVRAM_147_EMEM), d1	| pass memory size

	RELOC(_iiomapsize, a1)
	movl	#INTIOSIZE_147, a1@
	RELOC(_iiomapbase, a1)
	movl	#INTIOBASE_147, a1@
	bra	Lstart1
#endif

#ifdef MVME162
is162:
#if 0
	| the following 3 things are "just in case". they won't make
	| the kernel work properly, but they will at least let it get
	| far enough that you can figure out that something had an
	| interrupt pending. which the bootrom shouldn't allow, i don't
	| think..
	clrb	0xfff42002 		| XXX MCchip irq off
	clrl	0xfff42018		| XXX MCchip timers irq off
	clrb	0xfff4201d		| XXX MCchip scc irq off
#endif
	RELOC(_memsize162, a1)		| how much memory?
	jbsr	a1@
	movl	d0, d2

	RELOC(_mmutype, a0)
	movl	#MMU_68040,a0@		| with a 68040 MMU

	RELOC(_cputype, a0)		| no, we have 68040
	movl	#CPU_68040,a0@		| set to reflect 68040 CPU

	RELOC(_fputype, a0)
	movl	#FPU_68040,a0@		| and a 68040 FPU

	bra	is16x
#endif

#ifdef MVME167
is167:
|	RELOC(_needprom,a0)		| this machine needs the prom mapped!
|	movl	#1,a0@

	RELOC(_memsize1x7, a1)		| how much memory?
	jbsr	a1@
	movl	d0, d2

	RELOC(_mmutype, a0)
	movl	#MMU_68040,a0@		| with a 68040 MMU

	RELOC(_cputype, a0)		| no, we have 68040
	movl	#CPU_68040,a0@		| set to reflect 68040 CPU

	RELOC(_fputype, a0)
	movl	#FPU_68040,a0@		| and a 68040 FPU

	bra	is16x
#endif

#ifdef MVME177
is177:
|	RELOC(_needprom,a0)		| this machine needs the prom mapped!
|	movl	#1,a0@

	RELOC(_memsize1x7, a1)		| how much memory?
	jbsr	a1@
	movl	d0, d2

	RELOC(_mmutype, a0)
	movl	#MMU_68060,a0@		| with a 68060 MMU

	RELOC(_cputype, a0)		| no, we have 68060
	movl	#CPU_68060,a0@		| set to reflect 68060 CPU

	bra	is16x
#endif

#if defined(MVME162) || defined(MVME167) || defined(MVME177)
	.data
#define	ROMPKT_LEN	200
	.comm	_rompkt, ROMPKT_LEN
	.even
	.text
is16x:
	RELOC(_iiomapsize, a1)
	movl	#INTIOSIZE_162, a1@
	RELOC(_iiomapbase, a1)
	movl	#INTIOBASE_162, a1@

	/* get ethernet address */
	RELOC(_rompkt, a0)		| build a .NETCTRL packet
	movb	#0, a0@(NETCTRL_DEV)	| onboard ethernet
	movb	#0, a0@(NETCTRL_CTRL)	| onboard ethernet
	movl	#NETCTRLCMD_GETETHER, a0@(NETCTRL_CMD)
	RELOC(_myea, a1)
	movl	a1, a0@(NETCTRL_ADDR)	| where to put it
	movl	#6, a0@(NETCTRL_LEN)	| it is 6 bytes long

	movl	a0, sp@-
	trap	#15
	.short	MVMEPROM_NETCTRL	| ask the rom
	addl	#4, sp

#if 0
	/* 
	 * get memory size using ENVIRON. unfortunately i've not managed
	 * to get this working.
	 */
	RELOC(_rompkt, a0)
	movl	#ENVIRONCMD_READ, sp@-	| request environment information
	movl	#ROMPKT_LEN, sp@-	| max length
	movl	a0, sp@-		| point to info packet
	trap	#15
	.short	MVMEPROM_ENVIRON	| ask the rom
	addl	#12, sp
	| XXX should check return values

	clrl	d2			| memsize = 0
1:	clrl	d0
	movb	a0@+, d0		| look for a "memsize" chunk in the
	cmpb	#ENVIRONTYPE_EOL, d0	| environment
	beq	3f
	cmpb	#ENVIRONTYPE_MEMSIZE, d0
	beq	2f
	movb	a0@+, d0
	addl	d0, a0
	bra	1b
2:	movl	a0@(7), d2		| XXX memory size (fix @(7) offset!)
3:
#endif

	| if memory size is unknown, print a diagnostic and make an
	| assumption
	movl	d2, d1
	cmpl	#0, d1
	bne	Lstart1

	movl	#unkmemend, sp@-
	movl	#unkmem, sp@-
	trap	#15
	.short	MVMEPROM_OUTSTRCRLF
	addql	#8,sp

	movl	#4*1024*1024, d1	| XXX assume 4M of ram
	bra	Lstart1

	.data
unkmem:	.ascii	"could not figure out how much memory; assuming 4M."
unkmemend:
	.even
	.text

#endif

Lstart1:
/* initialize source/destination control registers for movs */
	moveq	#FC_USERD,d0		| user space
	movc	d0,sfc			|   as source
	movc	d0,dfc			|   and destination of transfers
	moveq	#PGSHIFT,d2
	lsrl	d2,d1			| convert to page (click) number
	RELOC(_maxmem, a0)
	movl	d1,a0@			| save as maxmem
	movl	a5,d0			| lowram value from ROM via boot
	lsrl	d2,d0			| convert to page number
	subl	d0,d1			| compute amount of RAM present
	RELOC(_physmem, a0)
	movl	d1,a0@			| and physmem
/* configure kernel and proc0 VA space so we can get going */
	.globl	_Sysseg, _pmap_bootstrap, _avail_start
#if defined(DDB) || NKSYMS > 0
	RELOC(_esym,a0)			| end of static kernel test/data/syms
	movl	a0@,d2
	jne	Lstart2
#endif
	movl	#_end,d2		| end of static kernel text/data
Lstart2:
	addl	#NBPG-1,d2
	andl	#PG_FRAME,d2		| round to a page
	movl	d2,a4
	addl	a5,a4			| convert to PA
#if 0
	| XXX clear from end-of-kernel to 1M, as a workaround for an
	| inane pmap_bootstrap bug I cannot find (68040-specific)
	movl	a4,a0
	movl	#1024*1024,d0
	cmpl	a0,d0			| end of kernel is beyond 1M?
	jlt	2f
	subl	a0,d0
1:	clrb	a0@+
	subql	#1,d0
	bne	1b
2:
#endif
	pea	a5@			| firstpa
	pea	a4@			| nextpa
	RELOC(_pmap_bootstrap,a0)
	jbsr	a0@			| pmap_bootstrap(firstpa, nextpa)
	addql	#8,sp

/*
 * Enable the MMU.
 * Since the kernel is mapped logical == physical, we just turn it on.
 */
	RELOC(_Sysseg, a0)		| system segment table addr
	movl	a0@,d1			| read value (a KVA)
	addl	a5,d1			| convert to PA
	RELOC(_mmutype, a0)
	cmpl	#MMU_68040,a0@		| 68040?
	jne	Lmotommu1		| no, skip
	.long	0x4e7b1807		| movc d1,srp
	jra	Lstploaddone
Lmotommu1:
	RELOC(_protorp, a0)
	movl	#0x80000202,a0@		| nolimit + share global + 4 byte PTEs
	movl	d1,a0@(4)		| + segtable address
	pmove	a0@,srp			| load the supervisor root pointer
	movl	#0x80000002,a0@		| reinit upper half for CRP loads
Lstploaddone:
	RELOC(_mmutype, a0)
	cmpl	#MMU_68040,a0@		| 68040?
	jne	Lmotommu2		| no, skip

	RELOC(_needprom,a0)
	cmpl	#0,a0@
	beq	1f
	/*
	 * this machine needs the prom mapped. we use the translation
	 * registers to map it in.. and the ram it needs.
	 */
	movel	#0xff00a044,d0		| map top 16meg 1/1 for bug eprom exe
	.long	0x4e7b0004		| movc d0,itt0
	moveq	#0,d0			| ensure itt1 is disabled
	.long	0x4e7b0005		| movc d0,itt1
	movel	#0xff00a040,d0		| map top 16meg 1/1 for bug io access
	.long	0x4e7b0006		| movc d0,dtt0
	moveq	#0,d0			| ensure dtt1 is disabled
	.long	0x4e7b0007		| movc d0,dtt1
	bra	2f
1:
	moveq	#0,d0			| ensure TT regs are disabled
	.long	0x4e7b0004		| movc d0,itt0
	.long	0x4e7b0005		| movc d0,itt1
	.long	0x4e7b0006		| movc d0,dtt0
	.long	0x4e7b0007		| movc d0,dtt1
2:

	.word	0xf4d8			| cinva bc
	.word	0xf518			| pflusha
	movl	#0x8000,d0
	.long	0x4e7b0003		| movc d0,tc
	movl	#0x80008000,d0
	movc	d0,cacr			| turn on both caches
	jmp	Lenab1
Lmotommu2:
	movl	#0x82c0aa00,a2@		| value to load TC with
	pmove	a2@,tc			| load it
Lenab1:

/*
 * Should be running mapped from this point on
 */
/* select the software page size now */
	lea	tmpstk,sp		| temporary stack
	jbsr	_vm_set_page_size	| select software page size
/* set kernel stack, user SP, and initial pcb */
	movl	_proc0paddr,a1		| get proc0 pcb addr
	lea	a1@(USPACE-4),sp	| set kernel stack to end of area
	movl	#USRSTACK-4,a2
	movl	a2,usp			| init user SP
	movl	a1,_curpcb		| proc0 is running

	tstl	_fputype		| Have an FPU?
	jeq	Lenab2			| No, skip.
	clrl	a1@(PCB_FPCTX)		| ensure null FP context
	movl	a1,sp@-
	jbsr	_m68881_restore		| restore it (does not kill a1)
	addql	#4,sp
Lenab2:
/* flush TLB and turn on caches */
	jbsr	_TBIA			| invalidate TLB
	cmpl	#MMU_68040,_mmutype	| 68040?
	jeq	Lnocache0		| yes, cache already on
	movl	#CACHE_ON,d0
	movc	d0,cacr			| clear cache(s)
Lnocache0:
/* final setup for C code */
	movl	#_vectab,d2		| set VBR
	movc	d2,vbr
	movw	#PSL_LOWIPL,sr		| lower SPL
	movl	d3, _bootpart		| save bootpart
	movl	d4, _bootdevlun		| save bootdevlun
	movl	d5, _bootctrllun	| save bootctrllun
	movl	d6, _bootaddr		| save bootaddr
	movl	d7, _boothowto		| save boothowto
	/* d3-d7 now free */

/* Final setup for call to main(). */
	jbsr	_C_LABEL(mvme68k_init)
 
/*
 * Create a fake exception frame so that cpu_fork() can copy it.
 * main() nevers returns; we exit to user mode from a forked process
 * later on.
 */
	clrw	sp@-			| vector offset/frame type
	clrl	sp@-			| PC - filled in by "execve"
	movw	#PSL_USER,sp@-		| in user mode
	clrl	sp@-			| stack adjust count and padding
	lea	sp@(-64),sp		| construct space for D0-D7/A0-A7
	lea	_proc0,a0		| save pointer to frame
	movl	sp,a0@(P_MD_REGS)	|   in proc0.p_md.md_regs

	jra	_main			| main()

	pea	1f
	jbsr	_panic
1:
	.asciz	"main returned"
	.even

	.globl	_proc_trampoline
_proc_trampoline:
	movl	a3,sp@-
	jbsr	a2@
	addql	#4,sp
	movl	sp@(FR_SP),a0		| grab and load
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most user regs
	addql	#8,sp			| toss SP and stack adjust
	jra	rei			| and return

/*
 * Signal "trampoline" code (18 bytes).  Invoked from RTE setup by sendsig().
 * 
 * Stack looks like:
 *
 *	sp+0 ->	signal number
 *	sp+4	pointer to siginfo (sip)
 *	sp+8	pointer to signal context frame (scp)
 *	sp+12	address of handler
 *	sp+16	saved hardware state
 *			.
 *			.
 *	scp+0->	beginning of signal context frame
 */
	.globl	_sigcode, _esigcode, _sigcodetrap
	.data
_sigcode:
	movl	sp@(12),a0		| signal handler addr	(4 bytes)
	jsr	a0@			| call signal handler	(2 bytes)
	addql	#4,sp			| pop signo		(2 bytes)
_sigcodetrap:
	trap	#1			| special syscall entry	(2 bytes)
	movl	d0,sp@(4)		| save errno		(4 bytes)
	moveq	#1,d0			| syscall == exit	(2 bytes)
	trap	#0			| exit(errno)		(2 bytes)
	.align	2
_esigcode:
	.text

/*
 * Do a dump.
 * Called by auto-restart.
 */
	.globl	_dumpsys
	.globl	_doadump
_doadump:
	jbsr	_dumpsys
	jbsr	_doboot
	/*NOTREACHED*/

/*
 * Trap/interrupt vector routines
 */ 

	.globl	_trap, _nofault, _longjmp
_buserr:
	tstl	_nofault		| device probe?
	jeq	Lberr			| no, handle as usual
	movl	_nofault,sp@-		| yes,
	jbsr	_longjmp		|  longjmp(nofault)
Lberr:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	_addrerr		| no, skip
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
	moveq	#0,d0
	movw	a1@(12),d0		| grab SSW
	movl	a1@(20),d1		| and fault VA
	btst	#11,d0			| check for mis-aligned access
	jeq	Lberr2			| no, skip
	addl	#3,d1			| yes, get into next page
	andl	#PG_FRAME,d1		| and truncate
Lberr2:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	btst	#10,d0			| ATC bit set?
	jeq	Lisberr			| no, must be a real bus error
	movc	dfc,d1			| yes, get MMU fault
	movc	d0,dfc			| store faulting function code
	movl	sp@(4),a0		| get faulting address
	.word	0xf568			| ptestr a0@
	movc	d1,dfc
	.long	0x4e7a0805		| movc mmusr,d0
	movw	d0,sp@			| save (ONLY LOW 16 BITS!)
	jra	Lismerr
#endif
_addrerr:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lbenot040		| no, skip
	movl	a1@(8),sp@-		| yes, push fault address
	clrl	sp@-			| no SSW for address fault
	jra	Lisaerr			| go deal with it
Lbenot040:
#endif
	moveq	#0,d0
	movw	a1@(10),d0		| grab SSW for fault processing
	btst	#12,d0			| RB set?
	jeq	LbeX0			| no, test RC
	bset	#14,d0			| yes, must set FB
	movw	d0,a1@(10)		| for hardware too
LbeX0:
	btst	#13,d0			| RC set?
	jeq	LbeX1			| no, skip
	bset	#15,d0			| yes, must set FC
	movw	d0,a1@(10)		| for hardware too
LbeX1:
	btst	#8,d0			| data fault?
	jeq	Lbe0			| no, check for hard cases
	movl	a1@(16),d1		| fault address is as given in frame
	jra	Lbe10			| thats it
Lbe0:
	btst	#4,a1@(6)		| long (type B) stack frame?
	jne	Lbe4			| yes, go handle
	movl	a1@(2),d1		| no, can use save PC
	btst	#14,d0			| FB set?
	jeq	Lbe3			| no, try FC
	addql	#4,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe3:
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, done
	addql	#2,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe4:
	movl	a1@(36),d1		| long format, use stage B address
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, all done
	subql	#2,d1			| yes, adjust address
Lbe10:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	movw	a1@(6),d0		| get frame format/vector offset
	andw	#0x0FFF,d0		| clear out frame format
	cmpw	#12,d0			| address error vector?
	jeq	Lisaerr			| yes, go to it
	movl	d1,a0			| fault address
	movl	sp@,d0			| function code from ssw
	btst	#8,d0			| data fault?
	jne	Lbe10a
	movql	#1,d0			| user program access FC
					| (we dont seperate data/program)
	btst	#5,a1@			| supervisor mode?
	jeq	Lbe10a			| if no, done
	movql	#5,d0			| else supervisor program access
Lbe10a:
	ptestr	d0,a0@,#7		| do a table search
	pmove	psr,sp@			| save result
	movb	sp@,d1
	btst	#2,d1			| invalid (incl. limit viol. and berr)?
	jeq	Lmightnotbemerr		| no -> wp check
	btst	#7,d1			| is it MMU table berr?
	jeq	Lismerr			| no, must be fast
	jra	Lisberr1		| real bus err needs not be fast.
Lmightnotbemerr:
	btst	#3,d1			| write protect bit set?
	jeq	Lisberr1		| no: must be bus error
	movl	sp@,d0			| ssw into low word of d0
	andw	#0xc0,d0		| Write protect is set on page:
	cmpw	#0x40,d0		| was it read cycle?
	jeq	Lisberr1		| yes, was not WPE, must be bus err
Lismerr:
	movl	#T_MMUFLT,sp@-		| show that we are an MMU fault
	jra	Ltrapnstkadj		| and deal with it
Lisaerr:
	movl	#T_ADDRERR,sp@-		| mark address error
	jra	Ltrapnstkadj		| and deal with it
Lisberr1:
	clrw	sp@			| re-clear pad word
Lisberr:
	movl	#T_BUSERR,sp@-		| mark bus error
Ltrapnstkadj:
	jbsr	_trap			| handle the error
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore user SP
	movl	a0,usp			|   from save area
	movw	sp@(FR_ADJ),d0		| need to adjust stack?
	jne	Lstkadj			| yes, go to it
	moveml	sp@+,#0x7FFF		| no, restore most user regs
	addql	#8,sp			| toss SSP and stkadj
	jra	rei			| all done
Lstkadj:
	lea	sp@(FR_HW),a1		| pointer to HW frame
	addql	#8,a1			| source pointer
	movl	a1,a0			| source
	addw	d0,a0			|  + hole size = dest pointer
	movl	a1@-,a0@-		| copy
	movl	a1@-,a0@-		|  8 bytes
	movl	a0,sp@(FR_SP)		| new SSP
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	jra	rei			| all done

/*
 * FP exceptions.
 */
_fpfline:
#if defined(M68040)
	cmpl	#FPU_68040,_fputype	| 68040 FPU?
	jne	Lfp_unimp		| no, skip FPSP
	cmpw	#0x202c,sp@(6)		| format type 2?
	jne	_illinst		| no, not an FP emulation
Ldofp_unimp:
#ifdef FPSP
	.globl	fpsp_unimp
	jmp	fpsp_unimp		| yes, go handle it
#endif
Lfp_unimp:
#endif/* M68040 */
#ifdef FPU_EMULATE
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULI,d0		| denote as FP emulation trap
	jra	fault			| do it
#else
	jra	_illinst
#endif

_fpunsupp:
#if defined(M68040)
	cmpl	#FPU_68040,_fputype	| 68040 FPU?
	jne	_illinst		| no, treat as illinst
#ifdef FPSP
	.globl	fpsp_unsupp
	jmp	fpsp_unsupp		| yes, go handle it
#endif
Lfp_unsupp:
#endif /* M68040 */
#ifdef FPU_EMULATE
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULD,d0		| denote as FP emulation trap
	jra	fault			| do it
#else
	jra	_illinst
#endif

/*
 * Handles all other FP coprocessor exceptions.
 * Note that since some FP exceptions generate mid-instruction frames
 * and may cause signal delivery, we need to test for stack adjustment
 * after the trap call.
 */
	.globl	_fpfault
_fpfault:
	clrl	sp@-		| stack adjust count
	moveml	#0xFFFF,sp@-	| save user registers
	movl	usp,a0		| and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	movl	_curpcb,a0	| current pcb
	lea	a0@(PCB_FPCTX),a0 | address of FP savearea
	fsave	a0@		| save state
#if defined(M68040) || defined(M68060)
	/* always null state frame on 68040, 68060 */
	cmpl	#CPU_68040,_cputype
	jle	Lfptnull
#endif
	tstb	a0@		| null state frame?
	jeq	Lfptnull	| yes, safe
	clrw	d0		| no, need to tweak BIU
	movb	a0@(1),d0	| get frame size
	bset	#3,a0@(0,d0:w)	| set exc_pend bit of BIU
Lfptnull:
	fmovem	fpsr,sp@-	| push fpsr as code argument
	frestore a0@		| restore state
	movl	#T_FPERR,sp@-	| push type arg
	jra	Ltrapnstkadj	| call trap and deal with stack cleanup

/*
 * Coprocessor and format errors can generate mid-instruction stack
 * frames and cause signal delivery hence we need to check for potential
 * stack adjustment.
 */
_coperr:
	clrl	sp@-		| stack adjust count
	moveml	#0xFFFF,sp@-
	movl	usp,a0		| get and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	clrl	sp@-		| or code arg
	movl	#T_COPERR,sp@-	| push trap type
	jra	Ltrapnstkadj	| call trap and deal with stack adjustments

_fmterr:
	clrl	sp@-		| stack adjust count
	moveml	#0xFFFF,sp@-
	movl	usp,a0		| get and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	clrl	sp@-		| or code arg
	movl	#T_FMTERR,sp@-	| push trap type
	jra	Ltrapnstkadj	| call trap and deal with stack adjustments

/*
 * Other exceptions only cause four and six word stack frame and require
 * no post-trap stack adjustment.
 */
_illinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_ILLINST,d0
	jra	fault

_zerodiv:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_ZERODIV,d0
	jra	fault

_chkinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_CHKINST,d0
	jra	fault

_trapvinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_TRAPVINST,d0
	jra	fault

_privinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_PRIVINST,d0
	jra	fault

	.globl	fault
fault:
	movl	usp,a0			| get and save
	movl	a0,sp@(FR_SP)		|   the user stack pointer
	clrl	sp@-			| no VA arg
	clrl	sp@-			| or code arg
	movl	d0,sp@-			| push trap type
	jbsr	_trap			| handle trap
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most user regs
	addql	#8,sp			| pop SP and stack adjust
	jra	rei			| all done

	.globl	_hardtrap, _hardintr
_hardtrap:
	moveml	#0xC0C0,sp@-		| save scratch regs
	lea	sp@(16),a1		| get pointer to frame
	movl	a1,sp@-
	movw	sp@(26),d0
	movl	d0,sp@-			| push exception vector info
	movl	sp@(26),sp@-		| and PC
	jbsr	_hardintr		| doit
	lea	sp@(12),sp		| pop args
	moveml	sp@+,#0x0303		| restore regs
	jra	rei			| all done

	.globl	_straytrap
_badtrap:
	moveml	#0xC0C0,sp@-		| save scratch regs
	movw	sp@(22),sp@-		| push exception vector info
	clrw	sp@-
	movl	sp@(22),sp@-		| and PC
	jbsr	_straytrap		| report
	addql	#8,sp			| pop args
	moveml	sp@+,#0x0303		| restore regs
	jra	rei			| all done

	.globl	_syscall
_trap0:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	d0,sp@-			| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall arg
	tstl	_astpending
	jne	Lrei2
	tstb	_ssir
	jeq	Ltrap1
	movw	#SPL1,sr
	tstb	_ssir
	jne	Lsir1
Ltrap1:
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most registers
	addql	#8,sp			| pop SP and stack adjust
	rte

/*
 * Trap 1 - sigreturn
 */
_trap1:
	jra	_ASM_LABEL(sigreturn)

/*
 * Trap 2 - trace trap
 */
_trap2:
	jra	_C_LABEL(trace)

/*
 * Trap 12 is the entry point for the cachectl "syscall" (both HPUX & BSD)
 *	cachectl(command, addr, length)
 * command in d0, addr in a1, length in d1
 */
	.globl	_cachectl
_trap12:
	movl	d1,sp@-			| push length
	movl	a1,sp@-			| push addr
	movl	d0,sp@-			| push command
	jbsr	_cachectl		| do it
	lea	sp@(12),sp		| pop args
	jra	rei			| all done

/*
 * Trap 15 is used for:
 *	- KGDB traps
 *	- trace traps for SUN binaries (not fully supported yet)
 *	- calling the prom, but only from the kernel
 * We just pass it on and let trap() sort it all out
 */
_trap15:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	tstl	_promcall
	jeq	L_notpromcall
	moveml	sp@+,#0xFFFF
	addql	#4, sp
	| unwind stack to put to known value
	| this routine is from the 147 BUG manual
	| currently save and restore are excessive.
	subql	#4,sp
	link	a6,#0
	moveml	#0xFFFE,sp@-
	movl	_promvbr,a0
	movw	a6@(14),d0
	andl	#0xfff,d0
	movl	a0@(d0:w),a6@(4)
	moveml	sp@+,#0x7FFF
	unlk	a6
	rts
	| really jumps to the bug trap handler
L_notpromcall:
#ifdef KGDB
	moveq	#T_TRAP15,d0
	movw	sp@(FR_HW),d1		| get PSW
	andw	#PSL_S,d1		| from user mode?
	jeq	fault			| yes, just a regular fault
	movl	d0,sp@-
	.globl	_kgdb_trap_glue
	jbsr	_kgdb_trap_glue		| returns if no debugger
	addl	#4,sp
#endif
	moveq	#T_TRAP15,d0
	jra	fault

/*
 * Hit a breakpoint (trap 1 or 2) instruction.
 * Push the code and treat as a normal fault.
 */
_trace:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
#ifdef KGDB
	moveq	#T_TRACE,d0
	movw	sp@(FR_HW),d1		| get SSW
	andw	#PSL_S,d1		| from user mode?
	jeq	fault			| no, regular fault
	movl	d0,sp@-
	jbsr	_kgdb_trap_glue		| returns if no debugger
	addl	#4,sp
#endif
	moveq	#T_TRACE,d0
	jra	fault

#include <m68k/m68k/sigreturn.s>

/*
 * Interrupt handlers.
 * No device interrupts are auto-vectored.
 */

_spurintr:
	addql	#1,_intrcnt+0
	addql	#1,_cnt+V_INTR
	jra	rei

/*
 * Emulation of VAX REI instruction.
 *
 * This code deals with checking for and servicing ASTs
 * (profiling, scheduling) and software interrupts (network, softclock).
 * We check for ASTs first, just like the VAX.  To avoid excess overhead
 * the T_ASTFLT handling code will also check for software interrupts so we
 * do not have to do it here.  After identifing that we need an AST we
 * drop the IPL to allow device interrupts.
 *
 * This code is complicated by the fact that sendsig may have been called
 * necessitating a stack cleanup.
 */
	.comm	_ssir,1
	.globl	_astpending
	.globl	rei
rei:
	tstl	_astpending		| AST pending?
	jeq	Lchksir			| no, go check for SIR
Lrei1:
	btst	#5,sp@			| yes, are we returning to user mode?
	jne	Lchksir			| no, go check for SIR
	movw	#PSL_LOWIPL,sr		| lower SPL
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lrei2:
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_ASTFLT,sp@-		| type == async system trap
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore user SP
	movl	a0,usp			|   from save area
	movw	sp@(FR_ADJ),d0		| need to adjust stack?
	jne	Laststkadj		| yes, go to it
	moveml	sp@+,#0x7FFF		| no, restore most user regs
	addql	#8,sp			| toss SP and stack adjust
	rte				| and do real RTE
Laststkadj:
	lea	sp@(FR_HW),a1		| pointer to HW frame
	addql	#8,a1			| source pointer
	movl	a1,a0			| source
	addw	d0,a0			|  + hole size = dest pointer
	movl	a1@-,a0@-		| copy
	movl	a1@-,a0@-		|  8 bytes
	movl	a0,sp@(FR_SP)		| new SSP
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	rte				| and do real RTE
Lchksir:
	tstb	_ssir			| SIR pending?
	jeq	Ldorte			| no, all done
	movl	d0,sp@-			| need a scratch register
	movw	sp@(4),d0		| get SR
	andw	#PSL_IPL7,d0		| mask all but IPL
	jne	Lnosir			| came from interrupt, no can do
	movl	sp@+,d0			| restore scratch register
Lgotsir:
	movw	#SPL1,sr		| prevent others from servicing int
	tstb	_ssir			| too late?
	jeq	Ldorte			| yes, oh well...
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lsir1:
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_SSIR,sp@-		| type == software interrupt
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and all remaining registers
	addql	#8,sp			| pop SP and stack adjust
	rte
Lnosir:
	movl	sp@+,d0			| restore scratch register
Ldorte:
	rte				| real return

/* Use standard m68k support. */
#include <m68k/m68k/support.s>

/*
 * The following primitives manipulate the run queues.  _whichqs tells which
 * of the 32 queues _qs have processes in them.  Setrunqueue puts processes
 * into queues, Remrq removes them from queues.  The running process is on
 * no queue, other processes are on a queue related to p->p_priority, divided
 * by 4 actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
 */

	.globl	_whichqs,_qs,_cnt,_panic
	.globl	_curproc,_want_resched

/*
 * Setrunqueue(p)
 *
 * Call should be made at spl6(), and p->p_stat should be SRUN
 */
ENTRY(setrunqueue)
	movl	sp@(4),a0
#ifdef DIAGNOSTIC
	tstl	a0@(P_BACK)
	jne	Lset1
	tstl	a0@(P_WCHAN)
	jne	Lset1
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lset1
#endif
	clrl	d0
	movb	a0@(P_PRIORITY),d0
	lsrb	#2,d0
	movl	_whichqs,d1
	bset	d0,d1
	movl	d1,_whichqs
	lslb	#3,d0
	addl	#_qs,d0
	movl	d0,a0@(P_FORW)
	movl	d0,a1
	movl	a1@(P_BACK),a0@(P_BACK)
	movl	a0,a1@(P_BACK)
	movl	a0@(P_BACK),a1
	movl	a0,a1@(P_FORW)
	rts
#ifdef DIAGNOSTIC
Lset1:
	movl	#Lset2,sp@-
	jbsr	_panic
Lset2:
	.asciz	"setrunqueue"
	.even
#endif

/*
 * Remrq(p)
 *
 * Call should be made at spl6().
 */
ENTRY(remrunqueue)
	movl	sp@(4),a0
	movb	a0@(P_PRIORITY),d0
#ifdef DIAGNOSTIC
	lsrb	#2,d0
	movl	_whichqs,d1
	btst	d0,d1
	jeq	Lrem2
#endif
	movl	a0@(P_BACK),a1
	clrl	a0@(P_BACK)
	movl	a0@(P_FORW),a0
	movl	a0,a1@(P_FORW)
	movl	a1,a0@(P_BACK)
	cmpal	a0,a1
	jne	Lrem1
#ifndef DIAGNOSTIC
	lsrb	#2,d0
	movl	_whichqs,d1
#endif
	bclr	d0,d1
	movl	d1,_whichqs
Lrem1:
	rts
#ifdef DIAGNOSTIC
Lrem2:
	movl	#Lrem3,sp@-
	jbsr	_panic
Lrem3:
	.asciz	"remrunqueue"
	.even
#endif

Lsw0:
	.asciz	"switch"
	.even

	.globl	_curpcb
	.globl	_masterpaddr	| XXX compatibility (debuggers)
	.data
_masterpaddr:			| XXX compatibility (debuggers)
_curpcb:
	.long	0
mdpflag:
	.byte	0		| copy of proc md_flags low byte
	.align	2
	.comm	nullpcb,SIZEOF_PCB
	.text

/*
 * At exit of a process, do a switch for the last time.
 * Switch to a safe stack and PCB, and deallocate the process's resources.
 */
ENTRY(switch_exit)
	movl	sp@(4),a0
	movl	#nullpcb,_curpcb	| save state into garbage pcb
	lea	tmpstk,sp		| goto a tmp stack

	/* Free old process's resources. */
	movl	#USPACE,sp@-		| size of u-area
	movl	a0@(P_ADDR),sp@-	| address of process's u-area
	movl	_kernel_map,sp@-	| map it was allocated in
	jbsr	_kmem_free		| deallocate it
	lea	sp@(12),sp		| pop args

	jra	_cpu_switch

/*
 * When no processes are on the runq, Swtch branches to Idle
 * to wait for something to come ready.
 */
	.globl	Idle
Idle:
	stop	#PSL_LOWIPL
	movw	#PSL_HIGHIPL,sr
	movl	_whichqs,d0
	jeq	Idle
	jra	Lsw1

Lbadsw:
	movl	#Lsw0,sp@-
	jbsr	_panic
	/*NOTREACHED*/

/*
 * cpu_switch()
 *
 * NOTE: On the mc68851 we attempt to avoid flushing the
 * entire ATC.  The effort involved in selective flushing may not be
 * worth it, maybe we should just flush the whole thing?
 *
 * NOTE 2: With the new VM layout we now no longer know if an inactive
 * user's PTEs have been changed (formerly denoted by the SPTECHG p_flag
 * bit).  For now, we just always flush the full ATC.
 */
ENTRY(cpu_switch)
	movl	_curpcb,a0		| current pcb
	movw	sr,a0@(PCB_PS)		| save sr before changing ipl
#ifdef notyet
	movl	_curproc,sp@-		| remember last proc running
#endif
	clrl	_curproc

	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	movl	_whichqs,d0
	jeq	Idle
Lsw1:
	movl	d0,d1
	negl	d0
	andl	d1,d0
	bfffo	d0{#0:#32},d1
	eorib	#31,d1

	movl	d1,d0
	lslb	#3,d1			| convert queue number to index
	addl	#_qs,d1			| locate queue (q)
	movl	d1,a1
	movl	a1@(P_FORW),a0		| p = q->p_forw
	cmpal	d1,a0			| anyone on queue?
	jeq	Lbadsw			| no, panic
	movl	a0@(P_FORW),a1@(P_FORW)	| q->p_forw = p->p_forw
	movl	a0@(P_FORW),a1		| n = p->p_forw
	movl	d1,a1@(P_BACK)		| n->p_back = q
	cmpal	d1,a1			| anyone left on queue?
	jne	Lsw2			| yes, skip
	movl	_whichqs,d1
	bclr	d0,d1			| no, clear bit
	movl	d1,_whichqs
Lsw2:
	movl	a0,_curproc
	clrl	_want_resched
#ifdef notyet
	movl	sp@+,a1
	cmpl	a0,a1			| switching to same proc?
	jeq	Lswdone			| yes, skip save and restore
#endif
	/*
	 * Save state of previous process in its pcb.
	 */
	movl	_curpcb,a1
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers
	movl	usp,a2			| grab USP (a2 has been saved)
	movl	a2,a1@(PCB_USP)		| and save it

	tstl	_fputype		| If we don't have an FPU,
	jeq	Lswnofpsave		|  don't try to save it.
	lea	a1@(PCB_FPCTX),a2	| pointer to FP save area
	fsave	a2@			| save FP state
	tstb	a2@			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a2@(312)	| save FP control registers
Lswnofpsave:

#ifdef DIAGNOSTIC
	tstl	a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lbadsw
#endif
	clrl	a0@(P_BACK)		| clear back link
	movb	a0@(P_MD_FLAGS+3),mdpflag | low byte of p_md.md_flags
	movl	a0@(P_ADDR),a1		| get p_addr
	movl	a1,_curpcb

	/* see if pmap_activate needs to be called; should remove this */
	movl	a0@(P_VMSPACE),a0	| vmspace = p->p_vmspace
#ifdef DIAGNOSTIC
	tstl	a0			| map == VM_MAP_NULL?
	jeq	Lbadsw			| panic
#endif
	lea	a0@(VM_PMAP),a0		| pmap = &vmspace.vm_pmap
	tstl	a0@(PM_STCHG)		| pmap->st_changed?
	jeq	Lswnochg		| no, skip
	pea	a1@			| push pcb (at p_addr)
	pea	a0@			| push pmap
	jbsr	_pmap_activate		| pmap_activate(pmap, pcb)
	addql	#8,sp
	movl	_curpcb,a1		| restore p_addr
Lswnochg:

	lea	tmpstk,sp		| now goto a tmp stack for NMI
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lres1a			| no, skip
	.word	0xf518			| yes, pflusha
	movl	a1@(PCB_USTP),d0	| get USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
	.long	0x4e7b0806		| movc d0,urp
	jra	Lcxswdone
Lres1a:
#endif
	movl	#CACHE_CLR,d0
	movc	d0,cacr			| invalidate cache(s)
	pflusha				| flush entire TLB
	movl	a1@(PCB_USTP),d0	| get USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
	lea	_protorp,a0		| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load new user root pointer
Lcxswdone:
	moveml	a1@(PCB_REGS),#0xFCFC	| and registers
	movl	a1@(PCB_USP),a0
	movl	a0,usp			| and USP

	tstl	_fputype		| If we don't have an FPU,
	jeq	Lnofprest		|  don't try to restore it.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	tstb	a0@			| null state frame?
	jeq	Lresfprest		| yes, easy
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lresnot040		| no, skip
	clrl	sp@-			| yes...
	frestore sp@+			| ...magic!
Lresnot040:
#endif
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lresfprest:
	frestore a0@			| restore state
Lnofprest:
	movw	a1@(PCB_PS),sr		| no, restore PS
	moveq	#1,d0			| return 1 (for alternate returns)
	rts

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	movl	sp@(4),a1
	movw	sr,a1@(PCB_PS)
	movl	usp,a0			| grab USP
	movl	a0,a1@(PCB_USP)		| and save it
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers

	tstl	_fputype		| If we don't have an FPU,
	jeq	Lsvnofpsave		|  don't try to save it.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	fsave	a0@			| save FP state
	tstb	a0@			| null state frame?
	jeq	Lsvnofpsave		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a0@(312)	| save FP control registers
Lsvnofpsave:
	moveq	#0,d0			| return 0
	rts

#if defined(M68040)
ENTRY(suline)
	movl	sp@(4),a0		| address to write
	movl	_curpcb,a1		| current pcb
	movl	#Lslerr,a1@(PCB_ONFAULT) | where to return to on a fault
	movl	sp@(8),a1		| address of line
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	moveq	#0,d0			| indicate no fault
	jra	Lsldone
Lslerr:
	moveq	#-1,d0
Lsldone:
	movl	_curpcb,a1		| current pcb
	clrl	a1@(PCB_ONFAULT)	| clear fault address
	rts
#endif

/*
 * Invalidate entire TLB.
 */
ENTRY(TBIA)
__TBIA:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu3		| no, skip
	.word	0xf518			| yes, pflusha
	rts
Lmotommu3:
#endif
	tstl	_mmutype		| what mmu?
	jpl	Lmc68851a		| 68851 implies no d-cache
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
Lmc68851a:
	rts

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
ENTRY(TBIS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush entire TLB
#endif
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu4		| no, skip
	movl	sp@(4),a0
	movc	dfc,d1
	moveq	#1,d0			| user space
	movc	d0,dfc
	.word	0xf508			| pflush a0@
	moveq	#5,d0			| super space
	movc	d0,dfc
	.word	0xf508			| pflush a0@
	movc	d1,dfc
	rts
Lmotommu4:
#endif
	tstl	_mmutype		| is 68851?
	jpl	Lmc68851b		| 
	movl	sp@(4),a0		| get addr to flush
	pflush	#0,#0,a0@		| flush address from both sides
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip data cache
	rts
Lmc68851b:
	pflushs	#0,#0,a0@		| flush address from both sides
	rts

/*
 * Invalidate supervisor side of TLB
 */
ENTRY(TBIAS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu5		| no, skip
	.word	0xf518			| yes, pflusha (for now) XXX
	rts
Lmotommu5:
#endif
	pflush	#4,#4			| flush supervisor TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

/*
 * Invalidate user side of TLB
 */
ENTRY(TBIAU)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu6		| no, skip
	.word	0xf518			| yes, pflusha (for now) XXX
	rts
Lmotommu6:
#endif
	pflush	#0,#4			| flush user TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
#if defined(M68040)
ENTRY(ICPA)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	Lmotommu7		| no, skip
	.word	0xf498			| cinva ic
	rts
Lmotommu7:
#endif
	movl	#IC_CLEAR,d0
	movc	d0,cacr			| invalidate i-cache
	rts

/*
 * Invalidate data cache.
 * NOTE: we do not flush 68030 on-chip cache as there are no aliasing
 * problems with DC_WA.  The only cases we have to worry about are context
 * switch and TLB changes, both of which are handled "in-line" in resume
 * and TBI*.
 */
ENTRY(DCIA)
__DCIA:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	Lmotommu8		| no, skip
	.word	0xf478			| cpusha dc
	rts
Lmotommu8:
#endif
	rts

ENTRY(DCIS)
__DCIS:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	Lmotommu9		| no, skip
	.word	0xf478			| cpusha dc
	rts
Lmotommu9:
#endif
	rts

| Invalid single cache line
ENTRY(DCIAS)
__DCIAS:
	cmpl	#MMU_68040,_mmutype	| 68040
	jeq	Ldciasx
	movl	sp@(4),a0
	.word	0xf468			| cpushl dc,a0@
Ldciasx:
	rts

ENTRY(DCIU)
__DCIU:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	LmotommuA		| no, skip
	.word	0xf478			| cpusha dc
	rts
LmotommuA:
#endif
	rts

#if defined(M68040)
ENTRY(ICPL)
	movl	sp@(4),a0		| address
	.word	0xf488			| cinvl ic,a0@
	rts
ENTRY(ICPP)
	movl	sp@(4),a0		| address
	.word	0xf490			| cinvp ic,a0@
	rts
ENTRY(DCPL)
	movl	sp@(4),a0		| address
	.word	0xf448			| cinvl dc,a0@
	rts
ENTRY(DCPP)
	movl	sp@(4),a0		| address
	.word	0xf450			| cinvp dc,a0@
	rts
ENTRY(DCPA)
	.word	0xf458			| cinva dc
	rts
ENTRY(DCFL)
	movl	sp@(4),a0		| address
	.word	0xf468			| cpushl dc,a0@
	rts
ENTRY(DCFP)
	movl	sp@(4),a0		| address
	.word	0xf470			| cpushp dc,a0@
	rts
#endif

ENTRY(PCIA)
#if defined(M68040)
ENTRY(DCFA)
	cmpl	#MMU_68040,_mmutype	| 68040
	jne	LmotommuB		| no, skip
	.word	0xf478			| cpusha dc
	rts
LmotommuB:
#endif
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

ENTRY(ecacheon)
	rts

ENTRY(ecacheoff)
	rts

/*
 * Get callers current SP value.
 * Note that simply taking the address of a local variable in a C function
 * doesn't work because callee saved registers may be outside the stack frame
 * defined by A6 (e.g. GCC generated code).
 */
	.globl	_getsp
_getsp:
	movl	sp,d0			| get current SP
	addql	#4,d0			| compensate for return address
	rts

	.globl	_getsfc, _getdfc
_getsfc:
	movc	sfc,d0
	rts
_getdfc:
	movc	dfc,d0
	rts

/*
 * Load a new user segment table pointer.
 */
ENTRY(loadustp)
	movl	sp@(4),d0		| new USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	LmotommuC		| no, skip
	.word	0xf518			| pflusha XXX TDR
	.long	0x4e7b0806		| movc d0,urp
	rts
LmotommuC:
#endif
	pflusha				| XXX TDR
	lea	_protorp,a0		| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load root pointer
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts				|   since pmove flushes TLB

ENTRY(ploadw)
	movl	sp@(4),a0		| address to load
	ploadw	#1,a0@			| pre-load translation
	rts

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, spl0 requires special handling
 * as we need to check for our emulated software interrupts.
 */

ENTRY(spl0)
	moveq	#0,d0
	movw	sr,d0			| get old SR for return
	movw	#PSL_LOWIPL,sr		| restore new SR
	tstb	_ssir			| software interrupt pending?
	jeq	Lspldone		| no, all done
	subql	#4,sp			| make room for RTE frame
	movl	sp@(4),sp@(2)		| position return address
	clrw	sp@(6)			| set frame type 0
	movw	#PSL_LOWIPL,sp@		| and new SR
	jra	Lgotsir			| go handle it
Lspldone:
	rts

/*
 * Save and restore 68881 state.
 */
ENTRY(m68881_save)
	movl	sp@(4),a0		| save area pointer
	fsave	a0@			| save state
	tstb	a0@			| null state frame?
	jeq	Lm68881sdone		| yes, all done
	fmovem fp0-fp7,a0@(216)		| save FP general registers
	fmovem fpcr/fpsr/fpi,a0@(312)	| save FP control registers
Lm68881sdone:
	rts

ENTRY(m68881_restore)
	movl	sp@(4),a0		| save area pointer
	tstb	a0@			| null state frame?
	jeq	Lm68881rdone		| yes, easy
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lm68881rdone:
	frestore a0@			| restore state
	rts

/*
 * Handle the nitty-gritty of rebooting the machine.
 * Basically we just turn off the MMU and jump to the appropriate ROM routine.
 * XXX add support for rebooting -- that means looking at boothowto and doing
 * the right thing
 */
	.globl	_doboot
_doboot:
	lea	tmpstk,sp		| physical SP in case of NMI
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lbootnot040		| no, skip
	movl	#0,d0
	movc	d0,cacr			| caches off
	.long	0x4e7b0003		| movc d0,tc (turn off MMU)
	bra	1f
Lbootnot040:
#endif
	movl	#CACHE_OFF,d0
	movc	d0,cacr			| disable on-chip cache(s)
	movl	#0,a7@			| value for pmove to TC (turn off MMU)
	pmove	a7@,tc			| disable MMU

1:	movl	#0,d0
	movc	d0,vbr			| ROM VBR

	/*
	 * We're going down. Make various sick attempts to reset the board.
	 */
	RELOC(_cputyp, a0)
	movl	a0@,d0
	cmpw	#CPU_147,d0
	bne	not147
	movl	#0xfffe2000,a0		| MVME147: "struct vme1reg *"
	movw	a0@,d0
	movl	d0,d1
	andw	#0x0001,d1		| is VME1_SCON_SWITCH set?
	beq	1f			| not SCON. may not use SRESET.
	orw	#0x0002,d0		| ok, assert VME1_SCON_SRESET
	movw	d0,a0@
1:
	movl	#0xff800000,a0		| if we get here, SRESET did not work.
	movl	a0@(4),a0		| try jumping directly to the ROM.
	jsr	a0@
	| still alive! just return to the prom..
	bra	3f

not147:
	movl	#0xfff40000,a0		| MVME16x: "struct vme2reg *"
	movl	a0@(0x60),d0
	movl	d0,d1
	andl	#0x40000000,d1		| is VME2_TCTL_SCON set?
	beq	1f			| not SCON. may not use SRESET.
	orw	#0x00800000,d0		| ok, assert VME2_TCTL_SRST
	movl	d0,a0@(0x60)
1:
	| lets try the local bus reset
	movl	#0xfff40000,a0		| MVME16x: "struct vme2reg *"
	movl	a0@(0x104),d0
	orw	#0x00000080,d0
	movl	d0,a0@(0x104)
	| lets try jumping off to rom.
	movl	#0xff800000,a0		| if we get here, SRESET did not work.
	movl	a0@(4),a0		| try jumping directly to the ROM.
	jsr	a0@
	| still alive! just return to the prom..

3:	trap	#15
	.short	MVMEPROM_EXIT		| return to m68kbug
	/*NOTREACHED*/

	.data
	.globl	_mmutype,_protorp,_cputype,_fputype
_mmutype:
	.long	MMU_68030	| default to MMU_68030
_cputype:
	.long	CPU_68030	| default to CPU_68030
_fputype:
	.long	FPU_68881	| default to 68881 FPU
_protorp:
	.long	0,0		| prototype root pointer
	.globl	_cold
_cold:
	.long	1		| cold start flag
	.globl	_want_resched
_want_resched:
	.long	0
	.globl	_intiobase, _intiolimit, _extiobase
	.globl	_proc0paddr
_proc0paddr:
	.long	0		| KVA of proc0 u-area
_intiobase:
	.long	0		| KVA of base of internal IO space
_intiolimit:
	.long	0		| KVA of end of internal IO space
_extiobase:
	.long	0		| KVA of base of external IO space
#ifdef DEBUG
	.globl	fulltflush, fullcflush
fulltflush:
	.long	0
fullcflush:
	.long	0
#endif
/* interrupt counters */
	.globl	_intrcnt,_eintrcnt,_intrnames,_eintrnames
_intrnames:
	.asciz	"spur"
	.asciz	"lev1"
	.asciz	"lev2"
	.asciz	"lev3"
	.asciz	"lev4"
	.asciz	"clock"
	.asciz	"lev6"
	.asciz	"nmi"
	.asciz	"statclock"
_eintrnames:
	.even
_intrcnt:
	.long	0,0,0,0,0,0,0,0,0,0
_eintrcnt:

#include <mvme68k/mvme68k/vectors.s>
