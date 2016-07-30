
/* os-netbsd.h
   Operating system specific libburn definitions and declarations. Included
   by os.h in case of compilation for
           NetBSD 6 or OpenBSD 5.9
           with  MMC transport adapter sg-netbsd.c

   Copyright (C) 2010 - 2016 Thomas Schmitt <scdbackup@gmx.net>
   provided under GPLv2+

   Derived 2014 from libburn/os-solaris.c
   Adapted 2016 to OpenBSD by help of SASANO Takayoshi <uaa@mx5.nisiq.net>
*/


/** List of all signals which shall be caught by signal handlers and trigger
    a graceful abort of libburn. (See man signal.h)
*/
/* Once as system defined macros */
#define BURN_OS_SIGNAL_MACRO_LIST \
 SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, \
 SIGABRT, SIGEMT, SIGFPE, SIGBUS, SIGSEGV, \
 SIGSYS, SIGPIPE, SIGALRM, SIGTERM, SIGXCPU, \
 SIGXFSZ, SIGVTALRM, SIGPROF, SIGUSR1, SIGUSR2

/* Once as text 1:1 list of strings for messages and interpreters */
#define BURN_OS_SIGNAL_NAME_LIST \
 "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", \
 "SIGABRT", "SIGEMT", "SIGFPE", "SIGBUS", "SIGSEGV", \
 "SIGSYS", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGXCPU", \
 "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGUSR1", "SIGUSR2"

/* The number of above list items */
#define BURN_OS_SIGNAL_COUNT 20

#ifdef __OpenBSD__

/** To list all signals which shall surely not be caught */
#define BURN_OS_NON_SIGNAL_MACRO_LIST \
 SIGKILL, SIGURG, SIGSTOP, SIGTSTP, SIGCONT, \
 SIGCHLD, SIGTTIN, SIGTTOU, SIGIO, SIGWINCH, \
 SIGINFO

/* The number of above list items */
#define BURN_OS_NON_SIGNAL_COUNT 11

/* ts B60730 */
/* Either OpenBSD or SASANO Takayoshi's LG BH14NS48 throw 2,0,0
   on Immed bit with BLANK and SYNCHRONIZE CACHE.
   Until it is clear that the drive is to blame, the OpenBSD default is
   not to use Immed.

   This may be overridden at ./configure time by
     export CFLAGS
     CFLAGS="$CFLAGS -DLibburn_do_no_immed_defaulT=0"
*/
#ifndef Libburn_do_no_immed_defaulT
#define Libburn_do_no_immed_defaulT 1
#endif

#else /* __OpenBSD__ */

/** To list all signals which shall surely not be caught */
#define BURN_OS_NON_SIGNAL_MACRO_LIST \
 SIGKILL, SIGURG, SIGSTOP, SIGTSTP, SIGCONT, \
 SIGCHLD, SIGTTIN, SIGTTOU, SIGIO, SIGWINCH, \
 SIGINFO, SIGPWR

/* The number of above list items */
#define BURN_OS_NON_SIGNAL_COUNT 12

#endif /* ! __OpenBSD__ */

/* The maximum size for a (SCSI) i/o transaction */
/* Important : MUST be at least 32768 ! */
/* My Blu-ray burner LG GGW-H20 writes junk if stream recording is combined
   with buffer size 32 kB. So stream recording is allowed only with size 64k.
*/
/* >>> ??? Does it do 64 kB ? */
#define BURN_OS_TRANSPORT_BUFFER_SIZE 65536


/* To hold the position of the most recently delivered address from
   device enumeration.
*/
struct burn_drive_enumerator_struct {
	int cdno;
};

#define BURN_OS_DEFINE_DRIVE_ENUMERATOR_T \
typedef struct burn_drive_enumerator_struct burn_drive_enumerator_t;


/* The list of operating system dependent elements in struct burn_drive.
   Usually they are initialized in  sg-*.c:enumerate_common().
*/
#define BURN_OS_TRANSPORT_DRIVE_ELEMENTS \
	int fd;

