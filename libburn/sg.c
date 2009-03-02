
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

/* The dummy adapter formally fulfills the expectations of libburn towards
   its SCSI command transport. It will show no drives and perform no SCSI
   commands.
   libburn will then be restricted to using its stdio pseudo drives.
*/
static int intentional_compiler_warning(void)
{
 int INTENTIONAL_COMPILER_WARNING_;
 int Cannot_recognize_Linux_nor_FreeBSD_;
 int Have_to_use_dummy_MMC_transport_adapter_;
 int This_libburn_will_not_be_able_to_operate_on_real_CD_drives;
 int Have_to_use_dummy_MMC_transport_adapter;
 int Cannot_recognize_Linux_nor_FreeBSD;
 int INTENTIONAL_COMPILER_WARNING;

 return(0);
}

#include "sg-dummy.c"

#endif /* ! __linux */
#endif /* ! __FreeBSD__ */

