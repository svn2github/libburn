
/* os-freebsd.h
   Operating system specific libburn definitions and declarations. Included
   by os.h in case of compilation for
                                  FreeBSD with CAM

   Copyright (C) 2006 Thomas Schmitt <scdbackup@gmx.net>, provided under GPL
*/

#ifndef BURN_OS_H_INCLUDED
#define BURN_OS_H_INCLUDED 1


/** List of all signals which shall be caught by signal handlers and trigger
    a graceful abort of libburn. (See man 7 signal.)
*/
/* Once as system defined macros */
#define BURN_OS_SIGNAL_MACRO_LIST \
 SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGABRT, \
 SIGFPE, SIGSEGV, SIGPIPE, SIGALRM, SIGTERM, \
 SIGUSR1, SIGUSR2, SIGXCPU, SIGTSTP, SIGTTIN, \
 SIGTTOU, \
 SIGBUS, SIGPROF, SIGSYS, SIGTRAP, \
 SIGVTALRM, SIGXCPU, SIGXFSZ 

/* Once as text 1:1 list of strings for messages and interpreters */
#define BURN_OS_SIGNAL_NAME_LIST \
 "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGABRT", \
 "SIGFPE", "SIGSEGV", "SIGPIPE", "SIGALRM", "SIGTERM", \
 "SIGUSR1", "SIGUSR2", "SIGXCPU", "SIGTSTP", "SIGTTIN", \
 "SIGTTOU", \
 "SIGBUS", "SIGPROF", "SIGSYS", "SIGTRAP", \
 "SIGVTALRM", "SIGXCPU", "SIGXFSZ"

/* The number of above list items */
#define BURN_OS_SIGNAL_COUNT 23

/** To list all signals which shall surely not be caught */
#define BURN_OS_NON_SIGNAL_MACRO_LIST \
SIGKILL, SIGCHLD, SIGSTOP, SIGURG

/* The number of above list items */
#define BURN_OS_NON_SIGNAL_COUNT 4


/* The maximum size for a (SCSI) i/o transaction */
#define BURN_OS_TRANSPORT_BUFFER_SIZE 65536/2


/** To hold all state information of BSD device enumeration
    which are now local in sg_enumerate() . So that sg_give_next_adr()
    can work in BSD and sg_enumerate() can use it.
*/
#define BURN_OS_DEFINE_DRIVE_ENUMERATOR_T \
struct burn_drive_enumeration_state { \
        union ccb ccb; \
        int bufsize, fd; \
        unsigned int i; \
        int skip_device; \
}; \
typedef struct burn_drive_enumeration_state burn_drive_enumerator_t;


/* The list of operating system dependent elements in struct burn_drive.
   To be initialized and used within sg-*.c .
*/
#define BURN_OS_TRANSPORT_DRIVE_ELEMENTS \
struct cam_device* cam;


#endif /* ! BURN_OS_H_INCLUDED */

