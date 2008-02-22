/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#include <unistd.h>

/* ts A61007 */
/* #include <a ssert.h> */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

/* ts A70928 : init.h is for others, not for init .c
#include "init.h"
*/


#include "sg.h"
#include "error.h"
#include "libburn.h"
#include "drive.h"
#include "transport.h"

/* ts A60825 : The storage location for back_hacks.h variables. */
#define BURN_BACK_HACKS_INIT 1
#include "back_hacks.h"

/* ts A60924 : a new message handling facility */
#include "libdax_msgs.h"
struct libdax_msgs *libdax_messenger= NULL;


int burn_running = 0;

/* ts A60813 : Linux: wether to use O_EXCL on open() of device files */
int burn_sg_open_o_excl = 1;

/* ts A70403 : Linux: wether to use fcntl(,F_SETLK,)
                      after open() of device files  */
int burn_sg_fcntl_f_setlk = 1;

/* ts A70314 : Linux: what device family to use :
    0= default family
    1= sr
    2= scd
   (3= st)
    4= sg
*/
int burn_sg_use_family = 0;

/* O_NONBLOCK was hardcoded in enumerate_ata() which i hardly use.
   For enumerate_sg() it seems ok.
   So it should stay default mode until enumerate_ata() without O_NONBLOCK
   has been thoroughly tested. */
int burn_sg_open_o_nonblock = 1;

/* wether to take a busy drive as an error */
/* Caution: this is implemented by a rough hack and eventually leads
	    to unconditional abort of the process  */
int burn_sg_open_abort_busy = 0;

/* ts A61002 */

#include "cleanup.h"

/* Parameters for builtin abort handler */
static char abort_message_prefix[81] = {"libburn : "};
static pid_t abort_control_pid= 0;
volatile int burn_global_abort_level= 0;
int burn_global_abort_signum= 0;
void *burn_global_signal_handle = NULL;
burn_abort_handler_t burn_global_signal_handler = NULL;


/* ts A70223 : wether implemented untested profiles are supported */
int burn_support_untested_profiles = 0;


/* ts A60925 : ticket 74 */
/** Create the messenger object for libburn. */
int burn_msgs_initialize(void)
{
	int ret;

	if(libdax_messenger == NULL) {
		ret = libdax_msgs_new(&libdax_messenger,0);
		if (ret <= 0)
			return 0;
	}
	libdax_msgs_set_severities(libdax_messenger, LIBDAX_MSGS_SEV_NEVER,
				   LIBDAX_MSGS_SEV_FATAL, "libburn: ", 0);
	return 1;
}

/* ts A60924 : ticket 74 : Added use of global libdax_messenger */
int burn_initialize(void)
{
	int ret;

	if (burn_running)
		return 1;
	burn_support_untested_profiles = 0;
	ret = burn_msgs_initialize();
	if (ret <= 0)
		return 0;
	burn_running = 1;
	return 1;
}

void burn_finish(void)
{
	/* ts A61007 : assume no messageing system */
	/* a ssert(burn_running); */
	if (!burn_running)
		return;

	/* ts A61007 */
	/* burn_wait_all(); */
	if (!burn_drives_are_clear(0)) {
		libdax_msgs_submit(libdax_messenger, -1, 0x00020107,
			LIBDAX_MSGS_SEV_WARNING, LIBDAX_MSGS_PRIO_HIGH,
			"A drive is still busy on shutdown of library", 0, 0);
		usleep(1000001);
		burn_abort(4440, burn_abort_pacifier, abort_message_prefix);
	}

	/* ts A60904 : ticket 62, contribution by elmom : name addon "_all" */
	burn_drive_free_all();

	/* ts A60924 : ticket 74 */
	libdax_msgs_destroy(&libdax_messenger,0);

	burn_running = 0;
}


/* ts A60813 */
/** API function. See libburn.h */
void burn_preset_device_open(int exclusive, int blocking, int abort_on_busy)
{
	/* ts A61007 */
	/* a ssert(burn_running); */
	if (!burn_running)
		return;	
	burn_sg_open_o_excl = exclusive & 3;
	burn_sg_fcntl_f_setlk = !!(exclusive & 32);
	burn_sg_use_family = (exclusive >> 2) & 7;
	burn_sg_open_o_nonblock = !blocking;
	burn_sg_open_abort_busy = !!abort_on_busy;
}


/* ts A60924 : ticket 74 */
/** Control queueing and stderr printing of messages from libburn.
    Severity may be one of "NEVER", "FATAL", "SORRY", "WARNING", "HINT",
    "NOTE", "UPDATE", "DEBUG", "ALL".
    @param queue_severity Gives the minimum limit for messages to be queued.
                          Default: "NEVER". If you queue messages then you
                          must consume them by burn_msgs_obtain().
    @param print_severity Does the same for messages to be printed directly
                          to stderr.
    @param print_id       A text prefix to be printed before the message.
    @return               >0 for success, <=0 for error

*/
int burn_msgs_set_severities(char *queue_severity,
                             char *print_severity, char *print_id)
{
	int ret, queue_sevno, print_sevno;

	ret = libdax_msgs__text_to_sev(queue_severity, &queue_sevno, 0);
	if (ret <= 0)
		return 0;
	ret = libdax_msgs__text_to_sev(print_severity, &print_sevno, 0);
	if (ret <= 0)
		return 0;
	ret = libdax_msgs_set_severities(libdax_messenger, queue_sevno,
					 print_sevno, print_id, 0);
	if (ret <= 0)
		return 0;
	return 1;
}


/* ts A60924 : ticket 74 */
#define BURM_MSGS_MESSAGE_LEN 4096

/** Obtain the oldest pending libburn message from the queue which has at
    least the given minimum_severity. This message and any older message of
    lower severity will get discarded from the queue and is then lost forever.
    Severity may be one of "NEVER", "FATAL", "SORRY", "WARNING", "HINT",
    "NOTE", "UPDATE", "DEBUG", "ALL". To call with minimum_severity "NEVER"
    will discard the whole queue.
    @param error_code Will become a unique error code as liste in 
                      libburn/libdax_msgs.h
    @param msg_text   Must provide at least BURM_MSGS_MESSAGE_LEN bytes.
    @param os_errno   Will become the eventual errno related to the message
    @param severity   Will become the severity related to the message and
                      should provide at least 80 bytes.
    @return 1 if a matching item was found, 0 if not, <0 for severe errors
*/
int burn_msgs_obtain(char *minimum_severity,
                     int *error_code, char msg_text[], int *os_errno,
                     char severity[])
{
	int ret, minimum_sevno, sevno, priority;
	char *textpt, *sev_name;
	struct libdax_msgs_item *item = NULL;

	ret = libdax_msgs__text_to_sev(minimum_severity, &minimum_sevno, 0);
	if (ret <= 0)
		return 0;
	if (libdax_messenger == NULL)
		return 0;
	ret = libdax_msgs_obtain(libdax_messenger, &item, minimum_sevno,
				LIBDAX_MSGS_PRIO_ZERO, 0);
	if (ret <= 0)
		goto ex;
	ret = libdax_msgs_item_get_msg(item, error_code, &textpt, os_errno, 0);
	if (ret <= 0)
		goto ex;
	strncpy(msg_text, textpt, BURM_MSGS_MESSAGE_LEN-1);
	if(strlen(textpt) >= BURM_MSGS_MESSAGE_LEN)
		msg_text[BURM_MSGS_MESSAGE_LEN-1] = 0;

	severity[0]= 0;
	ret = libdax_msgs_item_get_rank(item, &sevno, &priority, 0);
	if(ret <= 0)
		goto ex;
	ret = libdax_msgs__sev_to_text(sevno, &sev_name, 0);
	if(ret <= 0)
		goto ex;
	strcpy(severity,sev_name);

	ret = 1;
ex:
	libdax_msgs_destroy_item(libdax_messenger, &item, 0);
	return ret;
}


/* ts A70922 : API */
int burn_msgs_submit(int error_code, char msg_text[], int os_errno,
			char severity[], struct burn_drive *d)
{
	int ret, sevno, global_index = -1;

	ret = libdax_msgs__text_to_sev(severity, &sevno, 0);
	if (ret <= 0)
		sevno = LIBDAX_MSGS_SEV_ALL;
	if (error_code <= 0) {
		switch(sevno) {
		       case LIBDAX_MSGS_SEV_ABORT:   error_code = 0x00040000;
		break; case LIBDAX_MSGS_SEV_FATAL:   error_code = 0x00040001;
		break; case LIBDAX_MSGS_SEV_SORRY:   error_code = 0x00040002;
		break; case LIBDAX_MSGS_SEV_WARNING: error_code = 0x00040003;
		break; case LIBDAX_MSGS_SEV_HINT:    error_code = 0x00040004;
		break; case LIBDAX_MSGS_SEV_NOTE:    error_code = 0x00040005;
		break; case LIBDAX_MSGS_SEV_UPDATE:  error_code = 0x00040006;
		break; case LIBDAX_MSGS_SEV_DEBUG:   error_code = 0x00040007;
		break; default:                      error_code = 0x00040008;
		}
	}
	if (d != NULL)
		global_index = d->global_index;
	ret = libdax_msgs_submit(libdax_messenger, global_index, error_code,
			sevno, LIBDAX_MSGS_PRIO_HIGH, msg_text, os_errno, 0);
	return ret;
}


/* ts A71016 API */
int burn_text_to_sev(char *severity_name, int *sevno, int flag)
{
	int ret;

	ret = libdax_msgs__text_to_sev(severity_name, sevno, 0);
	return ret;
}


/* ts A80202 API */
int burn_sev_to_text(int severity_number, char **severity_name, int flag)
{
	int ret;

	ret = libdax_msgs__sev_to_text(severity_number, severity_name, 0);
	return ret;
}


int burn_builtin_abort_handler(void *handle, int signum, int flag)
{

#define Libburn_new_thread_signal_handleR 1
/*
#define Libburn_signal_handler_verbouS 1
*/
	int ret;
	struct burn_drive *d;

#ifdef Libburn_signal_handler_verbouS
	fprintf(stderr,
		"libburn_ABORT: pid = %d , abort_control_pid = %d , sig= %d\n",
		getpid(), abort_control_pid, signum);
#endif

	/* ts A70928:
	Must be quick. Allowed to coincide with other thread and to share
	the increment with that one. It must not decrease, though, and
	yield at least 1 if any thread calls this function.
	*/
	burn_global_abort_level++;
	burn_global_abort_signum= signum;

	if(getpid() != abort_control_pid) {

#ifdef Libburn_new_thread_signal_handleR

		ret = burn_drive_find_by_thread_pid(&d, getpid());
		if (ret > 0 && d->busy == BURN_DRIVE_WRITING) {
					/* This is an active writer thread */

#ifdef Libburn_signal_handler_verbouS
			fprintf(stderr, "libburn_ABORT: pid %d found drive busy with writing, (level= %d)\n", (int) getpid(), burn_global_abort_level);
#endif

			d->sync_cache(d);

			/* >>> perform a more qualified end of burn process */;

			d->busy = BURN_DRIVE_IDLE;

			if (burn_global_abort_level > 0) {
				/* control process did not show up yet */
#ifdef Libburn_signal_handler_verbouS
					fprintf(stderr, "libburn_ABORT: pid %d sending signum %d to pid %d\n", (int) getpid(), (int) signum, (int) abort_control_pid);
#endif
					kill(abort_control_pid, signum);
			}

#ifdef Libburn_signal_handler_verbouS
					fprintf(stderr, "libburn_ABORT: pid %d signum %d returning -2\n", (int) getpid(), (int) signum);
#endif

			return -2;
		} else {
			usleep(1000000); /* calm down */
			return -2;
		}

#else
		usleep(1000000); /* calm down */
		return -2;
#endif /* ! Libburn_new_thread_signal_handleR */

	}
	burn_global_abort_level = -1;
	Cleanup_set_handlers(NULL, NULL, 2);
	fprintf(stderr,"%sABORT : Trying to shut down drive and library\n",
		abort_message_prefix);
	fprintf(stderr,
		"%sABORT : Wait the normal burning time before any kill -9\n",
		abort_message_prefix);
	close(0); /* somehow stdin as input blocks abort until EOF */

	burn_abort(4440, burn_abort_pacifier, abort_message_prefix);

	fprintf(stderr,
	"\n%sABORT : Program done. Even if you do not see a shell prompt.\n\n",
		abort_message_prefix);
	burn_global_abort_level = -2;
	return(1);
}

void burn_set_signal_handling(void *handle, burn_abort_handler_t handler,
			     int mode)
{
	if(handler == NULL && mode == 0) {
		handler = burn_builtin_abort_handler;
/*
		fprintf(stderr, "libburn_experimental: activated burn_builtin_abort_handler() with handle '%s'\n",(handle==NULL ? "libburn : " : (char *) handle));
*/

	}
	strcpy(abort_message_prefix, "libburn : ");
	if(handle != NULL)
		strncpy(abort_message_prefix, (char *) handle,
			sizeof(abort_message_prefix)-1);
	abort_message_prefix[sizeof(abort_message_prefix)-1] = 0;
	abort_control_pid = getpid();
	Cleanup_set_handlers(handle, (Cleanup_app_handler_T) handler, mode|4);
	burn_global_signal_handle = handle;
	burn_global_signal_handler = handler;
}


/* ts A70223 : API */
void burn_allow_untested_profiles(int yes)
{
	burn_support_untested_profiles = !!yes;
}


/* ts A70915 : API */
int burn_set_messenger(void *messenger)
{
	struct libdax_msgs *pt;

	if (libdax_msgs_refer(&pt, messenger, 0) <= 0)
		return 0;
	libdax_msgs_destroy(&libdax_messenger, 0);
	libdax_messenger = (struct libdax_msgs *) pt;
	return 1;
}

