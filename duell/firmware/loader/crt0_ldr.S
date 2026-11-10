/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca update loader entry. The SPL decompresses the loader to its load
 * address (0x01C0A800) and calls its first byte. Interrupts off,
 * our own stacks, clear .bss, then C. Never returns. */
	.section .text.crt0,"ax",@progbits
	.globl _start
_start:
	goto 1f
	.ascii "FELUCCA-LOADER-1"	/* 16 bytes: the shared FM-1 update loader tag (2026-11-10, owner: keep it for now): Melodee, CORALROOT, JIANT FM, fm1-x0x, fm1-fomni and the others start an update only with this tag, so DUELL installs from any of them. DUELL itself no longer checks it (ota.c). Never shown to users */
	.balign 2
1:	cli
	sp = _stack_top
	ssp = _sstack_top
	call ldr_cstart
	goto .
