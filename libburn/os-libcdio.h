
/* os-libcdio.h
   Operating system specific libburn definitions and declarations. Included
   by os.h in case of compilation for
           Unknown X/Open-like systems
           with GNU libcdio MMC transport adapter sg-libcdio.c

   Copyright (C) 2009 Thomas Schmitt <scdbackup@gmx.net>, provided under GPL
*/


/** List of all signals which shall be caught by signal handlers and trigger
    a graceful abort of libburn. (See man 7 signal.)
*/
/* Once as system defined macros */
#define BURN_OS_SIGNAL_MACRO_LIST \
 SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, \
 SIGFPE, SIGSEGV, SIGPIPE, SIGALRM, SIGTERM, \
 SIGUSR1, SIGUSR2, SIGXCPU, SIGTSTP, SIGTTIN, \
 SIGTTOU

/* Once as text 1:1 list of strings for messages and interpreters */
#define BURN_OS_SIGNAL_NAME_LIST \
 "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGABRT", \
 "SIGFPE", "SIGSEGV", "SIGPIPE", "SIGALRM", "SIGTERM", \
 "SIGUSR1", "SIGUSR2", "SIGXCPU", "SIGTSTP", "SIGTTIN", \
 "SIGTTOU"

/* The number of above list items */
#define BURN_OS_SIGNAL_COUNT 16

/** To list all signals which shall surely not be caught */
#define BURN_OS_NON_SIGNAL_MACRO_LIST \
SIGKILL, SIGCHLD, SIGSTOP

/* The number of above list items */
#define BURN_OS_NON_SIGNAL_COUNT 3


/* The maximum size for a (SCSI) i/o transaction */
/* Important : MUST be at least 32768 ! */
/* (It might be risky to use 64k. FreeBSD is said to can only 32k.) */
/* On Linux kernel 2.6.18 when stream recording 2x BD-RE 
   this would bring about 10 % more speed:
     BURN_OS_TRANSPORT_BUFFER_SIZE 65536
*/
#define BURN_OS_TRANSPORT_BUFFER_SIZE 32768


/* To hold the position of the most recently delivered address from
   device enumeration.
*/
struct burn_drive_enumerator_struct {
	char **ppsz_cd_drives;
	char **pos;
};

#define BURN_OS_DEFINE_DRIVE_ENUMERATOR_T \
typedef struct burn_drive_enumerator_struct burn_drive_enumerator_t;


/* The list of operating system dependent elements in struct burn_drive.
   Usually they are initialized in  sg-*.c:enumerate_common().
*/
#define BURN_OS_TRANSPORT_DRIVE_ELEMENTS \
	void *p_cdio; /* actually a pointer to CdIo_t */ \
	char libcdio_name[4096]; /* The drive path as used by libcdio */ \

