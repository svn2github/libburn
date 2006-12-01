
/* libdax_msgs
   Message handling facility of libdax.
   Copyright (C) 2006 Thomas Schmitt <scdbackup@gmx.net>, provided under GPL
*/


/*
  *Never* set this macro outside libdax_msgs.c !
  The entrails of the message handling facility are not to be seen by
  the other library components or the applications.
*/
#ifdef LIBDAX_MSGS_H_INTERNAL


#ifndef LIBDAX_MSGS_SINGLE_THREADED
#include <pthread.h>
#endif


struct libdax_msgs_item {

 double timestamp;
 pid_t process_id;
 int driveno;

 int severity;
 int priority;

 /* Apply for your developer's error code range at
      libburn-hackers@pykix.org
    Report introduced codes in the list below. */
 int error_code;

 char *msg_text;
 int os_errno;
  
 struct libdax_msgs_item *prev,*next;

};


struct libdax_msgs {

 struct libdax_msgs_item *oldest;
 struct libdax_msgs_item *youngest;
 int count;

 int queue_severity;
 int print_severity;
 char print_id[81];
 
#ifndef LIBDAX_MSGS_SINGLE_THREADED
 pthread_mutex_t lock_mutex;
#endif


};

#endif /* LIBDAX_MSGS_H_INTERNAL */


#ifndef LIBDAX_MSGS_H_INCLUDED
#define LIBDAX_MSGS_H_INCLUDED 1


#ifndef LIBDAX_MSGS_H_INTERNAL


                          /* Public Opaque Handles */

/** A pointer to this is a opaque handle to a message handling facility */
struct libdax_msgs;

/** A pointer to this is a opaque handle to a single message item */
struct libdax_msgs_item;

#endif /* ! LIBDAX_MSGS_H_INTERNAL */


                            /* Public Macros */


/* Registered Severities */

/* It is well advisable to let applications select severities via strings and
   forwarded functions libdax_msgs__text_to_sev(), libdax_msgs__sev_to_text().
   These macros are for use by libdax/libburn only.
*/

/** Use this to get messages of any severity. Do not use for submitting.
*/
#define LIBDAX_MSGS_SEV_ALL                                          0x00000000

/** Debugging messages not to be visible to normal users by default
*/
#define LIBDAX_MSGS_SEV_DEBUG                                        0x10000000

/** Update of a progress report about long running actions
*/
#define LIBDAX_MSGS_SEV_UPDATE                                       0x20000000

/** Not so usual events which were gracefully handled
*/
#define LIBDAX_MSGS_SEV_NOTE                                         0x30000000

/** Possibilities to achieve a better result
*/
#define LIBDAX_MSGS_SEV_HINT                                         0x40000000

/** Warnings about problems which could not be handled optimally
*/
#define LIBDAX_MSGS_SEV_WARNING                                      0x50000000

/** Non-fatal error messages indicating that parts of the action failed
    but processing will/should go on
*/
#define LIBDAX_MSGS_SEV_SORRY                                        0x60000000

/** An error message which puts the whole operation of libdax in question
*/
#define LIBDAX_MSGS_SEV_FATAL                                        0x70000000

/** A message from an abort handler which will finally finish libburn
*/
#define LIBDAX_MSGS_SEV_ABORT                                        0x71000000

/** A severity to exclude resp. discard any possible message.
    Do not use this severity for submitting.
*/
#define LIBDAX_MSGS_SEV_NEVER                                        0x7fffffff


/* Registered Priorities */

/* Priorities are to be used by libburn/libdax only. */

#define LIBDAX_MSGS_PRIO_ZERO                                        0x00000000 
#define LIBDAX_MSGS_PRIO_LOW                                         0x10000000 
#define LIBDAX_MSGS_PRIO_MEDIUM                                      0x20000000
#define LIBDAX_MSGS_PRIO_HIGH                                        0x30000000
#define LIBDAX_MSGS_PRIO_TOP                                         0x7ffffffe

/* Do not use this priority for submitting */
#define LIBDAX_MSGS_PRIO_NEVER                                       0x7fffffff


                            /* Public Functions */

               /* Calls initiated from inside libdax/libburn */


/** Create new empty message handling facility with queue.
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return >0 success, <=0 failure
*/
int libdax_msgs_new(struct libdax_msgs **m, int flag);


/** Destroy a message handling facility and all its eventual messages.
    The submitted pointer gets set to NULL.
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return 1 for success, 0 for pointer to NULL
*/
int libdax_msgs_destroy(struct libdax_msgs **m, int flag);


/** Submit a message to a message handling facility.
    @param driveno libdax drive number. Use -1 if no number is known.
    @param error_code  Unique error code. Use only registered codes. See below.
                   The same unique error_code may be issued at different
                   occasions but those should be equivalent out of the view
                   of a libdax application. (E.g. "cannot open ATA drive"
                   versus "cannot open SCSI drive" would be equivalent.)
    @param severity The LIBDAX_MSGS_SEV_* of the event.
    @param priority The LIBDAX_MSGS_PRIO_* number of the event.
    @param msg_text Printable and human readable message text.
    @param os_errno Eventual error code from operating system (0 if none)
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return 1 on success, 0 on rejection, <0 for severe errors
*/
int libdax_msgs_submit(struct libdax_msgs *m, int driveno, int error_code,
                       int severity, int priority, char *msg_text, 
                       int os_errno, int flag);


     /* Calls from applications (to be forwarded by libdax/libburn) */


/** Convert a registered severity number into a severity name
    @param flag Bitfield for control purposes:
      bit0= list all severity names in a newline separated string
    @return >0 success, <=0 failure
*/
int libdax_msgs__sev_to_text(int severity, char **severity_name,
                             int flag);


/** Convert a severity name into a severity number,
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return >0 success, <=0 failure
*/
int libdax_msgs__text_to_sev(char *severity_name, int *severity,
                             int flag);


/** Set minimum severity for messages to be queued (default
    LIBDAX_MSGS_SEV_ALL) and for messages to be printed directly to stderr
    (default LIBDAX_MSGS_SEV_NEVER).
    @param print_id A text of at most 80 characters to be printed before
                    any eventually printed message (default is "libdax: ").
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return always 1 for now
*/
int libdax_msgs_set_severities(struct libdax_msgs *m, int queue_severity,
                               int print_severity, char *print_id, int flag);


/** Obtain a message item that has at least the given severity and priority.
    Usually all older messages of lower severity are discarded then. If no
    item of sufficient severity was found, all others are discarded from the
    queue.
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return 1 if a matching item was found, 0 if not, <0 for severe errors
*/
int libdax_msgs_obtain(struct libdax_msgs *m, struct libdax_msgs_item **item,
                       int severity, int priority, int flag);


/** Destroy a message item obtained by libdax_msgs_obtain(). The submitted
    pointer gets set to NULL.
    Caution: Copy eventually obtained msg_text before destroying the item,
             if you want to use it further.
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return 1 for success, 0 for pointer to NULL, <0 for severe errors
*/
int libdax_msgs_destroy_item(struct libdax_msgs *m,
                             struct libdax_msgs_item **item, int flag);


/** Obtain from a message item the three application oriented components as
    submitted with the originating call of libdax_msgs_submit().
    Caution: msg_text becomes a pointer into item, not a copy.
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return 1 on success, 0 on invalid item, <0 for servere errors
*/
int libdax_msgs_item_get_msg(struct libdax_msgs_item *item, 
                             int *error_code, char **msg_text, int *os_errno,
                             int flag);


/** Obtain from a message item the submitter identification submitted
    with the originating call of libdax_msgs_submit().
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return 1 on success, 0 on invalid item, <0 for servere errors
*/
int libdax_msgs_item_get_origin(struct libdax_msgs_item *item, 
                            double *timestamp, pid_t *process_id, int *driveno,
                            int flag);


/** Obtain from a message item severity and priority as submitted
    with the originating call of libdax_msgs_submit().
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return 1 on success, 0 on invalid item, <0 for servere errors
*/
int libdax_msgs_item_get_rank(struct libdax_msgs_item *item, 
                              int *severity, int *priority, int flag);


#ifdef LIDBAX_MSGS_________________


                      /* Registered Error Codes */


Format: error_code  (LIBDAX_MSGS_SEV_*,LIBDAX_MSGS_PRIO_*) = explanation
If no severity or priority are fixely associates, use "(,)".

------------------------------------------------------------------------------
Range "libdax_msgs"        :  0x00000000 to 0x0000ffff

 0x00000000 (ALL,ZERO)     = Initial setting in new libdax_msgs_item
 0x00000001 (DEBUG,ZERO)   = Test error message
 0x00000002 (DEBUG,ZERO)   = Debugging message


------------------------------------------------------------------------------
Range "elmom"              :  0x00010000 to 0x0001ffff



------------------------------------------------------------------------------
Range "scdbackup"          :  0x00020000 to 0x0002ffff

 Acessing and defending drives:

 0x00020001 (SORRY,LOW)    = Cannot open busy device
 0x00020002 (SORRY,HIGH)   = Encountered error when closing drive
 0x00020003 (SORRY,HIGH)   = Could not grab drive
 0x00020004 (NOTE,HIGH)    = Opened O_EXCL scsi sibling
 0x00020005 (FATAL,HIGH)   = Failed to open device
 0x00020006 (FATAL,HIGH)   = Too many scsi siblings
 0x00020007 (NOTE,HIGH)    = Closed O_EXCL scsi siblings
           
 General library operations:

 0x00020101 (WARNING,HIGH) = Cannot find given worker item
 0x00020102 (SORRY,HIGH)   = A drive operation is still going on
 0x00020103 (WARNING,HIGH) = After scan a drive operation is still going on
 0x00020104 (SORRY,HIGH)   = NULL pointer caught
 0x00020105 (SORRY,HIGH)   = Drive is already released
 0x00020106 (SORRY,HIGH)   = Drive is busy on attempt to close
 0x00020107 (SORRY,HIGH)   = Drive is busy on attempt to shut down library
 0x00020108 (SORRY,HIGH)   = Drive is not grabbed on disc status inquiry
 0x00020108 (FATAL,HIGH)   = Could not allocate new drive object
 0x00020109 (FATAL,HIGH)   = Library not running
 0x0002010a (FATAL,HIGH)   = Unsuitable track mode
 0x0002010b (FATAL,HIGH)   = Burn run failed
 0x0002010c (FATAL,HIGH)   = Failed to transfer command to drive
 0x0002010d (DEBUG,HIGH)   = Could not inquire TOC
 0x0002010e (FATAL,HIGH)   = Attempt to read ATIP from ungrabbed drive
 0x0002010f (DEBUG,HIGH)   = SCSI error condition on command
 0x00020110 (FATAL,HIGH)   = Persistent drive address too long
 0x00020111 (FATAL,HIGH)   = Could not allocate new auxiliary object
 0x00020112 (SORRY,HIGH)   = Bad combination of write_type and block_type
 0x00020113 (FATAL,HIGH)   = Drive capabilities not inquired yet
 0x00020114 (SORRY,HIGH)   = Attempt to set ISRC with bad data
 0x00020115 (SORRY,HIGH)   = Attempt to set track mode to unusable value
 0x00020116 (FATAL,HIGH)   = Track mode has unusable value
 0x00020117 (FATAL,HIGH)   = toc_entry of drive is already in use
 0x00020118 (DEBUG,HIGH)   = Closing track
 0x00020119 (DEBUG,HIGH)   = Closing session
 0x0002011a (NOTE,HIGH)    = Padding up track to minimum size
 0x0002011b (FATAL,HIGH)   = Attempt to read track info from ungrabbed drive
 0x0002011c (FATAL,HIGH)   = Attempt to read track info from busy drive
 0x0002011d (FATAL,HIGH)   = SCSI error condition on write
 0x0002011e (SORRY, HIGH)  = Unsuitable media detected

 libdax_audioxtr:
 0x00020200 (SORRY,HIGH)   = Cannot open audio source file
 0x00020201 (SORRY,HIGH)   = Audio source file has unsuitable format
 0x00020202 (SORRY,HIGH)   = Failed to prepare reading of audio data
 

------------------------------------------------------------------------------

#endif /* LIDBAX_MSGS_________________ */



#ifdef LIBDAX_MSGS_H_INTERNAL

                             /* Internal Functions */


/** Lock before doing side effect operations on m */
static int libdax_msgs_lock(struct libdax_msgs *m, int flag);

/** Unlock after effect operations on m are done */
static int libdax_msgs_unlock(struct libdax_msgs *m, int flag);


/** Create new empty message item.
    @param link Previous item in queue
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return >0 success, <=0 failure
*/
static int libdax_msgs_item_new(struct libdax_msgs_item **item, 
                                struct libdax_msgs_item *link, int flag);

/** Destroy a message item obtained by libdax_msgs_obtain(). The submitted
    pointer gets set to NULL.
    @param flag Bitfield for control purposes (unused yet, submit 0)
    @return 1 for success, 0 for pointer to NULL
*/
static int libdax_msgs_item_destroy(struct libdax_msgs_item **item, int flag);


#endif /* LIBDAX_MSGS_H_INTERNAL */


#endif /* ! LIBDAX_MSGS_H_INCLUDED */
