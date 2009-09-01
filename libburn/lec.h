/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef __LEC
#define __LEC

#ifndef Libburn_disable_lec_C

#define RS_L12_BITS 8

void scramble(unsigned char *);
void parity_p(unsigned char *in);
void parity_q(unsigned char *in);

#endif /* ! Libburn_disable_lec_C */

#endif /* __LEC */
