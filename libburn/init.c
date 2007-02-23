/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#include <unistd.h>

/* ts A61007 */
/* #include <a ssert.h> */

#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "init.h"
#include "sg.h"
#include "error.h"
#include "libburn.h"
#include "drive.h"

/* ts A60825 : The storage location for back_hacks.h variables. */
#define BURN_BACK_HACKS_INIT 1
#include "back_hacks.h"

/* ts A60924 : a new message handling facility */
#include "libdax_msgs.h"
struct libdax_msgs *libdax_messenger= NULL;


int burn_running = 0;

/* ts A60813 : wether to use O_EXCL and/or O_NONBLOCK in libburn/sg.c */
int burn_sg_open_o_excl = 1;

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
	if (!burn_drives_are_clear()) {
		libdax_msgs_submit(libdax_messenger, -1, 0x00020107,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Drive is busy on attempt to shut down library", 0, 0);
		return;
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

	burn_sg_open_o_excl= exclusive;
	burn_sg_open_o_nonblock= !blocking;
	burn_sg_open_abort_busy= !!abort_on_busy;
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

int burn_builtin_abort_handler(void *handle, int signum, int flag)
{
	if(getpid() != abort_control_pid) {

#ifdef Not_yeT
		pthread_t thread_id;

		/* >>> need better handling of self-induced SIGs 
		       like SIGSEGV or SIGFPE.
		       Like bonking the control thread if it did not show up
		       after a short while.
		*/

		/* >>> if this is a non-fatal signal : return -2 */

		thread_id = pthread_self();
		/* >>> find thread_id  in worker list of async.c */
		/* >>> if owning a drive : mark idle and canceled
		       (can't do anything more) */

		usleep(1000000); /* calm down */

 		/* forward signal to control thread */
		if (abort_control_pid>1)
			kill(abort_control_pid, signum);

		/* >>> ??? end thread */;

#else
		usleep(1000000); /* calm down */
		return -2;
#endif /* ! Not_yeT */

	}
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
	abort_control_pid= getpid();
	Cleanup_set_handlers(handle, (Cleanup_app_handler_T) handler, mode|4);
}


/* ts A70223 : API */
void burn_allow_untested_profiles(int yes)
{
	burn_support_untested_profiles = !!yes;
}

