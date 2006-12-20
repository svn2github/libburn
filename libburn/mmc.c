/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/* ts A61009 */
/* #include <a ssert.h> */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "error.h"
#include "sector.h"
#include "libburn.h"
#include "transport.h"
#include "mmc.h"
#include "spc.h"
#include "drive.h"
#include "debug.h"
#include "toc.h"
#include "structure.h"
#include "options.h"


#ifdef Libburn_log_in_and_out_streaM
/* <<< ts A61031 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif /* Libburn_log_in_and_out_streaM */


/* ts A61005 */
#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;


/* ts A61219 : ! HIGHLY EXPERIMENTAL !
               Based on knowlege from dvd+rw-tools-7.0 and mmc5r03c.pdf
*/
/*
#define Libburn_support_dvd_plus_rW 1
*/
/* Progress report (with Libburn_support_dvd_plus_rW defined):
   ts A61219 : It seems to work with a used (i.e. thoroughly formatted) DVD+RW.
               Error messages of class DEBUG appear because of inability to
               read TOC or track info. Nevertheless, the written images verify.
   ts A61220 : Burned to a virgin DVD+RW by help of new mmc_format_unit()
               (did not test wether it would work without). Burned to a
               not completely formatted DVD+RW. (Had worked before without
               mmc_format_unit(). I did not exceed the formatted range
               as reported by dvd+rw-mediainfo. 
*/


static unsigned char MMC_GET_TOC[] = { 0x43, 2, 2, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_GET_ATIP[] = { 0x43, 2, 4, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_GET_DISC_INFO[] =
	{ 0x51, 0, 0, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_READ_CD[] = { 0xBE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char MMC_ERASE[] = { 0xA1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char MMC_SEND_OPC[] = { 0x54, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char MMC_SET_SPEED[] =
	{ 0xBB, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char MMC_WRITE_12[] =
	{ 0xAA, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char MMC_WRITE_10[] = { 0x2A, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* ts A61201 : inserted 0, before 16, */
static unsigned char MMC_GET_CONFIGURATION[] =
	{ 0x46, 0, 0, 0, 0, 0, 0, 16, 0, 0 };

static unsigned char MMC_SYNC_CACHE[] = { 0x35, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char MMC_GET_EVENT[] = { 0x4A, 1, 0, 0, 16, 0, 0, 0, 8, 0 };
static unsigned char MMC_CLOSE[] = { 0x5B, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char MMC_TRACK_INFO[] = { 0x52, 0, 0, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_SEND_CUE_SHEET[] =
	{ 0x5D, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* ts A61023 : get size and free space of drive buffer */
static unsigned char MMC_READ_BUFFER_CAPACITY[] = { 0x5C, 0, 0, 0, 0, 0, 0, 16, 0, 0 };

/* ts A61219 : format DVD+RW (and various others) */
static unsigned char MMC_FORMAT_UNIT[] = { 0x04, 0x11, 0, 0, 0, 0 };


static int mmc_function_spy_do_tell = 0;

int mmc_function_spy(char * text)
{

	if (mmc_function_spy_do_tell)
		fprintf(stderr,"libburn: experimental: mmc_function_spy: %s\n",
			text);
	return 1;
}

int mmc_function_spy_ctrl(int do_tell)
{
	mmc_function_spy_do_tell= !!do_tell;
	return 1;
}


void mmc_send_cue_sheet(struct burn_drive *d, struct cue_sheet *s)
{
	struct buffer buf;
	struct command c;


	mmc_function_spy("mmc_send_cue_sheet");
	c.retry = 1;
	c.oplen = sizeof(MMC_SEND_CUE_SHEET);
	memcpy(c.opcode, MMC_SEND_CUE_SHEET, sizeof(MMC_SEND_CUE_SHEET));
	c.page = &buf;
	c.page->bytes = s->count * 8;
	c.page->sectors = 0;
	c.opcode[6] = (c.page->bytes >> 16) & 0xFF;
	c.opcode[7] = (c.page->bytes >> 8) & 0xFF;
	c.opcode[8] = c.page->bytes & 0xFF;
	c.dir = TO_DRIVE;
	memcpy(c.page->data, s->data, c.page->bytes);
	d->issue_command(d, &c);
}

/* ts A61110 : added parameters trackno, lba, nwa. Redefined return value. 
   @return 1=nwa is valid , 0=nwa is not valid , -1=error */
int mmc_get_nwa(struct burn_drive *d, int trackno, int *lba, int *nwa)
{
	struct buffer buf;
	struct command c;
	unsigned char *data;

	mmc_function_spy("mmc_get_nwa");
	c.retry = 1;
	c.oplen = sizeof(MMC_TRACK_INFO);
	memcpy(c.opcode, MMC_TRACK_INFO, sizeof(MMC_TRACK_INFO));
	c.opcode[1] = 1;
	if(trackno<=0) {
		if (d->current_profile = 0x1a) /* DVD+RW */
			c.opcode[5] = 1;
		else /* mmc5r03c.pdf: valid only for CD, DVD+R, DVD+R DL */
			c.opcode[5] = 0xFF;
	} else
		c.opcode[5] = trackno;
	c.page = &buf;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);
	data = c.page->data;

	*lba = (data[8] << 24) + (data[9] << 16)
		+ (data[10] << 8) + data[11];
	*nwa = (data[12] << 24) + (data[13] << 16)
		+ (data[14] << 8) + data[15];
	if (d->current_profile = 0x1a) { /* DVD+RW */
		*nwa = *nwa = 0;
	} else if (!(data[7]&1)) {
		/* ts A61106 :  MMC-1 Table 142 : NWA_V = NWA Valid Flag */
		libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   "mmc_get_nwa: Track Info Block: NWA_V == 0", 0, 0);
		return 0;
	}
	return 1;
}

/* ts A61009 : function is obviously unused. */
/* void mmc_close_disc(struct burn_drive *d, struct burn_write_opts *o) */
void mmc_close_disc(struct burn_write_opts *o)
{
	struct burn_drive *d;

	mmc_function_spy("mmc_close_disc");

	libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   "HOW THAT ? mmc_close_disc() was called", 0, 0);

	/* ts A61009 : made impossible by removing redundant parameter d */
	/* a ssert(o->drive == d); */
	d = o->drive;

	o->multi = 0;
	spc_select_write_params(d, o);
	mmc_close(d, 1, 0);
}

/* ts A61009 : function is obviously unused. */
/* void mmc_close_session(struct burn_drive *d, struct burn_write_opts *o) */
void mmc_close_session(struct burn_write_opts *o)
{
	struct burn_drive *d;

	mmc_function_spy("mmc_close_session");

	libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   "HOW THAT ? mmc_close_session() was called", 0, 0);

	/* ts A61009 : made impossible by removing redundant parameter d */
	/* a ssert(o->drive == d); */
	d = o->drive;

	o->multi = 3;
	spc_select_write_params(d, o);
	mmc_close(d, 1, 0);
}

void mmc_close(struct burn_drive *d, int session, int track)
{
	struct command c;

	mmc_function_spy("mmc_close");

	c.retry = 1;
	c.oplen = sizeof(MMC_CLOSE);
	memcpy(c.opcode, MMC_CLOSE, sizeof(MMC_CLOSE));

	/* ts A61030 : shifted !!session rather than or-ing plain session */
	c.opcode[2] = ((!!session)<<1) | !!track;

	c.opcode[4] = track >> 8;
	c.opcode[5] = track & 0xFF;
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
}

void mmc_get_event(struct burn_drive *d)
{
	struct buffer buf;
	struct command c;

	mmc_function_spy("mmc_get_event");
	c.retry = 1;
	c.oplen = sizeof(MMC_GET_EVENT);
	memcpy(c.opcode, MMC_GET_EVENT, sizeof(MMC_GET_EVENT));
	c.page = &buf;
	c.page->bytes = 0;
	c.page->sectors = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);
	burn_print(12, "0x%x:0x%x:0x%x:0x%x\n",
		   c.page->data[0], c.page->data[1], c.page->data[2],
		   c.page->data[3]);
	burn_print(12, "event: %d:%d:%d:%d\n", c.page->data[4],
		   c.page->data[5], c.page->data[6], c.page->data[7]);
}


void mmc_write_12(struct burn_drive *d, int start, struct buffer *buf)
{
	struct command c;
	int len;

	mmc_function_spy("mmc_write_12");
	len = buf->sectors;

	/* ts A61009 */
	/* a ssert(buf->bytes >= buf->sectors);*/	/* can be == at 0... */

	burn_print(100, "trying to write %d at %d\n", len, start);
	memcpy(c.opcode, MMC_WRITE_12, sizeof(MMC_WRITE_12));
	c.retry = 1;
	c.oplen = sizeof(MMC_WRITE_12);
	c.opcode[2] = start >> 24;
	c.opcode[3] = (start >> 16) & 0xFF;
	c.opcode[4] = (start >> 8) & 0xFF;
	c.opcode[5] = start & 0xFF;
	c.opcode[6] = len >> 24;
	c.opcode[7] = (len >> 16) & 0xFF;
	c.opcode[8] = (len >> 8) & 0xFF;
	c.opcode[9] = len & 0xFF;
	c.page = buf;
	c.dir = TO_DRIVE;

	d->issue_command(d, &c);
}

int mmc_write(struct burn_drive *d, int start, struct buffer *buf)
{
	int cancelled;
	struct command c;
	int len;

#ifdef Libburn_log_in_and_out_streaM
	/* <<< ts A61031 */
	static int tee_fd= -1;
	if(tee_fd==-1)
		tee_fd= open("/tmp/libburn_sg_written",
				O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
#endif /* Libburn_log_in_and_out_streaM */

	mmc_function_spy("mmc_write");
	pthread_mutex_lock(&d->access_lock);
	cancelled = d->cancel;
	pthread_mutex_unlock(&d->access_lock);

	if (cancelled)
		return BE_CANCELLED;

	len = buf->sectors;

	/* ts A61009 : buffer fill problems are to be handled by caller */
	/* a ssert(buf->bytes >= buf->sectors);*/	/* can be == at 0... */

	burn_print(100, "trying to write %d at %d\n", len, start);
	memcpy(c.opcode, MMC_WRITE_10, sizeof(MMC_WRITE_10));
	c.retry = 1;
	c.oplen = sizeof(MMC_WRITE_10);
	c.opcode[2] = start >> 24;
	c.opcode[3] = (start >> 16) & 0xFF;
	c.opcode[4] = (start >> 8) & 0xFF;
	c.opcode[5] = start & 0xFF;
	c.opcode[6] = 0;
	c.opcode[7] = (len >> 8) & 0xFF;
	c.opcode[8] = len & 0xFF;
	c.page = buf;
	c.dir = TO_DRIVE;
/*
	burn_print(12, "%d, %d, %d, %d - ", c->opcode[2], c->opcode[3], c->opcode[4], c->opcode[5]);
	burn_print(12, "%d, %d, %d, %d\n", c->opcode[6], c->opcode[7], c->opcode[8], c->opcode[9]);
*/

#ifdef Libburn_log_in_and_out_streaM
	/* <<< ts A61031 */
	if(tee_fd!=-1) {
		write(tee_fd,c.page->data,len*2048);
	}
#endif /* Libburn_log_in_and_out_streaM */

	d->issue_command(d, &c);

	/* ts A61112 : react on eventual error condition */ 
	if (c.error && c.sense[2]!=0) {

		/* >>> make this scsi_notify_error() when liberated */
		if (c.sense[2]!=0) {
			char msg[160];
			sprintf(msg,
		"SCSI error on write(%d,%d): key=%X asc=%2.2Xh ascq=%2.2Xh",
				start, len,
				c.sense[2],c.sense[12],c.sense[13]);
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002011d,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
		}
		pthread_mutex_lock(&d->access_lock);
		d->cancel = 1;
		pthread_mutex_unlock(&d->access_lock);
		return BE_CANCELLED;
	} 

	return 0;
}

void mmc_read_toc(struct burn_drive *d)
{
/* read full toc, all sessions, in m/s/f form, 4k buffer */
	struct burn_track *track;
	struct burn_session *session;
	struct buffer buf;
	struct command c;
	int dlen;
	int i, bpl= 12;
	unsigned char *tdata;

	mmc_function_spy("mmc_read_toc");
	memcpy(c.opcode, MMC_GET_TOC, sizeof(MMC_GET_TOC));
	c.retry = 1;
	c.oplen = sizeof(MMC_GET_TOC);
	c.page = &buf;
	c.page->bytes = 0;
	c.page->sectors = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	if (c.error) {

		/* ts A61020 : this snaps on non-blank DVD media */
		/* ts A61106 : also snaps on CD with unclosed track/session */
		/* Very unsure wether this old measure is ok.
		   Obviously higher levels do not care about this.
		   outdated info: DVD+RW burns go on after passing through here.

		d->busy = BURN_DRIVE_IDLE;
		*/
		libdax_msgs_submit(libdax_messenger, d->global_index,
			 0x0002010d,
			 LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH,
			 "Could not inquire TOC (non-blank DVD media ?)", 0,0);
		d->status = BURN_DISC_UNSUITABLE;
		d->toc_entries = 0;
		/* Prefering memory leaks over fandangos */
		d->toc_entry = malloc(sizeof(struct burn_toc_entry));

		return;
	}

	dlen = c.page->data[0] * 256 + c.page->data[1];
	d->toc_entries = (dlen - 2) / 11;
/*
	some drives fail this check.

	ts A61007 : if re-enabled then not via Assert.
	a ssert(((dlen - 2) % 11) == 0);
*/
	d->toc_entry = malloc(d->toc_entries * sizeof(struct burn_toc_entry));
	tdata = c.page->data + 4;

	burn_print(12, "TOC:\n");

	d->disc = burn_disc_create();

	for (i = 0; i < c.page->data[3]; i++) {
		session = burn_session_create();
		burn_disc_add_session(d->disc, session, BURN_POS_END);
		burn_session_free(session);
	}

	/* ts A61022 */
	burn_print(bpl, "-----------------------------------\n");

	for (i = 0; i < d->toc_entries; i++, tdata += 11) {

		/* ts A61022: was burn_print level 12 */
		burn_print(bpl, "S %d, PT %2.2Xh, TNO %d :", tdata[0],tdata[3],
			   tdata[2]);
		burn_print(bpl, " MSF(%d:%d:%d)", tdata[4],tdata[5],tdata[6]);
		burn_print(bpl, " PMSF(%d:%d:%d %d)",
				tdata[8], tdata[9], tdata[10],
				burn_msf_to_lba(tdata[8], tdata[9], tdata[10])
			);
		burn_print(bpl, " - control %d, adr %d\n", tdata[1] & 0xF,
			   tdata[1] >> 4);

/*
		fprintf(stderr, "libburn_experimental: toc entry #%d : %d %d %d\n",i,tdata[8], tdata[9], tdata[10]); 
*/

		if (tdata[3] == 1) {
			if (burn_msf_to_lba(tdata[8], tdata[9], tdata[10])) {
				d->disc->session[0]->hidefirst = 1;
				track = burn_track_create();
				burn_session_add_track(d->disc->
						       session[tdata[0] - 1],
						       track, BURN_POS_END);
				burn_track_free(track);

			}
		}
		if (tdata[0] <= 0 || tdata[0] > d->disc->sessions)
			tdata[0] = d->disc->sessions;
		if (tdata[3] < 100 && tdata[0] > 0) {
			track = burn_track_create();
			burn_session_add_track(d->disc->session[tdata[0] - 1],
					       track, BURN_POS_END);
			track->entry = &d->toc_entry[i];
			burn_track_free(track);
		}
		d->toc_entry[i].session = tdata[0];
		d->toc_entry[i].adr = tdata[1] >> 4;
		d->toc_entry[i].control = tdata[1] & 0xF;
		d->toc_entry[i].tno = tdata[2];
		d->toc_entry[i].point = tdata[3];
		d->toc_entry[i].min = tdata[4];
		d->toc_entry[i].sec = tdata[5];
		d->toc_entry[i].frame = tdata[6];
		d->toc_entry[i].zero = tdata[7];
		d->toc_entry[i].pmin = tdata[8];
		d->toc_entry[i].psec = tdata[9];
		d->toc_entry[i].pframe = tdata[10];
		if (tdata[3] == 0xA0)
			d->disc->session[tdata[0] - 1]->firsttrack = tdata[8];
		if (tdata[3] == 0xA1)
			d->disc->session[tdata[0] - 1]->lasttrack = tdata[8];
		if (tdata[3] == 0xA2)
			d->disc->session[tdata[0] - 1]->leadout_entry =
				&d->toc_entry[i];
	}

	/* ts A61022 */
	burn_print(bpl, "-----------------------------------\n");

	if (d->status != BURN_DISC_APPENDABLE)
		d->status = BURN_DISC_FULL;
	toc_find_modes(d);
}

void mmc_read_disc_info(struct burn_drive *d)
{
	struct buffer buf;
	unsigned char *data;
	struct command c;
	char msg[160];

	/* ts A61020 */
	d->start_lba = d->end_lba = -2000000000;
	d->erasable = 0;

	/* ts A61202 */
	d->toc_entries = 0;
	if (d->status == BURN_DISC_EMPTY)
		return;

	mmc_get_configuration(d);
	if ((d->current_profile != 0 || d->status != BURN_DISC_UNREADY) 
		&& ! d->current_is_supported_profile) {
		if (!d->silent_on_scsi_error) {
			sprintf(msg,
				"Unsuitable media detected. Profile %4.4Xh  %s",
				d->current_profile, d->current_profile_text);
			libdax_msgs_submit(libdax_messenger, d->global_index,
				 0x0002011e,
				 LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				 msg, 0,0);
		}
		d->status = BURN_DISC_UNSUITABLE;
		return;
	}

	mmc_function_spy("mmc_read_disc_info");
	memcpy(c.opcode, MMC_GET_DISC_INFO, sizeof(MMC_GET_DISC_INFO));
	c.retry = 1;
	c.oplen = sizeof(MMC_GET_DISC_INFO);
	c.page = &buf;
	c.page->sectors = 0;
	c.page->bytes = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	if (c.error) {
		d->busy = BURN_DRIVE_IDLE;
		return;
	}

	data = c.page->data;
	d->erasable = !!(data[2] & 16);

/*
	fprintf(stderr, "libburn_experimental: data[2]= %d  0x%x\n",
			(unsigned) data[2], (unsigned) data[2]);
*/
	switch (data[2] & 3) {
	case 0:
		d->toc_entries = 0;
		d->start_lba = burn_msf_to_lba(data[17], data[18], data[19]);
		d->end_lba = burn_msf_to_lba(data[21], data[22], data[23]);

/*
		fprintf(stderr, "libburn_experimental: start_lba = %d (%d %d %d) , end_lba = %d (%d %d %d)\n",
			d->start_lba, data[17], data[18], data[19],
			d->end_lba, data[21], data[22], data[23]);
*/

		d->status = BURN_DISC_BLANK;
		break;
	case 1:
		d->status = BURN_DISC_APPENDABLE;
	case 2:
		if (d->current_profile == -1 || d->current_is_cd_profile)
			mmc_read_toc(d);
		break;
	}

	/* >>> ts A61217 : Note for future
	   growisofs performs OPC if (data[0]<<8)|data[1]<=32
	   which indicates no OPC entries are attached to the
	   reply from the drive.
	*/

	/* ts A61219 : mmc5r03c.pdf 6.22.3.1.13 BG Format Status
	   0=blank (not yet started)
           1=started but neither running nor complete
	   2=in progress
	   3=completed
	*/
	d->bg_format_status = data[7] & 3;

	/* <<< ts A61219 : preliminarily declare all DVD+RW blank,
		(which is not the same as bg_format_status==0 "blank") */
	if (d->current_profile == 0x1a)
		d->status = BURN_DISC_BLANK;
}

void mmc_read_atip(struct burn_drive *d)
{
	struct buffer buf;
	struct command c;

        /* ts A61021 */
	unsigned char *data;
	/* Speed values from A1: 
	   With 4 cdrecord tells "10" or "8" where MMC-1 says "8".
	   cdrecord "8" appear on 4xCD-RW and thus seem to be quite invalid.
	   My CD-R (>=24 speed) tell no A1.
	   The higher non-MMC-1 values are hearsay.
	*/
        static int speed_value[16]= {  0,  2,  4,  6,  10,  -5,  16,  -7,
				      24, 32, 40, 48, -12, -13, -14, -15};

	mmc_function_spy("mmc_read_atip");
	memcpy(c.opcode, MMC_GET_ATIP, sizeof(MMC_GET_ATIP));
	c.retry = 1;
	c.oplen = sizeof(MMC_GET_ATIP);
	c.page = &buf;
	c.page->bytes = 0;
	c.page->sectors = 0;

	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);
	burn_print(1, "atip shit for you\n");


	/* ts A61021 */
	data = c.page->data;
	d->erasable= !!(data[6]&64);
	d->start_lba= burn_msf_to_lba(data[8],data[9],data[10]);
	d->end_lba= burn_msf_to_lba(data[12],data[13],data[14]);
	if (data[6]&4) {
		if (speed_value[(data[16]>>4)&7] > 0)
			d->mdata->min_write_speed = 
				speed_value[(data[16]>>4)&7]*176;
		if (speed_value[(data[16])&15] > 0)
			d->mdata->max_write_speed = 
				speed_value[(data[16])&15]*176;
	}

#ifdef Burn_mmc_be_verbous_about_atiP
	{ int i;
	fprintf(stderr,"libburn_experimental: Returned ATIP Data\n");
	for(i= 0; i<28; i++)
		fprintf(stderr,"%3.3d (0x%2.2x)%s",
			data[i],data[i],((i+1)%5 ? "  ":"\n"));
	fprintf(stderr,"\n");

	fprintf(stderr,
		"libburn_experimental: Indicative Target Writing Power= %d\n",
		(data[4]>>4)&7);
	fprintf(stderr,
		"libburn_experimental: Reference speed= %d ->%d\n",
		data[4]&7, speed_value[data[4]&7]);
	fprintf(stderr,
		"libburn_experimental: Is %sunrestricted\n",
		(data[5]&64?"":"not "));
	fprintf(stderr,
		"libburn_experimental: Is %serasable, sub-type %d\n",
		(data[6]&64?"":"not "),(data[6]>>3)&3);
	fprintf(stderr,
		"libburn_experimental: lead in: %d (%-2.2d:%-2.2d/%-2.2d)\n",
		burn_msf_to_lba(data[8],data[9],data[10]),
		data[8],data[9],data[10]);
	fprintf(stderr,
		"libburn_experimental: lead out: %d (%-2.2d:%-2.2d/%-2.2d)\n",
		burn_msf_to_lba(data[12],data[13],data[14]),
		data[12],data[13],data[14]);
	if(data[6]&4)
	  fprintf(stderr,
		"libburn_experimental: A1 speed low %d   speed high %d\n",
		speed_value[(data[16]>>4)&7], speed_value[(data[16])&7]);
	if(data[6]&2)
	  fprintf(stderr,
		"libburn_experimental: A2 speed low %d   speed high %d\n",
		speed_value[(data[20]>>4)&7], speed_value[(data[20])&7]);
	if(data[6]&1)
	  fprintf(stderr,
		"libburn_experimental: A3 speed low %d   speed high %d\n",
		speed_value[(data[24]>>4)&7], speed_value[(data[24])&7]);
	}

#endif /* Burn_mmc_be_verbous_about_atiP */

/* ts A61020
http://www.t10.org/ftp/t10/drafts/mmc/mmc-r10a.pdf , table 77 :

 0 ATIP Data Length MSB
 1 ATIP Data Length LSB
 2 Reserved
 3 Reserved
 4 bit7=1, bit4-6="Indicative Target Writing Power", bit3=reserved ,
   bit0-2="Reference speed"
 5 bit7=0, bit6="URU" , bit0-5=reserved
 6 bit7=1, bit6="Disc Type", bit3-4="Disc Sub-Type", 
   bit2="A1", bit1="A2", bit0="A3"
 7 reserved
 8 ATIP Start Time of lead-in (Min)
 9 ATIP Start Time of lead-in (Sec)
10 ATIP Start Time of lead-in (Frame)
11 reserved
12 ATIP Last Possible Start Time of lead-out (Min)
13 ATIP Last Possible Start Time of lead-out (Sec)
14 ATIP Last Possible Start Time of lead-out (Frame)
15 reserved
16 bit7=0, bit4-6="Lowest Usable CLV Recording speed"
   bit0-3="Highest Usable CLV Recording speed"
17 bit7=0, bit4-6="Power Multiplication Factor p", 
   bit1-3="Target y value of the Modulation/Power function", bit0=reserved
18 bit7=1, bit4-6="Recommended Erase/Write Power Ratio (P(inf)/W(inf))"
   bit0-3=reserved
19 reserved
20-22 A2 Values
23 reserved
24-26 A3 Values
27 reserved

Disc Type - zero indicates CD-R media; one indicates CD-RW media.

Disc Sub-Type - shall be set to zero.

A1 - when set to one, indicates that bytes 16-18 are valid.

Lowest Usable CLV Recording Speed
000b Reserved
001b 2X
010b - 111b Reserved

Highest CLV Recording Speeds
000b Reserved
001b 2X
010b 4X
011b 6X
100b 8X
101b - 111b Reserved

MMC-3 seems to recommend MODE SENSE (5Ah) page 2Ah rather than A1, A2, A3.
This page is loaded in libburn function  spc_sense_caps() .
Speed is given in kbytes/sec there. But i suspect this to be independent
of media. So one would habe to associate the speed descriptor blocks with
the ATIP media characteristics ? How ?

*/
}

void mmc_read_sectors(struct burn_drive *d,
		      int start,
		      int len,
		      const struct burn_read_opts *o, struct buffer *buf)
{
	int temp;
	int errorblock, req;
	struct command c;

	mmc_function_spy("mmc_read_sectors");

	/* ts A61009 : to be ensured by callers */
	/* a ssert(len >= 0); */

/* if the drive isn't busy, why the hell are we here? */ 
	/* ts A61006 : i second that question */
	/* a ssert(d->busy); */

	burn_print(12, "reading %d from %d\n", len, start);
	memcpy(c.opcode, MMC_READ_CD, sizeof(MMC_READ_CD));
	c.retry = 1;
	c.oplen = sizeof(MMC_READ_CD);
	temp = start;
	c.opcode[5] = temp & 0xFF;
	temp >>= 8;
	c.opcode[4] = temp & 0xFF;
	temp >>= 8;
	c.opcode[3] = temp & 0xFF;
	temp >>= 8;
	c.opcode[2] = temp & 0xFF;
	c.opcode[8] = len & 0xFF;
	len >>= 8;
	c.opcode[7] = len & 0xFF;
	len >>= 8;
	c.opcode[6] = len & 0xFF;
	req = 0xF8;

	/* ts A61106 : LG GSA-4082B dislikes this. key=5h asc=24h ascq=00h

	if (d->busy == BURN_DRIVE_GRABBING || o->report_recovered_errors)
		req |= 2;
	*/

	c.opcode[10] = 0;
/* always read the subcode, throw it away later, since we don't know
   what we're really reading
*/
	if (d->busy == BURN_DRIVE_GRABBING || (o->subcodes_audio)
	    || (o->subcodes_data))
		c.opcode[10] = 1;

	c.opcode[9] = req;
	c.page = buf;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	if (c.error) {
		burn_print(12, "got an error over here\n");
		burn_print(12, "%d, %d, %d, %d\n", c.sense[3], c.sense[4],
			   c.sense[5], c.sense[6]);
		errorblock =
			(c.sense[3] << 24) + (c.sense[4] << 16) +
			(c.sense[5] << 8) + c.sense[6];
		c.page->sectors = errorblock - start + 1;
		burn_print(1, "error on block %d\n", errorblock);
		burn_print(12, "error on block %d\n", errorblock);
		burn_print(12, "returning %d sectors\n", c.page->sectors);
	}
}

void mmc_erase(struct burn_drive *d, int fast)
{
	struct command c;

	mmc_function_spy("mmc_erase");
	memcpy(c.opcode, MMC_ERASE, sizeof(MMC_ERASE));
	c.opcode[1] = 16;	/* IMMED set to 1 */
	c.opcode[1] |= !!fast;
	c.retry = 1;
	c.oplen = sizeof(MMC_ERASE);
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
}

void mmc_read_lead_in(struct burn_drive *d, struct buffer *buf)
{
	int len;
	struct command c;

	mmc_function_spy("mmc_read_lead_in");
	len = buf->sectors;
	memcpy(c.opcode, MMC_READ_CD, sizeof(MMC_READ_CD));
	c.retry = 1;
	c.oplen = sizeof(MMC_READ_CD);
	c.opcode[5] = 0;
	c.opcode[4] = 0;
	c.opcode[3] = 0;
	c.opcode[2] = 0xF0;
	c.opcode[8] = 1;
	c.opcode[7] = 0;
	c.opcode[6] = 0;
	c.opcode[9] = 0;
	c.opcode[10] = 2;
	c.page = buf;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);
}

void mmc_perform_opc(struct burn_drive *d)
{
	struct command c;

	mmc_function_spy("mmc_perform_opc");
	memcpy(c.opcode, MMC_SEND_OPC, sizeof(MMC_SEND_OPC));
	c.retry = 1;
	c.oplen = sizeof(MMC_SEND_OPC);
	c.opcode[1] = 1;
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
}

void mmc_set_speed(struct burn_drive *d, int r, int w)
{
	struct command c;

	/* ts A61112 : MMC standards prescribe FFFFh as max speed.
			But libburn.h prescribes 0. */
	if (r<=0 || r>0xffff)
		r = 0xffff;
	if (w<=0 || w>0xffff)
		w = 0xffff;

	mmc_function_spy("mmc_set_speed");
	memcpy(c.opcode, MMC_SET_SPEED, sizeof(MMC_SET_SPEED));
	c.retry = 1;
	c.oplen = sizeof(MMC_SET_SPEED);
	c.opcode[2] = r >> 8;
	c.opcode[3] = r & 0xFF;
	c.opcode[4] = w >> 8;
	c.opcode[5] = w & 0xFF;
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
}


/* ts A61201 */
static char *mmc_obtain_profile_name(int profile_number)
{
	static char *texts[0x53] = {NULL};
	int i, max_pno = 0x53;
	
	if (texts[0] == NULL) {
		for (i = 0; i<max_pno; i++)
			texts[i] = "";
		/* mmc5r03c.pdf , Table 89, Spelling: guessed cdrecord style */
		texts[0x01] = "Non-removable disk";
		texts[0x02] = "Removable disk";
		texts[0x03] = "MO erasable";
		texts[0x04] = "Optical write once";
		texts[0x05] = "AS-MO";
		texts[0x08] = "CD-ROM";
		texts[0x09] = "CD-R";
		texts[0x0a] = "CD-RW";
		texts[0x10] = "DVD-ROM";
		texts[0x11] = "DVD-R sequential recording";
		texts[0x12] = "DVD-RAM";
		texts[0x13] = "DVD-RW restricted overwrite";
		texts[0x14] = "DVD-RW sequential overwrite";
		texts[0x15] = "DVD-R/DL sequential recording";
		texts[0x16] = "DVD-R/DL layer jump recording";
		texts[0x1a] = "DVD+RW";
		texts[0x1b] = "DVD+R";
		texts[0x2a] = "DVD+RW/DL";
		texts[0x2b] = "DVD+R/DL";
		texts[0x40] = "BD-ROM";
		texts[0x41] = "BD-R sequential recording";
		texts[0x42] = "BD-R random recording";
		texts[0x43] = "BD-RE";
		texts[0x50] = "HD-DVD-ROM";
		texts[0x51] = "HD-DVD-R";
		texts[0x52] = "HD-DVD-RAM";
	}
	if (profile_number<0 || profile_number>=max_pno)
		return "";
	return texts[profile_number];
}


/* ts A61201 : found in unfunctional state */
void mmc_get_configuration(struct burn_drive *d)
{
	struct buffer buf;
	int len, cp;
	struct command c;

	d->current_profile = 0;
        d->current_profile_text[0] = 0;
	d->current_is_cd_profile = 0;
	d->current_is_supported_profile = 0;

	mmc_function_spy("mmc_get_configuration");
	memcpy(c.opcode, MMC_GET_CONFIGURATION, sizeof(MMC_GET_CONFIGURATION));
	c.retry = 1;
	c.oplen = sizeof(MMC_GET_CONFIGURATION);
	c.page = &buf;
	c.page->sectors = 0;
	c.page->bytes = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	if (c.error)
		return;
	len = (c.page->data[0] << 24)
		+ (c.page->data[1] << 16)
		+ (c.page->data[2] << 8)
		+ c.page->data[3];

	if (len<8)
		return;
	cp = (c.page->data[6]<<8) | c.page->data[7];
	d->current_profile = cp;
	strcpy(d->current_profile_text, mmc_obtain_profile_name(cp));
	if (cp == 0x08 || cp == 0x09 || cp == 0x0a)
		d->current_is_supported_profile = d->current_is_cd_profile = 1;

#ifdef Libburn_support_dvd_plus_rW
	if (cp == 0x1a)
		d->current_is_supported_profile = 1;
#endif

}

void mmc_sync_cache(struct burn_drive *d)
{
	struct command c;

	mmc_function_spy("mmc_sync_cache");
	memcpy(c.opcode, MMC_SYNC_CACHE, sizeof(MMC_SYNC_CACHE));
	c.retry = 1;
	c.oplen = sizeof(MMC_SYNC_CACHE);
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
}


/* ts A61023 : http://libburn.pykix.org/ticket/14
               get size and free space of drive buffer
*/
int mmc_read_buffer_capacity(struct burn_drive *d)
{
	struct buffer buf;
	struct command c;
	unsigned char *data;

	mmc_function_spy("mmc_read_buffer_capacity");
	memcpy(c.opcode, MMC_READ_BUFFER_CAPACITY,
		 sizeof(MMC_READ_BUFFER_CAPACITY));
	c.retry = 1;
	c.oplen = sizeof(MMC_READ_BUFFER_CAPACITY);
	c.page = &buf;
	c.page->bytes = 0;
	c.page->sectors = 0;

	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	/* >>> ??? error diagnostics */

	data = c.page->data;

	d->progress.buffer_capacity =
			(data[4]<<24)|(data[5]<<16)|(data[6]<<8)|data[7];
	d->progress.buffer_available =
			(data[8]<<24)|(data[9]<<16)|(data[10]<<8)|data[11];
	if (d->progress.buffered_bytes >= d->progress.buffer_capacity){
		double fill;

		fill = d->progress.buffer_capacity
			      - d->progress.buffer_available;
		if (fill < d->progress.buffer_min_fill && fill>=0)
			d->progress.buffer_min_fill = fill;
	}
	return 1;
}


/* ts A61219 : learned much from dvd+rw-tools-7.0: plus_rw_format() */
int mmc_format_unit(struct burn_drive *d)
{
	struct buffer buf;
	struct command c;
	int ret;
	char msg[160],descr[80];

	mmc_function_spy("mmc_format_unit");
	c.retry = 1;
	c.oplen = sizeof(MMC_FORMAT_UNIT);
	memcpy(c.opcode, MMC_FORMAT_UNIT, sizeof(MMC_FORMAT_UNIT));
	c.page = &buf;
	c.page->bytes = 12;
	c.page->sectors = 0;
	c.dir = TO_DRIVE;
	memset(c.page->data, 0, c.page->bytes);

	descr[0] = 0;
	if (d->current_profile == 0x1a) { /* DVD+RW */
		c.page->data[1] = 0x02;          /* Immed */
		c.page->data[3] = 8;             /* Format descriptor length */
		/* mmc5r03c.pdf , 6.5.4.2.14 */
		c.page->data[8] = 0x26 << 2;        /* Format type */
		memset(c.page->data + 4, 0xff, 4);  /* maximum blocksize */
		if (d->bg_format_status == 1)       /* is partly formatted */
			c.page->data[11] = 1;       /* Restart bit */
		else if(d->bg_format_status == 2) { /* format in progress */
			strcpy(msg,"FORMAT UNIT ignored. Already in progress");
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020120,
				LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0,0);
		}
		sprintf(descr, "DVD+RW, BGFS %d",
			d->bg_format_status);
	} else { 

	/* >>> other formattable types to come */

		sprintf(msg, "Unsuitable media detected. Profile %4.4Xh  %s",
			d->current_profile, d->current_profile_text);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x0002011e,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			msg, 0,0);
		return 0;
	}

	d->issue_command(d, &c);
	if (c.error) {
		if (c.sense[2]!=0) {
			sprintf(msg,
		"SCSI error on format_unit(%s): key=%X asc=%2.2Xh ascq=%2.2Xh",
				descr,
				c.sense[2],c.sense[12],c.sense[13]);
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020122,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
		}
		return 0;
	}

	for (ret = 0; ret <= 0 ;)
		ret = spc_test_unit_ready(d);
	mmc_sync_cache(d);
	return 1;
}


/* ts A61021 : the mmc specific part of sg.c:enumerate_common()
*/
int mmc_setup_drive(struct burn_drive *d)
{
	d->read_atip = mmc_read_atip;
	d->read_toc = mmc_read_toc;
	d->write = mmc_write;
	d->erase = mmc_erase;
	d->read_sectors = mmc_read_sectors;
	d->perform_opc = mmc_perform_opc;
	d->set_speed = mmc_set_speed;
	d->send_cue_sheet = mmc_send_cue_sheet;
	d->sync_cache = mmc_sync_cache;
	d->get_nwa = mmc_get_nwa;
	d->close_disc = mmc_close_disc;
	d->close_session = mmc_close_session;
	d->close_track_session = mmc_close;
	d->read_buffer_capacity = mmc_read_buffer_capacity;
	d->format_unit = mmc_format_unit;

	/* ts A61020 */
	d->start_lba = -2000000000;
	d->end_lba = -2000000000;

	/* ts A61201 */
	d->erasable = 0;
	d->current_profile = -1;
	d->current_profile_text[0] = 0;
	d->current_is_cd_profile = 0;
	d->current_is_supported_profile = 0;
	d->bg_format_status = -1;

	return 1;
}

