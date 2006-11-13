/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef BURN__CRC_H
#define BURN__CRC_H

unsigned short crc_ccitt(unsigned char *, int len);
unsigned int crc_32(unsigned char *, int len);

#endif /* BURN__CRC_H */
