
/* sg.c
   Switcher for operating system dependent transport level modules of libburn.
   Copyright (C) 2009 Thomas Schmitt <scdbackup@gmx.net>, provided under GPL
*/


#ifdef __FreeBSD__

#include "sg-freebsd.c"

#else
#ifdef __linux

#include "sg-linux.c"

#else

#include "sg-dummy.c"

#endif /* ! __linux */
#endif /* ! __FreeBSD__ */

