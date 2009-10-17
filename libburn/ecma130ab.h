
/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/* ts A91016 : libburn/ecma130ab.h is the replacement for old libburn/lec.h

   Copyright 2009, Thomas Schmitt <scdbackup@gmx.net>, libburnia-project.org

   This code module implements the computations prescribed in ECMA-130 Annex A
   and B. For explanations of the underlying mathematics see ecma130ab.c .

*/

#ifndef Libburn_ecma130ab_includeD
#define Libburn_ecma130ab_includeD 1

int burn_rspc_parity_p(unsigned char *sector);

int burn_rspc_parity_q(unsigned char *sector);

int burn_ecma130_scramble(unsigned char *sector);

#endif /* ! Libburn_ecma130ab_includeD */

