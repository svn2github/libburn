/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#include "libburn.h"
#include "transport.h"
#include "drive.h"
#include "write.h"
#include "options.h"
#include "async.h"
#include "init.h"
#include "back_hacks.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
#include <a ssert.h>
*/
#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;

#define SCAN_GOING() (workers && !workers->drive)

typedef void *(*WorkerFunc) (void *);

struct scan_opts
{
	struct burn_drive_info **drives;
	unsigned int *n_drives;

	int done;
};

struct erase_opts
{
	struct burn_drive *drive;
	int fast;
};

/* ts A61230 */
struct format_opts
{
	struct burn_drive *drive;
	off_t size;
	int flag;
};

struct write_opts
{
	struct burn_drive *drive;
	struct burn_write_opts *opts;
	struct burn_disc *disc;
};


struct w_list
{
	struct burn_drive *drive;
	pthread_t thread;

	struct w_list *next;

	union w_list_data
	{
		struct scan_opts scan;
		struct erase_opts erase;
		struct format_opts format;
		struct write_opts write;
	} u;
};

static struct w_list *workers;

static struct w_list *find_worker(struct burn_drive *d)
{
	struct w_list *a;

	for (a = workers; a; a = a->next)
		if (a->drive == d)
			return a;
	return NULL;
}

static void add_worker(struct burn_drive *d, WorkerFunc f, void *data)
{
	struct w_list *a;
	struct w_list *tmp;

	a = malloc(sizeof(struct w_list));
	a->drive = d;
	a->u = *(union w_list_data *)data;

	/* insert at front of the list */
	a->next = workers;
	tmp = workers;
	workers = a;

	if (d != NULL)
		d->busy = BURN_DRIVE_SPAWNING;

	if (pthread_create(&a->thread, NULL, f, a)) {
		free(a);
		workers = tmp;
		return;
	}
}

static void remove_worker(pthread_t th)
{
	struct w_list *a, *l = NULL;

	for (a = workers; a; l = a, a = a->next)
		if (a->thread == th) {
			if (l)
				l->next = a->next;
			else
				workers = a->next;
			free(a);
			break;
		}

	/* ts A61006 */
	/* a ssert(a != NULL);/ * wasn't found.. this should not be possible */
	if (a == NULL)
		libdax_msgs_submit(libdax_messenger, -1, 0x00020101,
			LIBDAX_MSGS_SEV_WARNING, LIBDAX_MSGS_PRIO_HIGH,
			"remove_worker() cannot find given worker item", 0, 0);
}

static void *scan_worker_func(struct w_list *w)
{
	int ret;

	ret = burn_drive_scan_sync(w->u.scan.drives, w->u.scan.n_drives, 1);
	if (ret <= 0)
		w->u.scan.done = -1;
	else
		w->u.scan.done = 1;
	return NULL;
}

int burn_drive_scan(struct burn_drive_info *drives[], unsigned int *n_drives)
{
	struct scan_opts o;
	int ret = 0;

	/* ts A61006 : moved up from burn_drive_scan_sync , former Assert */
	if (!burn_running) {
		libdax_msgs_submit(libdax_messenger, -1, 0x00020109,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Library not running (on attempt to scan)", 0, 0);
		*drives = NULL;
		*n_drives = 0;
		return -1;
	}

	/* cant be anything working! */

	/* ts A61006 */
	/* a ssert(!(workers && workers->drive)); */
	if (workers != NULL && workers->drive != NULL) {
drive_is_active:;
		libdax_msgs_submit(libdax_messenger, -1, 0x00020102,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"A drive operation is still going on (want to scan)",
			0, 0);
		*drives = NULL;
		*n_drives = 0;
		return -1;
	}

	if (workers == NULL) {
		/* start it */

		/* ts A61007 : test moved up from burn_drive_scan_sync()
				was burn_wait_all() */
		/* ts A70907 : now demanding freed drives, not only released */
		if (!burn_drives_are_clear(1))
			goto drive_is_active;
		*drives = NULL;
		*n_drives = 0;

		o.drives = drives;
		o.n_drives = n_drives;
		o.done = 0;
		add_worker(NULL, (WorkerFunc) scan_worker_func, &o);
	} else if (workers->u.scan.done) {
		/* its done */
		ret = workers->u.scan.done;
		remove_worker(workers->thread);

		/* ts A61006 */
		/* a ssert(workers == NULL); */
		if (workers != NULL) {
			libdax_msgs_submit(libdax_messenger, -1, 0x00020101,
				LIBDAX_MSGS_SEV_WARNING, LIBDAX_MSGS_PRIO_HIGH,
			     "After scan a drive operation is still going on",
				0, 0);
			return -1;
		}

	} else {
		/* still going */
	}
	return ret;
}

static void *erase_worker_func(struct w_list *w)
{
	burn_disc_erase_sync(w->u.erase.drive, w->u.erase.fast);
	remove_worker(pthread_self());
	return NULL;
}

void burn_disc_erase(struct burn_drive *drive, int fast)
{
	struct erase_opts o;

	/* ts A61006 */
	/* a ssert(drive); */
	/* a ssert(!SCAN_GOING()); */
	/* a ssert(!find_worker(drive)); */
	if((drive == NULL)) {
		libdax_msgs_submit(libdax_messenger, drive->global_index,
			0x00020104,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"NULL pointer caught in burn_disc_erase", 0, 0);
		return;
	}
	if ((SCAN_GOING()) || find_worker(drive)) {
		libdax_msgs_submit(libdax_messenger, drive->global_index,
			0x00020102,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"A drive operation is still going on (want to erase)",
			0, 0);
		return;
	}
	/* A70103 : will be set to 0 by burn_disc_erase_sync() */
	drive->cancel = 1;

	/* ts A70103 moved up from burn_disc_erase_sync() */
	/* ts A60825 : allow on parole to blank appendable CDs */
	/* ts A70131 : allow blanking of overwriteable DVD-RW (profile 0x13) */
	/* ts A70216 : allow blanking of CD-RW or DVD-RW in any regular state
	               and of any kind of full media */
	/* ts A70909 : the willingness to burn any BURN_DISC_FULL media is
	               inappropriate. One would rather need a -force option
	               Note: keep this in sync with mmc_read_disc_info() */
	if ((drive->current_profile != 0x0a &&
	     drive->current_profile != 0x13 &&
	     drive->current_profile != 0x14 &&
	     drive->status != BURN_DISC_FULL)
	    ||
	    (drive->status != BURN_DISC_FULL &&
	     drive->status != BURN_DISC_APPENDABLE &&
	     drive->status != BURN_DISC_BLANK)
	    ||
	    (drive->drive_role != 1)
	   ) {
		libdax_msgs_submit(libdax_messenger, drive->global_index,
			0x00020130,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Drive and media state unsuitable for blanking",
			0, 0);
		return;
	}

	o.drive = drive;
	o.fast = fast;
	add_worker(drive, (WorkerFunc) erase_worker_func, &o);
}


/* ts A61230 */
static void *format_worker_func(struct w_list *w)
{
	burn_disc_format_sync(w->u.format.drive, w->u.format.size,
				w->u.format.flag);
	remove_worker(pthread_self());
	return NULL;
}


/* ts A61230 */
void burn_disc_format(struct burn_drive *drive, off_t size, int flag)
{
	struct format_opts o;
	int ok = 0;
	char msg[160];

	if ((SCAN_GOING()) || find_worker(drive)) {
		libdax_msgs_submit(libdax_messenger, drive->global_index,
			0x00020102,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"A drive operation is still going on (want to format)",
			0, 0);
		return;
	}
	if (drive->drive_role != 1) {
		libdax_msgs_submit(libdax_messenger, drive->global_index,
			0x00020146,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Drive is a virtual placeholder", 0, 0);
		drive->cancel = 1;
		return;
	}
	if (flag & 128)     /* application prescribed format type */
		flag |= 16; /* enforce re-format */

	if (drive->current_profile == 0x14)
		ok = 1; /* DVD-RW sequential */
	else if (drive->current_profile == 0x13 && (flag & 16)) 
		ok = 1; /* DVD-RW Restricted Overwrite with force bit */
	else if (drive->current_profile == 0x1a) {
		ok = 1;         /* DVD+RW */
		size = 0;
		flag &= ~(2|8); /* no insisting in size 0, no expansion */
		flag |= 4;      /* format up to maximum size */
	}

	if (!ok) {
		sprintf(msg,"Will not format media type %4.4Xh",
			drive->current_profile);
		libdax_msgs_submit(libdax_messenger, drive->global_index,
			0x00020129,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			msg, 0, 0);
		drive->cancel = 1;
		return;
	}
	o.drive = drive;
	o.size = size;
	o.flag = flag;
	add_worker(drive, (WorkerFunc) format_worker_func, &o);
}


static void *write_disc_worker_func(struct w_list *w)
{
	struct burn_drive *d = w->u.write.drive;

	d->thread_pid = getpid();
	d->thread_pid_valid= 1;
	burn_disc_write_sync(w->u.write.opts, w->u.write.disc);
	d->thread_pid_valid= 0;
	d->thread_pid = 0;

	/* the options are refcounted, free out ref count which we added below 
	 */
	burn_write_opts_free(w->u.write.opts);

	remove_worker(pthread_self());
	return NULL;
}

void burn_disc_write(struct burn_write_opts *opts, struct burn_disc *disc)
{
	struct write_opts o;
	char reasons[BURN_REASONS_LEN+80];

	/* ts A61006 */
	/* a ssert(!SCAN_GOING()); */
	/* a ssert(!find_worker(opts->drive)); */
	if ((SCAN_GOING()) || find_worker(opts->drive)) {
		libdax_msgs_submit(libdax_messenger, opts->drive->global_index,
			0x00020102,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"A drive operation is still going on (want to write)",
			0, 0);
		return;
	}

	/* For the next lines any return indicates failure */
	opts->drive->cancel = 1;

	/* ts A70203 : people have been warned in API specs */
	if (opts->write_type == BURN_WRITE_NONE)
		return;

	if (opts->drive->drive_role == 0) {
		libdax_msgs_submit(libdax_messenger, opts->drive->global_index,
			0x00020146,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Drive is a virtual placeholder (null-drive)", 0, 0);
		return;
	}

	/* ts A61007 : obsolete Assert in spc_select_write_params() */
	if (opts->drive->drive_role == 1 && !opts->drive->mdata->valid) {
		libdax_msgs_submit(libdax_messenger,
				opts->drive->global_index, 0x00020113,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				"Drive capabilities not inquired yet", 0, 0);
		return;
	}

	/* ts A70219 : intended to replace all further tests here and many
	               tests in burn_*_write_sync()
	*/
	strcpy(reasons, "Write job parameters are unsuitable:\n");
	if (burn_precheck_write(opts, disc, reasons + strlen(reasons), 1)
	     == BURN_WRITE_NONE) {
		libdax_msgs_submit(libdax_messenger,
				opts->drive->global_index, 0x00020139,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				reasons, 0, 0);
		return;
	}

	opts->drive->cancel = 0; /* End of the return = failure area */

	o.drive = opts->drive;
	o.opts = opts;
	o.disc = disc;

	opts->refcount++;

	add_worker(opts->drive, (WorkerFunc) write_disc_worker_func, &o);
}

void burn_async_join_all(void)
{
	void *ret;

	while (workers)
		pthread_join(workers->thread, &ret);
}
