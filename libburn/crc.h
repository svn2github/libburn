/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef BURN__CRC_H
#define BURN__CRC_H


#ifdef Xorriso_standalonE
/* Do not use source module crc.c of yet unclear ancestry in GNU xorriso */
#ifndef Libburn_no_crc_C
#define Libburn_no_crc_C 1
#endif
#endif


#ifndef Libburn_no_crc_C

unsigned short crc_ccitt(unsigned char *, int len);
unsigned int crc_32(unsigned char *, int len);

#endif /* Libburn_no_crc_C */


#endif /* BURN__CRC_H */
