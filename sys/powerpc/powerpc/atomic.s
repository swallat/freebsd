/*
 * Copyright (c) 2000, 2001 Benno Rice
 * All rights reserved.
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
 * $FreeBSD$
 */

#include <machine/asm.h>
	
	.text
	
ASENTRY(atomic_set_8)
0:	lwarx	0, 0, 3		/* load old value */
	slwi	4, 4, 24	/* shift the byte so it's in the right place */
	or	0, 0, 4		/* generate new value */
	stwcx.	0, 0, 3		/* attempt to store */
	bne-	0		/* loop if failed */
	eieio			/* synchronise */
	sync
	blr			/* return */

ASENTRY(atomic_clear_8)
0:	lwarx 	0, 0, 3		/* load old value */
	slwi	4, 4, 24	/* shift the byte so it's in the right place */
	andc	0, 0, 4		/* generate new value */
	stwcx.	0, 0, 3		/* attempt to store */
	bne-	0		/* loop if failed */
	eieio			/* synchronise */
	sync
	blr			/* return */

ASENTRY(atomic_add_8)
0:	lwarx	9, 0, 3		/* load old value */
	srwi	0, 9, 24	/* byte alignment */
	add	0, 4, 0		/* calculate new value */
	slwi	0, 9, 24	/* byte alignment */
	clrlwi	9, 9, 8		/* clear the byte in the original word */
	or	9, 9, 0		/* copy back in to the original word */
	stwcx.	9, 0, 3		/* attempt to store */
	bne-	0		/* loop if failed */
	eieio			/* synchronise */
	sync
	blr			/* return */

ASENTRY(atomic_subtract_8)
0:	lwarx	9, 0, 3		/* load old value */
	srwi	0, 9, 24	/* byte alignment */
	subf	0, 4, 0		/* calculate new value */
	slwi	0, 9, 24	/* byte alignment */
	clrlwi	9, 9, 8		/* clear the byte in the original word */
	or	9, 9, 0		/* copy back in to the original word */
	stwcx.	9, 0, 3		/* attempt to store */
	bne-	0		/* loop if failed */
	eieio			/* synchronise */
	sync
	blr			/* return */

ASENTRY(atomic_set_16)
0:	lwarx	0, 0, 3		/* load old value */
	slwi	4, 4, 16	/* realign operand */
	or	0, 0, 4		/* calculate new value */
	stwcx.	0, 0, 3		/* attempt to store */
	bne-	0		/* loop if failed */
	eieio			/* synchronise */
	sync
	blr			/* return */

ASENTRY(atomic_clear_16)
0:	lwarx	0, 0, 3		/* load old value */
	slwi	4, 4, 16	/* realign operand */
	andc	0, 0, 4		/* calculate new value */
	stwcx.	0, 0, 3		/* attempt to store */
	bne-	0		/* loop if failed */
	eieio			/* synchronise */
	sync
	blr			/* return */

ASENTRY(atomic_add_16)
0:	lwarx	0, 0, 3		/* load old value */
	srwi	0, 9, 16	/* realign */
	add	0, 4, 0		/* calculate new value */
	slwi	0, 0, 16	/* realign */
	clrlwi	9, 9, 16	/* clear old value */
	or	9, 9, 0		/* copy in new value */
	stwcx.	0, 0, 3		/* attempt to store */
	bne-	0		/* loop if failed */
	eieio			/* synchronise */
	sync
	blr			/* return */

ASENTRY(atomic_subtract_16)
0:	lwarx	0, 0, 3		/* load old value */
	srwi	0, 9, 16	/* realign */
	subf	0, 4, 0		/* calculate new value */
	slwi	0, 0, 16	/* realign */
	clrlwi	9, 9, 16	/* clear old value */
	or	9, 9, 0		/* copy in new value */
	stwcx.	0, 0, 3		/* attempt to store */
	bne-	0		/* loop if failed */
	eieio			/* synchronise */
	sync
	blr			/* return */
