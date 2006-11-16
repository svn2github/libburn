
/* os.h
   Operating system specific libburn definitions and declarations.
   The macros defined here are used by libburn modules in order to
   avoid own system dependent case distinctions.
   Copyright (C) 2006 Thomas Schmitt <scdbackup@gmx.net>, provided under GPL
*/

#ifndef BURN_OS_H_INCLUDED
#define BURN_OS_H_INCLUDED 1

/*
   Operating system case distinction
*/

#ifdef __FreeBSD__


/* ----------------------------- FreeBSD with CAM -------------------------- */
#include "os-freebsd.h"


#else  /* operating system case distinction */


/* --------- Linux kernels 2.4 and 2.6 with Linux SCSI Generic (sg) -------- */
#include "os-linux.h"


#endif /* End of operating system case distinction */


#endif /* ! BURN_OS_H_INCLUDED */

