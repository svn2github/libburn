/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/* ts A61009 */
/* #include <a ssert.h> */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
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


/* ts A70223 : in init.c */
extern int burn_support_untested_profiles;


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
/* ts A80410 : applies to BD-RE too */
#define Libburn_support_dvd_raM 1

/* ts A70129 */
#define Libburn_support_dvd_r_seQ 1

/* ts A70306 */
#define Libburn_support_dvd_plus_R 1

/* ts A70509 : handling 0x41 as read-only type */
#define Libburn_support_bd_r_readonlY 1

/* ts A81208 */
#define Libburn_support_bd_plus_r_srM 1


/* ts A80410 : <<< Dangerous experiment: Pretend that DVD-RAM is BD-RE
 # define Libburn_dvd_ram_as_bd_rE yes
*/
/* ts A80509 : <<< Experiment: pretend that DVD-ROM and CD-ROM are other media
                   like BD-ROM (0x40), BD-R seq (0x41), BD-R random (0x42)
 # define Libburn_rom_as_profilE 0x40
*/


/* ts A80425 : Prevents command FORMAT UNIT for DVD-RAM or BD-RE.
               Useful only to test the selection of format descriptors without
               actually formatting the media.
 # define Libburn_do_not_format_dvd_ram_or_bd_rE 1
*/


/* ts A90603 : Simulate the command restrictions of an old MMC-1 drive
 # define Libisofs_simulate_old_mmc1_drivE 1
*/


/* DVD/BD progress report:
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
   ts A70203 : DVD-RW need to get blanked fully. Then feature 0021h persists.
               Meanwhile Incremental streaming is supported like CD TAO:
               with unpredicted size, multi-track, multi-session.
   ts A70205 : Beginning to implement DVD-R[W] DAO : single track and session,
               size prediction mandatory.
   ts A70208 : Finally made tests with DVD-R. Worked exactly as new DVD-RW.
   ts A70306 : Implemented DVD+R (always -multi for now)
   ts A70330 : Allowed finalizing of DVD+R.
   ts A80228 : Made DVD+R/DL support official after nightmorph reported success
               in http://libburnia-project.org/ticket/13
   ts A80416 : drive->do_stream_recording brings DVD-RAM to full nominal
               writing speed at cost of no defect management.
   ts A80416 : Giulio Orsero reports success with BD-RE writing. With
               drive->do_stream_recording it does full nominal speed.
   ts A80506 : Giulio Orsero reports success with BD-RE formatting.
               BD-RE is now an officially supported profile.
   ts A81209 : The first two sessions have been written to BD-R SRM
               (auto formatted without Defect Management).
   ts A90107 : BD-R is now supported media type
*/

/* ts A70519 : With MMC commands of data direction FROM_DRIVE:
               Made struct command.dxfer_len equal to Allocation Length
               of MMC commands. Made sure that not more bytes are allowed
               for transfer than there are available. 
*/


/* ts A70711 Trying to keep writing from clogging the SCSI driver due to
             full buffer at burner drive: 0=waiting disabled, 1=enabled
             These are only defaults which can be overwritten by
             burn_drive_set_buffer_waiting()
*/
#define Libburn_wait_for_buffer_freE          0
#define Libburn_wait_for_buffer_min_useC        10000
#define Libburn_wait_for_buffer_max_useC       100000
#define Libburn_wait_for_buffer_tio_seC     120
#define Libburn_wait_for_buffer_min_perC     65
#define Libburn_wait_for_buffer_max_perC     95


static unsigned char MMC_GET_MSINFO[] =
	{ 0x43, 0, 1, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_GET_TOC[] = { 0x43, 2, 2, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char MMC_GET_TOC_FMT0[] = { 0x43, 0, 0, 0, 0, 0, 0, 16, 0, 0 };
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

/* ts A70205 : To describe the layout of a DVD-R[W] DAO session */
static unsigned char MMC_RESERVE_TRACK[] =
	{ 0x53, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* ts A70812 : Read data sectors (for types with 2048 bytes/sector only) */
static unsigned char MMC_READ_10[] =
	{ 0x28, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* ts A81210 : Determine the upper limit of readable data size */
static unsigned char MMC_READ_CAPACITY[] =
	{ 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static int mmc_function_spy_do_tell = 0;

int mmc_function_spy(struct burn_drive *d, char * text)
{
	if (mmc_function_spy_do_tell)
		fprintf(stderr,"libburn: experimental: mmc_function_spy: %s\n",
			text);
	if (d == NULL)
		return 1;
	if (d->drive_role != 1) {
		char msg[4096];
	
		sprintf(msg, "Emulated drive caught in SCSI adapter \"%s\"",
				text);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002014c,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
		d->cancel = 1;
		return 0;
	}
	if (d->is_stopped && strcmp(text, "stop_unit") != 0 &&
	    strcmp(text, "start_unit") != 0) {
		d->start_unit(d);
		d->is_stopped = 0;
	}
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
	return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
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


	if (mmc_function_spy(d, "mmc_send_cue_sheet") <= 0)
		return;

	scsi_init_command(&c, MMC_SEND_CUE_SHEET, sizeof(MMC_SEND_CUE_SHEET));
/*
	c.oplen = sizeof(MMC_SEND_CUE_SHEET);
	memcpy(c.opcode, MMC_SEND_CUE_SHEET, sizeof(MMC_SEND_CUE_SHEET));
*/
	c.retry = 1;
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


/* ts A70205 : Announce size of a DVD-R[W] DAO session.
   @param size The size in bytes to be announced to the drive.
               It will get rounded up to align to 32 KiB.
*/
int mmc_reserve_track(struct burn_drive *d, off_t size)
{
	struct command c;
	int lba;
	char msg[80];

	if (mmc_function_spy(d, "mmc_reserve_track") <= 0)
		return 0;

	scsi_init_command(&c, MMC_RESERVE_TRACK, sizeof(MMC_RESERVE_TRACK));
/*
	c.oplen = sizeof(MMC_RESERVE_TRACK);
	memcpy(c.opcode, MMC_RESERVE_TRACK, sizeof(MMC_RESERVE_TRACK));
*/
	c.retry = 1;
	/* Round to 32 KiB and divide by 2048
	   (by nice binary rounding trick learned from dvd+rw-tools) */
	lba = ((size + (off_t) 0x7fff) >> 11) & ~0xf;
	mmc_int_to_four_char(c.opcode+5, lba);

	sprintf(msg, "reserving track of %d blocks", lba);
	libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   msg, 0, 0);

	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
	return !c.error;
}


/* ts A70201 :
   Common track info fetcher for mmc_get_nwa() and mmc_fake_toc()
*/
int mmc_read_track_info(struct burn_drive *d, int trackno, struct buffer *buf,
			int alloc_len)
{
	struct command c;

	if (mmc_function_spy(d, "mmc_read_track_info") <= 0)
		return 0;

	scsi_init_command(&c, MMC_TRACK_INFO, sizeof(MMC_TRACK_INFO));
/*
	c.oplen = sizeof(MMC_TRACK_INFO);
	memcpy(c.opcode, MMC_TRACK_INFO, sizeof(MMC_TRACK_INFO));
*/
	c.dxfer_len = alloc_len;
	c.opcode[7] = (c.dxfer_len >> 8) & 0xff;
	c.opcode[8] = c.dxfer_len & 0xff;
	c.retry = 1;
	c.opcode[1] = 1;
	if(trackno<=0) {
		if (d->current_profile == 0x1a || d->current_profile == 0x13 ||
		    d->current_profile == 0x12 || d->current_profile == 0x42 ||
		    d->current_profile == 0x43)
			 /* DVD+RW , DVD-RW restricted overwrite , DVD-RAM
			    BD-R random recording, BD-RE */
			trackno = 1;
		else if (d->current_profile == 0x10 ||
			 d->current_profile == 0x11 ||
			 d->current_profile == 0x14 ||
			 d->current_profile == 0x15 ||
			 d->current_profile == 0x40 ||
			 d->current_profile == 0x41)
			/* DVD-ROM ,  DVD-R[W] Sequential ,
			   BD-ROM , BD-R sequential */
			trackno = d->last_track_no;
		else /* mmc5r03c.pdf: valid only for CD, DVD+R, DVD+R DL */
			trackno = 0xFF;
	}
	mmc_int_to_four_char(c.opcode + 2, trackno);
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
	int ret, num, alloc_len = 20;
	unsigned char *data;

	if (mmc_function_spy(d, "mmc_get_nwa") <= 0)
		return -1;

	ret = mmc_read_track_info(d, trackno, &buf, alloc_len);
	if (ret <= 0)
		return ret;
	data = buf.data;
	*lba = mmc_four_char_to_int(data + 8);
	*nwa = mmc_four_char_to_int(data + 12);
	num = mmc_four_char_to_int(data + 16);
	if (d->current_profile == 0x1a || d->current_profile == 0x13 ||
	    d->current_profile == 0x12 || d->current_profile == 0x43) {
		 /* overwriteable */
		*lba = *nwa = num = 0;
	} else if (!(data[7]&1)) {
		/* ts A61106 :  MMC-1 Table 142 : NWA_V = NWA Valid Flag */
		libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   "mmc_get_nwa: Track Info Block: NWA_V == 0", 0, 0);
		return 0;
	}
	if (num > 0) {
		burn_drive_set_media_capacity_remaining(d,
					((off_t) num) * ((off_t) 2048));
		d->media_lba_limit = *nwa + num;
	} else
		d->media_lba_limit = 0;

/*
	fprintf(stderr, "LIBBURN_DEBUG: media_lba_limit= %d\n",
		 d->media_lba_limit);
*/

	return 1;
}

/* ts A61009 : function is obviously unused. */
/* void mmc_close_disc(struct burn_drive *d, struct burn_write_opts *o) */
void mmc_close_disc(struct burn_write_opts *o)
{
	struct burn_drive *d = o->drive;

	if (mmc_function_spy(d, "mmc_close_disc") <= 0)
		return;

	libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   "HOW THAT ? mmc_close_disc() was called", 0, 0);

	/* ts A61009 : made impossible by removing redundant parameter d */
	/* a ssert(o->drive == d); */

	o->multi = 0;
	spc_select_write_params(d, o);
	mmc_close(d, 1, 0);
}

/* ts A61009 : function is obviously unused. */
/* void mmc_close_session(struct burn_drive *d, struct burn_write_opts *o) */
void mmc_close_session(struct burn_write_opts *o)
{
	struct burn_drive *d = o->drive;

	if (mmc_function_spy(d, "mmc_close_session") <= 0)
		return;

	libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   "HOW THAT ? mmc_close_session() was called", 0, 0);

	/* ts A61009 : made impossible by removing redundant parameter d */
	/* a ssert(o->drive == d); */

	o->multi = 3;
	spc_select_write_params(d, o);
	mmc_close(d, 1, 0);
}

/* ts A70227 : extended meaning of session to address all possible values
               of 5Bh CLOSE TRACK SESSION to address any Close Function.
               @param session contains the two high bits of Close Function
               @param track if not 0: sets the lowest bit of Close Function
*/
void mmc_close(struct burn_drive *d, int session, int track)
{
	struct command c;

	if (mmc_function_spy(d, "mmc_close") <= 0)
		return;

	scsi_init_command(&c, MMC_CLOSE, sizeof(MMC_CLOSE));
/*
	c.oplen = sizeof(MMC_CLOSE);
	memcpy(c.opcode, MMC_CLOSE, sizeof(MMC_CLOSE));
*/
	c.retry = 1;

	c.opcode[1] |= 1; /* ts A70918 : Immed */

	/* (ts A61030 : shifted !!session rather than or-ing plain session ) */
	c.opcode[2] = ((session & 3) << 1) | !!track;
	c.opcode[4] = track >> 8;
	c.opcode[5] = track & 0xFF;
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);

	/* ts A70918 : Immed : wait for drive to complete command */
	if (c.error) {
		d->cancel = 1;
		return;
	}
	if (spc_wait_unit_attention(d, 3600, "CLOSE TRACK SESSION", 0) <= 0)
		d->cancel = 1;
}

void mmc_get_event(struct burn_drive *d)
{
	struct buffer buf;
	struct command c;
	int alloc_len= 8;

	if (mmc_function_spy(d, "mmc_get_event") <= 0)
		return;

	scsi_init_command(&c, MMC_GET_EVENT, sizeof(MMC_GET_EVENT));
/*
	c.oplen = sizeof(MMC_GET_EVENT);
	memcpy(c.opcode, MMC_GET_EVENT, sizeof(MMC_GET_EVENT));
*/
	c.dxfer_len = alloc_len;
	c.opcode[7] = (c.dxfer_len >> 8) & 0xff;
	c.opcode[8] = c.dxfer_len & 0xff;
	c.retry = 1;
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


/* ts A70711
   This has become a little monster because of the creative buffer reports of
   my LG GSA-4082B : Belated, possibly statistically dampened. But only with
   DVD media. With CD it is ok.
*/
static int mmc_wait_for_buffer_free(struct burn_drive *d, struct buffer *buf)
{
	int usec= 0, need, reported_3s = 0, first_wait = 1;
	struct timeval t0,tnow;
	struct timezone dummy_tz;
	double max_fac, min_fac, waiting;

/* Enable to get reported waiting activities and total time.
#define Libburn_mmc_wfb_debuG 1
*/
#ifdef Libburn_mmc_wfb_debuG
	char sleeplist[32768];
	static int buffer_still_invalid = 1;
#endif

	max_fac = ((double) d->wfb_max_percent) / 100.0;

	/* Buffer info from the drive is valid only after writing has begun.
	   Caring for buffer space makes sense mostly after max_percent of the
	   buffer was transmitted. */
	if (d->progress.buffered_bytes <= 0 ||
		d->progress.buffer_capacity <= 0 ||
		d->progress.buffered_bytes + buf->bytes <=
 					d->progress.buffer_capacity * max_fac)
		return 2;

#ifdef Libburn_mmc_wfb_debuG
	if (buffer_still_invalid)
			fprintf(stderr,
			"\nLIBBURN_DEBUG: Buffer considered valid now\n");
	buffer_still_invalid = 0;
#endif

	/* The pessimistic counter does not assume any buffer consumption */
	if (d->pessimistic_buffer_free - buf->bytes >=
		( 1.0 - max_fac) * d->progress.buffer_capacity)
		return 1;

	/* There is need to inquire the buffer fill */
	d->pessimistic_writes++;
	min_fac = ((double) d->wfb_min_percent) / 100.0;
	gettimeofday(&t0, &dummy_tz);
#ifdef Libburn_mmc_wfb_debuG
	sleeplist[0]= 0;
	sprintf(sleeplist,"(%d%s %d)",
		(int) (d->pessimistic_buffer_free - buf->bytes),
		(d->pbf_altered ? "? -" : " -"),
		(int) ((1.0 - max_fac) * d->progress.buffer_capacity));
#endif

	while (1) {
		if ((!first_wait) || d->pbf_altered) {
			d->pbf_altered = 1;
			mmc_read_buffer_capacity(d);
		}
#ifdef Libburn_mmc_wfb_debuG
		if(strlen(sleeplist) < sizeof(sleeplist) - 80)
			sprintf(sleeplist+strlen(sleeplist)," (%d%s %d)",
			(int) (d->pessimistic_buffer_free - buf->bytes),
			(d->pbf_altered ? "? -" : " -"),
			(int) ((1.0 - min_fac) * d->progress.buffer_capacity));
#endif
		gettimeofday(&tnow,&dummy_tz);
		waiting = (tnow.tv_sec - t0.tv_sec) +
			  ((double) (tnow.tv_usec - t0.tv_usec)) / 1.0e6;
		if (d->pessimistic_buffer_free - buf->bytes >=
			(1.0 - min_fac) * d->progress.buffer_capacity) {
#ifdef Libburn_mmc_wfb_debuG
			if(strlen(sleeplist) >= sizeof(sleeplist) - 80)
				strcat(sleeplist," ...");
			sprintf(sleeplist+strlen(sleeplist)," -> %d [%.6f]",
				(int) (
				 d->pessimistic_buffer_free - buf->bytes -
				 (1.0 - min_fac) * d->progress.buffer_capacity
				), waiting);
			fprintf(stderr,
				"\nLIBBURN_DEBUG: sleeplist= %s\n",sleeplist);
#endif
			return 1;
		}

		/* Waiting is needed */
		if (waiting >= 3 && !reported_3s) {
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002013d,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_LOW,
			"Waiting for free buffer takes more than 3 seconds",
				0,0);
			reported_3s = 1;
		} else if (d->wfb_timeout_sec > 0 &&
				waiting > d->wfb_timeout_sec) {
			d->wait_for_buffer_free = 0;
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002013d,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Timeout with waiting for free buffer. Now disabled.",
				0,0);
	break;
		}

		need = (1.0 - min_fac) * d->progress.buffer_capacity +
			buf->bytes - d->pessimistic_buffer_free;
		usec = 0;
		if (d->nominal_write_speed > 0)
			usec = ((double) need) / 1000.0 /
				((double) d->nominal_write_speed) * 1.0e6;
		else
			usec = d->wfb_min_usec * 2;

		/* >>> learn about buffer progress and adjust usec */

		if (usec < d->wfb_min_usec)
			usec = d->wfb_min_usec;
		else if (usec > d->wfb_max_usec)
			usec = d->wfb_max_usec;
		usleep(usec);
		if (d->waited_usec < 0xf0000000)
			d->waited_usec += usec;
		d->waited_tries++;
		if(first_wait)
			d->waited_writes++;
#ifdef Libburn_mmc_wfb_debuG
		if(strlen(sleeplist) < sizeof(sleeplist) - 80)
			sprintf(sleeplist+strlen(sleeplist)," %d", usec);
#endif
		first_wait = 0;
	}
	return 0;
}


void mmc_write_12(struct burn_drive *d, int start, struct buffer *buf)
{
	struct command c;
	int len;

	if (mmc_function_spy(d, "mmc_write_12") <= 0)
		return;

	len = buf->sectors;

	/* ts A61009 */
	/* a ssert(buf->bytes >= buf->sectors);*/	/* can be == at 0... */

	burn_print(100, "trying to write %d at %d\n", len, start);

	scsi_init_command(&c, MMC_WRITE_12, sizeof(MMC_WRITE_12));
/*
	memcpy(c.opcode, MMC_WRITE_12, sizeof(MMC_WRITE_12));
	c.oplen = sizeof(MMC_WRITE_12);
*/
	c.retry = 1;
	mmc_int_to_four_char(c.opcode + 2, start);
	mmc_int_to_four_char(c.opcode + 6, len);
	c.page = buf;
	c.dir = TO_DRIVE;

	d->issue_command(d, &c);

	/* ts A70711 */
	d->pessimistic_buffer_free -= buf->bytes;
	d->pbf_altered = 1;
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

	if (mmc_function_spy(d, "mmc_write") <= 0)
		return BE_CANCELLED;

	cancelled = d->cancel;
	if (cancelled)
		return BE_CANCELLED;

	/* ts A70215 */
	if (d->media_lba_limit > 0 && start >= d->media_lba_limit) {
		char msg[160];

		sprintf(msg,
		"Exceeding range of permissible write addresses (%d >= %d)",
				start, d->media_lba_limit);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002012d,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
		d->cancel = 1; /* No need for mutexing because atomic */
		return BE_CANCELLED;
	}

	len = buf->sectors;

	/* ts A61009 : buffer fill problems are to be handled by caller */
	/* a ssert(buf->bytes >= buf->sectors);*/	/* can be == at 0... */

	burn_print(100, "trying to write %d at %d\n", len, start);

	/* ts A70711 */
	if(d->wait_for_buffer_free)
		mmc_wait_for_buffer_free(d, buf);

	/* ts A80412 */
	if(d->do_stream_recording > 0 && start >= d->stream_recording_start) {

		/* >>> ??? is WRITE12 available ?  */
			/* >>> ??? inquire feature 107h Stream Writing bit ? */

		scsi_init_command(&c, MMC_WRITE_12, sizeof(MMC_WRITE_12));
		mmc_int_to_four_char(c.opcode + 2, start);
		mmc_int_to_four_char(c.opcode + 6, len);
		c.opcode[10] = 1<<7; /* Streaming bit */
	} else {
		scsi_init_command(&c, MMC_WRITE_10, sizeof(MMC_WRITE_10));
		mmc_int_to_four_char(c.opcode + 2, start);
		c.opcode[6] = 0;
		c.opcode[7] = (len >> 8) & 0xFF;
		c.opcode[8] = len & 0xFF;
	}
	c.retry = 1;
	c.page = buf;
	c.dir = TO_DRIVE;

#ifdef Libburn_log_in_and_out_streaM
	/* <<< ts A61031 */
	if(tee_fd!=-1) {
		write(tee_fd,c.page->data,len*2048);
	}
#endif /* Libburn_log_in_and_out_streaM */

	d->issue_command(d, &c);

	/* ts A70711 */
	d->pessimistic_buffer_free -= buf->bytes;
	d->pbf_altered = 1;

	/* ts A61112 : react on eventual error condition */ 
	if (c.error && c.sense[2]!=0) {

		/* >>> make this scsi_notify_error() when liberated */
		if (c.sense[2]!=0) {

#ifdef NIX
			char msg[160];
			sprintf(msg,
		"SCSI error on write(%d,%d): key=%X asc=%2.2Xh ascq=%2.2Xh",
				start, len,
				c.sense[2],c.sense[12],c.sense[13]);
#else /* NIX */
			char msg[256];
			int key, asc, ascq;

			sprintf(msg, "SCSI error on write(%d,%d): ",
					start, len);
			scsi_error_msg(d, c.sense, 14, msg + strlen(msg), 
					&key, &asc, &ascq);

#endif /* !NIX */

			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002011d,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
		}
		d->cancel = 1;
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
	num = mmc_four_char_to_int(size_data);
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
	num = mmc_four_char_to_int(start_data);
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


/* ts A71128 : for DVD-ROM drives which offer no reliable track information */
static int mmc_read_toc_fmt0_al(struct burn_drive *d, int *alloc_len)
{
	struct burn_track *track;
	struct burn_session *session;
	struct burn_toc_entry *entry;
	struct buffer buf;
	struct command c;
	int dlen, i, old_alloc_len, session_number, prev_session = -1;
	int lba, size;
	unsigned char *tdata, size_data[4], start_data[4];

	if (*alloc_len < 4)
		return 0;

	scsi_init_command(&c, MMC_GET_TOC_FMT0, sizeof(MMC_GET_TOC_FMT0));
	c.dxfer_len = *alloc_len;
	c.opcode[7] = (c.dxfer_len >> 8) & 0xff;
	c.opcode[8] = c.dxfer_len & 0xff;
	c.retry = 1;
	c.page = &buf;
	c.page->bytes = 0;
	c.page->sectors = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	if (c.error) {
err_ex:;
		libdax_msgs_submit(libdax_messenger, d->global_index,
			 0x0002010d,
			 LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH,
			 "Could not inquire TOC", 0,0);
		d->status = BURN_DISC_UNSUITABLE;
		d->toc_entries = 0;
		/* Prefering memory leaks over fandangos */
		d->toc_entry = calloc(1, sizeof(struct burn_toc_entry));
		return 0;
	}
	dlen = c.page->data[0] * 256 + c.page->data[1];
	old_alloc_len = *alloc_len;
	*alloc_len = dlen + 2;
	if (old_alloc_len < 12)
		return 1;
	if (dlen + 2 > old_alloc_len)
		dlen = old_alloc_len - 2;
	d->complete_sessions = 1 + c.page->data[3] - c.page->data[2];
	d->last_track_no = d->complete_sessions;
	if (dlen - 2 < (d->last_track_no + 1) * 8) {
		libdax_msgs_submit(libdax_messenger, d->global_index,
			 0x00020159,
			 LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH,
			 "TOC Format 0 returns inconsistent data", 0,0);
		goto err_ex;
	}

	d->toc_entries = d->last_track_no + d->complete_sessions;
	if (d->toc_entries < 1)
		return 0;
	d->toc_entry = calloc(d->toc_entries, sizeof(struct burn_toc_entry));
	if(d->toc_entry == NULL)
		return 0;

	d->disc = burn_disc_create();
	if (d->disc == NULL)
		return 0;
	for (i = 0; i < d->complete_sessions; i++) {
		session = burn_session_create();
		if (session == NULL)
			return 0;
		burn_disc_add_session(d->disc, session, BURN_POS_END);
		burn_session_free(session);
	}


	for (i = 0; i < d->last_track_no; i++) {
		tdata = c.page->data + 4 + i * 8;
		session_number = i + 1;
		if (session_number != prev_session && prev_session > 0) {
			/* leadout entry previous session */
			entry = &(d->toc_entry[(i - 1) + prev_session]);
			lba = mmc_four_char_to_int(start_data) +
			      mmc_four_char_to_int(size_data);
			mmc_int_to_four_char(start_data, lba);
			mmc_int_to_four_char(size_data, 0);
			mmc_fake_toc_entry(entry, prev_session, 0xA2,
					 size_data, start_data);
			entry->min= entry->sec= entry->frame= 0;
			d->disc->session[prev_session - 1]->leadout_entry =
									entry;
		}

		/* ??? >>> d->media_capacity_remaining , d->media_lba_limit
				as of mmc_fake_toc()
		*/

		entry = &(d->toc_entry[i + session_number - 1]);
 		track = burn_track_create();
		if (track == NULL)
			return -1;
		burn_session_add_track(
			d->disc->session[session_number - 1],
			track, BURN_POS_END);
		track->entry = entry;
		burn_track_free(track);

		memcpy(start_data, tdata + 4, 4);
			/* size_data are estimated from next track start */
		memcpy(size_data, tdata + 8 + 4, 4);
		size = mmc_four_char_to_int(size_data) -
	      		mmc_four_char_to_int(start_data);
		mmc_int_to_four_char(size_data, size);
		mmc_fake_toc_entry(entry, session_number, i + 1,
					 size_data, start_data);
		if (prev_session != session_number)
			d->disc->session[session_number - 1]->firsttrack = i+1;
		d->disc->session[session_number - 1]->lasttrack = i+1;
		prev_session = session_number;
	}
	if (prev_session > 0 && prev_session <= d->disc->sessions) {
		/* leadout entry of last session of closed disc */
		tdata = c.page->data + 4 + d->last_track_no * 8;
		entry = &(d->toc_entry[(d->last_track_no - 1) + prev_session]);
		memcpy(start_data, tdata + 4, 4);
		mmc_int_to_four_char(size_data, 0);
		mmc_fake_toc_entry(entry, prev_session, 0xA2,
				 size_data, start_data);
		entry->min= entry->sec= entry->frame= 0;
		d->disc->session[prev_session - 1]->leadout_entry = entry;
	}
	return 1;
}


/* ts A71128 : for DVD-ROM drives which offer no reliable track information */
static int mmc_read_toc_fmt0(struct burn_drive *d)
{
	int alloc_len = 4, ret;

	if (mmc_function_spy(d, "mmc_read_toc_fmt0") <= 0)
		return -1;
	ret = mmc_read_toc_fmt0_al(d, &alloc_len);
	if (alloc_len >= 12)
		ret = mmc_read_toc_fmt0_al(d, &alloc_len);
	return ret;
}


/* ts A70131 : compose a disc TOC structure from d->complete_sessions
               and 52h READ TRACK INFORMATION */
int mmc_fake_toc(struct burn_drive *d)
{
	struct burn_track *track;
	struct burn_session *session;
	struct burn_toc_entry *entry;
	struct buffer buf;
	int i, session_number, prev_session = -1, ret, lba, alloc_len = 34;
	unsigned char *tdata, size_data[4], start_data[4];
	char msg[160];

	if (mmc_function_spy(d, "mmc_fake_toc") <= 0)
		return -1;

	if (d->last_track_no <= 0 || d->complete_sessions <= 0 ||
	    d->status == BURN_DISC_BLANK)
		return 2;
	if (d->last_track_no > BURN_MMC_FAKE_TOC_MAX_SIZE) {
		sprintf(msg,
			"Too many logical tracks recorded (%d , max. %d)\n",
			d->last_track_no, BURN_MMC_FAKE_TOC_MAX_SIZE);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				 0x0002012c,
				 LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				 msg, 0,0);
		return 0;
	}
	/* ts A71128 : My DVD-ROM drive issues no reliable track info.
			One has to try 43h READ TOC/PMA/ATIP Form 0. */
	if ((d->current_profile == 0x10 || d->current_profile == 0x40) &&
	     d->last_track_no <= 1) {
		ret = mmc_read_toc_fmt0(d);
		return ret;
	}
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
		if (session == NULL)
			return -1;
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
		ret = mmc_read_track_info(d, i+1, &buf, alloc_len);
		if (ret <= 0)
			return ret;
		tdata = buf.data;
		session_number = (tdata[33] << 8) | tdata[3];
		if (session_number <= 0)
	continue;

		if (session_number != prev_session && prev_session > 0) {
			/* leadout entry previous session */
			entry = &(d->toc_entry[(i - 1) + prev_session]);
			lba = mmc_four_char_to_int(start_data) +
			      mmc_four_char_to_int(size_data);
			mmc_int_to_four_char(start_data, lba);
			mmc_int_to_four_char(size_data, 0);
			mmc_fake_toc_entry(entry, prev_session, 0xA2,
					 size_data, start_data);
			entry->min= entry->sec= entry->frame= 0;
			d->disc->session[prev_session - 1]->leadout_entry =
									entry;
		}

		if (session_number > d->disc->sessions) {
			if (i == d->last_track_no - 1) {
				/* ts A70212 : Last track field Free Blocks */
				burn_drive_set_media_capacity_remaining(d,
				  ((off_t) mmc_four_char_to_int(tdata + 16)) *
				  ((off_t) 2048));
				d->media_lba_limit = 0;
			}	
	continue;
		}

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

		if (prev_session != session_number)
			d->disc->session[session_number - 1]->firsttrack = i+1;
		d->disc->session[session_number - 1]->lasttrack = i+1;
		prev_session = session_number;
	}
	if (prev_session > 0 && prev_session <= d->disc->sessions) {
		/* leadout entry of last session of closed disc */
		entry = &(d->toc_entry[(d->last_track_no - 1) + prev_session]);
		lba = mmc_four_char_to_int(start_data) +
		      mmc_four_char_to_int(size_data);
		mmc_int_to_four_char(start_data, lba);
		mmc_int_to_four_char(size_data, 0);
		mmc_fake_toc_entry(entry, prev_session, 0xA2,
				 size_data, start_data);
		entry->min= entry->sec= entry->frame= 0;
		d->disc->session[prev_session - 1]->leadout_entry = entry;
	}
	return 1;
}


static int mmc_read_toc_al(struct burn_drive *d, int *alloc_len)
{
/* read full toc, all sessions, in m/s/f form, 4k buffer */
/* ts A70201 : or fake a toc from track information */
	struct burn_track *track;
	struct burn_session *session;
	struct buffer buf;
	struct command c;
	int dlen;
	int i, bpl= 12, old_alloc_len, t_idx, ret;
	unsigned char *tdata;
	char msg[321];

	if (*alloc_len < 4)
		return 0;

	if (!(d->current_profile == -1 || d->current_is_cd_profile)) {
		/* ts A70131 : MMC_GET_TOC uses Response Format 2 
		   For DVD this fails with 5,24,00 */
		/* mmc_read_toc_fmt0() uses
                   Response Format 0: mmc5r03.pdf 6.26.3.2
		   which does not yield the same result with the same disc
		   on different drives.
		*/
		/* ts A70201 :
		   This uses the session count from 51h READ DISC INFORMATION
		   and the track records from 52h READ TRACK INFORMATION.
		   mmc_read_toc_fmt0() is used as fallback for dull DVD-ROM.
		*/
		mmc_fake_toc(d);

		if (d->status == BURN_DISC_UNREADY)
			d->status = BURN_DISC_FULL;
		return 1;
	}

	/* ts A90823:
	   SanDisk Cruzer U3 memory stick stalls on format 2.
	   Format 0 seems to be more conservative with read-only drives.
	*/
	if (!(d->mdata->cdrw_write || d->current_profile != 0x08)) {
		ret = mmc_read_toc_fmt0(d);
		return ret;
	}

	scsi_init_command(&c, MMC_GET_TOC, sizeof(MMC_GET_TOC));
/*
	memcpy(c.opcode, MMC_GET_TOC, sizeof(MMC_GET_TOC));
	c.oplen = sizeof(MMC_GET_TOC);
*/
	c.dxfer_len = *alloc_len;
	c.opcode[7] = (c.dxfer_len >> 8) & 0xff;
	c.opcode[8] = c.dxfer_len & 0xff;
	c.retry = 1;
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
		d->toc_entry = calloc(1, sizeof(struct burn_toc_entry));
		return 0;
	}

	dlen = c.page->data[0] * 256 + c.page->data[1];
	old_alloc_len = *alloc_len;
	*alloc_len = dlen + 2;
	if (old_alloc_len < 15)
		return 1;
	if (dlen + 2 > old_alloc_len)
		dlen = old_alloc_len - 2;
	d->toc_entries = (dlen - 2) / 11;
	if (d->toc_entries < 1)
		return 0;
/*
	some drives fail this check.

	ts A61007 : if re-enabled then not via Assert.
	a ssert(((dlen - 2) % 11) == 0);
*/
	/* ts A81202: plus number of sessions as reserve for leadout default */
	d->toc_entry = calloc(d->toc_entries + (unsigned char) c.page->data[3],
				 sizeof(struct burn_toc_entry));
	if(d->toc_entry == NULL) /* ts A70825 */
		return 0;
	tdata = c.page->data + 4;

	burn_print(12, "TOC:\n");

	d->disc = burn_disc_create();
	if (d->disc == NULL) /* ts A70825 */
		return 0;

	for (i = 0; i < c.page->data[3]; i++) {
		session = burn_session_create();
		if (session == NULL) /* ts A70825 */
			return 0;
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

	/* ts A81202 ticket 146 : a drive reported a session with no leadout */
	for (i = 0; i < d->disc->sessions; i++) {
		if (d->disc->session[i]->leadout_entry != NULL)
	continue;
		sprintf(msg, "Session %d of %d encountered without leadout",
			i + 1, d->disc->sessions);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020160,
				LIBDAX_MSGS_SEV_WARNING, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);

		/* Produce default leadout entry from last track of session
		   which will thus get its size set to 0 */;
		if (d->disc->session[i]->track != NULL &&
		    d->disc->session[i]->tracks > 0) {
			t_idx = d->toc_entries++;
			memcpy(d->toc_entry + t_idx,
				d->disc->session[i]->track[
				       d->disc->session[i]->tracks - 1]->entry,
				sizeof(struct burn_toc_entry));
			d->toc_entry[t_idx].point = 0xA2;
			d->disc->session[i]->leadout_entry =
							 d->toc_entry + t_idx;
		} else {
			burn_disc_remove_session(d->disc, d->disc->session[i]);
			sprintf(msg,
				"Empty session %d deleted. Now %d sessions.",
				i + 1, d->disc->sessions);
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020161,
				LIBDAX_MSGS_SEV_WARNING, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
			i--;
		}
	}

	/* A80808 */
	burn_disc_cd_toc_extensions(d->disc, 0);

	return 1;
}


void mmc_read_toc(struct burn_drive *d)
{
	int alloc_len = 4, ret;

	if (mmc_function_spy(d, "mmc_read_toc") <= 0)
		return;

	ret = mmc_read_toc_al(d, &alloc_len);
/*
	fprintf(stderr,
		"LIBBURN_DEBUG: 43h READ TOC alloc_len = %d , ret = %d\n",
		alloc_len, ret);
*/
	if (alloc_len >= 15)
		ret = mmc_read_toc_al(d, &alloc_len);
}


/* ts A70131 : This tries to get the start of the last complete session */
/* man mkisofs , option -C :
   The first number is the sector number of the first sector in
   the last session of the disk that should be appended to.
*/
int mmc_read_multi_session_c1(struct burn_drive *d, int *trackno, int *start)
{
	struct buffer buf;
	struct command c;
	unsigned char *tdata;
	int num_sessions, session_no, num_tracks, alloc_len = 12;
	struct burn_disc *disc;
	struct burn_session **sessions;
	struct burn_track **tracks;
	struct burn_toc_entry toc_entry;

	if (mmc_function_spy(d, "mmc_read_multi_session_c1") <= 0)
		return 0;

	/* First try to evaluate the eventually loaded TOC before issueing
	   a MMC command. This search obtains the first track of the last
	   complete session which has a track.
	*/
	*trackno = 0;
	disc = burn_drive_get_disc(d);
	if (disc == NULL)
		goto inquire_drive;
	sessions = burn_disc_get_sessions(disc, &num_sessions);
	for (session_no = 0; session_no<num_sessions; session_no++) {
		tracks = burn_session_get_tracks(sessions[session_no],
						&num_tracks);
		if (tracks == NULL || num_tracks <= 0)
	continue;
		burn_track_get_entry(tracks[0], &toc_entry);
		if (toc_entry.extensions_valid & 1) { /* DVD extension valid */
			*start = toc_entry.start_lba;
			*trackno = (toc_entry.point_msb << 8)| toc_entry.point;
		} else {
			*start = burn_msf_to_lba(toc_entry.pmin,
					toc_entry.psec, toc_entry.pframe);
			*trackno = toc_entry.point;
		}
	}
	burn_disc_free(disc);
	if(*trackno > 0)
		return 1;

inquire_drive:;
	/* mmc5r03.pdf 6.26.3.3.3 states that with non-CD this would
	   be a useless fake always starting at track 1, lba 0.
	   My drives return useful data, though.
	   MMC-3 states that DVD had no tracks. So maybe this mandatory fake
	   is a forgotten legacy ?
	*/
	scsi_init_command(&c, MMC_GET_MSINFO, sizeof(MMC_GET_MSINFO));
/*
	memcpy(c.opcode, MMC_GET_MSINFO, sizeof(MMC_GET_MSINFO));
	c.oplen = sizeof(MMC_GET_MSINFO);
*/
	c.dxfer_len = alloc_len;
	c.opcode[7]= (c.dxfer_len >> 8) & 0xff;
	c.opcode[8]= c.dxfer_len & 0xff;
	c.retry = 1;
	c.page = &buf;
	c.page->bytes = 0;
	c.page->sectors = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	if (c.error)
		return 0;

	tdata = c.page->data + 4;
	*trackno = tdata[2];
	*start = mmc_four_char_to_int(tdata + 4);
	return 1;
}


/* ts A61201 */
char *mmc_obtain_profile_name(int profile_number)
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


/* ts A90603 : to be used if the drive knows no GET CONFIGURATION
*/
static int mmc_guess_profile(struct burn_drive *d, int flag)
{
	int cp;

	cp = 0;
	if (d->status == BURN_DISC_BLANK ||
	    d->status == BURN_DISC_APPENDABLE) {
		cp = 0x09;
	} else if (d->status == BURN_DISC_FULL) {
		cp = 0x08;
	}
	if (cp)
		if (d->erasable)
			cp = 0x0a;
	d->current_profile = cp;
	if (cp == 0)
		return 0;
	d->current_is_cd_profile = 1;
	d->current_is_supported_profile = 1;
	strcpy(d->current_profile_text, mmc_obtain_profile_name(cp));
	return 1;
}


static int mmc_read_disc_info_al(struct burn_drive *d, int *alloc_len)
{
	struct buffer buf;
	unsigned char *data;
	struct command c;
	char msg[160];
	/* ts A70131 : had to move mmc_read_toc() to end of function */
	int do_read_toc = 0, disc_status, len, old_alloc_len;
	int ret, number_of_sessions = -1;

	/* ts A61020 */
	d->start_lba = d->end_lba = -2000000000;
	d->erasable = 0;
	d->last_track_no = 1;

	/* ts A70212 - A70215 */
	d->media_capacity_remaining = 0;
	d->media_lba_limit = 0;

	/* ts A81210 */
	d->media_read_capacity = 0x7fffffff;

	/* ts A61202 */
	d->toc_entries = 0;
	if (d->status == BURN_DISC_EMPTY)
		return 1;

	mmc_get_configuration(d);

/* ts A70910 : found this as condition for mmc_function_spy() which went up
	if (*alloc_len < 2)
*/

	scsi_init_command(&c, MMC_GET_DISC_INFO, sizeof(MMC_GET_DISC_INFO));
/*
	memcpy(c.opcode, MMC_GET_DISC_INFO, sizeof(MMC_GET_DISC_INFO));
	c.oplen = sizeof(MMC_GET_DISC_INFO);
*/
	c.dxfer_len = *alloc_len;
	c.opcode[7]= (c.dxfer_len >> 8) & 0xff;
	c.opcode[8]= c.dxfer_len & 0xff;
	c.retry = 1;
	c.page = &buf;
	c.page->sectors = 0;
	c.page->bytes = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	if (c.error) {
		d->busy = BURN_DRIVE_IDLE;
		return 0;
	}

	data = c.page->data;
	len = (data[0] << 8) | data[1];
	old_alloc_len = *alloc_len;
	*alloc_len = len + 2;
	if (old_alloc_len < 34)
		return 1;
	if (*alloc_len < 24) /* data[23] is the last byte used here */
		return 0;
	if (len + 2 > old_alloc_len)
		len = old_alloc_len - 2;

	d->erasable = !!(data[2] & 16);

 	disc_status = data[2] & 3;
	d->state_of_last_session = (data[2] >> 2) & 3;
	number_of_sessions = (data[9] << 8) | data[4];

	if (d->current_profile == 0x10 || d->current_profile == 0x40) {
							 /* DVD-ROM , BD-ROM */
		disc_status = 2; /* always full and finalized */
		d->erasable = 0; /* never erasable */
	}

	/* ts A80207 : DVD - R DL can normally be read but not be written */
	if(d->current_profile == 0x15 && !burn_support_untested_profiles) {
		disc_status = 2; /* always full and finalized */
		d->erasable = 0; /* never erasable */
	}

#ifdef Libburn_support_bd_r_readonlY
	/* <<< For now: declaring BD-R read-only
	*/
#ifndef Libburn_support_bd_plus_r_srM
	if (d->current_profile == 0x41) {
					/* BD-R seq as readonly dummy */
		disc_status = 2; /* always full and finalized */
		d->erasable = 0; /* never erasable */
	}
#endif
	if (d->current_profile == 0x42) {
						 /* BD-R rnd */
		disc_status = 2; /* always full and finalized */
		d->erasable = 0; /* never erasable */
	}
#endif /* Libburn_support_bd_r_readonlY */

	switch (disc_status) {
	case 0:
regard_as_blank:;
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
		if (disc_status == 2)
			d->status = BURN_DISC_FULL;

		/* ts A81210 */
		ret = mmc_read_capacity(d);
		/* Freshly formatted, unwritten BD-R pretend to be appendable
		   but in our model they need to be regarded as blank.
		   Criterion: BD-R seq, read capacity known and 0,
		              declared appendable, single empty session
		*/
		if (d->current_profile == 0x41 &&
		    d->status == BURN_DISC_APPENDABLE &&
		    ret > 0 && d->media_read_capacity == 0 &&
		    d->state_of_last_session == 0 && number_of_sessions == 1)
			goto regard_as_blank;

		if (d->current_profile == 0x41 &&
		    d->status == BURN_DISC_APPENDABLE &&
		    d->state_of_last_session == 1) {

			/* ??? apply this test to other media types ? */

			libdax_msgs_submit(libdax_messenger, d->global_index,
				 0x00020169,
				 LIBDAX_MSGS_SEV_WARNING, LIBDAX_MSGS_PRIO_HIGH,
				 "Last session on media is still open.", 0, 0);
		}

		do_read_toc = 1;
		break;
	}

	/* ts A90603 : An MMC-1 drive might not know the media type yet */
	if (d->current_is_guessed_profile && d->current_profile == 0)
		mmc_guess_profile(d, 0);

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
		return 0;
	}

	/* ts A61217 : Note for future
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

	/* Preliminarily declare blank:
	   ts A61219 : DVD+RW (is not bg_format_status==0 "blank")
	   ts A61229 : same for DVD-RW Restricted overwrite
	   ts A70112 : same for DVD-RAM
	*/
	if (d->current_profile == 0x1a || d->current_profile == 0x13 ||
	    d->current_profile == 0x12 || d->current_profile == 0x43)
		d->status = BURN_DISC_BLANK;

	if (d->status == BURN_DISC_BLANK) {
                d->last_track_no = 1; /* The "incomplete track" */
		d->complete_sessions = 0;
	} else {
		/* ts A70131 : number of non-empty sessions */
		d->complete_sessions = number_of_sessions;
		/* mmc5r03c.pdf 6.22.3.1.3 State of Last Session: 3=complete */
		if (d->state_of_last_session != 3 && d->complete_sessions >= 1)
			d->complete_sessions--;

		/* ts A70129 : mmc5r03c.pdf 6.22.3.1.7
		   This includes the "incomplete track" if the disk is
		   appendable. I.e number of complete tracks + 1. */
		d->last_track_no = (data[11] << 8) | data[6];
	}
	if (d->current_profile != 0x0a && d->current_profile != 0x13 &&
	    d->current_profile != 0x14 && d->status != BURN_DISC_FULL)
		d->erasable = 0; /* stay in sync with burn_disc_erase() */

	if (do_read_toc)
		mmc_read_toc(d);
	return 1;
}


void mmc_read_disc_info(struct burn_drive *d)
{
	int alloc_len = 34, ret;

	if (mmc_function_spy(d, "mmc_read_disc_info") <= 0)
		return;

	ret = mmc_read_disc_info_al(d, &alloc_len);
/*
	fprintf(stderr,"LIBBURN_DEBUG: 51h alloc_len = %d , ret = %d\n",
			alloc_len, ret);
*/
	/* for now there is no need to inquire the variable lenght part */
}


void mmc_read_atip(struct burn_drive *d)
{
	struct buffer buf;
	struct command c;
	int alloc_len = 28;

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

	if (mmc_function_spy(d, "mmc_read_atip") <= 0)
		return;

	scsi_init_command(&c, MMC_GET_ATIP, sizeof(MMC_GET_ATIP));
/*
	memcpy(c.opcode, MMC_GET_ATIP, sizeof(MMC_GET_ATIP));
	c.oplen = sizeof(MMC_GET_ATIP);
*/
	c.dxfer_len = alloc_len;
	c.opcode[7]= (c.dxfer_len >> 8) & 0xff;
	c.opcode[8]= c.dxfer_len & 0xff;
	c.retry = 1;
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

	if (mmc_function_spy(d, "mmc_read_sectors") <= 0)
		return;

	/* ts A61009 : to be ensured by callers */
	/* a ssert(len >= 0); */

/* if the drive isn't busy, why the hell are we here? */ 
	/* ts A61006 : i second that question */
	/* a ssert(d->busy); */

	burn_print(12, "reading %d from %d\n", len, start);

	scsi_init_command(&c, MMC_READ_CD, sizeof(MMC_READ_CD));
/*
	memcpy(c.opcode, MMC_READ_CD, sizeof(MMC_READ_CD));
	c.oplen = sizeof(MMC_READ_CD);
*/
	c.retry = 1;
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

	if (mmc_function_spy(d, "mmc_erase") <= 0)
		return;

	scsi_init_command(&c, MMC_BLANK, sizeof(MMC_BLANK));
/*
	memcpy(c.opcode, MMC_BLANK, sizeof(MMC_BLANK));
	c.oplen = sizeof(MMC_BLANK);
*/
	c.opcode[1] = 16;	/* IMMED set to 1 */
	c.opcode[1] |= !!fast;
	c.retry = 1;
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
}

void mmc_read_lead_in(struct burn_drive *d, struct buffer *buf)
{
	int len;
	struct command c;

	if (mmc_function_spy(d, "mmc_read_lead_in") <= 0)
		return;

	len = buf->sectors;
	scsi_init_command(&c, MMC_READ_CD, sizeof(MMC_READ_CD));
/*
	memcpy(c.opcode, MMC_READ_CD, sizeof(MMC_READ_CD));
	c.oplen = sizeof(MMC_READ_CD);
*/
	c.retry = 1;
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

	if (mmc_function_spy(d, "mmc_perform_opc") <= 0)
		return;

	scsi_init_command(&c, MMC_SEND_OPC, sizeof(MMC_SEND_OPC));
/*
	memcpy(c.opcode, MMC_SEND_OPC, sizeof(MMC_SEND_OPC));
	c.oplen = sizeof(MMC_SEND_OPC);
*/
	c.retry = 1;
	c.opcode[1] = 1;
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
}


/* ts A61221 : Learned much from dvd+rw-tools-7.0 set_speed_B6h() but then
   made own experiments on base of mmc5r03c.pdf 6.8.3 and 6.39 in the hope
   to achieve a leaner solution
   ts A70712 : That leaner solution does not suffice for my LG GSA-4082B.
   Meanwhile there is a speed descriptor list anyway.
*/
int mmc_set_streaming(struct burn_drive *d,
			 int r_speed, int w_speed, int end_lba)
{
	struct buffer buf;
	struct command c;
	int b, eff_end_lba;
	char msg[256];
	unsigned char *pd;
	int key, asc, ascq;

	if (mmc_function_spy(d, "mmc_set_streaming") <= 0)
		return 0;

	scsi_init_command(&c, MMC_SET_STREAMING, sizeof(MMC_SET_STREAMING));
/*
	c.oplen = sizeof(MMC_SET_STREAMING);
	memcpy(c.opcode, MMC_SET_STREAMING, sizeof(MMC_SET_STREAMING));
*/
	c.retry = 1;
	c.page = &buf;
	c.page->bytes = 28;
	c.opcode[9] = (c.page->bytes >> 8) & 0xff;
	c.opcode[10] = c.page->bytes & 0xff;
	c.page->sectors = 0;
	c.dir = TO_DRIVE;
	memset(c.page->data, 0, c.page->bytes);
	pd = c.page->data;

	pd[0] = 0; /* WRC=0 (Default Rotation Control), RDD=Exact=RA=0 */

	if (w_speed == 0)
		w_speed = 0x10000000; /* ~ 2 TB/s */
	else if (w_speed < 0)
		w_speed = 177; /* 1x CD */
	if (r_speed == 0)
		r_speed = 0x10000000; /* ~ 2 TB/s */
	else if (r_speed < 0)
		r_speed = 177; /* 1x CD */
	if (end_lba == 0) {
		/* Default computed from 4.7e9 */
		eff_end_lba = 2294921 - 1;
		if (d->mdata->max_end_lba > 0)
			eff_end_lba = d->mdata->max_end_lba - 1;
	} else
		eff_end_lba = end_lba;

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

#ifdef NIX
			sprintf(msg,
	"SCSI error on set_streaming(%d): key=%X asc=%2.2Xh ascq=%2.2Xh",
				w_speed,
				c.sense[2],c.sense[12],c.sense[13]);
				libdax_msgs_submit(libdax_messenger,
				d->global_index,
				0x00020124,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
#else /* NIX */

			sprintf(msg,
				"SCSI error on set_streaming(%d): ", w_speed);
			scsi_error_msg(d, c.sense, 14, msg + strlen(msg), 
					&key, &asc, &ascq);

#endif /* !NIX */

		}
		return 0;
	}
	return 1;
}


void mmc_set_speed(struct burn_drive *d, int r, int w)
{
	struct command c;
	int ret, end_lba = 0;
	struct burn_speed_descriptor *best_sd = NULL;

	if (mmc_function_spy(d, "mmc_set_speed") <= 0)
		return;

	if (r <= 0 || w <= 0) {
		/* ts A70712 : now searching for best speed descriptor */
		if (w > 0 && r <= 0) 
			burn_drive_get_best_speed(d, r, &best_sd, 1);
		else
			burn_drive_get_best_speed(d, w, &best_sd, 0);
		if (best_sd != NULL) {
			w = best_sd->write_speed;
			d->nominal_write_speed = w;
			r = best_sd->read_speed;
			end_lba = best_sd->end_lba;
		}
	}

	/* A70711 */
	d->nominal_write_speed = w;

	/* ts A61221 : try to set DVD speed via command B6h */
	if (strstr(d->current_profile_text, "DVD") == d->current_profile_text){
		ret = mmc_set_streaming(d, r, w, end_lba);
		if (ret != 0)
			return; /* success or really fatal failure */ 
	}

	/* ts A61112 : MMC standards prescribe FFFFh as max speed.
			But libburn.h prescribes 0.
	   ts A70715 : <0 now means minimum speed */
	if (r == 0 || r > 0xffff)
		r = 0xffff;
	else if (r < 0)
		r = 177; /* 1x CD */
	if (w == 0 || w > 0xffff)
		w = 0xffff;
	else if (w < 0)
		w = 177; /* 1x CD */

	scsi_init_command(&c, MMC_SET_SPEED, sizeof(MMC_SET_SPEED));
/*
	memcpy(c.opcode, MMC_SET_SPEED, sizeof(MMC_SET_SPEED));
	c.oplen = sizeof(MMC_SET_SPEED);
*/
	c.retry = 1;
	c.opcode[2] = r >> 8;
	c.opcode[3] = r & 0xFF;
	c.opcode[4] = w >> 8;
	c.opcode[5] = w & 0xFF;
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
}


/* ts A61201 : found in unfunctional state
 */
static int mmc_get_configuration_al(struct burn_drive *d, int *alloc_len)
{
	struct buffer buf;
	int len, cp, descr_len = 0, feature_code, prf_number, only_current = 1;
	int old_alloc_len, only_current_profile = 0;
	unsigned char *descr, *prf, *up_to, *prf_end;
	struct command c;
	int phys_if_std = 0;
	char *phys_name = "";

	if (*alloc_len < 8)
		return 0;

	d->current_profile = 0;
        d->current_profile_text[0] = 0;
	d->current_is_cd_profile = 0;
	d->current_is_supported_profile = 0;
        d->current_is_guessed_profile = 0;
	d->num_profiles = 0;
	d->current_has_feat21h = 0;
	d->current_feat21h_link_size = -1;
	d->current_feat23h_byte4 = 0;
	d->current_feat23h_byte8 = 0;
	d->current_feat2fh_byte4 = -1;

	scsi_init_command(&c, MMC_GET_CONFIGURATION,
			 sizeof(MMC_GET_CONFIGURATION));
	c.dxfer_len= *alloc_len;
	c.retry = 1;
	c.opcode[7] = (c.dxfer_len >> 8) & 0xff;
	c.opcode[8] = c.dxfer_len & 0xff;
	c.page = &buf;
	c.page->sectors = 0;
	c.page->bytes = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

#ifdef Libisofs_simulate_old_mmc1_drivE
	c.error = 1;
	c.sense[2] = 0x5;
	c.sense[12] = 0x20;
	c.sense[13] = 0x0;
#endif /* Libisofs_simulate_old_mmc1_drivE */

	if (c.error) {
		/* ts A90603 : MMC-1 drive do not know 46h GET CONFIGURATION */
		if (c.sense[2] == 0x5 && c.sense[12] == 0x20 &&
		    c.sense[13] == 0x0) {
			d->current_is_guessed_profile = 1;
			/* Will yield a non-zero profile only after
			   mmc_read_disc_info_al() was called */
			mmc_guess_profile(d, 0);
		}
		return 0;
	}
	old_alloc_len = *alloc_len;
	*alloc_len = len = mmc_four_char_to_int(c.page->data);
	if (len > old_alloc_len)
		len = old_alloc_len;
	if (len < 8 || len > 4096)
		return 0;
	cp = (c.page->data[6]<<8) | c.page->data[7];

#ifdef Libburn_rom_as_profilE
	if (cp == 0x08 || cp == 0x10 || cp==0x40)
		cp = Libburn_rom_as_profilE;
#endif /* Libburn_rom_as_profilE */

	d->current_profile = cp;
	strcpy(d->current_profile_text, mmc_obtain_profile_name(cp));

	/* Read-only supported media */

	if (cp == 0x08) /* CD-ROM */
		d->current_is_supported_profile = d->current_is_cd_profile = 1;
	if (cp == 0x10) /* DVD-ROM */
		d->current_is_supported_profile = 1;
	if (cp == 0x40) /* BD-ROM */
		d->current_is_supported_profile = 1;

#ifdef Libburn_support_bd_r_readonlY
#ifndef Libburn_support_bd_plus_r_srM
	if (cp == 0x41) /* BD-R sequential (here as read-only dummy) */
		d->current_is_supported_profile = 1;
#endif
	if (cp == 0x42) /* BD-R random recording */
		d->current_is_supported_profile = 1;
#endif


	/* Write supported media (they get declared suitable in
	                          burn_disc_get_multi_caps) */

	if (cp == 0x09 || cp == 0x0a)
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
	if (cp == 0x12 || cp == 0x43) {                  /* DVD-RAM , BD-RE */
		d->current_is_supported_profile = 1;

#ifdef Libburn_dvd_ram_as_bd_rE
		cp = d->current_profile = 0x43;
		strcpy(d->current_profile_text, mmc_obtain_profile_name(cp));
#endif

	}
#endif
#ifdef Libburn_support_dvd_r_seQ
	if (cp == 0x11 || cp == 0x14) /* DVD-R, DVD-RW */
		d->current_is_supported_profile = 1;
	if (cp == 0x15) /* DVD-R/DL . */
		 	/* Writeable only if burn_support_untested_profiles */
		d->current_is_supported_profile = 1;
#endif
#ifdef Libburn_support_dvd_plus_R
	if (cp == 0x1b || cp == 0x2b) /* DVD+R , DVD+R/DL */
		d->current_is_supported_profile = 1;
#endif
#ifdef Libburn_support_bd_plus_r_srM
	if (cp == 0x41) /* BD-R SRM */
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
		len, (int) (up_to - c.page->data));
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
			d->num_profiles = descr[3] / 4;
			if (d->num_profiles > 64)
				d->num_profiles = 64;
			if (d->num_profiles > 0)
				memcpy(d->all_profiles, descr + 4,
							d->num_profiles * 4);
			for (prf = descr + 4; prf + 2 < prf_end; prf += 4) {
				if (only_current_profile && !(prf[2] & 1))
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

		} else if (feature_code == 0x23) {
			d->current_feat23h_byte4 = descr[4];
			d->current_feat23h_byte8 = descr[8];
#ifdef Libburn_print_feature_descriptorS
			if (cp >= 0x41 && cp <= 0x43) 
				fprintf(stderr,
			"LIBBURN_EXPERIMENTAL : BD formats: %s%s%s%s%s\n",
					descr[4] & 1 ? " Cert" : "",
					descr[4] & 2 ? " QCert" : "",
					descr[4] & 4 ? " Expand" : "",
					descr[4] & 8 ? " RENoSA" : "",
					descr[8] & 1 ? " RRM" : "");
#endif /* Libburn_print_feature_descriptorS */

		} else if (feature_code == 0x2F) {
			if (descr[2] & 1)
				d->current_feat2fh_byte4 = descr[4];

#ifdef Libburn_print_feature_descriptorS
			fprintf(stderr, "LIBBURN_EXPERIMENTAL :     BUF = %d , Test Write = %d , DVD-RW = %d\n",
				!!(descr[4] & 64), !!(descr[4] & 4),
				!!(descr[4] & 2));
#endif /* Libburn_print_feature_descriptorS */
			
		} else if (feature_code == 0x01) {
			phys_if_std = (descr[4] << 24) | (descr[5] << 16) |
					(descr[6] << 8) | descr[9];
			if (phys_if_std == 1)
				phys_name = "SCSI Family";
			else if(phys_if_std == 2)
				phys_name = "ATAPI";
			else if(phys_if_std == 3 || phys_if_std == 4 ||
				 phys_if_std == 6)
				phys_name = "IEEE 1394 FireWire";
			else if(phys_if_std == 7)
				phys_name = "Serial ATAPI";
			else if(phys_if_std == 8)
				phys_name = "USB";
			
			d->phys_if_std = phys_if_std;
			strcpy(d->phys_if_name, phys_name);

#ifdef Libburn_print_feature_descriptorS

			fprintf(stderr,
	"LIBBURN_EXPERIMENTAL :     Phys. Interface Standard %Xh \"%s\"\n",
				phys_if_std, phys_name);

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
	return 1;
}


void mmc_get_configuration(struct burn_drive *d)
{
	int alloc_len = 8, ret;

	if (mmc_function_spy(d, "mmc_get_configuration") <= 0)
		return;

	/* first command execution to learn Allocation Length */
	ret = mmc_get_configuration_al(d, &alloc_len);
/*
	fprintf(stderr,"LIBBURN_DEBUG: 46h alloc_len = %d , ret = %d\n",
			alloc_len, ret);
*/
	if (alloc_len > 8 && ret > 0)
		/* second execution with announced length */
		mmc_get_configuration_al(d, &alloc_len);
}


/* ts A70108 */
/* mmc5r03c.pdf 6.24 */
static int mmc_read_format_capacities_al(struct burn_drive *d,
					int *alloc_len, int top_wanted)
{
	struct buffer buf;
	int len, type, score, num_descr, max_score = -2000000000, i, sign = 1;
	int old_alloc_len;
	off_t size, num_blocks;
	struct command c;
	unsigned char *dpt;
/* <<<
	char msg[160];
*/

	if (*alloc_len < 4)
		return 0;

	d->format_descr_type = 3;
	d->format_curr_max_size = 0;
	d->format_curr_blsas = 0;
	d->best_format_type = -1;
	d->best_format_size = 0;

	scsi_init_command(&c, MMC_READ_FORMAT_CAPACITIES,
			 sizeof(MMC_READ_FORMAT_CAPACITIES));
/*
	memcpy(c.opcode, MMC_READ_FORMAT_CAPACITIES,
		 sizeof(MMC_READ_FORMAT_CAPACITIES));
	c.oplen = sizeof(MMC_READ_FORMAT_CAPACITIES);
*/
	c.dxfer_len = *alloc_len;
	c.retry = 1;
	c.opcode[7]= (c.dxfer_len >> 8) & 0xff;
	c.opcode[8]= c.dxfer_len & 0xff;
	c.page = &buf;
	c.page->sectors = 0;
	c.page->bytes = 0;
	c.dir = FROM_DRIVE;

	d->issue_command(d, &c);
	if (c.error)
		return 0;

	len = c.page->data[3];
	old_alloc_len = *alloc_len;
	*alloc_len = len + 4;
	if (old_alloc_len < 12)
		return 1;
	if (len + 4 > old_alloc_len)
		len = old_alloc_len - 4;
	if (len < 8)
		return 0;

	dpt = c.page->data + 4;
	/* decode 6.24.3.2 Current/Maximum Capacity Descriptor */
	d->format_descr_type = dpt[4] & 3;
	d->format_curr_max_size = (((off_t) dpt[0]) << 24)
		 		  + (dpt[1] << 16) + (dpt[2] << 8) + dpt[3];
	if (d->format_descr_type == BURN_FORMAT_IS_UNKNOWN)
		d->format_curr_max_size = 0;
	d->format_curr_blsas = (dpt[5] << 16) + (dpt[6] << 8) + dpt[7];

/* <<<
	sprintf(msg,
		"Current/Maximum Capacity Descriptor : type = %d : %.f",
		d->format_descr_type, (double) d->format_curr_max_size);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   msg, 0, 0);
*/

	d->format_curr_max_size *= (off_t) 2048;
	if((d->current_profile == 0x12 || d->current_profile == 0x43)
	   && d->media_capacity_remaining == 0) {
		burn_drive_set_media_capacity_remaining(d,
						d->format_curr_max_size);
		d->media_lba_limit = d->format_curr_max_size / 2048;
	}


#ifdef Libburn_dvd_ram_as_bd_rE
	/* <<< dummy format descriptor list as obtained from
	       dvd+rw-mediainfo by Giulio Orsero in April 2008
	*/
	d->num_format_descr = 5;
	d->format_descriptors[0].type = 0x00;
	d->format_descriptors[0].size = (off_t) 11826176 * (off_t) 2048;
	d->format_descriptors[0].tdp = 0x3000;
	d->format_descriptors[1].type = 0x30;
	d->format_descriptors[1].size = (off_t) 11826176 * (off_t) 2048;
	d->format_descriptors[1].tdp = 0x3000;
	d->format_descriptors[2].type = 0x30;
	d->format_descriptors[2].size = (off_t) 11564032 * (off_t) 2048;
	d->format_descriptors[2].tdp = 0x5000;
	d->format_descriptors[3].type = 0x30;
	d->format_descriptors[3].size = (off_t) 12088320 * (off_t) 2048;
	d->format_descriptors[3].tdp = 0x1000;
	d->format_descriptors[4].type = 0x31;
	d->format_descriptors[4].size = (off_t) 12219392 * (off_t) 2048;
	d->format_descriptors[4].tdp = 0x800;
	d->best_format_type = 0x00;
	d->best_format_size = (off_t) 11826176 * (off_t) 2048;

	/* silencing compiler warnings about unused variables */
	num_blocks = size = sign = i = max_score = num_descr = score = type = 0;

	if (d->current_profile == 0x12 || d->current_profile == 0x43)
		return 1;
	d->num_format_descr = 0;

#endif /* Libburn_dvd_ram_as_bd_rE */

	if (top_wanted == 0x00 || top_wanted == 0x10)
		sign = -1; /* the caller clearly desires full format */

	/* 6.24.3.3 Formattable Capacity Descriptors */
	num_descr = (len - 8) / 8;
	for (i = 0; i < num_descr; i++) {
		dpt = c.page->data + 12 + 8 * i;
		num_blocks = mmc_four_char_to_int(dpt);
		size = num_blocks * (off_t) 2048;
		type = dpt[4] >> 2;

		if (i < 32) {
			d->format_descriptors[i].type = type;
			d->format_descriptors[i].size = size;
			d->format_descriptors[i].tdp =
				(dpt[5] << 16) + (dpt[6] << 8) + dpt[7];
			d->num_format_descr = i + 1;
		}
		
/* <<<
		sprintf(msg, "Capacity Descriptor %2.2Xh  %.fs = %.1f MB",type,
			((double) size)/2048.0, ((double) size)/1024.0/1024.0);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			 0x00000002,
			 LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			 msg, 0, 0);
*/

		/* Criterion is proximity to quick intermediate state */
		if (type == 0x00) { /* full format (with lead out) */
			score = 1 * sign;
		} else if (type == 0x10) { /* DVD-RW full format */
			score = 10 * sign;
		} else if(type == 0x13) { /* DVD-RW quick grow last session */
			score = 100 * sign;
		} else if(type == 0x15) { /* DVD-RW Quick */
			score = 50 * sign;
			if(d->current_profile == 0x13) {
				burn_drive_set_media_capacity_remaining(d,
									size);
				d->media_lba_limit = num_blocks;
			}
		} else if(type == 0x26) { /* DVD+RW */
			score = 1 * sign;
			burn_drive_set_media_capacity_remaining(d, size);
			d->media_lba_limit = num_blocks;
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

/* <<<
	sprintf(msg,
		"best_format_type = %2.2Xh , best_format_size = %.f",
		d->best_format_type, (double) d->best_format_size);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   msg, 0, 0);
*/

	return 1;
}


int mmc_read_format_capacities(struct burn_drive *d, int top_wanted)
{
	int alloc_len = 4, ret;

	if (mmc_function_spy(d, "mmc_read_format_capacities") <= 0)
		return 0;

	ret = mmc_read_format_capacities_al(d, &alloc_len, top_wanted);
/*
	fprintf(stderr,"LIBBURN_DEBUG: 23h alloc_len = %d , ret = %d\n",
		 alloc_len, ret);
*/
	if (alloc_len >= 12 && ret > 0)
		ret = mmc_read_format_capacities_al(d, &alloc_len, top_wanted);

	return ret;
}


void mmc_sync_cache(struct burn_drive *d)
{
	struct command c;

	if (mmc_function_spy(d, "mmc_sync_cache") <= 0)
		return;

	scsi_init_command(&c, MMC_SYNC_CACHE, sizeof(MMC_SYNC_CACHE));
/*
	memcpy(c.opcode, MMC_SYNC_CACHE, sizeof(MMC_SYNC_CACHE));
	c.oplen = sizeof(MMC_SYNC_CACHE);
*/
	c.retry = 1;

	c.opcode[1] |= 2; /* ts A70918 : Immed */

	c.page = NULL;
	c.dir = NO_TRANSFER;

	libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
			   LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			   "syncing cache", 0, 0);
	if(d->wait_for_buffer_free) {
		char msg[80];

		sprintf(msg,
			"Checked buffer %u times. Waited %u+%u times = %.3f s",
			d->pessimistic_writes, d->waited_writes,
			d->waited_tries - d->waited_writes,
			((double) d->waited_usec) / 1.0e6);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002013f,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_LOW,
				msg, 0,0);
	}

	d->issue_command(d, &c);

	/* ts A70918 */
	if (c.error) {
		d->cancel = 1;
		return;
	}
	if (spc_wait_unit_attention(d, 3600, "SYNCHRONIZE CACHE", 0) <= 0)
		d->cancel = 1;
	else
		d->needs_sync_cache = 0;
}


/* ts A61023 : http://libburn.pykix.org/ticket/14
               get size and free space of drive buffer
*/
int mmc_read_buffer_capacity(struct burn_drive *d)
{
	struct buffer buf;
	struct command c;
	unsigned char *data;
	int alloc_len = 12;

	if (mmc_function_spy(d, "mmc_read_buffer_capacity") <= 0)
		return 0;

	scsi_init_command(&c, MMC_READ_BUFFER_CAPACITY,
			 sizeof(MMC_READ_BUFFER_CAPACITY));
/*
	memcpy(c.opcode, MMC_READ_BUFFER_CAPACITY,
		 sizeof(MMC_READ_BUFFER_CAPACITY));
	c.oplen = sizeof(MMC_READ_BUFFER_CAPACITY);
*/
	c.dxfer_len = alloc_len;
	c.opcode[7] = (c.dxfer_len >> 8) & 0xff;
	c.opcode[8] = c.dxfer_len & 0xff;
	c.retry = 1;
	c.page = &buf;
	memset(c.page->data, 0, alloc_len);
	c.page->bytes = 0;
	c.page->sectors = 0;

	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

	/* >>> ??? error diagnostics */
	if (c.error)
		return 0;

	data = c.page->data;

	d->progress.buffer_capacity =
			(data[4]<<24)|(data[5]<<16)|(data[6]<<8)|data[7];
	d->progress.buffer_available =
			(data[8]<<24)|(data[9]<<16)|(data[10]<<8)|data[11];
	d->pessimistic_buffer_free = d->progress.buffer_available;
	d->pbf_altered = 0;
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
   @param flag bit1+2: size mode
                 0 = use parameter size as far as it makes sense
                 1 = insist in size 0 even if there is a better default known
                 2 = without bit7: format to maximum available size
                     with bit7   : take size from indexed format descriptor
                 3 = format to default size
               bit3= expand format up to at least size
               bit4= enforce re-format of (partly) formatted media
               bit5= try to disable eventual defect management
               bit6= try to avoid lengthy media certification
               bit7= bit8 to bit15 contain the index of the format to use
               bit8-bit15 = see bit7
              bit16= enable POW on blank BD-R
*/
int mmc_format_unit(struct burn_drive *d, off_t size, int flag)
{
	struct buffer buf;
	struct command c;
	int ret, tolerate_failure = 0, return_immediately = 0, i, format_type;
	int index, format_sub_type = 0, format_00_index, size_mode;
	int accept_count = 0;
	off_t num_of_blocks = 0, diff, format_size, i_size, format_00_max_size;
	off_t min_size = -1, max_size = -1;
	char msg[256],descr[80];
	int key, asc, ascq;
	int full_format_type = 0x00; /* Full Format (or 0x10 for DVD-RW ?) */

	if (mmc_function_spy(d, "mmc_format_unit") <= 0)
		return 0;
	size_mode = (flag >> 1) & 3;

	scsi_init_command(&c, MMC_FORMAT_UNIT, sizeof(MMC_FORMAT_UNIT));
/*
	c.oplen = sizeof(MMC_FORMAT_UNIT);
	memcpy(c.opcode, MMC_FORMAT_UNIT, sizeof(MMC_FORMAT_UNIT));
*/
	c.retry = 1;
	c.page = &buf;
	c.page->bytes = 12;
	c.page->sectors = 0;
	c.dir = TO_DRIVE;
	memset(c.page->data, 0, c.page->bytes);

	descr[0] = 0;
	c.page->data[1] = 0x02;                  /* Immed */
	c.page->data[3] = 8;                     /* Format descriptor length */
	num_of_blocks = size / 2048;
	mmc_int_to_four_char(c.page->data + 4, num_of_blocks);

	if (flag & 128) { /* explicitely chosen format descriptor */
		/* use case: the app knows what to do */

		ret = mmc_read_format_capacities(d, -1);
		if (ret <= 0)
			goto selected_not_suitable;
		index = (flag >> 8) & 0xff;
		if(index < 0 || index >= d->num_format_descr) {
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
			d->current_profile == 0x1a ||
			d->current_profile == 0x12 ||
			d->current_profile == 0x41 ||
			d->current_profile == 0x43))
			goto unsuitable_media;
		      
		format_type = d->format_descriptors[index].type;
		if (!(format_type == 0x00 || format_type == 0x01 ||
		      format_type == 0x10 ||
		      format_type == 0x11 || format_type == 0x13 ||
		      format_type == 0x15 || format_type == 0x26 ||
 		      format_type == 0x30 || format_type == 0x31 ||
		      format_type == 0x32))
			goto selected_not_suitable;
		if (flag & 4) {
			num_of_blocks =
				d->format_descriptors[index].size / 2048;
			mmc_int_to_four_char(c.page->data + 4, num_of_blocks);
		}
		if (format_type != 0x26)
			for (i = 0; i < 3; i++)
				 c.page->data[9 + i] =
					( d->format_descriptors[index].tdp >>
					  (16 - 8 * i)) & 0xff;
		if (format_type == 0x30 || format_type == 0x31 ||
		    format_type == 0x32) {
			if (flag & 64)
				format_sub_type = 3; /* Quick certification */
			else
				format_sub_type = 2; /* Full certification */
		}
		if (d->current_profile == 0x12 && format_type !=0x01 &&
		    (flag & 64)) {
			/* DCRT and CmpList, see below */
			c.page->data[1] |= 0x20;
			c.opcode[1] |= 0x08;
		}
		c.page->data[1] |= 0x80;  /* FOV = this flag vector is valid */
		sprintf(descr, "%s (descr %d)", d->current_profile_text,index);
		return_immediately = 1; /* caller must do the waiting */

	} else if (d->current_profile == 0x1a) { /* DVD+RW */
		/* use case: background formatting during write     !(flag&4)
	                     de-icing as explicit formatting action (flag&4)
		*/

		/* mmc5r03c.pdf , 6.5.4.2.14, DVD+RW Basic Format */
		format_type = 0x26;

					/* >>> ??? is this "| 8" a bug ? */

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
				mmc_int_to_four_char(c.page->data + 4,
							num_of_blocks);
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
				mmc_int_to_four_char(c.page->data + 4,
							num_of_blocks);
			}

		} else {
no_suitable_formatting_type:;
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

	} else if (d->current_profile == 0x12) {
		/* ts A80417 : DVD-RAM */
		/*  6.5.4.2.1  Format Type = 00h (Full Format)
		    6.5.4.2.2  Format Type = 01h (Spare Area Expansion)
		*/
		index = format_00_index = -1;
		format_size = format_00_max_size = -1;
		for (i = 0; i < d->num_format_descr; i++) {
			format_type = d->format_descriptors[i].type;
			i_size = d->format_descriptors[i].size;
			if (format_type != 0x00 && format_type != 0x01)
		continue;
			if (flag & 32) { /* No defect mgt */
				/* Search for largest 0x00 format descriptor */
				if (format_type != 0x00)
		continue;
				if (i_size < format_size)
		continue;
				format_size = i_size;
				index = i;
		continue;
			} else if (flag & 4) { /*Max or default size with mgt*/
				/* Search for second largest 0x00
				   format descriptor. For max size allow
				   format type 0x01.
				 */
				if (format_type == 0x00) {
					if (i_size < format_size) 
		continue;
					if (i_size < format_00_max_size) {
						format_size = i_size;
						index = i;
		continue;
					}
					format_size = format_00_max_size;
					index = format_00_index;
					format_00_max_size = i_size;
					format_00_index = i;
		continue;
				}
				if (size_mode==3)
		continue;
				if (i_size > format_size) {
					format_size = i_size;
					index = i;
				}
		continue;
			} 
			/* Search for smallest 0x0 or 0x01
			   descriptor >= size */;
			if (d->format_descriptors[i].size >= size &&
			    (format_size < 0 || i_size < format_size)
			   ) {
				format_size = i_size;
				index = i;
			}
		}
		if(index < 0 && (flag & 4) && !(flag & 32)) {
			format_size = format_00_max_size;
			index = format_00_index;
		}
		if(index < 0)
			goto no_suitable_formatting_type;
		format_type = d->format_descriptors[index].type;
		num_of_blocks = d->format_descriptors[index].size / 2048;
		mmc_int_to_four_char(c.page->data + 4, num_of_blocks);
		for (i = 0; i < 3; i++)
			 c.page->data[9 + i] =
				( d->format_descriptors[index].tdp >>
					  (16 - 8 * i)) & 0xff;
		sprintf(descr, "%s", d->current_profile_text);
		return_immediately = 1; /* caller must do the waiting */
		c.page->data[1] |= 0x80;  /* FOV = this flag vector is valid */

		if ((flag & 64) && format_type != 0x01) {
			/* MMC-5 6.5.3.2 , 6.5.4.2.1.2
			   DCRT: Disable Certification and maintain number
			         of blocks
   		           CmpList: Override maintaining of number of blocks
			            with DCRT
			*/
			/* ts A80426 : prevents change of formatted size
		               with PHILIPS SPD3300L and Verbatim 3x DVD-RAM
			       and format_type 0x00. Works on TSSTcorp SH-S203B
			*/
			c.page->data[1] |= 0x20;
			c.opcode[1] |= 0x08;
		}

	} else if (d->current_profile == 0x41) {
		/* BD-R SRM */

		index = -1;
		format_size = -1;
		if (d->num_format_descr <= 0)
			goto no_suitable_formatting_type;
		if (d->format_descriptors[0].type != 0)
			goto no_suitable_formatting_type;
		for (i = 0; i < d->num_format_descr; i++) {
			format_type = d->format_descriptors[i].type;
			i_size = d->format_descriptors[i].size;
			if (format_type != 0x00 && format_type != 0x32)
		continue;
			if (flag & 32) { /* No defect mgt */
				/* ts A81211 : MMC-5 6.5.4.2.17.1
				   When formatted with Format Type 32h,
				   the BD-R disc is required to allocate
				   a non-zero number of spares.
				*/
				goto no_suitable_formatting_type;

			} else if(size_mode == 2) { /* max payload size */
				/* search largest 0x32 format descriptor */
				if(format_type != 0x32)
		continue;
			} else if(size_mode == 3) { /* default payload size */
				if (format_type == 0x00) {
					index = i;
		break;
				}
		continue;
			} else { /* defect managed format with size wish */

#ifdef Libburn_bd_r_format_olD

				/* search for smallest 0x32 >= size */
				if(format_type != 0x32)
		continue;
				if (i_size < size)
		continue;
				if (format_size >= 0 && i_size >= format_size)
		continue;
				index = i;
				format_size = i_size;
		continue;

#else /* Libburn_bd_r_format_olD */

				/* search largest and smallest 0x32 */
				if(format_type != 0x32)
		continue;
				if (i_size < min_size || min_size < 0)
					min_size = i_size;
				if (i_size > max_size)
					max_size = i_size;

#endif /* ! Libburn_bd_r_format_olD */

			}
			/* common for all cases which search largest
			   descriptors */
			if (i_size > format_size) {
				format_size = i_size;
				index = i;
			}
		}
		if (size_mode == 2 && index < 0 && !(flag & 32))
			index = 0;
		if (index < 0)
			goto no_suitable_formatting_type;
		format_type = d->format_descriptors[index].type;
		if (flag & (1 << 16))
			format_sub_type = 0; /* SRM + POW  */
		else
			format_sub_type = 1; /* SRM  (- POW) */

#ifdef Libburn_bd_r_format_olD
		if (0) {
#else
		if (size_mode == 0 || size_mode == 1) {
#endif /* ! Libburn_bd_r_format_olD */

			if (min_size < 0 || max_size < 0)
				goto no_suitable_formatting_type;
			if (size <= 0)
				size = min_size;
			if (size % 0x10000)
				size += 0x10000 - (size % 0x10000);
			if (size < min_size)
				goto no_suitable_formatting_type;
			else if(size > max_size)
				goto no_suitable_formatting_type;
			num_of_blocks = size / 2048;
			mmc_int_to_four_char(c.page->data + 4, num_of_blocks);
			for (i = 0; i < 3; i++)
				 c.page->data[9 + i] = 0;
		} else {
			num_of_blocks = 
				d->format_descriptors[index].size / 2048;
			mmc_int_to_four_char(c.page->data + 4, num_of_blocks);
			for (i = 0; i < 3; i++)
				 c.page->data[9 + i] =
					( d->format_descriptors[index].tdp >>
						  (16 - 8 * i)) & 0xff;
		}
		sprintf(descr, "%s", d->current_profile_text);
		return_immediately = 1; /* caller must do the waiting */
		c.page->data[1] |= 0x80;  /* FOV = this flag vector is valid */

	} else if (d->current_profile == 0x43) {
		/* BD-RE */
		index = -1;
		format_size = -1;
		if (d->num_format_descr <= 0)
			goto no_suitable_formatting_type;
		if (d->format_descriptors[0].type != 0)
			goto no_suitable_formatting_type;
		for (i = 0; i < d->num_format_descr; i++) {
			format_type = d->format_descriptors[i].type;
			i_size = d->format_descriptors[i].size;
			if (format_type != 0x00 && format_type != 0x30 &&
			    format_type != 0x31)
		continue;
			if (flag & 32) { /* No defect mgt */
				/* search largest format 0x31 */
				if(format_type != 0x31)
		continue;
			} else if(size_mode == 2) { /* max payload size */
				/* search largest 0x30 format descriptor */
				if(format_type != 0x30)
		continue;
			} else if(size_mode == 3) { /* default payload size */
				if (accept_count < 1)
					index = 0; /* this cannot certify */

				/* ts A81129
				   LG GGW-H20L YL03 refuses on 0x30 with 
				   "Quick certification". dvd+rw-format
				   does 0x00 by default and succeeds quickly.
				*/
				if ((flag & 64) && format_type == 0x00) {
					index = i;
		break;
				}

				if(format_type != 0x30)
		continue;
				accept_count++;
				if (accept_count == 1)
					index = i;
		continue;
			} else { /* defect managed format with size wish */

#ifdef Libburn_bd_re_format_olD

				/* search for smallest 0x30 >= size */
				if(format_type != 0x30)
		continue;
				if (i_size < size)
		continue;
				if (format_size >= 0 && i_size >= format_size)
		continue;
				index = i;
				format_size = i_size;
		continue;

#else /* Libburn_bd_re_format_olD */

				/* search largest and smallest 0x30 */
				if(format_type != 0x30)
		continue;
				if (i_size < min_size || min_size < 0)
					min_size = i_size;
				if (i_size > max_size)
					max_size = i_size;

#endif /* ! Libburn_bd_re_format_olD */

			}
			/* common for all cases which search largest
			   descriptors */
			if (i_size > format_size) {
				format_size = i_size;
				index = i;
			}
		}

		if (size_mode == 2 && index < 0 && !(flag & 32))
			index = 0;
		if (index < 0)
			goto no_suitable_formatting_type;
		format_type = d->format_descriptors[index].type;
		if (format_type == 0x30 || format_type == 0x31) {
			if (flag & 64)
				format_sub_type = 3; /* Quick certification */
			else
				format_sub_type = 2; /* Full certification */
		}

#ifdef Libburn_bd_re_format_olD
		if (0) {
#else
		if (size_mode == 0 || size_mode == 1) {
#endif /* ! Libburn_bd_re_format_olD */

			if (min_size < 0 || max_size < 0)
				goto no_suitable_formatting_type;
			if (size <= 0)
				size = min_size;
			if (size % 0x10000)
				size += 0x10000 - (size % 0x10000);
			if (size < min_size)
				goto no_suitable_formatting_type;
			else if(size > max_size)
				goto no_suitable_formatting_type;
			num_of_blocks = size / 2048;
			mmc_int_to_four_char(c.page->data + 4, num_of_blocks);
			for (i = 0; i < 3; i++)
				 c.page->data[9 + i] = 0;
		} else {
			num_of_blocks = 
				d->format_descriptors[index].size / 2048;
			mmc_int_to_four_char(c.page->data + 4, num_of_blocks);
			for (i = 0; i < 3; i++)
				 c.page->data[9 + i] =
					( d->format_descriptors[index].tdp >>
						  (16 - 8 * i)) & 0xff;
		}
		sprintf(descr, "%s", d->current_profile_text);
		return_immediately = 1; /* caller must do the waiting */
		c.page->data[1] |= 0x80;  /* FOV = this flag vector is valid */
		
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
	c.page->data[8] = (format_type << 2) | (format_sub_type & 3);

	sprintf(msg, "Format type %2.2Xh \"%s\", blocks = %.f",
		format_type, descr, (double) num_of_blocks);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00000002,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			msg, 0, 0);
	sprintf(msg, "CDB: ");
	for (i = 0; i < 6; i++)
		sprintf(msg + strlen(msg), "%2.2X ", c.opcode[i]);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00000002,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			msg, 0, 0);
	sprintf(msg, "Format list: ");
	for (i = 0; i < 12; i++)
		sprintf(msg + strlen(msg), "%2.2X ", c.page->data[i]);
	strcat(msg, "\n");
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00000002,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			msg, 0, 0);
	
#ifdef Libburn_do_not_format_dvd_ram_or_bd_rE
	if(d->current_profile == 0x43 || d->current_profile == 0x12) {
		sprintf(msg,
		   "Formatting of %s not implemented yet - This is a dummy",
		   d->current_profile_text);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00000002,
			LIBDAX_MSGS_SEV_WARNING, LIBDAX_MSGS_PRIO_ZERO,
			msg, 0, 0);
		return 1;
	}
#endif /* Libburn_do_not_format_dvd_ram_or_bd_rE */

	d->issue_command(d, &c);
	if (c.error && !tolerate_failure) {
		if (c.sense[2]!=0) {

#ifdef NIX
			sprintf(msg,
		"SCSI error on format_unit(%s): key=%X asc=%2.2Xh ascq=%2.2Xh",
				descr,
				c.sense[2],c.sense[12],c.sense[13]);
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020122,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
#else /* NIX */
			sprintf(msg, "SCSI error on format_unit(%s): ", descr);
			scsi_error_msg(d, c.sense, 14, msg + strlen(msg), 
					&key, &asc, &ascq);

#endif /* !NIX */

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
static int mmc_get_write_performance_al(struct burn_drive *d,
		 int *alloc_len, int *max_descr)
{
	struct buffer buf;
	int len, i, b, num_descr, ret, old_alloc_len;
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

	if (d->current_profile <= 0)
		mmc_get_configuration(d);

	if (*alloc_len < 8)
		return 0;

	scsi_init_command(&c, MMC_GET_PERFORMANCE,
			 sizeof(MMC_GET_PERFORMANCE));
/*
	memcpy(c.opcode, MMC_GET_PERFORMANCE, sizeof(MMC_GET_PERFORMANCE));
	c.oplen = sizeof(MMC_GET_PERFORMANCE);
*/
/*     ts A70519 : now controlled externally
	max_descr = ( BUFFER_SIZE - 8 ) / 16 - 1;
*/

	/* >>> future: maintain a list of write descriptors 
	if (max_descr > d->max_write_descr - d->num_write_descr)
		max_descr = d->max_write_descr;
	*/
	c.dxfer_len = *alloc_len;

	c.opcode[8] = ( *max_descr >> 8 ) & 0xff;
	c.opcode[9] = ( *max_descr >> 0 ) & 0xff;
	c.opcode[10] = 3;
	c.retry = 1;
	c.page = &buf;
	c.page->sectors = 0;
	c.page->bytes = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);

#ifdef Libisofs_simulate_old_mmc1_drivE
	c.error = 1;
	c.sense[2] = 0x5;
	c.sense[12] = 0x20;
	c.sense[13] = 0x0;
#endif /* Libisofs_simulate_old_mmc1_drivE */	

	if (c.error)
		return 0;
        len = mmc_four_char_to_int(c.page->data);
	old_alloc_len = *alloc_len;
        *alloc_len = len + 4;
	if (len + 4 > old_alloc_len)
		len = old_alloc_len - 4;
	num_descr = ( *alloc_len - 8 ) / 16;
	if (*max_descr == 0) {
		*max_descr = num_descr;
		return 1;
	}
	if (old_alloc_len < 16)
		return 1;
	if (len < 12)
		return 0;

	pd = c.page->data;
	if (num_descr > *max_descr)
		num_descr = *max_descr;
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


int mmc_get_write_performance(struct burn_drive *d)
{
	int alloc_len = 8, max_descr = 0, ret;

	if (mmc_function_spy(d, "mmc_get_write_performance") <= 0)
		return 0;

	/* first command execution to learn number of descriptors and 
           dxfer_len */
	ret = mmc_get_write_performance_al(d, &alloc_len, &max_descr);
/*
	fprintf(stderr,"LIBBURN_DEBUG: ACh alloc_len = %d , ret = %d\n",
			alloc_len, ret);
*/
	if (max_descr > 0 && ret > 0)
		/* second execution with announced length */
		ret = mmc_get_write_performance_al(d, &alloc_len, &max_descr);
	return ret; 
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

	if (d->current_profile == 0x13) {
		/* A61229 : DVD-RW restricted overwrite */
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

	} else if ((d->current_profile == 0x14 || d->current_profile == 0x11 ||
			d->current_profile == 0x15)
		&& o->write_type == BURN_WRITE_SAO) {
		/* ts A70205 : DVD-R[W][/DL] : Disc-at-once, DAO */
		/* Learned from dvd+rw-tools and mmc5r03c.pdf .
		   See doc/cookbook.txt for more detailed references. */

		/* BUFE , LS_V = 0, Test Write, Write Type = 2 SAO (DAO) */
		pd[2] = ((!!o->underrun_proof) << 6)
			| ((!!o->simulate) << 4)
			| 2;
		/* No multi-session , FP = 0 , Track Mode = 5 */
		pd[3] = 5;
		/* Data Block Type = 8 */
		pd[4] = 8;

	} else if (d->current_profile == 0x14 || d->current_profile == 0x11 ||
			d->current_profile == 0x15) {
		/* ts A70128 : DVD-R[W][/DL] Incremental Streaming */
		/* Learned from transport.hxx : page05_setup()
		   and mmc5r03c.pdf 7.5, 4.2.3.4 Table 17
		   and spc3r23.pdf 6.8, 7.4.3 */

		/* BUFE , LS_V = 1, Test Write,
		   Write Type = 0 Packet/Incremental */
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

	} else if (d->current_profile == 0x1a || d->current_profile == 0x1b ||
	           d->current_profile == 0x2b || d->current_profile == 0x12 ||
		   d->current_profile == 0x41 || d->current_profile == 0x42 ||
		   d->current_profile == 0x43) {
		/* not with DVD+R[W][/DL] or DVD-RAM or BD-R[E] */;
		return 0;
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


/* A70812 ts */
int mmc_read_10(struct burn_drive *d, int start,int amount, struct buffer *buf)
{
	struct command c;

	if (mmc_function_spy(d, "mmc_read_10") <= 0)
		return -1;
;
	if (amount > BUFFER_SIZE / 2048)
		return -1;

	scsi_init_command(&c, MMC_READ_10, sizeof(MMC_READ_10));
	c.dxfer_len = amount * 2048;
	c.retry = 1;
	mmc_int_to_four_char(c.opcode + 2, start);
	c.opcode[7] = (amount >> 8) & 0xFF;
	c.opcode[8] = amount & 0xFF;
	c.page = buf;
	c.page->bytes = 0;
	c.page->sectors = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);
	if (c.error) {

#ifdef NIX
		char msg[160];

		sprintf(msg,
		"SCSI error on read_10(%d,%d): key=%X asc=%2.2Xh ascq=%2.2Xh",
			start, amount,
			c.sense[2],c.sense[12],c.sense[13]);
#else /* NIX */
		char msg[256];
		int key, asc, ascq;

		sprintf(msg, "SCSI error on read_10(%d,%d): ", start, amount);
		scsi_error_msg(d, c.sense, 14, msg + strlen(msg), 
				&key, &asc, &ascq);

#endif /* !NIX */

		if(!d->silent_on_scsi_error)
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020144,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
		return BE_CANCELLED;
	}

	buf->sectors = amount;
	buf->bytes = amount * 2048;
	return 0;
}


/* ts A81210 : Determine the upper limit of readable data size */
int mmc_read_capacity(struct burn_drive *d)
{
	struct buffer buf;
	struct command c;
	int alloc_len= 8;

	d->media_read_capacity = 0x7fffffff;
	if (mmc_function_spy(d, "mmc_read_capacity") <= 0)
		return 0;

	scsi_init_command(&c, MMC_READ_CAPACITY, sizeof(MMC_READ_CAPACITY));
	c.dxfer_len = alloc_len;
	c.retry = 1;
	c.page = &buf;
	c.page->bytes = 0;
	c.page->sectors = 0;
	c.dir = FROM_DRIVE;
	d->issue_command(d, &c);
	d->media_read_capacity = mmc_four_char_to_int(c.page->data);
	if (d->media_read_capacity < 0) {
		d->media_read_capacity = 0x7fffffff;
		return 0;
	}
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
	d->reserve_track = mmc_reserve_track;
	d->sync_cache = mmc_sync_cache;
	d->get_nwa = mmc_get_nwa;
	d->read_multi_session_c1 = mmc_read_multi_session_c1;
	d->close_disc = mmc_close_disc;
	d->close_session = mmc_close_session;
	d->close_track_session = mmc_close;
	d->read_buffer_capacity = mmc_read_buffer_capacity;
	d->format_unit = mmc_format_unit;
	d->read_format_capacities = mmc_read_format_capacities;
	d->read_10 = mmc_read_10;


	/* ts A70302 */
	d->phys_if_std = -1;
	d->phys_if_name[0] = 0;

	/* ts A61020 */
	d->start_lba = -2000000000;
	d->end_lba = -2000000000;

	/* ts A61201 - A90815*/
	d->erasable = 0;
	d->current_profile = -1;
	d->current_profile_text[0] = 0;
	d->current_is_cd_profile = 0;
	d->current_is_supported_profile = 0;
	d->current_is_guessed_profile = 0;
	memset(d->all_profiles, 0, 256);
	d->num_profiles = 0;
	d->current_has_feat21h = 0;
	d->current_feat21h_link_size = -1;
	d->current_feat23h_byte4 = 0;
	d->current_feat23h_byte8 = 0;
	d->current_feat2fh_byte4 = -1;
	d->needs_close_session = 0;
	d->needs_sync_cache = 0;
	d->bg_format_status = -1;
	d->num_format_descr = 0;
	d->complete_sessions = 0;
	d->state_of_last_session = -1;
	d->last_track_no = 1;
	d->media_capacity_remaining = 0;
	d->media_lba_limit = 0;
	d->media_read_capacity = 0x7fffffff;
	d->pessimistic_buffer_free = 0;
	d->pbf_altered = 0;
	d->wait_for_buffer_free = Libburn_wait_for_buffer_freE;
	d->nominal_write_speed = 0;
	d->pessimistic_writes = 0;
	d->waited_writes = 0;
	d->waited_tries = 0;
	d->waited_usec = 0;
	d->wfb_min_usec = Libburn_wait_for_buffer_min_useC;
	d->wfb_max_usec = Libburn_wait_for_buffer_max_useC;
	d->wfb_timeout_sec = Libburn_wait_for_buffer_tio_seC;
	d->wfb_min_percent = Libburn_wait_for_buffer_min_perC;
	d->wfb_max_percent = Libburn_wait_for_buffer_max_perC;

	return 1;
}


