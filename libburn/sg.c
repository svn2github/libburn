
/* sg.c
   Switcher for operating system dependent transport level modules of libburn.
   Copyright (C) 2006 Thomas Schmitt <scdbackup@gmx.net>, provided under GPL
*/


#ifdef __FreeBSD__

#include "sg-freebsd.c"

#else

#include "sg-linux.c"

#endif

