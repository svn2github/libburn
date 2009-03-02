
/* os.h
   Operating system specific libburn definitions and declarations.
   The macros defined here are used by libburn modules in order to
   avoid own system dependent case distinctions.
   Copyright (C) 2009 Thomas Schmitt <scdbackup@gmx.net>, provided under GPL
*/

#ifndef BURN_OS_H_INCLUDED
#define BURN_OS_H_INCLUDED 1

/*
   Operating system case distinction
*/

#ifdef __FreeBSD__


/* ----------------------------- FreeBSD with CAM -------------------------- */
#include "os-freebsd.h"


#else
#ifdef __linux


/* --------- Linux kernels 2.4 and 2.6 with Linux SCSI Generic (sg) -------- */
#include "os-linux.h"


#else


/* --------- Any other system. With dummy MMC transport sg-dummy.c --------- */
#include "os-dummy.h"


#endif /* ! __linux */
#endif /* ! __FreeBSD__ */


#endif /* ! BURN_OS_H_INCLUDED */

