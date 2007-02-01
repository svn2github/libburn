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


/* ts A61219 : Based on knowlege from dvd+rw-tools-7.0 and mmc5r03c.pdf */
#define Libburn_support_dvd_plus_rW 1

/* ts A61229 */
#define Libburn_support_dvd_minusrw_overW 1

/* ts A70112 */
#define Libburn_support_dvd_raM 1


/* ts A70129 >>> EXPERIMENTAL UNTESTED
*/
#define Libburn_support_dvd_r_seQ 1


/* Progress report:
   ts A61219 : It seems to work with a used (i.e. thoroughly formatted) DVD+RW.
               Error messages of class DEBUG appear because of inability to
               read TOC or track info. Nevertheless, the written images verify.
   ts A61220 : Burned to a virgin DVD+RW by help of new mmc_format_unit()
               (did not test wether it would work without). Burned to a
               not completely formatted DVD+RW. (Had worked before without
               mmc_format_unit() but i did not exceed the formatted range
               as reported by dvd+rw-mediainfo.) 
   ts A61221 : Speed setting now works for both of my drives. The according
               functions in dvd+rw-tools are a bit intimidating to the reader.
               I hope it is possible to leave much of this to the drive. 
               And if it fails ... well, it's only speed setting. :))
   ts A61229 : Burned to several DVD-RW formatted to mode Restricted Overwrite
               by dvd+rw-format. Needs Libburn_support_dvd_minusrw_overW.
   ts A61230 : Other than growisofs, libburn does not send a mode page 5 for
               such DVD-RW (which the MMC-5 standard does deprecate) and it
               really seems to work without such a page.
   ts A70101 : Formatted DVD-RW media. Success is varying with media, but
               dvd+rw-format does not do better with the same media.
   ts A70112 : Support for writing to DVD-RAM.
   ts A70130 : Burned a first non-multi sequential DVD-RW. Feature 0021h
               Incremental Recording vanishes after that and media thus gets
               not recognized as suitable any more.
               After a run with -multi another disc still offers 0021h .
               dvd+rw-mediainfo shows two tracks. The second, an afio archive
               is readable by afio. Third and forth veryfy too. Suddenly
               dvd+rw-mediainfo sees lba 0 with track 2. But #2 still verifies
               if one knows its address.

Todo:
   Determine first free lba for appending data. 
*/


static unsigned char MMC_GET_MSINFO[] =
	{ 0x43, 0, 1, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_GET_TOC[] = { 0x43, 2, 2, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_GET_ATIP[] = { 0x43, 2, 4, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_GET_DISC_INFO[] =
	{ 0x51, 0, 0, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_READ_CD[] = { 0xBE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char MMC_BLANK[] = { 0xA1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
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
static unsigned char MMC_READ_BUFFER_CAPACITY[] =
	{ 0x5C, 0, 0, 0, 0, 0, 0, 16, 0, 0 };

/* ts A61219 : format DVD+RW (and various others) */
static unsigned char MMC_FORMAT_UNIT[] = { 0x04, 0x11, 0, 0, 0, 0 };

/* ts A61221 :
   To set speed for DVD media (0xBB is for CD but works on my LG GSA drive) */
static unsigned char MMC_SET_STREAMING[] =
	{ 0xB6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* ts A61225 :
   To obtain write speed descriptors (command can do other things too) */
static unsigned char MMC_GET_PERFORMANCE[] =
	{ 0xAC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* ts A70108 : To obtain info about drive and media formatting opportunities */
static unsigned char MMC_READ_FORMAT_CAPACITIES[] =
	{ 0x23, 0, 0, 0, 0, 0, 0, 0, 0, 0};


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


/* ts A70201 */
int mmc_four_char_to_int(unsigned char *data)
{
	return (data[0] << 24) | (data[1] << 16) |
		(data[2] << 8) | data[3];
}


/* ts A70201 */
int mmc_int_to_four_char(unsigned char *data, int num)
{
	data[0] = (num >> 24) & 0xff;
	data[1] = (num >> 16) & 0xff;
	data[2] = (num >> 8) & 0xff;
	data[3] = num & 0xff;
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


/* ts A70201 :
   Common track info fetcher for mmc_get_nwa() and mmc_fake_toc()
*/
int mmc_read_track_info(struct burn_drive *d, int trackno, struct buffer *buf)
{
	struct command c;
	int i;

	mmc_function_spy("mmc_read_track_info");
	c.retry = 1;
	c.oplen = sizeof(MMC_TRACK_INFO);
	memcpy(c.opcode, MMC_TRACK_INFO, sizeof(MMC_TRACK_INFO));
	c.opcode[1] = 1;
	if(trackno<=0) {
		if (d->current_profile == 0x1a || d->current_profile == 0x13 ||
		    d->current_profile == 0x12 )
			 /* DVD+RW , DVD-RW restricted overwrite , DVD-RAM */
			trackno = 1;
		else if (d->current_profile == 0x11 ||
			 d->current_profile == 0x14) /* DVD-R[W] Sequential */
			trackno = d->last_track_no;
		else /* mmc5r03c.pdf: valid only for CD, DVD+R, DVD+R DL */
			trackno = 0xFF;
	}
	for (i = 0; i < 4; i++)
		c.opcode[2 + i] = (trackno >> (24 - 8 * i)) & 0xff;
	c.page = buf;
	memset(buf->data, 0, BUFFER_SIZE);
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);
	if (c.error)
		return 0;
	return 1;
}


/* ts A61110 : added parameters trackno, lba, nwa. Redefined return value. 
   @return 1=nwa is valid , 0=nwa is not valid , -1=error */
/* ts A70201 : outsourced 52h READ TRACK INFO command */
int mmc_get_nwa(struct burn_drive *d, int trackno, int *lba, int *nwa)
{
	struct buffer buf;
	int ret;

#ifdef Libburn_get_nwa_standalonE
	struct command c;
	int i;
#endif

	unsigned char *data;

	mmc_function_spy("mmc_get_nwa");
	if(trackno<=0) {
		if (d->current_profile == 0x1a || d->current_profile == 0x13 ||
		    d->current_profile == 0x12 )
			 /* DVD+RW , DVD-RW restricted overwrite , DVD-RAM */
			trackno = 1;
		else if (d->current_profile == 0x11 ||
			 d->current_profile == 0x14) /* DVD-R[W] Sequential */
			trackno = d->last_track_no;
		else /* mmc5r03c.pdf: valid only for CD, DVD+R, DVD+R DL */
			trackno = 0xFF;
	}

#ifdef Libburn_get_nwa_standalonE

	c.retry = 1;
	c.oplen = sizeof(MMC_TRACK_INFO);
	memcpy(c.opcode, MMC_TRACK_INFO, sizeof(MMC_TRACK_INFO));
	c.opcode[1] = 1;
	for (i = 0; i < 4; i++)
		c.opcode[2 + i] = (trackno >> (24 - 8 * i)) & 0xff;
	c.page = &buf;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);
	data = c.page->data;

#else /* Libburn_get_nwa_standalonE */

	ret = mmc_read_track_info(d, trackno, &buf);
	if (ret <= 0)
		return ret;
	data = buf.data;

#endif /* ! Libburn_get_nwa_standalonE */


	*lba = (data[8] << 24) + (data[9] << 16)
		+ (data[10] << 8) + data[11];
	*nwa = (data[12] << 24) + (data[13] << 16)
		+ (data[14] << 8) + data[15];
	if (d->current_profile == 0x1a || d->current_profile == 0x13) {
		 /* DVD+RW or DVD-RW restricted overwrite */
		*lba = *nwa = 0;
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


/* ts A70201 : Set up an entry for mmc_fake_toc() */
int mmc_fake_toc_entry(struct burn_toc_entry *entry, int session_number,
			 int track_number,
			 unsigned char *size_data, unsigned char *start_data)
{
	int min, sec, frames, num;

	/* mark DVD extensions as valid */
	entry->extensions_valid |= 1; 

	/* defaults are as of mmc5r03.pdf 6.26.3.2.4 Fabricated TOC */
	entry->session = session_number & 0xff;
	entry->session_msb = (session_number >> 8) & 0xff;
	entry->adr = 1;
	entry->control = 4;
	entry->tno = 0;
	entry->point = track_number & 0xff;
	entry->point_msb = (track_number >> 8) & 0xff;
	num = (size_data[0] << 24) | (size_data[1] << 16) |
		(size_data[2] << 8) | size_data[3];
	entry->track_blocks = num;
	burn_lba_to_msf(num, &min, &sec, &frames);
	if (min > 255) {
		min = 255;
		sec = 255;
		frames = 255;
	}
	entry->min = min;
	entry->sec = sec;
	entry->frame = frames;
	entry->zero = 0;
	num = (start_data[0] << 24) | (start_data[1] << 16) |
		(start_data[2] << 8) | start_data[3];
	entry->start_lba = num;
	burn_lba_to_msf(num, &min, &sec, &frames);
	if (min > 255) {
		min = 255;
		sec = 255;
		frames = 255;
	}
	entry->pmin = min;
	entry->psec = sec;
	entry->pframe = frames;
	return 1;
}


/* ts A70131 : compose a disc TOC structure from d->complete_sessions
               and 52h READ TRACK INFORMATION */
int mmc_fake_toc(struct burn_drive *d)
{
	struct burn_track *track;
	struct burn_session *session;
	struct burn_toc_entry *entry;
	struct buffer buf;
	int i, session_number, prev_session = -1, ret, lba;
	unsigned char *tdata, size_data[4], start_data[4];

	if (d->last_track_no <= 0 || d->complete_sessions <= 0 ||
	    d->status == BURN_DISC_BLANK)
		return 2;
	d->disc = burn_disc_create();
	if (d->disc == NULL)
		return -1;
	d->toc_entries = d->last_track_no + d->complete_sessions;
	d->toc_entry = malloc(d->toc_entries * sizeof(struct burn_toc_entry));
	if (d->toc_entry == NULL)
		return -1;
	memset(d->toc_entry, 0,d->toc_entries * sizeof(struct burn_toc_entry));
	for (i = 0; i < d->complete_sessions; i++) {
		session = burn_session_create();
		burn_disc_add_session(d->disc, session, BURN_POS_END);
		burn_session_free(session);
	}
	memset(size_data, 0, 4);
	memset(start_data, 0, 4);

	/* Entry Layout :
		session 1   track 1     entry 0
		...
		session 1   track N     entry N-1
		leadout 1               entry N
		session 2   track N+1   entry N+1
		...
		session 2   track M+1   entry M+1
		leadout 2               entry M+2
		session X   track K     entry (K-1)+(X-1)
		...
		session X   track i+1   entry i+(X-1)
		leadout X               entry i+X
	*/
	for (i = 0; i < d->last_track_no; i++) {
		ret = mmc_read_track_info(d, i+1, &buf);
		if (ret < 0)
			return ret;
		if (ret == 0)
	continue;
		tdata = buf.data;
		session_number = (tdata[33] << 8) | tdata[3];
		if (session_number <= 0)
	continue;

		if (session_number != prev_session && prev_session > 0) {
			entry = &(d->toc_entry[(i - 1) + prev_session]);
			lba = mmc_four_char_to_int(start_data) +
			      mmc_four_char_to_int(size_data);
			mmc_int_to_four_char(start_data, lba);
			mmc_int_to_four_char(size_data, 0);
			mmc_fake_toc_entry(entry, session_number, 0xA2,
					 size_data, start_data);
			entry->min= entry->sec= entry->frame= 0;
			d->disc->session[prev_session - 1]->leadout_entry =
									entry;
		}

		if (session_number > d->disc->sessions)
	continue;

		entry = &(d->toc_entry[i + session_number - 1]);
 		track = burn_track_create();
		if (track == NULL)
			return -1;
		burn_session_add_track(
			d->disc->session[session_number - 1],
			track, BURN_POS_END);
		track->entry = entry;
		burn_track_free(track);

		memcpy(size_data, tdata + 24, 4);
		memcpy(start_data, tdata + 8, 4);
		mmc_fake_toc_entry(entry, session_number, i + 1,
					 size_data, start_data);

		if (session_number < d->disc->sessions) {
			if (prev_session != session_number)
				d->disc->session[session_number - 1]->
							firsttrack = i+1;
			d->disc->session[session_number - 1]->lasttrack = i+1;
		}
		prev_session = session_number;
	}
	return 1;
}


void mmc_read_toc(struct burn_drive *d)
{
/* read full toc, all sessions, in m/s/f form, 4k buffer */
/* ts A70201 : or fake a toc from track information */
	struct burn_track *track;
	struct burn_session *session;
	struct buffer buf;
	struct command c;
	int dlen;
	int i, bpl= 12;
	unsigned char *tdata;

	mmc_function_spy("mmc_read_toc");
	if (!(d->current_profile == -1 || d->current_is_cd_profile)) {
		/* ts A70131 : MMC_GET_TOC uses Response Format 2 
		   For DVD this fails with 5,24,00 */
		/* One could try Response Format 0: mmc5r03.pdf 6.26.3.2
		   which does not yield the same result wit the same disc
		   on different drives.
		*/
		/* ts A70201 :
		   This uses the session count from 51h READ DISC INFORMATION
		   and the track records from 52h READ TRACK INFORMATION
		*/
		mmc_fake_toc(d);

		if (d->status == BURN_DISC_UNREADY)
			d->status = BURN_DISC_FULL;
		return;
	}
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
			 "Could not inquire TOC", 0,0);
		d->status = BURN_DISC_UNSUITABLE;
		d->toc_entries = 0;
		/* Prefering memory leaks over fandangos */
		d->toc_entry = malloc(sizeof(struct burn_toc_entry));
		memset(&(d->toc_entry[0]), 0, sizeof(struct burn_toc_entry));

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
	for (i = 0; i < d->toc_entries; i++)
		memset(&(d->toc_entry[i]), 0, sizeof(struct burn_toc_entry));
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
				burn_msf_to_lba(tdata[8], tdata[9], tdata[10]));
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

	/* ts A70131 : was (d->status != BURN_DISC_BLANK) */
	if (d->status == BURN_DISC_UNREADY)
		d->status = BURN_DISC_FULL;
	toc_find_modes(d);
}


/* ts A70131 : If no TOC is at hand, this tries to get the start of the
		last complete session (mksifs -c first parameter) */
int mmc_read_multi_session_c1(struct burn_drive *d, int *trackno, int *start)
{
	struct buffer buf;
	struct command c;
	unsigned char *tdata;

	mmc_function_spy("mmc_read_multi_session_c");

	/* mmc5r03.pdf 6.26.3.3.3 states that with non-CD this would
	   be a useless fake always starting at track 1, lba 0.
	   My drives return useful data, though.
           MMC-3 states that DVD had not tracks. So maybe this fake is
	   a legacy ?
	*/

	/* >>>
	   mmc_fake_toc() meanwhile tries to establish a useable TOC.
	   Evaluate this first before issueing a MMC command.
	*/

	memcpy(c.opcode, MMC_GET_MSINFO, sizeof(MMC_GET_MSINFO));
	c.retry = 1;
	c.oplen = sizeof(MMC_GET_MSINFO);
	c.page = &buf;
	c.page->bytes = 0;
	c.page->sectors = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	if (c.error)
		return 0;

	tdata = c.page->data + 4;
	*trackno = tdata[2];
	*start = (tdata[4] << 24) | (tdata[5] << 16)
		| (tdata[6] << 8) | tdata[7];
	return 1;
}


void mmc_read_disc_info(struct burn_drive *d)
{
	struct buffer buf;
	unsigned char *data;
	struct command c;
	char msg[160];
	/* ts A70131 : had to move mmc_read_toc() to end of function */
	int do_read_toc = 0, session_state;

	/* ts A61020 */
	d->start_lba = d->end_lba = -2000000000;
	d->erasable = 0;
	d->last_track_no = 1;

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
		if ((data[2] & 3) == 2)
			d->status = BURN_DISC_FULL;
		do_read_toc = 1;
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

	if (d->status == BURN_DISC_BLANK) {
                d->last_track_no = 1; /* The "incomplete track" */
		d->complete_sessions = 0;
	} else {
		/* ts A70131 : number of non-empty sessions */
		d->complete_sessions = (data[9] << 8) | data[4];
		session_state = (data[2] >> 2) & 3;
		/* mmc5r03c.pdf 6.22.3.1.3 State of Last Session: 3=complete */
		if (session_state != 3 && d->complete_sessions >= 1)
			d->complete_sessions--;

		/* ts A70129 : mmc5r03c.pdf 6.22.3.1.7
		   This includes the "incomplete track" if the disk is
		   appendable. I.e number of complete tracks + 1. */
		d->last_track_no = (data[11] << 8) | data[6];
	}

	/* Preliminarily declare blank:
	   ts A61219 : DVD+RW (is not bg_format_status==0 "blank")
	   ts A61229 : same for DVD-RW Restricted overwrite
	   ts A70112 : same for DVD-RAM
	*/
	if (d->current_profile == 0x1a || d->current_profile == 0x13 ||
	    d->current_profile == 0x12)
		d->status = BURN_DISC_BLANK;

	if (do_read_toc)
		mmc_read_toc(d);
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
	                          /*  0,   2,   4,    6,   10,  -,   16,  -, */
        static int speed_value[16]= { 0, 353, 706, 1059, 1764, -5, 2824, -7,
	                   4234, 5646, 7056, 8468, -12, -13, -14, -15};
	               /*    24,   32,   40,   48,   -,   -,   -,   - */

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
		if (speed_value[(data[16]>>4)&7] > 0) {
			d->mdata->min_write_speed = 
				speed_value[(data[16]>>4)&7];
			if (speed_value[(data[16])&15] <= 0)
				d->mdata->max_write_speed = 
					speed_value[(data[16]>>4)&7];
		}
		if (speed_value[(data[16])&15] > 0) {
			d->mdata->max_write_speed = 
				speed_value[(data[16])&15];
			if (speed_value[(data[16]>>4)&7] <= 0)
				d->mdata->min_write_speed = 
					speed_value[(data[16])&15];
		}
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
	memcpy(c.opcode, MMC_BLANK, sizeof(MMC_BLANK));
	c.opcode[1] = 16;	/* IMMED set to 1 */
	c.opcode[1] |= !!fast;
	c.retry = 1;
	c.oplen = sizeof(MMC_BLANK);
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


/* ts A61221 : Learned much from dvd+rw-tools-7.0 set_speed_B6h() but then
   made own experiments on base of mmc5r03c.pdf 6.8.3 and 6.39 in the hope
   to achieve a leaner solution */
int mmc_set_streaming(struct burn_drive *d, int r_speed, int w_speed)
{
	struct buffer buf;
	struct command c;
	int b, end_lba;
	char msg[160];
	unsigned char *pd;

	mmc_function_spy("mmc_set_streaming");

	if (r_speed <= 0)
		r_speed = 0x10000000; /* ~ 2 TB/s */
	if (w_speed <= 0)
		w_speed = 0x10000000; /* ~ 2 TB/s */
	c.retry = 1;
	c.oplen = sizeof(MMC_SET_STREAMING);
	memcpy(c.opcode, MMC_SET_STREAMING, sizeof(MMC_SET_STREAMING));
	c.page = &buf;
	c.page->bytes = 28;
	c.opcode[9] = (c.page->bytes >> 8) & 0xff;
	c.opcode[10] = c.page->bytes & 0xff;
	c.page->sectors = 0;
	c.dir = TO_DRIVE;
	memset(c.page->data, 0, c.page->bytes);
	pd = c.page->data;

	/* Trying to avoid inquiry of available speed descriptors but rather
	   to allow the drive to use the liberties of Exact==0.
	*/
	pd[0] = 0; /* WRC=0 (Default Rotation Control), RDD=Exact=RA=0 */

	/* Default computed from 4.7e9 */
	end_lba = 2294921 - 1;
	if (d->mdata->max_end_lba > 0)
		end_lba = d->mdata->max_end_lba - 1;

	sprintf(msg, "mmc_set_streaming: end_lba=%d ,  r=%d ,  w=%d",
		end_lba, r_speed, w_speed);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   msg, 0, 0);

	/* start_lba is 0 , 1000 = 1 second as base time for data rate */
	for (b = 0; b < 4 ; b++) {
		pd[8+b] = (end_lba >> (24 - 8 * b)) & 0xff;
		pd[12+b] = (r_speed >> (24 - 8 * b)) & 0xff;
		pd[16+b] = (1000 >> (24 - 8 * b)) & 0xff;
		pd[20+b] = (w_speed >> (24 - 8 * b)) & 0xff;
		pd[24+b] = (1000 >> (24 - 8 * b)) & 0xff;
	}

/* <<<
	fprintf(stderr,"LIBBURN_EXPERIMENTAL : B6h Performance descriptor:\n");
	for (b = 0; b < 28 ; b++)
		fprintf(stderr, "%2.2X%c", pd[b], ((b+1)%4 ? ' ' : '\n'));
*/

	
	d->issue_command(d, &c);
	if (c.error) {
		if (c.sense[2]!=0 && !d->silent_on_scsi_error) {
			sprintf(msg,
	"SCSI error on set_streaming(%d): key=%X asc=%2.2Xh ascq=%2.2Xh",
				w_speed,
				c.sense[2],c.sense[12],c.sense[13]);
				libdax_msgs_submit(libdax_messenger,
				d->global_index,
				0x00020124,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
		}
		return 0;
	}
	return 1;
}


void mmc_set_speed(struct burn_drive *d, int r, int w)
{
	struct command c;
	int ret;

	mmc_function_spy("mmc_set_speed");

	/* ts A61221 : try to set DVD speed via command B6h */
	if (strstr(d->current_profile_text, "DVD") == d->current_profile_text){
		ret = mmc_set_streaming(d, r, w);
		if (ret != 0)
			return; /* success or really fatal failure */ 
	}

	/* ts A61112 : MMC standards prescribe FFFFh as max speed.
			But libburn.h prescribes 0. */
	if (r<=0 || r>0xffff)
		r = 0xffff;
	if (w<=0 || w>0xffff)
		w = 0xffff;

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
		texts[0x14] = "DVD-RW sequential recording";
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
	int len, cp, descr_len = 0, feature_code, prf_number, only_current = 1;
	unsigned char *descr, *prf, *up_to, *prf_end;
	struct command c;

	d->current_profile = 0;
        d->current_profile_text[0] = 0;
	d->current_is_cd_profile = 0;
	d->current_is_supported_profile = 0;
	d->current_has_feat21h = 0;
	d->current_feat21h_link_size = -1;
	d->current_feat2fh_byte4 = -1;

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
		| (c.page->data[1] << 16)
		| (c.page->data[2] << 8)
		| c.page->data[3];

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
#ifdef Libburn_support_dvd_minusrw_overW
	if (cp == 0x13)
		d->current_is_supported_profile = 1;
#endif
#ifdef Libburn_support_dvd_raM
	if (cp == 0x12)
		d->current_is_supported_profile = 1;
#endif

/* Enable this to get loud and repeated reports about the feature set :
#define Libburn_print_feature_descriptorS 1
*/
	/* ts A70127 : Interpret list of profile and feature descriptors.
 	see mmc5r03c.pdf 5.2
	>>> Ouch: What to do if list is larger than buffer size.
	          Specs state that the call has to be repeated.
	*/
	up_to = c.page->data + (len < BUFFER_SIZE ? len : BUFFER_SIZE);

#ifdef Libburn_print_feature_descriptorS
	fprintf(stderr,
	"-----------------------------------------------------------------\n");
	fprintf(stderr,
	  "LIBBURN_EXPERIMENTAL : feature list length = %d , shown = %d\n",
		len, up_to - c.page->data);
#endif /* Libburn_print_feature_descriptorS */

	for (descr = c.page->data + 8; descr + 3 < up_to; descr += descr_len) {
		descr_len = 4 + descr[3];
		feature_code = (descr[0] << 8) | descr[1];
		if (only_current && !(descr[2] & 1))
	continue;

#ifdef Libburn_print_feature_descriptorS
		fprintf(stderr,
			"LIBBURN_EXPERIMENTAL : %s feature %4.4Xh\n",
			descr[2] & 1 ? "+" : "-",
			feature_code);
#endif /* Libburn_print_feature_descriptorS */

		if (feature_code == 0x0) {
			prf_end = descr + 4 + descr[3];
			for (prf = descr + 4; prf + 2 < prf_end; prf += 4) {
				if (only_current && !(prf[2] & 1))
			continue;
				prf_number =  (prf[0] << 8) | prf[1];

#ifdef Libburn_print_feature_descriptorS
				fprintf(stderr,
			"LIBBURN_EXPERIMENTAL :   %s profile %4.4Xh  \"%s\"\n",
					prf[2] & 1 ? "+" : "-",
					prf_number,
					mmc_obtain_profile_name(prf_number));
#endif /* Libburn_print_feature_descriptorS */

			}

		} else if (feature_code == 0x21) {
			int i;

			d->current_has_feat21h = (descr[2] & 1);
			for (i = 0; i < descr[7]; i++) {
				if (i == 0 || descr[8 + i] == 16)
					d->current_feat21h_link_size = 
								descr[8 + i];

#ifdef Libburn_print_feature_descriptorS
				fprintf(stderr,
				"LIBBURN_EXPERIMENTAL :   + Link Size = %d\n",
					descr[8 + i]);
#endif /* Libburn_print_feature_descriptorS */

			}

		} else if (feature_code == 0x2F) {
			if (descr[2] & 1)
				d->current_feat2fh_byte4 = descr[4];

#ifdef Libburn_print_feature_descriptorS
			fprintf(stderr, "LIBBURN_EXPERIMENTAL :     BUF = %d , Test Write = %d , DVD-RW = %d\n",
				!!(descr[4] & 64), !!(descr[4] & 4),
				!!(descr[4] & 2));
#endif /* Libburn_print_feature_descriptorS */
			
#ifdef Libburn_print_feature_descriptorS
		} else if (feature_code == 0x01) {
			int pys_if_std = 0;
			char *phys_name = "";

			pys_if_std = (descr[4] << 24) | (descr[5] << 16) |
					(descr[6] << 8) | descr[9];
			if (pys_if_std == 1)
				phys_name = "SCSI Family";
			else if(pys_if_std == 2)
				phys_name = "ATAPI";
			else if(pys_if_std == 3 || pys_if_std == 4 ||
				 pys_if_std == 6)
				phys_name = "IEEE 1394 FireWire";
			else if(pys_if_std == 7)
				phys_name = "Serial ATAPI";
			else if(pys_if_std == 7)
				phys_name = "USB";
			
			fprintf(stderr,
	"LIBBURN_EXPERIMENTAL :     Phys. Interface Standard %Xh \"%s\"\n",
				pys_if_std, phys_name);

		} else if (feature_code == 0x107) {

			fprintf(stderr, "LIBBURN_EXPERIMENTAL :     CD SPEED = %d , page 2Ah = %d , SET STREAMING = %d\n",
				!!(descr[4] & 8), !!(descr[4] & 4),
				!!(descr[4] & 2));

		} else if (feature_code == 0x108 || feature_code == 0x10c) {
			int i, c_limit;

			fprintf(stderr, "LIBBURN_EXPERIMENTAL :     %s = ", 
				feature_code == 0x108 ? 
				"Drive Serial Number" : "Drive Firmware Date");
			c_limit = descr[3] - 2 * (feature_code == 0x10c);
			for (i = 0; i < c_limit; i++)
				if (descr[4 + i] < 0x20 || descr[4 + i] > 0x7e
					|| descr[4 + i] == '\\')
					fprintf(stderr,"\\%2.2X",descr[4 + i]);
				else
					fprintf(stderr, "%c", descr[4 + i]);
			fprintf(stderr, "\n");

#endif /* Libburn_print_feature_descriptorS */

		}
	}
#ifdef Libburn_support_dvd_r_seQ
	if ((cp == 0x11 || cp == 0x14) && d->current_has_feat21h)
		d->current_is_supported_profile = 1;
#endif
}


/* ts A70108 */
/* mmc5r03c.pdf 6.24 */
int mmc_read_format_capacities(struct burn_drive *d, int top_wanted)
{
	struct buffer buf;
	int len, type, score, num_descr, max_score = -2000000000, i, sign = 1;
	off_t size;
	struct command c;
	unsigned char *dpt;
	char msg[160];

	mmc_function_spy("mmc_read_format_capacities");

	d->format_descr_type = 3;
	d->format_curr_max_size = 0;
	d->format_curr_blsas = 0;
	d->best_format_type = -1;
	d->best_format_size = 0;

	memcpy(c.opcode, MMC_READ_FORMAT_CAPACITIES,
		 sizeof(MMC_GET_CONFIGURATION));
	c.retry = 1;
	c.oplen = sizeof(MMC_READ_FORMAT_CAPACITIES);
	c.opcode[7]= 0x02;
	c.opcode[8]= 0x00; /* accept 512 bytes (not more than 260 possible) */
	c.page = &buf;
	c.page->sectors = 0;
	c.page->bytes = 0;
	c.dir = FROM_DRIVE;

	d->issue_command(d, &c);
	if (c.error)
		return 0;

	len = c.page->data[3];
	if (len<8)
		return 0;

	dpt = c.page->data + 4;
	/* decode 6.24.3.2 Current/Maximum Capacity Descriptor */
	d->format_descr_type = dpt[4] & 3;
	d->format_curr_max_size = (((off_t) dpt[0]) << 24)
		 		  + (dpt[1] << 16) + (dpt[2] << 8) + dpt[3];
	d->format_curr_blsas = (dpt[5] << 16) + (dpt[6] << 8) + dpt[7];

	sprintf(msg,
		"Current/Maximum Capacity Descriptor : type = %d : %.f",
		d->format_descr_type, (double) d->format_curr_max_size);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   msg, 0, 0);

	d->format_curr_max_size *= (off_t) 2048;

	if (top_wanted == 0x00 || top_wanted == 0x10)
		sign = -1; /* the caller clearly desires full format */

	/* 6.24.3.3 Formattable Capacity Descriptors */
	num_descr = (len - 8) / 8;
	for (i = 0; i < num_descr; i++) {
		dpt = c.page->data + 12 + 8 * i;
		size = (((off_t) dpt[0]) << 24)
			+ (dpt[1] << 16) + (dpt[2] << 8) + dpt[3];
                size *= (off_t) 2048;
		type = dpt[4] >> 2;

		if (i < 32) {
			d->format_descriptors[i].type = type;
			d->format_descriptors[i].size = size;
			d->format_descriptors[i].tdp =
				(dpt[5] << 16) + (dpt[6] << 8) + dpt[7];
			d->num_format_descr = i + 1;
		}
		
		sprintf(msg, "Capacity Descriptor %2.2Xh  %.fs = %.1f MB",type,
			((double) size)/2048.0, ((double) size)/1024.0/1024.0);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			 0x00000002,
			 LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			 msg, 0, 0);

		/* Criterion is proximity to quick intermediate state */
		if (type == 0x00) { /* full format (with lead out) */
			score = 1 * sign;
		} else if (type == 0x10) { /* DVD-RW full format */
			score = 10 * sign;
		} else if(type == 0x13) { /* DVD-RW quick grow last session */
			score = 100 * sign;
		} else if(type == 0x15) { /* DVD-RW Quick */
			score = 50 * sign;
		} else {
	continue;
		}
		if (type == top_wanted)
			score += 1000000000;
		if (score > max_score) {
			d->best_format_type = type;
			d->best_format_size = size;
			max_score = score;
		}
	}

	sprintf(msg,
		"best_format_type = %2.2Xh , best_format_size = %.f",
		d->best_format_type, (double) d->best_format_size);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   msg, 0, 0);

	return 1;
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


/* ts A61219 : learned much from dvd+rw-tools-7.0: plus_rw_format()
               and mmc5r03c.pdf, 6.5 FORMAT UNIT */
/*
   @param size The size (in bytes) to be sent with the FORMAT comand
   @param flag bit1= insist in size 0 even if there is a better default known
               bit2= format to maximum available size
               bit3= expand format up to at least size
               bit4= enforce re-format of (partly) formatted media
               bit7= bit8 to bit15 contain the index of the format to use
               bit8-bit15 = see bit7
*/
int mmc_format_unit(struct burn_drive *d, off_t size, int flag)
{
	struct buffer buf;
	struct command c;
	int ret, tolerate_failure = 0, return_immediately = 0, i, format_type;
	int index;
	off_t num_of_blocks = 0, diff;
	char msg[160],descr[80];
	int full_format_type = 0x00; /* Full Format (or 0x10 for DVD-RW ?) */

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
	c.page->data[1] = 0x02;                  /* Immed */
	c.page->data[3] = 8;                     /* Format descriptor length */
	num_of_blocks = size / 2048;
	for (i = 0; i < 4; i++)
		c.page->data[4 + i] = (num_of_blocks >> (24 - 8 * i)) & 0xff;
	if (flag & 128) { /* explicitely chosen format descriptor */
		/* use case: the app knows what to do */

		ret = mmc_read_format_capacities(d, -1);
		if (ret <= 0)
			goto selected_not_suitable;
		index = (flag >> 8) & 0xff;
		if(index < 0 || index > d->num_format_descr) {
selected_not_suitable:;
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020132,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				"Selected format is not suitable for libburn",
				0, 0);
			return 0;
		}
		if (!(d->current_profile == 0x13 ||
			d->current_profile == 0x14 ||
			d->current_profile == 0x1a))
			goto unsuitable_media;
		      
		format_type = d->format_descriptors[index].type;
		if (!(format_type == 0x00 || format_type == 0x10 ||
		      format_type == 0x11 || format_type == 0x13 ||
		      format_type == 0x15 || format_type == 0x26))
			goto selected_not_suitable;
		if (flag & 4) {
			num_of_blocks =
				d->format_descriptors[index].size / 2048;
			for (i = 0; i < 4; i++)
				c.page->data[4 + i] =
					(num_of_blocks >> (24 - 8 * i)) & 0xff;
		}
		if (format_type != 0x26)
			for (i = 0; i < 3; i++)
				 c.page->data[9 + i] =
					( d->format_descriptors[index].tdp >>
					  (16 - 8 * i)) & 0xff;
		sprintf(descr, "%s (bit7)", d->current_profile_text);
		return_immediately = 1; /* caller must do the waiting */

	} else if (d->current_profile == 0x1a) { /* DVD+RW */
		/* use case: background formatting during write     !(flag&4)
	                     de-icing as explicit formatting action (flag&4)
		*/

		/* mmc5r03c.pdf , 6.5.4.2.14, DVD+RW Basic Format */
		format_type = 0x26;

		if ((size <= 0 && !(flag & 2)) || (flag & (4 | 8))) {
			/* maximum capacity */
			memset(c.page->data + 4, 0xff, 4); 
			num_of_blocks = 0xffffffff;
		}

		if(d->bg_format_status == 2 ||
			(d->bg_format_status == 3 && !(flag & 16))) {
			sprintf(msg,"FORMAT UNIT ignored. Already %s.",
				(d->bg_format_status == 2 ? "in progress" :
							"completed"));
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020120,
				LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0,0);
			return 2;
		}
		if (!(flag & 16))             /* if not re-format is desired */
			if (d->bg_format_status == 1) /* is partly formatted */
				c.page->data[11] = 1;         /* Restart bit */
		sprintf(descr, "DVD+RW (fs=%d,rs=%d)",
			d->bg_format_status, (c.page->data[11] == 1));
		if (flag & 4)
			return_immediately = 1;/* caller must do the waiting */

	} else if (d->current_profile == 0x13 && !(flag & 16)) {
		/*DVD-RW restricted overwrite*/
		/* use case: quick grow formatting during write */

		ret = mmc_read_format_capacities(d, 0x13);
		if (ret > 0) {
			if (d->best_format_type == 0x13) {
				if (d->best_format_size <= 0)
					return 1;
			} else {
				if (d->format_descr_type == 2) /* formatted */
					return 1;
				if (d->format_descr_type == 3){/*intermediate*/
					d->needs_close_session = 1;
					return 1;
				}
				/* does trying make sense at all ? */
				tolerate_failure = 1;
			}
		}
		if (d->best_format_type == 0x13 && (flag & (4 | 8))) {
			num_of_blocks = d->best_format_size / 2048;
			if (flag & 8) {
				/* num_of_blocks needed to reach size */
				diff = (size - d->format_curr_max_size) /32768;
				if ((size - d->format_curr_max_size) % 32768)
					diff++;
				diff *= 16;
				if (diff < num_of_blocks)
					num_of_blocks = diff;
			}
			if (num_of_blocks > 0)
				for (i = 0; i < 4; i++)
					c.page->data[4 + i] =
					(num_of_blocks >> (24 - 8 * i)) & 0xff;
		}
		/* 6.5.4.2.8 , DVD-RW Quick Grow Last Border */
		format_type = 0x13;
		c.page->data[11] = 16;              /* block size * 2k */
		sprintf(descr, "DVD-RW quick grow");

	} else if (d->current_profile == 0x14 ||
			(d->current_profile == 0x13 && (flag & 16))) {
		/* DVD-RW sequential recording (or Overwrite for re-format) */
		/* use case : transition from Sequential to Overwrite
	                      re-formatting of Overwrite media  */

		/* To Restricted Overwrite */
		/*    6.5.4.2.10 Format Type = 15h (DVD-RW Quick) */
		/* or 6.5.4.2.1  Format Type = 00h (Full Format) */
		/* or 6.5.4.2.5  Format Type = 10h (DVD-RW Full Format) */
		mmc_read_format_capacities(d,
					(flag & 4) ? full_format_type : 0x15);
		if (d->best_format_type == 0x15 ||
		    d->best_format_type == full_format_type) {
			if ((flag & 4)
				|| d->best_format_type == full_format_type) {
				num_of_blocks = d->best_format_size / 2048;
				for (i = 0; i < 4; i++)
					c.page->data[4 + i] =
					(num_of_blocks >> (24 - 8 * i)) & 0xff;
			}

		} else {
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020131,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				"No suitable formatting type offered by drive",
				0, 0);
			return 0;
		}
		format_type = d->best_format_type;
		c.page->data[11] = 16;              /* block size * 2k */
		sprintf(descr, "DVD-RW %s",
			format_type == 0x15 ? "quick" : "full");
		return_immediately = 1; /* caller must do the waiting */

	} else { 

	/* >>> other formattable types to come */
unsuitable_media:;
		sprintf(msg, "Unsuitable media detected. Profile %4.4Xh  %s",
			d->current_profile, d->current_profile_text);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x0002011e,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			msg, 0, 0);
		return 0;
	}
	c.page->data[8] = format_type << 2;

	sprintf(msg, "Format type %2.2Xh \"%s\", blocks = %.f\n",
		format_type, descr, (double) num_of_blocks);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00000002,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			msg, 0, 0);

	d->issue_command(d, &c);
	if (c.error && !tolerate_failure) {
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
	} else if ((!c.error) && (format_type == 0x13 || format_type == 0x15))
		d->needs_close_session = 1;
	if (return_immediately)
		return 1;
	usleep(1000000); /* there seems to be a little race condition */
	for (ret = 0; ret <= 0 ;) {
		usleep(50000);
		ret = spc_test_unit_ready(d);
	}
	mmc_sync_cache(d);
	return 1;
}


/* ts A61225 */
int mmc_get_write_performance(struct burn_drive *d)
{
	struct buffer buf;
	int len, i, b, max_descr, num_descr, ret;
	int exact_bit, read_speed, write_speed;
	/* if this call delivers usable data then they should override
	   previously recorded min/max speed and not compete with them */
	int min_write_speed = 0x7fffffff, max_write_speed = 0;
	int min_read_speed = 0x7fffffff, max_read_speed = 0;
	struct command c;
	unsigned long end_lba;
	unsigned char *pd;
	struct burn_speed_descriptor *sd;

	/* A61225 : 1 = report about speed descriptors */
	static int speed_debug = 0;

	mmc_function_spy("mmc_get_write_performance");

	if (d->current_profile <= 0)
		mmc_get_configuration(d);

	memcpy(c.opcode, MMC_GET_PERFORMANCE, sizeof(MMC_GET_PERFORMANCE));
	max_descr = ( BUFFER_SIZE - 8 ) / 16 - 1;

	/* >>> future: maintain a list of write descriptors 
	if (max_descr > d->max_write_descr - d->num_write_descr)
		max_descr = d->max_write_descr;
	*/

	c.opcode[8] = ( max_descr >> 8 ) & 0xff;
	c.opcode[9] = ( max_descr >> 0 ) & 0xff;
	c.opcode[10] = 3;
	c.retry = 1;
	c.oplen = sizeof(MMC_GET_PERFORMANCE);
	c.page = &buf;
	c.page->sectors = 0;
	c.page->bytes = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);
	if (c.error)
		return 0;
	len = (c.page->data[0] << 24)
		+ (c.page->data[1] << 16)
		+ (c.page->data[2] << 8)
		+ c.page->data[3];
	if (len<12)
		return 0;

	pd = c.page->data;
	num_descr = ( len - 4 ) / 16;
	if (num_descr > max_descr)
		num_descr = max_descr;
	for (i = 0; i < num_descr; i++) {
		exact_bit = !!(pd[8 + i*16] & 2);
		end_lba = read_speed = write_speed = 0;
		for (b = 0; b < 4 ; b++) {
			end_lba     += pd[8 + i*16 +  4 + b] << (24 - 8 * b);
			read_speed  += pd[8 + i*16 +  8 + b] << (24 - 8 * b);
			write_speed += pd[8 + i*16 + 12 + b] << (24 - 8 * b);
		}
		if (end_lba > 0x7ffffffe)
			end_lba = 0x7ffffffe;

		if (speed_debug)
			fprintf(stderr,
		"LIBBURN_DEBUG: kB/s: write=%d  read=%d  end=%lu  exact=%d\n",
				write_speed, read_speed, end_lba, exact_bit);

		/* ts A61226 */
		ret = burn_speed_descriptor_new(&(d->mdata->speed_descriptors),
				 NULL, d->mdata->speed_descriptors, 0);
		if (ret > 0) {
			sd = d->mdata->speed_descriptors;
			sd->source = 2;
			if (d->current_profile > 0) {
				sd->profile_loaded = d->current_profile;
				strcpy(sd->profile_name,
					d->current_profile_text);
			}
			sd->wrc = (pd[8 + i*16] >> 3 ) & 3;
			sd->exact = exact_bit;
			sd->mrw = pd[8 + i*16] & 1;
			sd->end_lba = end_lba;
			sd->write_speed = write_speed;
			sd->read_speed = read_speed;
		}

		if (end_lba > d->mdata->max_end_lba)
			d->mdata->max_end_lba = end_lba;
		if (end_lba < d->mdata->min_end_lba)
			d->mdata->min_end_lba = end_lba;
		if (write_speed < min_write_speed)
			min_write_speed = write_speed;
		if (write_speed > max_write_speed)
                        max_write_speed = write_speed;
		if (read_speed < min_read_speed)
			min_read_speed = read_speed;
		if (read_speed > max_read_speed)
                        max_read_speed = read_speed;
	}
	if (min_write_speed < 0x7fffffff)
		d->mdata->min_write_speed = min_write_speed;
	if (max_write_speed > 0)
		d->mdata->max_write_speed = max_write_speed;
	/* there is no mdata->min_read_speed yet 
	if (min_read_speed < 0x7fffffff)
		d->mdata->min_read_speed = min_read_speed;
	*/
	if (max_read_speed > 0)
		d->mdata->max_read_speed = max_read_speed;
	return num_descr;
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
	d->read_multi_session_c1 = mmc_read_multi_session_c1;
	d->close_disc = mmc_close_disc;
	d->close_session = mmc_close_session;
	d->close_track_session = mmc_close;
	d->read_buffer_capacity = mmc_read_buffer_capacity;
	d->format_unit = mmc_format_unit;
	d->read_format_capacities = mmc_read_format_capacities;

	/* ts A61020 */
	d->start_lba = -2000000000;
	d->end_lba = -2000000000;

	/* ts A61201 - A70128 */
	d->erasable = 0;
	d->current_profile = -1;
	d->current_profile_text[0] = 0;
	d->current_is_cd_profile = 0;
	d->current_is_supported_profile = 0;
	d->current_has_feat21h = 0;
	d->current_feat21h_link_size = -1;
	d->current_feat2fh_byte4 = -1;
	d->needs_close_session = 0;
	d->bg_format_status = -1;
	d->num_format_descr = 0;
	d->complete_sessions = 0;
	d->last_track_no = 1;

	return 1;
}


/* ts A61229 : outsourced from spc_select_write_params() */
/* Note: Page data is not zeroed here to allow preset defaults. Thus
           memset(pd, 0, 2 + d->mdata->write_page_length);
         is the eventual duty of the caller.
*/
int mmc_compose_mode_page_5(struct burn_drive *d,
				const struct burn_write_opts *o,
				unsigned char *pd)
{
	pd[0] = 5;
	pd[1] = d->mdata->write_page_length;

	/* ts A61229 */
	if (d->current_profile == 0x13) {     /* DVD-RW restricted overwrite */
		/* learned from transport.hxx : page05_setup()
		   and mmc3r10g.pdf table 347 */
 		/* BUFE (burnproof), no LS_V (i.e. default Link Size, i hope),
		   no simulate, write type 0 = packet */
		pd[2] = (1 << 6);
		/* no multi, fixed packet, track mode 5 */
		pd[3] = (1 << 5) | 5;
		/* Data Block Type */
		pd[4] = 8;
		/* Link size dummy */
		pd[5] = 0;

	} else if ((d->current_profile == 0x14 || d->current_profile == 0x11)
			&& d->current_has_feat21h == 1) { /* ts A70128 */
		/* learned from transport.hxx : page05_setup()
		   and mmc5r03c.pdf 7.5, 4.2.3.4 Table 17
		   and spc3r23.pdf 6.8, 7.4.3 */
		/* BUFE , LS_V = 1, Test Write, Write Type = 00h Incremental */
		pd[2] = ((!!o->underrun_proof) << 6)
			| (1 << 5)
			| ((!!o->simulate) << 4);
		/* Multi-session , FP = 1 , Track Mode = 5 */
		pd[3] = ((3 * !!o->multi) << 6) | (1 << 5) | 5;
		/* Data Block Type = 8 */
		pd[4] = 8;
		/* Link Size */
		if (d->current_feat21h_link_size >= 0)
			pd[5] = d->current_feat21h_link_size;
		else
			pd[5] = 16;
		if (d->current_feat21h_link_size != 16) {
			char msg[80];

			sprintf(msg,
				"Feature 21h Link Size = %d (expected 16)\n",
				d->current_feat21h_link_size);
			libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
				msg, 0, 0);
		}
		/* Packet Size */
		pd[13] = 16;

	} else {
		/* Traditional setup for CD */
		pd[2] = ((!!o->underrun_proof) << 6)
			| ((!!o->simulate) << 4)
			| (o->write_type & 0x0f);

		/* ts A61106 : MMC-1 table 110 : multi==0 or multi==3 */
		pd[3] = ((3 * !!o->multi) << 6) | (o->control & 0x0f);

		pd[4] = spc_block_type(o->block_type);

		/* ts A61104 */
		if(!(o->control&4)) /* audio (MMC-1 table 61) */
			if(o->write_type == BURN_WRITE_TAO)
				pd[4] = 0; /* Data Block Type: Raw Data */

		pd[14] = 0;     /* audio pause length MSB */
		pd[15] = 150;	/* audio pause length LSB */

/*XXX need session format! */
/* ts A61229 : but session format (pd[8]) = 0 seems ok */

	}
	return 1;
}

