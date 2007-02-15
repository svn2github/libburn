/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#include <sys/types.h>
#include <sys/stat.h>

/* #include <m alloc.h>  ts A61013 : not in Linux man 3 malloc */

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* ts A61007 */
/* #include <a ssert.h> */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include "libburn.h"
#include "drive.h"
#include "transport.h"
#include "debug.h"
#include "init.h"
#include "toc.h"
#include "util.h"
#include "sg.h"
#include "structure.h"

/* ts A70107 : to get BE_CANCELLED */
#include "error.h"

#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;

static struct burn_drive drive_array[255];
static int drivetop = -1;

/* ts A61021 : the unspecific part of sg.c:enumerate_common()
*/
int burn_setup_drive(struct burn_drive *d, char *fname)
{
	d->devname = burn_strdup(fname);
	memset(&d->params, 0, sizeof(struct params));
	d->released = 1;
	d->status = BURN_DISC_UNREADY;
	return 1;
}

/* ts A60904 : ticket 62, contribution by elmom */
/* splitting former burn_drive_free() (which freed all, into two calls) */
void burn_drive_free(struct burn_drive *d)
{
	if (d->global_index == -1)
		return;
	/* ts A60822 : close open fds before forgetting them */
	if (burn_drive_is_open(d))
		d->release(d);
	free((void *) d->idata);
	burn_mdata_free_subs(d->mdata);
	free((void *) d->mdata);
	if(d->toc_entry != NULL)
		free((void *) d->toc_entry);
	free(d->devname);
	d->global_index = -1;
}

void burn_drive_free_all(void)
{
	int i;

	for (i = 0; i < drivetop + 1; i++)
		burn_drive_free(&(drive_array[i]));
	drivetop = -1;
	memset(drive_array, 0, sizeof(drive_array));
}


/* ts A60822 */
int burn_drive_is_open(struct burn_drive *d)
{
	/* ts A61021 : moved decision to sg.c */
	return d->drive_is_open(d);
}


/* ts A60906 */
int burn_drive_force_idle(struct burn_drive *d)
{
	d->busy = BURN_DRIVE_IDLE;
	return 1;
}


/* ts A60906 */
int burn_drive_is_released(struct burn_drive *d)
{
        return !!d->released;
}


/* ts A60906 */
/** Inquires drive status in respect to degree of app usage.
    @param return -2 = drive is forgotten 
                  -1 = drive is closed (i.e. released explicitely)
                   0 = drive is open, not grabbed (after scan, before 1st grab)
                   1 = drive is grabbed but BURN_DRIVE_IDLE
                  10 = drive is grabbing (BURN_DRIVE_GRABBING)
                 100 = drive is busy in cancelable state
                1000 = drive is in non-cancelable state 
           Expect a monotonous sequence of usage severity to emerge in future.
*/
int burn_drive_is_occupied(struct burn_drive *d)
{
	if(d->global_index < 0)
		return -2;
	if(!burn_drive_is_open(d))
		return -1;
	if(d->busy == BURN_DRIVE_GRABBING)
		return 10;
	if(d->released)
		return 0;
	if(d->busy == BURN_DRIVE_IDLE)
		return 1;
	if(d->busy == BURN_DRIVE_READING || d->busy == BURN_DRIVE_WRITING)
		return 50;
	return 1000;
}


/*
void drive_read_lead_in(int dnum)
{
	mmc_read_lead_in(&drive_array[dnum], get_4k());
}
*/
unsigned int burn_drive_count(void)
{
	return drivetop + 1;
}


/* ts A61125 : media status aspects of burn_drive_grab() */
int burn_drive_inquire_media(struct burn_drive *d)
{

	/* ts A61225 : after loading the tray, mode page 2Ah can change */
	d->getcaps(d);

	/* ts A61020 : d->status was set to BURN_DISC_BLANK as pure guess */

	if (d->mdata->cdr_write || d->mdata->cdrw_write ||
	    d->mdata->dvdr_write || d->mdata->dvdram_write) {

#define Libburn_knows_correct_state_after_loaD 1
#ifdef Libburn_knows_correct_state_after_loaD

		d->read_disc_info(d);

#else
		/* ts A61227 : This repeated read_disc_info seems
		               to be obsoleted by above d->getcaps(d).
		*/
		/* ts A60908 */
		/* Trying to stabilize the disc status after eventual load
		   without closing and re-opening the drive */
		/* This seems to work for burn_disc_erasable() .
		   Speed values on RIP-14 and LITE-ON 48125S are stable
		   and false, nevertheless. */
		int was_equal = 0, must_equal = 3, max_loop = 20;
		int loop_count, old_speed = -1234567890, new_speed= -987654321;
		int old_erasable = -1234567890, new_erasable = -987654321;

		fprintf(stderr,"LIBBURN_DEBUG: read_disc_info()\n");
		for (loop_count = 0; loop_count < max_loop; loop_count++){
			old_speed = new_speed;
			old_erasable = new_erasable;

			d->read_disc_info(d);
			if(d->status == BURN_DISC_UNSUITABLE)
		break;

			new_speed = burn_drive_get_write_speed(d);
			new_erasable = burn_disc_erasable(d);
		        if (new_speed == old_speed &&
			    new_erasable == old_erasable) {
				was_equal++;
				if (was_equal >= must_equal)
		break;
			} else
				was_equal = 0;
			/*
			if (loop_count >= 1 && was_equal == 0)
			*/
				fprintf(stderr,"LIBBURN_DEBUG: %d : speed %d:%d   erasable %d:%d\n",
					loop_count,old_speed,new_speed,old_erasable,new_erasable);
			usleep(100000);
		}
#endif /* ! Libburn_knows_correct_state_after_loaD */

	} else {
		if (d->current_profile == -1 || d->current_is_cd_profile)
			d->read_toc(d);
	}
	return 1;
}


int burn_drive_grab(struct burn_drive *d, int le)
{
	int errcode;
	/* ts A61125 - A61202 */
	int ret, sose;

	if (!d->released) {
		burn_print(1, "can't grab - already grabbed\n");
		return 0;
	}
	d->status = BURN_DISC_UNREADY;
	errcode = d->grab(d);

	if (errcode == 0) {
		burn_print(1, "low level drive grab failed\n");
		return 0;
	}
	d->busy = BURN_DRIVE_GRABBING;

	if (le)
		d->load(d);

	d->lock(d);

	/* ts A61118 */
	d->start_unit(d);

	/* ts A61202 : gave bit1 of le a meaning */
	sose = d->silent_on_scsi_error;
	if (!le)
		d->silent_on_scsi_error = 1;
	/* ts A61125 : outsourced media state inquiry aspects */
	ret = burn_drive_inquire_media(d);
	d->silent_on_scsi_error = sose;
	d->busy = BURN_DRIVE_IDLE;
	return ret;
}

struct burn_drive *burn_drive_register(struct burn_drive *d)
{
#ifdef Libburn_ticket_62_re_register_is_possiblE
	int i;
#endif

	d->block_types[0] = 0;
	d->block_types[1] = 0;
	d->block_types[2] = 0;
	d->block_types[3] = 0;
	d->toc_temp = 0;
	d->nwa = 0;
	d->alba = 0;
	d->rlba = 0;
	d->cancel = 0;
	d->busy = BURN_DRIVE_IDLE;
	d->toc_entries = 0;
	d->toc_entry = NULL;
	d->disc = NULL;
	d->erasable = 0;

#ifdef Libburn_ticket_62_re_register_is_possiblE
	/* ts A60904 : ticket 62, contribution by elmom */
	/* Not yet accepted because no use case seen yet */

	/* This is supposed to find an already freed drive struct among
	   all the the ones that have been used before */
	for (i = 0; i < drivetop + 1; i++)
		if (drive_array[i].global_index == -1)
			break;
	d->global_index = i;
	memcpy(&drive_array[i], d, sizeof(struct burn_drive));
	pthread_mutex_init(&drive_array[i].access_lock, NULL);
	if (drivetop < i)
		drivetop = i;
	return &(drive_array[i]);

#else /* Libburn_ticket_62_re_register_is_possiblE */
	/* old A60904 : */
	/* Still active by default */

	d->global_index = drivetop + 1;
	memcpy(&drive_array[drivetop + 1], d, sizeof(struct burn_drive));
	pthread_mutex_init(&drive_array[drivetop + 1].access_lock, NULL);
	return &drive_array[++drivetop];

#endif /* ! Libburn_ticket_62_re_register_is_possiblE */

}


/* unregister most recently registered drive */
int burn_drive_unregister(struct burn_drive *d)
{
	if(d->global_index != drivetop)
		return 0;
	burn_drive_free(d);
	drivetop--;
	return 1;
}


/* ts A61021 : after-setup activities from sg.c:enumerate_common()
*/
struct burn_drive *burn_drive_finish_enum(struct burn_drive *d)
{
	struct burn_drive *t;
	/* ts A60821
   	<<< debug: for tracing calls which might use open drive fds */
	int mmc_function_spy(char * text);

	t = burn_drive_register(d);

	/* ts A60821 */
	mmc_function_spy("enumerate_common : -------- doing grab");

	/* try to get the drive info */
	if (t->grab(t)) {
	        burn_print(2, "getting drive info\n");
	        t->getcaps(t);
	        t->unlock(t);
	        t->released = 1;
	} else {
	        burn_print(2, "unable to grab new located drive\n");
	        burn_drive_unregister(t);
		t = NULL;
	}

	/* ts A60821 */
	mmc_function_spy("enumerate_common : ----- would release ");

	return t;
}


/* ts A61125 : model aspects of burn_drive_release */
int burn_drive_mark_unready(struct burn_drive *d)
{
	/* ts A61020 : mark media info as invalid */
	d->start_lba= -2000000000;
	d->end_lba= -2000000000;

	/* ts A61202 */
	d->current_profile = -1;
	d->current_has_feat21h = 0;
	d->current_feat2fh_byte4 = -1;

	d->status = BURN_DISC_UNREADY;
	if (d->toc_entry != NULL)
		free(d->toc_entry);
	d->toc_entry = NULL;
	d->toc_entries = 0;
	if (d->disc != NULL) {
		burn_disc_free(d->disc);
		d->disc = NULL;
	}
	return 1;
}


void burn_drive_release(struct burn_drive *d, int le)
{
	if (d->released) {
		/* ts A61007 */
		/* burn_print(1, "second release on drive!\n"); */
		libdax_msgs_submit(libdax_messenger,
				d->global_index, 0x00020105,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				"Drive is already released", 0, 0);
		return;
	}

	/* ts A61007 */
	/* ts A60906: one should not assume BURN_DRIVE_IDLE == 0 */
	/* a ssert(d->busy == BURN_DRIVE_IDLE); */
	if (d->busy != BURN_DRIVE_IDLE) {
		libdax_msgs_submit(libdax_messenger,
				d->global_index, 0x00020106,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				"Drive is busy on attempt to close", 0, 0);
		return;
	}

	d->unlock(d);
	if (le)
		d->eject(d);
	d->release(d);
	d->released = 1;

	/* ts A61125 : outsourced model aspects */
	burn_drive_mark_unready(d);
}



/* ts A61007 : former void burn_wait_all() */
int burn_drives_are_clear(void)
{
	int i;

	for (i = burn_drive_count() - 1; i >= 0; --i) {
		/* ts A60904 : ticket 62, contribution by elmom */
		if (drive_array[i].global_index == -1)
	continue;
		if (drive_array[i].released)
	continue;
		return 0;
	}
	return 1;
}


#if 0
void burn_wait_all(void)
{
	unsigned int i;
	int finished = 0;
	struct burn_drive *d;

	while (!finished) {
		finished = 1;
		d = drive_array;
		for (i = burn_drive_count(); i > 0; --i, ++d) {

			/* ts A60904 : ticket 62, contribution by elmom */
			if (d->global_index==-1)
				continue;

			a ssert(d->released); 
		}
		if (!finished)
			sleep(1);
	}
}
#endif


void burn_disc_erase_sync(struct burn_drive *d, int fast)
{
/* ts A60924 : libburn/message.c gets obsoleted
	burn_message_clear_queue();
*/

	burn_print(1, "erasing drive %s %s\n", d->idata->vendor,
		   d->idata->product);

	d->cancel = 0;
	d->busy = BURN_DRIVE_ERASING;
	d->erase(d, fast);
	/* reset the progress */
	d->progress.session = 0;
	d->progress.sessions = 1;
	d->progress.track = 0;
	d->progress.tracks = 1;
	d->progress.index = 0;
	d->progress.indices = 1;
	d->progress.start_sector = 0;
	d->progress.sectors = 0x10000;
	d->progress.sector = 0;
	/* read the initial 0 stage */
	while (!d->test_unit_ready(d) && d->get_erase_progress(d) == 0)
		sleep(1);
	while ((d->progress.sector = d->get_erase_progress(d)) > 0 ||
		!d->test_unit_ready(d))
		sleep(1);
	d->progress.sector = 0x10000;

	/* ts A61125 : update media state records */
	burn_drive_mark_unready(d);
	burn_drive_inquire_media(d);
	d->busy = BURN_DRIVE_IDLE;
}

/*
   @param flag: bit0 = fill formatted size with zeros
                bit1, bit2 , bit4, bit7 - bit15 are for d->format_unit()
*/
void burn_disc_format_sync(struct burn_drive *d, off_t size, int flag)
{
	int ret, buf_secs, err, i, stages = 1, pbase, pfill, pseudo_sector;
	off_t num_bufs;
	char msg[80];
	struct buffer buf;

	/* reset the progress */
	d->progress.session = 0;
	d->progress.sessions = 1;
	d->progress.track = 0;
	d->progress.tracks = 1;
	d->progress.index = 0;
	d->progress.indices = 1;
	d->progress.start_sector = 0;
	d->progress.sectors = 0x10000;
	d->progress.sector = 0;
	stages = 1 + ((flag & 1) && size > 1024 * 1024);
	d->cancel = 0;
	d->busy = BURN_DRIVE_FORMATTING;
	ret = d->format_unit(d, size, flag & 0xff96); /* forward bits */
	if (ret <= 0)
		d->cancel = 1;

	while (!d->test_unit_ready(d) && d->get_erase_progress(d) == 0)
		sleep(1);
	while ((pseudo_sector = d->get_erase_progress(d)) > 0 ||
		!d->test_unit_ready(d)) {
		d->progress.sector = pseudo_sector / stages;
		sleep(1);
        }
	d->sync_cache(d);

	if (size <= 0)
		goto ex;

	/* update media state records */
	burn_drive_mark_unready(d);
	burn_drive_inquire_media(d);
	if (flag & 1) {
		/* write size in zeros */;
		pbase = 0x8000 + 0x7fff * (stages == 1);
		pfill = 0xffff - pbase;
		buf_secs = 16; /* Must not be more than 16 */
		num_bufs = size / buf_secs / 2048;
		if (num_bufs > 0x7fffffff) {
			d->cancel = 1;
			goto ex;
		}

		/* <<< */
		sprintf(msg,
			"Writing %.f sectors of zeros to formatted media",
			(double) num_bufs * (double) buf_secs);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00000002,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
				msg, 0, 0);

		d->buffer = &buf;
		memset(d->buffer, 0, sizeof(struct buffer));
		d->buffer->bytes = buf_secs * 2048;
		d->buffer->sectors = buf_secs;
		d->busy = BURN_DRIVE_WRITING;
		for (i = 0; i < num_bufs; i++) {
			d->nwa = i * buf_secs;
			err = d->write(d, d->nwa, d->buffer);
			if (err == BE_CANCELLED || d->cancel) {
				d->cancel = 1;
		break;
			}
			d->progress.sector = pbase
				+ pfill * ((double) i / (double) num_bufs);
		}
		d->sync_cache(d);
		if (d->current_profile == 0x13 || d->current_profile == 0x1a) {
			/* DVD-RW or DVD+RW */
			d->busy = BURN_DRIVE_CLOSING_SESSION;
			/* CLOSE SESSION, 010b */
			d->close_track_session(d, 1, 0);
			d->busy = BURN_DRIVE_WRITING;
		}
	}
ex:;
	d->progress.sector = 0x10000;
	d->busy = BURN_DRIVE_IDLE;
	d->buffer = NULL;
}


/* ts A70112 API */
int burn_disc_get_formats(struct burn_drive *d, int *status, off_t *size,
				unsigned *bl_sas, int *num_formats)
{
	int ret;

	*status = 0;
	*size = 0;
	*bl_sas = 0;
	*num_formats = 0;
	ret = d->read_format_capacities(d, 0x00);
	if (ret <= 0)
		return 0;
	*status = d->format_descr_type;
	*size = d->format_curr_max_size;
	*bl_sas = d->format_curr_blsas;
	*num_formats = d->num_format_descr;
	return 1;
}


/* ts A70112 API */
int burn_disc_get_format_descr(struct burn_drive *d, int index,
				int *type, off_t *size, unsigned *tdp)
{
	*type = 0;
	*size = 0;
	*tdp = 0;
	if (index < 0 || index >= d->num_format_descr)
		return 0;
	*type = d->format_descriptors[index].type;
	*size = d->format_descriptors[index].size;
	*tdp = d->format_descriptors[index].tdp;
	return 1;
}


enum burn_disc_status burn_disc_get_status(struct burn_drive *d)
{
	/* ts A61007 */
	/* a ssert(!d->released); */
	if (d->released) {
		libdax_msgs_submit(libdax_messenger,
				d->global_index, 0x00020108,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				"Drive is not grabbed on disc status inquiry",
				0, 0);
		return BURN_DISC_UNGRABBED;
	}

	return d->status;
}

int burn_disc_erasable(struct burn_drive *d)
{
	return d->erasable;
}
enum burn_drive_status burn_drive_get_status(struct burn_drive *d,
					     struct burn_progress *p)
{
	if (p) {
		memcpy(p, &(d->progress), sizeof(struct burn_progress));
		/* TODO: add mutex */
	}
	return d->busy;
}

void burn_drive_cancel(struct burn_drive *d)
{
	pthread_mutex_lock(&d->access_lock);
	d->cancel = 1;
	pthread_mutex_unlock(&d->access_lock);
}

/* ts A61007 : defunct because unused */
#if 0
int burn_drive_get_block_types(struct burn_drive *d,
			       enum burn_write_types write_type)
{
	burn_print(12, "write type: %d\n", write_type);
	a ssert(			/* (write_type >= BURN_WRITE_PACKET) && */
		      (write_type <= BURN_WRITE_RAW));
	return d->block_types[write_type];
}
#endif

static void strip_spaces(char *str)
{
	char *tmp;

	tmp = str + strlen(str) - 1;
	while (isspace(*tmp))
		*(tmp--) = '\0';

	tmp = str;
	while (*tmp) {
		if (isspace(*tmp) && isspace(*(tmp + 1))) {
			char *tmp2;

			for (tmp2 = tmp + 1; *tmp2; ++tmp2)
				*(tmp2 - 1) = *tmp2;
			*(tmp2 - 1) = '\0';
		} else
			++tmp;
	}
}

static int drive_getcaps(struct burn_drive *d, struct burn_drive_info *out)
{
	struct burn_scsi_inquiry_data *id;

	/* ts A61007 : now prevented in enumerate_common() */
#if 0
	a ssert(d->idata);
	a ssert(d->mdata);
#endif

	if (!d->idata->valid || !d->mdata->valid)
		return 0;

	id = (struct burn_scsi_inquiry_data *)d->idata;

	memcpy(out->vendor, id->vendor, sizeof(id->vendor));
	strip_spaces(out->vendor);
	memcpy(out->product, id->product, sizeof(id->product));
	strip_spaces(out->product);
	memcpy(out->revision, id->revision, sizeof(id->revision));
	strip_spaces(out->revision);
	strncpy(out->location, d->devname, 16);
	out->location[16] = '\0';
	out->buffer_size = d->mdata->buffer_size;
	out->read_dvdram = !!d->mdata->dvdram_read;
	out->read_dvdr = !!d->mdata->dvdr_read;
	out->read_dvdrom = !!d->mdata->dvdrom_read;
	out->read_cdr = !!d->mdata->cdr_read;
	out->read_cdrw = !!d->mdata->cdrw_read;
	out->write_dvdram = !!d->mdata->dvdram_write;
	out->write_dvdr = !!d->mdata->dvdr_write;
	out->write_cdr = !!d->mdata->cdr_write;
	out->write_cdrw = !!d->mdata->cdrw_write;
	out->write_simulate = !!d->mdata->simulate;
	out->c2_errors = !!d->mdata->c2_pointers;
	out->drive = d;
	/* update available block types for burners */
	if (out->write_dvdram || out->write_dvdr ||
	    out->write_cdrw || out->write_cdr)
		d->probe_write_modes(d);
	out->tao_block_types = d->block_types[BURN_WRITE_TAO];
	out->sao_block_types = d->block_types[BURN_WRITE_SAO];
	out->raw_block_types = d->block_types[BURN_WRITE_RAW];
	out->packet_block_types = d->block_types[BURN_WRITE_PACKET];
	return 1;
}

int burn_drive_scan_sync(struct burn_drive_info *drives[],
			 unsigned int *n_drives)
{
	/* state vars for the scan process */
	/* ts A60904 : did set some default values to feel comfortable */
	static int scanning = 0, scanned = 0, found = 0;
	static unsigned num_scanned = 0, count = 0;
	unsigned int i;

	/* ts A61007 : moved up to burn_drive_scan() */
	/* a ssert(burn_running); */

	if (!scanning) {
		scanning = 1;

		/* ts A61007 : test moved up to burn_drive_scan()
		               burn_wait_all() is obsoleted */
#if 0
		/* make sure the drives aren't in use */
		burn_wait_all();	/* make sure the queue cleans up
					   before checking for the released
					   state */
#endif /* 0 */

		/* refresh the lib's drives */

		/* ts A61115 : formerly sg_enumerate(); ata_enumerate(); */
		scsi_enumerate_drives();

		count = burn_drive_count();
		if (count)
			*drives =
				malloc(sizeof(struct burn_drive_info) * count);
		else
			*drives = NULL;
		*n_drives = scanned = found = num_scanned = 0;
	}

	for (i = 0; i < count; ++i) {
		if (scanned & (1 << i))
			continue;	/* already scanned the device */

		while (!drive_getcaps(&drive_array[i],
				      &(*drives)[num_scanned])) {
			sleep(1);
		}
		scanned |= 1 << i;
		found |= 1 << i;
		num_scanned++;
		(*n_drives)++;
	}

	if (num_scanned == count) {
		/* done scanning */
		scanning = 0;
		return 1;
	}
	return 0;
}


void burn_drive_info_free(struct burn_drive_info drive_infos[])
{
/* ts A60904 : ticket 62, contribution by elmom */
/* clarifying the meaning and the identity of the victim */

	/* ts A60904 : This looks a bit weird.
	   burn_drive_info is not the manager of burn_drive but only its
	   spokesperson. To my knowlege drive_infos from burn_drive_scan()
	   are not memorized globally. */
	if(drive_infos != NULL)
		free((void *) drive_infos);

	burn_drive_free_all();
}

/* ts A61001 : internal call */
int burn_drive_forget(struct burn_drive *d, int force)
{
	int occup;

	occup = burn_drive_is_occupied(d);
/*
	fprintf(stderr, "libburn: experimental: occup == %d\n",occup);
*/
	if(occup <= -2)
		return 2;
	if(occup > 0)
		if(force < 1)
			return 0; 
	if(occup > 10)
		return 0;

	/* >>> do any drive calming here */;


	burn_drive_force_idle(d);
	if(occup > 0 && !burn_drive_is_released(d))
		burn_drive_release(d,0);
	burn_drive_free(d);
	return 1;
}

/* API call */
int burn_drive_info_forget(struct burn_drive_info *info, int force)
{
  return burn_drive_forget(info->drive, force);
}

struct burn_disc *burn_drive_get_disc(struct burn_drive *d)
{
	/* ts A61022: SIGSEGV on calling this function with blank media */
	if(d->disc == NULL)
		return NULL;

	d->disc->refcnt++;
	return d->disc;
}

void burn_drive_set_speed(struct burn_drive *d, int r, int w)
{
	d->set_speed(d, r, w);
}

int burn_msf_to_sectors(int m, int s, int f)
{
	return (m * 60 + s) * 75 + f;
}

void burn_sectors_to_msf(int sectors, int *m, int *s, int *f)
{
	*m = sectors / (60 * 75);
	*s = (sectors - *m * 60 * 75) / 75;
	*f = sectors - *m * 60 * 75 - *s * 75;
}

int burn_drive_get_read_speed(struct burn_drive *d)
{
	return d->mdata->max_read_speed;
}

int burn_drive_get_write_speed(struct burn_drive *d)
{
	return d->mdata->max_write_speed;
}

/* ts A61021 : New API function */
int burn_drive_get_min_write_speed(struct burn_drive *d)
{
	return d->mdata->min_write_speed;
}


/* ts A51221 */
static char *enumeration_whitelist[BURN_DRIVE_WHITELIST_LEN];
static int enumeration_whitelist_top = -1;

/** Add a device to the list of permissible drives. As soon as some entry is in
    the whitelist all non-listed drives are banned from enumeration.
    @return 1 success, <=0 failure
*/
int burn_drive_add_whitelist(char *device_address)
{
	char *new_item;
	if(enumeration_whitelist_top+1 >= BURN_DRIVE_WHITELIST_LEN)
		return 0;
	enumeration_whitelist_top++;
	new_item = malloc(strlen(device_address) + 1);
	if (new_item == NULL)
		return -1;
	strcpy(new_item, device_address);
	enumeration_whitelist[enumeration_whitelist_top] = new_item;
	return 1;
}

/** Remove all drives from whitelist. This enables all possible drives. */
void burn_drive_clear_whitelist(void)
{
	int i;
	for (i = 0; i <= enumeration_whitelist_top; i++)
		free(enumeration_whitelist[i]);
	enumeration_whitelist_top = -1;
}

int burn_drive_is_banned(char *device_address)
{
	int i;
	if(enumeration_whitelist_top<0)
		return 0;
	for (i = 0; i <= enumeration_whitelist_top; i++) 
		if (strcmp(enumeration_whitelist[i], device_address) == 0)
			return 0;
	return 1;
}

/* ts A60823 */
/** Aquire a drive with known persistent address. 
*/
int burn_drive_scan_and_grab(struct burn_drive_info *drive_infos[], char* adr,
			     int load)
{
	unsigned int n_drives;
	int ret;

	burn_drive_clear_whitelist();
	burn_drive_add_whitelist(adr);
/*
	fprintf(stderr,"libburn: experimental: burn_drive_scan_and_grab(%s)\n",
		adr);
*/
	while (1) {
		ret = burn_drive_scan(drive_infos, &n_drives);
		if (ret < 0)
			return -1;
		if (ret > 0)
	break;
		usleep(1002);
	}
	if (n_drives <= 0)
		return 0;
/*
	fprintf(stderr, "libburn: experimental: n_drives == %d\n",n_drives);
*/

	ret = burn_drive_grab(drive_infos[0]->drive, load);
	if (ret != 1)
		return -1;
	return 1;
}

/* ts A60925 */
/** Simple debug message frontend to libdax_msgs_submit().
    If arg is not NULL, then fmt MUST contain exactly one %s and no
    other sprintf() %-formatters.
*/
int burn_drive_adr_debug_msg(char *fmt, char *arg)
{
	int ret;
	char msg[4096], *msgpt;

        msgpt= msg;
	if(arg != NULL)
		sprintf(msg, fmt, arg);
	else
		msgpt= fmt;
	if(libdax_messenger == NULL)
		return 0;
	ret = libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
				msgpt, 0, 0);
	return ret;
}

/* ts A60923 */
/** Inquire the persistent address of the given drive. */
int burn_drive_raw_get_adr(struct burn_drive *d, char adr[])
{
	if (strlen(d->devname) >= BURN_DRIVE_ADR_LEN) {
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020110,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				"Persistent drive address too long", 0, 0);
		return -1;
	}
	strcpy(adr,d->devname);
	return 1;
}

/* ts A60823 - A60923 */
/** Inquire the persistent address of the given drive. */
int burn_drive_get_adr(struct burn_drive_info *drive_info, char adr[])
{
	int ret;

	ret = burn_drive_raw_get_adr(drive_info->drive, adr);
	return ret;
}


/* ts A60922 ticket 33 */
/** Evaluate wether the given address would be enumerated by libburn */
int burn_drive_is_enumerable_adr(char *adr)
{
	return sg_is_enumerable_adr(adr);
}

#define BURN_DRIVE_MAX_LINK_DEPTH 20

/* ts A60922 ticket 33 */
/* Try to find an enumerated address with the given stat.st_rdev number */
int burn_drive_resolve_link(char *path, char adr[], int *recursion_count)
{
	int ret;
	char link_target[4096], msg[4096+100], link_adr[4096], *adrpt;

	burn_drive_adr_debug_msg("burn_drive_resolve_link( %s )", path);
	if (*recursion_count >= BURN_DRIVE_MAX_LINK_DEPTH) {
		burn_drive_adr_debug_msg(
			"burn_drive_resolve_link aborts because link too deep",
			NULL);
		return 0;
	}
	(*recursion_count)++;
	ret = readlink(path, link_target, sizeof(link_target));
	if (ret == -1) {
		burn_drive_adr_debug_msg("readlink( %s ) returns -1", path);
		return 0;
	}
	if (ret >= sizeof(link_target) - 1) {
		sprintf(msg,"readlink( %s ) returns %d (too much)", path, ret);
		burn_drive_adr_debug_msg(msg, NULL);
		return -1;
	}
	link_target[ret] = 0;
	adrpt= link_target;
	if (link_target[0] != '/') {
		strcpy(link_adr, path);
		if ((adrpt = strrchr(link_adr, '/')) != NULL) {
			strcpy(adrpt + 1, link_target);
			adrpt = link_adr;
		} else
			adrpt = link_target;
	}
	ret = burn_drive_convert_fs_adr_sub(adrpt, adr, recursion_count);
	sprintf(msg,"burn_drive_convert_fs_adr( %s ) returns %d",
		link_target, ret);
	burn_drive_adr_debug_msg(msg, NULL);
	return ret;
}

/* ts A60922 - A61014 ticket 33 */
/* Try to find an enumerated address with the given stat.st_rdev number */
int burn_drive_find_devno(dev_t devno, char adr[])
{
	char fname[4096], msg[4096+100];
	int ret = 0, first = 1;
	struct stat stbuf;
	burn_drive_enumerator_t enm;

	while (1) {
		ret = sg_give_next_adr(&enm, fname, sizeof(fname), first);
		if(ret <= 0)
	break;
		first = 0;
		ret = stat(fname, &stbuf);
		if(ret == -1)
	continue;
		if(devno != stbuf.st_rdev)
	continue;
		if(strlen(fname) >= BURN_DRIVE_ADR_LEN)
			return -1;

		sprintf(msg, "burn_drive_find_devno( 0x%lX ) found %s",
			 (long) devno, fname);
		burn_drive_adr_debug_msg(msg, NULL);
		strcpy(adr, fname);
		{ ret = 1; goto ex;}
	}
	ret = 0;
ex:;
	if (first == 0)
		sg_give_next_adr(&enm, fname, sizeof(fname), -1);
	return ret;
}

/* ts A60923 */
/** Try to obtain host,channel,target,lun from path.
    @return     1 = success , 0 = failure , -1 = severe error
*/
int burn_drive_obtain_scsi_adr(char *path,
			       int *bus_no, int *host_no, int *channel_no,
			       int *target_no, int *lun_no)
{
	int ret, i;
	char adr[BURN_DRIVE_ADR_LEN];

	/* open drives cannot be inquired by sg_obtain_scsi_adr() */
	for (i = 0; i < drivetop + 1; i++) {
		if (drive_array[i].global_index < 0)
	continue;
		ret = burn_drive_raw_get_adr(&(drive_array[i]),adr);
		if (ret < 0)
			return -1;
		if (ret == 0)
	continue;
		if (strcmp(adr, path) == 0) {
			*host_no = drive_array[i].host;
			*channel_no = drive_array[i].channel;
			*target_no = drive_array[i].id;
			*lun_no = drive_array[i].lun;
			*bus_no = drive_array[i].bus_no;
			if (*host_no < 0 || *channel_no < 0 ||
			    *target_no < 0 || *lun_no < 0)
				return 0;
			return 1;
		}
	}

	ret = sg_obtain_scsi_adr(path, bus_no, host_no, channel_no,
				 target_no, lun_no);
	return ret;
}

/* ts A60923 */
int burn_drive_convert_scsi_adr(int bus_no, int host_no, int channel_no,
				int target_no, int lun_no, char adr[])
{
	char fname[4096],msg[4096+100];
	int ret = 0, first = 1, i_bus_no = -1;
	int i_host_no = -1, i_channel_no = -1, i_target_no = -1, i_lun_no = -1;
	burn_drive_enumerator_t enm;

	sprintf(msg,"burn_drive_convert_scsi_adr( %d,%d,%d,%d,%d )",
		bus_no, host_no, channel_no, target_no, lun_no);
	burn_drive_adr_debug_msg(msg, NULL);

	while (1) {
		ret= sg_give_next_adr(&enm, fname, sizeof(fname), first);
		if(ret <= 0)
	break;
		first = 0;
		ret = burn_drive_obtain_scsi_adr(fname, &i_bus_no, &i_host_no,
				 &i_channel_no, &i_target_no, &i_lun_no);
		if(ret <= 0)
	continue;
		if(bus_no >=0 && i_bus_no != bus_no)
	continue;
		if(host_no >=0 && i_host_no != host_no)
	continue;
		if(channel_no >= 0 && i_channel_no != channel_no)
	continue;
		if(target_no >= 0 && i_target_no != target_no)
	continue;
		if(lun_no >= 0 && i_lun_no != lun_no)
	continue;
		if(strlen(fname) >= BURN_DRIVE_ADR_LEN)
			{ ret = -1; goto ex;}
		burn_drive_adr_debug_msg(
			"burn_drive_convert_scsi_adr() found %s", fname);
		strcpy(adr, fname);
		{ ret = 1; goto ex;}
	}
	ret = 0;
ex:;
	if (first == 0)
		sg_give_next_adr(&enm, fname, sizeof(fname), -1);
	return ret;
}

/* ts A60922 ticket 33 */
/* Try to find an enumerated address with the same host,channel,target,lun
   as path */
int burn_drive_find_scsi_equiv(char *path, char adr[])
{
	int ret = 0;
	int bus_no, host_no, channel_no, target_no, lun_no;
	char msg[4096];

	ret = burn_drive_obtain_scsi_adr(path, &bus_no, &host_no, &channel_no,
					 &target_no, &lun_no);
	if(ret <= 0) {
		sprintf(msg,"burn_drive_obtain_scsi_adr( %s ) returns %d",
			path, ret);
		burn_drive_adr_debug_msg(msg, NULL);
		return 0;
	}
	sprintf(msg, "burn_drive_find_scsi_equiv( %s ) : (%d),%d,%d,%d,%d",
		path, bus_no, host_no, channel_no, target_no, lun_no);
	burn_drive_adr_debug_msg(msg, NULL);

	ret= burn_drive_convert_scsi_adr(-1, host_no, channel_no, target_no,
					 lun_no, adr);
	return ret;
}


/* ts A60922 ticket 33 */
/** Try to convert a given existing filesystem address into a persistent drive
    address.  */
int burn_drive_convert_fs_adr_sub(char *path, char adr[], int *rec_count)
{
	int ret;
	struct stat stbuf;

	burn_drive_adr_debug_msg("burn_drive_convert_fs_adr( %s )", path);
	if(burn_drive_is_enumerable_adr(path)) {
		if(strlen(path) >= BURN_DRIVE_ADR_LEN)
			return -1;
		burn_drive_adr_debug_msg(
			"burn_drive_is_enumerable_adr( %s ) is true", path);
		strcpy(adr, path);
		return 1;
	}

	if(lstat(path, &stbuf) == -1) {
		burn_drive_adr_debug_msg("lstat( %s ) returns -1", path);
		return 0;
	}
	if((stbuf.st_mode & S_IFMT) == S_IFLNK) {
		ret = burn_drive_resolve_link(path, adr, rec_count);
		if(ret > 0)
			return 1;
		burn_drive_adr_debug_msg("link fallback via stat( %s )", path);
		if(stat(path, &stbuf) == -1) {
			burn_drive_adr_debug_msg("stat( %s ) returns -1",path);
			return 0;
		}
	}
	if((stbuf.st_mode&S_IFMT) == S_IFBLK ||
	   (stbuf.st_mode&S_IFMT) == S_IFCHR) {
		ret = burn_drive_find_devno(stbuf.st_rdev, adr);
		if(ret > 0)
			return 1;
		ret = burn_drive_find_scsi_equiv(path, adr);
		if(ret > 0)
			return 1;
	}
	burn_drive_adr_debug_msg("Nothing found for %s", path);
	return 0;
}

/** Try to convert a given existing filesystem address into a persistent drive
    address.  */
int burn_drive_convert_fs_adr(char *path, char adr[])
{
	int ret, rec_count = 0;

	ret = burn_drive_convert_fs_adr_sub(path, adr, &rec_count);
	return ret;
}


/** A pacifier function suitable for burn_abort.
    @param handle If not NULL, a pointer to a text suitable for printf("%s")
*/
int burn_abort_pacifier(void *handle, int patience, int elapsed)
{
 char *prefix= "libburn : ";

 if(handle!=NULL)
	prefix= handle;
 fprintf(stderr,
         "\r%sABORT : Waiting for drive to finish ( %d s, %d max)",
         (char *) prefix, elapsed, patience);
 return(1);
}


/** Abort any running drive operation and finish libburn.
    @param patience Maximum number of seconds to wait for drives to finish
    @param pacifier_func Function to produce appeasing messages. See
                         burn_abort_pacifier() for an example.
    @return 1  ok, all went well
            0  had to leave a drive in unclean state
            <0 severe error, do no use libburn again
*/
int burn_abort(int patience, 
               int (*pacifier_func)(void *handle, int patience, int elapsed),
               void *handle)
{
	int ret, i, occup, still_not_done= 1, pacifier_off= 0, first_round= 1;
	unsigned long wait_grain= 100000;
	time_t start_time, current_time, pacifier_time, end_time;

	current_time = start_time = pacifier_time = time(0);
	end_time = start_time + patience;
	while(current_time-end_time < patience) {
		still_not_done = 0;

		for(i = 0; i < drivetop + 1; i++) {
			occup = burn_drive_is_occupied(&(drive_array[i]));
			if(occup == -2)
		continue;
			if(occup <= 10) {
				burn_drive_forget(&(drive_array[i]), 1);
			} else if(occup <= 100) {
				if(first_round)
					burn_drive_cancel(&(drive_array[i]));
				still_not_done++;
			} else if(occup <= 1000) {
				still_not_done++;
			}
		}
		first_round = 0;

		if(still_not_done == 0)
	break;
		usleep(wait_grain);
		current_time = time(0);
		if(current_time>pacifier_time) {
			if(pacifier_func != NULL && !pacifier_off) {
				ret = (*pacifier_func)(handle, patience,
						current_time-start_time);
				pacifier_off = (ret <= 0);
			}
			pacifier_time = current_time;
		}
	}
	burn_finish();
	return(still_not_done == 0); 
}


/* ts A61020 API function */
int burn_drive_get_start_end_lba(struct burn_drive *d, 
				int *start_lba, int *end_lba, int flag)
{
	if (d->start_lba == -2000000000 || d->end_lba == -2000000000)
		return 0;
	*start_lba = d->start_lba;
	*end_lba= d->end_lba;
	return 1;
}


/* ts A61020 API function */
int burn_disc_pretend_blank(struct burn_drive *d)
{
	if (d->status != BURN_DISC_UNREADY && 
	    d->status != BURN_DISC_UNSUITABLE)
		return 0;
	d->status = BURN_DISC_BLANK;
	return 1;
}

/* ts A61106 API function */
int burn_disc_pretend_full(struct burn_drive *d)
{
	if (d->status != BURN_DISC_UNREADY && 
	    d->status != BURN_DISC_UNSUITABLE)
		return 0;
	d->status = BURN_DISC_FULL;
	return 1;
}

/* ts A61021: new API function */
int burn_disc_read_atip(struct burn_drive *d)
{
	if (burn_drive_is_released(d)) {
		libdax_msgs_submit(libdax_messenger,
				d->global_index, 0x0002010e,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				"Attempt to read ATIP from ungrabbed drive",
				0, 0);
		return -1;
	}
	if (d->current_profile == -1 || d->current_is_cd_profile) {
		d->read_atip(d);
		/* >>> some control of success would be nice :) */
	} else {
		/* mmc5r03c.pdf 6.26.3.6.3 : ATIP is undefined for non-CD */;
		return 0;
	}
	return 1;
}

/* ts A61110 : new API function */
int burn_disc_track_lba_nwa(struct burn_drive *d, struct burn_write_opts *o,
			    int trackno, int *lba, int *nwa)
{
	int ret;

	if (burn_drive_is_released(d)) {
		libdax_msgs_submit(libdax_messenger,
			d->global_index, 0x0002011b,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Attempt to read track info from ungrabbed drive",
			0, 0);
		return -1;
	}
	if (d->busy != BURN_DRIVE_IDLE) {
		libdax_msgs_submit(libdax_messenger,
			d->global_index, 0x0002011c,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Attempt to read track info from busy drive",
			0, 0);
		return -1;
	}
	if (o != NULL)
		d->send_write_parameters(d, o);
	ret = d->get_nwa(d, trackno, lba, nwa);
	return ret;
}


/* ts A70131 : new API function */
int burn_disc_get_msc1(struct burn_drive *d, int *start)
{
	int ret, trackno;

	if (burn_drive_is_released(d)) {
		libdax_msgs_submit(libdax_messenger,
			d->global_index, 0x0002011b,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Attempt to read track info from ungrabbed drive",
			0, 0);
		return -1;
	}
	if (d->busy != BURN_DRIVE_IDLE) {
		libdax_msgs_submit(libdax_messenger,
			d->global_index, 0x0002011c,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Attempt to read track info from busy drive",
			0, 0);
		return -1;
	}
	ret = d->read_multi_session_c1(d, &trackno, start);
	return ret;
}


/* ts A70213 : new API function */
off_t burn_disc_available_space(struct burn_drive *d,
				 struct burn_write_opts *o)
{
	int lba, nwa;

	if (burn_drive_is_released(d))
		return d->media_capacity_remaining;
	if (d->busy != BURN_DRIVE_IDLE)
		return d->media_capacity_remaining;
	if (o != NULL)
		d->send_write_parameters(d, o);
	d->get_nwa(d, -1, &lba, &nwa);
	return d->media_capacity_remaining;
}


/* ts A61202 : New API function */
int burn_disc_get_profile(struct burn_drive *d, int *pno, char name[80])
{
	*pno = d->current_profile;
	strcpy(name,d->current_profile_text);
	return *pno >= 0;
}


/* ts A61223 : New API function */
int burn_drive_wrote_well(struct burn_drive *d)
{
	return !d->cancel;
}


/* ts A61226 */
int burn_speed_descriptor_new(struct burn_speed_descriptor **s,
			struct burn_speed_descriptor *prev,
			struct burn_speed_descriptor *next, int flag)
{
	struct burn_speed_descriptor *o;

	(*s) = o = malloc(sizeof(struct burn_speed_descriptor));
	if (o == NULL)
		return -1;
	o->source = 0;
	o->profile_loaded = -2;
	o->profile_name[0] = 0;
	o->wrc = 0;
	o->exact = 0;
	o->mrw = 0;
	o->end_lba = -1;
	o->write_speed = 0;
	o->read_speed = 0;

	o->prev = prev;
	if (prev != NULL) {
		next = prev->next;
		prev->next = o;
	} 
	o->next = next;
	if (next != NULL)
		next->prev = o;
	return 1;
}


/* ts A61226 */
/* @param flag bit0= destroy whole next-chain of descriptors */
int burn_speed_descriptor_destroy(struct burn_speed_descriptor **s, int flag)
{
	struct burn_speed_descriptor *next, *o;

	if ((*s) == NULL)
		return 0;
	if (flag&1)
		for (o = (*s); o->prev != NULL; o = o->prev);
	else
		o = (*s);
	next = o->next;
	if (next != NULL)
		next->prev = o->prev;
	if (o->prev != NULL)
		o->prev->next = next;
	free((char *) (*s));
	(*s) = NULL;
	if (flag&1)
		return burn_speed_descriptor_destroy(&next, flag&1);
	return 1;
}


/* ts A61226  */
int burn_speed_descriptor_copy(struct burn_speed_descriptor *from,
			struct burn_speed_descriptor *to, int flag)
{
	to->source = from->source;
	to->profile_loaded = from->profile_loaded;
	strcpy(to->profile_name, from->profile_name);
	to->wrc = from->wrc;
	to->exact = from->exact;
	to->mrw = from->mrw;
	to->end_lba = from->end_lba;
	to->write_speed = from->write_speed;
	to->read_speed = from->read_speed;
	return 1;
}


/* ts A61226 : free dynamically allocated sub data of struct scsi_mode_data */
int burn_mdata_free_subs(struct scsi_mode_data *m)
{
	burn_speed_descriptor_destroy(&(m->speed_descriptors), 1);
	return 1;
}


/* ts A61226 : API function */
int burn_drive_get_speedlist(struct burn_drive *d,
				 struct burn_speed_descriptor **speed_list)
{
	int ret;
	struct burn_speed_descriptor *sd, *csd = NULL;

	(*speed_list) = NULL;
	for (sd = d->mdata->speed_descriptors; sd != NULL; sd = sd->next) {
		ret = burn_speed_descriptor_new(&csd, NULL, csd, 0);
		if (ret <= 0)
			return -1;
		burn_speed_descriptor_copy(sd, csd, 0);
	}
	(*speed_list) = csd;
	return (csd != NULL);
}


/* ts A61226 : API function */
int burn_drive_free_speedlist(struct burn_speed_descriptor **speed_list)
{
	return burn_speed_descriptor_destroy(speed_list, 1);
}


/* ts A70203 : API function */
int burn_disc_get_multi_caps(struct burn_drive *d, enum burn_write_types wt,
                         struct burn_multi_caps **caps, int flag)
{
	enum burn_disc_status s;
	struct burn_multi_caps *o;
	int status, num_formats, ret, type, i;
	off_t size;
	unsigned dummy;

	*caps = NULL;
	s = burn_disc_get_status(d);
	if(s == BURN_DISC_UNGRABBED)
		return -1;
	*caps = o = (struct burn_multi_caps *)
		malloc(sizeof(struct burn_multi_caps));
	if(*caps == NULL)
		return -1;
	/* Default says nothing is available */
	o->multi_session = o->multi_track = 0;
	o-> start_adr = 0;
	o->start_alignment = o->start_range_low = o->start_range_high = 0;
	o->might_do_tao = o->might_do_sao = o->might_do_raw = 0;
	o->advised_write_mode = BURN_WRITE_NONE;
	o->selected_write_mode = wt;
	o->current_profile = d->current_profile;
	o->current_is_cd_profile = d->current_is_cd_profile;
	
	if (s != BURN_DISC_BLANK && s != BURN_DISC_APPENDABLE) {
		return 0;
	} else if (s == BURN_DISC_APPENDABLE &&
		 (wt == BURN_WRITE_SAO || wt == BURN_WRITE_RAW)) {
		return 0;
	} else if (wt == BURN_WRITE_RAW && !d->current_is_cd_profile) {
		return 0;
	} else if (d->current_profile == 0x09 || d->current_profile == 0x0a) {
		 /* CD-R , CD-RW */
		if (d->block_types[BURN_WRITE_TAO]) {
			o->multi_session = o->multi_track = 1;
			o->might_do_tao = 2;
			if (o->advised_write_mode == BURN_WRITE_NONE)
				o->advised_write_mode = BURN_WRITE_TAO;
		}
		if (d->block_types[BURN_WRITE_SAO]) {
			o->multi_session = o->multi_track = 1;
			o->might_do_sao = 1;
			if (o->advised_write_mode == BURN_WRITE_NONE)
				o->advised_write_mode = BURN_WRITE_SAO;
		}
		if (d->block_types[BURN_WRITE_RAW]) {
			o->might_do_raw = 1;
			if (o->advised_write_mode == BURN_WRITE_NONE)
				o->advised_write_mode = BURN_WRITE_RAW;
		}
		if (wt == BURN_WRITE_RAW)
			o->multi_session = o->multi_track = 0;
	} else if (d->current_profile == 0x11 || d->current_profile == 0x14) {
		/* DVD-R , sequential DVD-RW */
		if (s == BURN_DISC_BLANK) {
			o->might_do_sao = 1;
			o->advised_write_mode = BURN_WRITE_SAO;
		}
		if (d->current_has_feat21h) {
			o->multi_session = o->multi_track = 1;
			o->might_do_tao = 2;
			o->advised_write_mode = BURN_WRITE_TAO;
		}
		if (wt == BURN_WRITE_SAO)
			o->multi_session = o->multi_track = 0;
	} else if (d->current_profile == 0x12 || d->current_profile == 0x13 ||
			d->current_profile == 0x1a) {
		/* DVD-RAM, overwriteable DVD-RW, DVD+RW */
		o->start_adr = 1;
		ret = burn_disc_get_formats(d, &status, &size, &dummy,
					&num_formats);
		if (ret == 1) {
			if (status == BURN_FORMAT_IS_FORMATTED)
				o->start_range_high = size;
			if (d->current_profile == 0x13) {
				o->start_alignment = 32 * 1024;
				for (i = 0; i < num_formats; i++) {
					ret = burn_disc_get_format_descr(d, i,
						&type, &size, &dummy);
					if (ret <= 0)
				continue;
					if (type == 0x13) /* expandable */
				break;
				}
				if (i >= num_formats) /* not expandable */
					o->start_range_high -= 32 * 1024;
			} else {
				o->start_alignment = 2 * 1024;
				if (d->best_format_size - 2048 >
							 o->start_range_high)
					o->start_range_high =
						d->best_format_size - 2048;
			}
		}
		o->might_do_tao = 2;
		o->advised_write_mode = BURN_WRITE_TAO;
	} else /* unknown media */
		return 0;
		
	if (s == BURN_DISC_APPENDABLE)
		o->might_do_sao = o->might_do_raw = 0;

	if (wt == BURN_WRITE_TAO && !o->might_do_tao)
		return 0;
	else if (wt == BURN_WRITE_SAO && !o->might_do_sao)
		return 0;
	else if (wt == BURN_WRITE_RAW && !o->might_do_raw)
		return 0;
	return 1;
}


/* ts A70203 : API function */
int burn_disc_free_multi_caps(struct burn_multi_caps **caps)
{
	if (*caps == NULL)
		return 0;
	free((char *) *caps);
	*caps = NULL;
	return 1;
}


/* ts A70207 : evaluate write mode related peculiarities of a disc
   @param flag bit0= fill_up_media is active
*/
int burn_disc_get_write_mode_demands(struct burn_disc *disc,
			struct burn_disc_mode_demands *result, int flag)
{
	struct burn_session *session;
	struct burn_track *track;
	int i, j, mode, unknown_track_sizes = 0, last_track_is_unknown = 0;

	memset((char *) result, 0, sizeof(struct burn_disc_mode_demands));
	if (disc == NULL)
		return 2;
	if (disc->sessions > 1)
		result->multi_session = 1;
	for (i = 0; i < disc->sessions; i++) {
		session = disc->session[i];
		if (session->tracks <= 0)
	continue;
		mode = session->track[0]->mode;
		if (session->tracks > 1)
			result->multi_track = 1;
		for (j = 0; j < session->tracks; j++) {
			track = session->track[j];
			if (burn_track_is_open_ended(track)) {
				result->unknown_track_size = 1;
				unknown_track_sizes++;
				last_track_is_unknown = 1;
			} else
				last_track_is_unknown = 0;
			if (mode != track->mode)
				result->mixed_mode = 1;
			if (track->mode != BURN_MODE1)
				result->exotic_track = 1;
			if (track->mode == BURN_AUDIO)
				result->audio = 1;
		}
	}
	if (flag&1) {/* fill_up_media will define the size of the last track */
		if (unknown_track_sizes == 1 && last_track_is_unknown)
			result->unknown_track_size = 0;
	}
	return (disc->sessions > 0);
}
