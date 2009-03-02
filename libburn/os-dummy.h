
/* os-dummy.h
   Operating system specific libburn definitions and declarations. Included
   by os.h in case of compilation for
           Unknown POSIX like systems
           with the dummy MMC transport adapter sg-dummy.c

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
#define BURN_OS_TRANSPORT_BUFFER_SIZE 65536


/* To hold the position of the most recently delivered address from
   device enumeration.
*/
struct burn_drive_enumerator_struct {
	int pos;
	int info_count;
	char **info_list;
};

#define BURN_OS_DEFINE_DRIVE_ENUMERATOR_T \
typedef struct burn_drive_enumerator_struct burn_drive_enumerator_t;


/* The list of operating system dependent elements in struct burn_drive.
   Usually they are initialized in  sg-*.c:enumerate_common().
*/
#define BURN_OS_TRANSPORT_DRIVE_ELEMENTS \
	int just_a_dummy;

