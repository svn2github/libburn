/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#include <unistd.h>
#include <signal.h>

/* ts A61009 */
/* #include <a ssert.h> */


/* ts A61106 : Deliberate defect provocation macros
               DO NOT DEFINE THESE IF YOU WANT SUCCESSFUL TAO !
#define Libburn_experimental_no_close_tracK 1
#define Libburn_experimental_no_close_sessioN 1
*/

/* ts A61114 : Highly experimental : try to achieve SAO on appendables
               THIS DOES NOT WORK YET !
#define Libburn_sao_can_appenD 1
*/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "error.h"
#include "sector.h"
#include "libburn.h"
#include "drive.h"
#include "transport.h"
#include "crc.h"
#include "debug.h"
#include "init.h"
#include "lec.h"
#include "toc.h"
#include "util.h"
#include "sg.h"
#include "write.h"
#include "options.h"
#include "structure.h"

#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;


static int type_to_ctrl(int mode)
{
	int ctrl = 0;

	int data = BURN_MODE2 | BURN_MODE1 | BURN_MODE0;

	if (mode & data) {
		ctrl |= 4;
	} else if (mode & BURN_AUDIO) {
		if (mode & BURN_4CH)
			ctrl |= 8;
		if (mode & BURN_PREEMPHASIS)
			ctrl |= 1;
	} else
		/* ts A61008 */
		/* a ssert(0); */
		return -1;

	if (mode & BURN_COPY)
		ctrl |= 2;

	return ctrl;
}

/* only the ctrl nibble is set here (not adr) */
/* ts A61009 : removed "static" , reacted on type_to_ctrl() == -1
               preserved ignorance towards unknown modes (for now) */
void type_to_form(int mode, unsigned char *ctladr, int *form)
{
	int ret;

	ret = type_to_ctrl(mode) << 4;
	if (ret == -1) {
		*ctladr = 0xff;
		*form = -1;
		return;
	}
	*ctladr = ret;

	if (mode & BURN_AUDIO)
		*form = 0;
	if (mode & BURN_MODE0) {

		/* ts A61009 */
		/* a ssert(0); */
		*form = -1;
		return;
	}

	if (mode & BURN_MODE1)
		*form = 0x10;
	if (mode & BURN_MODE2) {

		/* ts A61009 */
		/* a ssert(0); */ /* XXX someone's gonna want this sometime */
		*form = -1;
		return;
	}

	if (mode & BURN_MODE_RAW)
		*form = 0;
	if (mode & BURN_SUBCODE_P16)	/* must be expanded to R96 */
		*form |= 0x40;
	if (mode & BURN_SUBCODE_P96)
		*form |= 0xC0;
	if (mode & BURN_SUBCODE_R96)
		*form |= 0x40;
}

int burn_write_flush(struct burn_write_opts *o, struct burn_track *track)
{
	struct burn_drive *d = o->drive;

	if (d->buffer->bytes && !d->cancel) {
		int err;
		err = d->write(d, d->nwa, d->buffer);
		if (err == BE_CANCELLED)
			return 0;
		/* A61101 */
		if(track != NULL) {
			track->writecount += d->buffer->bytes;
			track->written_sectors += d->buffer->sectors;
		}
		/* ts A61119 */
		d->progress.buffered_bytes += d->buffer->bytes;

		d->nwa += d->buffer->sectors;
		d->buffer->bytes = 0;
		d->buffer->sectors = 0;
	}
	d->sync_cache(d);
	return 1;
}


/* ts A61030 */
int burn_write_close_track(struct burn_write_opts *o, struct burn_session *s,
				int tnum)
{
	char msg[81];
	struct burn_drive *d;
	struct burn_track *t;
	int todo, step, cancelled, seclen;

	/* ts A61106 */
#ifdef Libburn_experimental_no_close_tracK
	return 1;
#endif

	d = o->drive;
	t = s->track[tnum];

	/* ts A61103 : pad up track to minimum size of 600 sectors */
	if (t->written_sectors < 300) {
		todo = 300 - t->written_sectors;
		sprintf(msg,"Padding up track to minimum size (+ %d sectors)",
			todo);
		libdax_msgs_submit(libdax_messenger, o->drive->global_index,
			0x0002011a,
			LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_HIGH, msg,0,0);
		step = BUFFER_SIZE / 4096; /* shall fit any sector size */
		if (step <= 0)
			step = 1;
		seclen = burn_sector_length(t->mode);
		if (seclen <= 0)
			seclen = 2048;
		memset(d->buffer, 0, sizeof(struct buffer));
		cancelled = d->cancel;
		for (; todo > 0; todo -= step) {
			if (step > todo)
				step = todo;
			d->buffer->bytes = step*seclen;
			d->buffer->sectors = step;
			d->cancel = 0;
			d->write(d, d->nwa, d->buffer);
			d->nwa += d->buffer->sectors;
			t->writecount += d->buffer->bytes;
			t->written_sectors += d->buffer->sectors;
			d->progress.buffered_bytes += d->buffer->bytes;
		}
		d->cancel = cancelled;
	}

	/* ts A61102 */
	d->busy = BURN_DRIVE_CLOSING_TRACK;

	sprintf(msg, "Closing track %2.2d", tnum+1);
	libdax_msgs_submit(libdax_messenger, o->drive->global_index,0x00020119,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH, msg,0,0);

	/* MMC-1 mentions track number 0xFF for "the incomplete track",
	   MMC-3 does not. I tried both. 0xFF was in effect when other
	   bugs finally gave up and made way for readable tracks. */
	/* ts A70129 
	   Probably the right value for appendables is d->last_track_no
	*/
	d->close_track_session(o->drive, 0, 0xff);

	/* ts A61102 */
	d->busy = BURN_DRIVE_WRITING;

	return 1;
}


/* ts A61030 */
int burn_write_close_session(struct burn_write_opts *o, struct burn_session *s)
{

	/* ts A61106 */
#ifdef Libburn_experimental_no_close_sessioN
	return 1;
#endif

	libdax_msgs_submit(libdax_messenger, o->drive->global_index,0x00020119,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH,
			"Closing session", 0, 0);

	/* ts A61102 */
	o->drive->busy = BURN_DRIVE_CLOSING_SESSION;

	o->drive->close_track_session(o->drive, 1, 0);

	/* ts A61102 */
	o->drive->busy = BURN_DRIVE_WRITING;

	return 1;
}


/* ts A60819:
   This is unused since about Feb 2006, icculus.org/burn CVS.
   The compiler complains. We shall please our compiler.
*/
#ifdef Libburn_write_with_function_print_cuE

static void print_cue(struct cue_sheet *sheet)
{
	int i;
	unsigned char *unit;

	printf("\n");
	printf("ctladr|trno|indx|form|scms|  msf\n");
	printf("------+----+----+----+----+--------\n");
	for (i = 0; i < sheet->count; i++) {
		unit = sheet->data + 8 * i;
		printf(" %1X  %1X | %02X | %02X | %02X | %02X |",
		       (unit[0] & 0xf0) >> 4, unit[0] & 0xf, unit[1], unit[2],
		       unit[3], unit[4]);
		printf("%02X:%02X:%02X\n", unit[5], unit[6], unit[7]);
	}
}

#endif /* Libburn_write_with_print_cuE */


/* ts A61009 : changed type from void to int */
/** @return 1 = success , <=0 failure */
static int add_cue(struct cue_sheet *sheet, unsigned char ctladr,
		    unsigned char tno, unsigned char indx,
		    unsigned char form, unsigned char scms, int lba)
{
	unsigned char *unit;
	unsigned char *ptr;
	int m, s, f;

	burn_lba_to_msf(lba, &m, &s, &f);

	sheet->count++;
	ptr = realloc(sheet->data, sheet->count * 8);

	/* ts A61009 */
	/* a ssert(ptr); */
	if (ptr == NULL) {
		libdax_msgs_submit(libdax_messenger, -1, 0x00020111,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
		"Could not allocate new auxiliary object (cue_sheet->data)",
			0, 0);
		return -1;
	}

	sheet->data = ptr;
	unit = sheet->data + (sheet->count - 1) * 8;
	unit[0] = ctladr;
	unit[1] = tno;
	unit[2] = indx;
	unit[3] = form;
	unit[4] = scms;
	unit[5] = m;
	unit[6] = s;
	unit[7] = f;
	return 1;
}

/* ts A61114: added parameter nwa */
struct cue_sheet *burn_create_toc_entries(struct burn_write_opts *o,
					  struct burn_session *session,
					  int nwa)
{
	int i, m, s, f, form, pform, runtime = -150, ret, track_length;
	unsigned char ctladr;
	struct burn_drive *d;
	struct burn_toc_entry *e;
	struct cue_sheet *sheet;
	struct burn_track **tar = session->track;
	int ntr = session->tracks;
	int rem = 0;

	d = o->drive;

#ifdef Libburn_sao_can_appenD
	if (d->status == BURN_DISC_APPENDABLE)
 		runtime = nwa-150;
#endif

	sheet = malloc(sizeof(struct cue_sheet));

	/* ts A61009 : react on failures of malloc(), add_cue_sheet()
	               type_to_form() */
	if (sheet == NULL) {
		libdax_msgs_submit(libdax_messenger, -1, 0x00020111,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Could not allocate new auxiliary object (cue_sheet)",
			0, 0);
		return NULL;
	}

	sheet->data = NULL;
	sheet->count = 0;
	type_to_form(tar[0]->mode, &ctladr, &form);
	if (form == -1) {
		libdax_msgs_submit(libdax_messenger, -1, 0x00020116,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Track mode has unusable value", 0, 0);
		goto failed;
	}
	ret = add_cue(sheet, ctladr | 1, 0, 0, 1, 0, runtime);
	if (ret <= 0)
		goto failed;
	ret = add_cue(sheet, ctladr | 1, 1, 0, form, 0, runtime);
	if (ret <= 0)
		goto failed;
	runtime += 150;

	burn_print(1, "toc for %d tracks:\n", ntr);
	d->toc_entries = ntr + 3;

	/* ts A61009 */
	/* a ssert(d->toc_entry == NULL); */
	if (d->toc_entry != NULL) {

		/* ts A61109 : this happens with appendable CDs 
			>>> Open question: is the existing TOC needed ? */

		/* ts A61109 : for non-SAO, this sheet is thrown away later */
		free((char *) d->toc_entry);

		/*
		libdax_msgs_submit(libdax_messenger,
			d->global_index, 0x00020117,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"toc_entry of drive is already in use", 0, 0);
		goto failed;
		*/
	}

	d->toc_entry = calloc(d->toc_entries, sizeof(struct burn_toc_entry));
	e = d->toc_entry;
	e[0].point = 0xA0;
	if (tar[0]->mode & BURN_AUDIO)
		e[0].control = TOC_CONTROL_AUDIO;
	else
		e[0].control = TOC_CONTROL_DATA;
	e[0].pmin = 1;
	e[0].psec = o->format;
	e[0].adr = 1;
	e[1].point = 0xA1;
	e[1].pmin = ntr;
	e[1].adr = 1;
	if (tar[ntr - 1]->mode & BURN_AUDIO)
		e[1].control = TOC_CONTROL_AUDIO;
	else
		e[1].control = TOC_CONTROL_DATA;
	e[2].point = 0xA2;
	e[2].control = e[1].control;
	e[2].adr = 1;

	/* ts A70121 : The pause before the first track is not a Pre-gap.
	   To count it as part 2 of a Pre-gap is a dirty hack. It also seems
	   to have caused confusion in dealing with part 1 of an eventual
	   real Pre-gap. mmc5r03c.pdf 6.33.3.2, 6.33.3.18 .
	*/
	tar[0]->pregap2 = 1;

	pform = form;
	for (i = 0; i < ntr; i++) {
		type_to_form(tar[i]->mode, &ctladr, &form);


		/* ts A70121 : This seems to be thw wrong test. Correct would
		   be to compare tar[]->mode or bit2 of ctladr.
		*/ 

		if (pform != form) {

			ret = add_cue(sheet, ctladr | 1, i + 1, 0, form, 0,
					 runtime);
			if (ret <= 0)
				goto failed;

			runtime += 150;
/* XXX fix pregap interval 1 for data tracks */
/* ts A60813 silence righteous compiler warning about C++ style comments
   This is possibly not a comment but rather a trace of Derek Foreman
   experiments. Thus not to be beautified - but to be preserved rectified.
/ /                      if (!(form & BURN_AUDIO))
/ /                              tar[i]->pregap1 = 1;
*/
/* ts A70121 : it is unclear why (form & BURN_AUDIO) should prevent pregap1.
   I believe, correct would be:
			runtime += 75;
			tar[i]->pregap1 = 1;

   The test for pform != form is wrong anyway. 

   Next one has to care for Post-gap: table 555 in mmc5r03c.pdf does not
   show any although 6.33.3.19 would prescribe some.

   Nobody seems to have ever tested this situation, up to now.
   It is banned for now in burn_disc_write().
   Warning have been placed in libburn.h .
*/

			tar[i]->pregap2 = 1;
		}
/* XXX HERE IS WHERE WE DO INDICES IN THE CUE SHEET */
/* XXX and we should make sure the gaps conform to ecma-130... */
		tar[i]->entry = &e[3 + i];
		e[3 + i].point = i + 1;
		burn_lba_to_msf(runtime, &m, &s, &f);
		e[3 + i].pmin = m;
		e[3 + i].psec = s;
		e[3 + i].pframe = f;
		e[3 + i].adr = 1;
		e[3 + i].control = type_to_ctrl(tar[i]->mode);
		burn_print(1, "track %d control %d\n", tar[i]->mode,
			   e[3 + i].control);

		ret = add_cue(sheet, ctladr | 1, i + 1, 1, form, 0, runtime);
		if (ret <= 0)
			goto failed;

		/* ts A70125 : 
		   Still not understanding the sense behind linking tracks,
		   i decided to at least enforce the MMC specs' minimum
		   track length.
		*/ 
		track_length = burn_track_get_sectors(tar[i]);
		if (track_length < 300 && !burn_track_is_open_ended(tar[i])) {
			track_length = 300;
			if (!tar[i]->pad)
				tar[i]->pad = 1;
			burn_track_set_sectors(tar[i], track_length);
		}
		runtime += track_length;

/* if we're padding, we'll clear any current shortage.
   if we're not, we'll slip toc entries by a sector every time our
   shortage is more than a sector
XXX this is untested :)
*/
		if (!tar[i]->pad) {
			rem += burn_track_get_shortage(tar[i]);

			/* ts A61101 : I doubt that linking would yield a
					desireable effect. With TAO it is
					counterproductive in any way.
			*/
			if (o->write_type == BURN_WRITE_TAO)
				tar[i]->source->next = NULL;
			else

				if (i +1 != ntr)
					tar[i]->source->next = tar[i+1]->source;
		} else if (rem) {
			rem = 0;
			runtime++;
		}
		if (rem > burn_sector_length(tar[i]->mode)) {
			rem -= burn_sector_length(tar[i]->mode);
			runtime--;
		}
		pform = form;
	}
	burn_lba_to_msf(runtime, &m, &s, &f);
	e[2].pmin = m;
	e[2].psec = s;
	e[2].pframe = f;
	burn_print(1, "run time is %d (%d:%d:%d)\n", runtime, m, s, f);
	for (i = 0; i < d->toc_entries; i++)
		burn_print(1, "point %d (%02d:%02d:%02d)\n",
			   d->toc_entry[i].point, d->toc_entry[i].pmin,
			   d->toc_entry[i].psec, d->toc_entry[i].pframe);
	ret = add_cue(sheet, ctladr | 1, 0xAA, 1, 1, 0, runtime);
	if (ret <= 0)
		goto failed;
	return sheet;

failed:;
	if (sheet != NULL)
		free((char *) sheet);
	return NULL;
}

int burn_sector_length(int tracktype)
{
	if (tracktype & BURN_AUDIO)
		return 2352;
	if (tracktype & BURN_MODE_RAW)
		return 2352;
	if (tracktype & BURN_MODE1)
		return 2048;
	/* ts A61009 */
	/* a ssert(0); */
	return -1;
}

int burn_subcode_length(int tracktype)
{
	if (tracktype & BURN_SUBCODE_P16)
		return 16;
	if ((tracktype & BURN_SUBCODE_P96) || (tracktype & BURN_SUBCODE_R96))
		return 96;
	return 0;
}

int burn_write_leadin(struct burn_write_opts *o,
		       struct burn_session *s, int first)
{
	struct burn_drive *d = o->drive;
	int count;

	d->busy = BURN_DRIVE_WRITING_LEADIN;

	burn_print(5, first ? "    first leadin\n" : "    leadin\n");

	if (first)
		count = 0 - d->alba - 150;
	else
		count = 4500;

	d->progress.start_sector = d->alba;
	d->progress.sectors = count;
	d->progress.sector = 0;

	while (count != 0) {
		if (!sector_toc(o, s->track[0]->mode))
			return 0;
		count--;
		d->progress.sector++;
	}
	d->busy = BURN_DRIVE_WRITING;
	return 1;
}

int burn_write_leadout(struct burn_write_opts *o,
			int first, unsigned char control, int mode)
{
	struct burn_drive *d = o->drive;
	int count;

	d->busy = BURN_DRIVE_WRITING_LEADOUT;

	d->rlba = -150;
	burn_print(5, first ? "    first leadout\n" : "    leadout\n");
	if (first)
		count = 6750;
	else
		count = 2250;
	d->progress.start_sector = d->alba;
	d->progress.sectors = count;
	d->progress.sector = 0;

	while (count != 0) {
		if (!sector_lout(o, control, mode))
			return 0;
		count--;
		d->progress.sector++;
	}
	d->busy = BURN_DRIVE_WRITING;
	return 1;
}

int burn_write_session(struct burn_write_opts *o, struct burn_session *s)
{
	struct burn_drive *d = o->drive;
	struct burn_track *prev = NULL, *next = NULL;
	int i, ret;

	d->rlba = 0;
	burn_print(1, "    writing a session\n");
	for (i = 0; i < s->tracks; i++) {
		if (i > 0)
			prev = s->track[i - 1];
		if (i + 1 < s->tracks)
			next = s->track[i + 1];
		else
			next = NULL;

		if (!burn_write_track(o, s, i))
			{ ret = 0; goto ex; }
	}

	/* ts A61103 */
	ret = 1;
ex:;
	if (o->write_type == BURN_WRITE_TAO)
		burn_write_close_session(o, s);
	return ret;
}


/* ts A61218 : outsourced from burn_write_track() */
int burn_disc_init_track_status(struct burn_write_opts *o,
				struct burn_session *s, int tnum, int sectors)
{
	struct burn_drive *d = o->drive;

	/* Update progress */
	d->progress.start_sector = d->nwa;
	d->progress.sectors = sectors;
	d->progress.sector = 0;

	/* ts A60831: added tnum-line, extended print message on proposal
           by bonfire-app@wanadoo.fr in http://libburn.pykix.org/ticket/58 */
        d->progress.track = tnum;

	/* ts A61102 */
	d->busy = BURN_DRIVE_WRITING;

	return 1;
}


int burn_write_track(struct burn_write_opts *o, struct burn_session *s,
		      int tnum)
{
	struct burn_track *t = s->track[tnum];
	struct burn_drive *d = o->drive;
	int i, tmp = 0, open_ended = 0, ret= 0, nwa, lba;
	int sectors;
	char msg[80];

	d->rlba = -150;

/* XXX for tao, we don't want the pregaps  but still want post? */
	if (o->write_type != BURN_WRITE_TAO) {

		/* ts A61102 */
		d->busy = BURN_DRIVE_WRITING_PREGAP;

		if (t->pregap1)
			d->rlba += 75;
		if (t->pregap2)
			d->rlba += 150;

		if (t->pregap1) {

			struct burn_track *pt;
			/* ts A70121 : Removed pseudo suicidal initializer 
				 = s->track[tnum - 1];
			*/

			if (tnum == 0) {

				/* ts A70121 : This is not possible because
				   track 1 cannot have a pregap at all.
				   MMC-5 6.33.3.2 precribes a mandatory pause
				   prior to any track 1. Pre-gap is prescribed
				   for mode changes like audio-to-data.
				   To set burn_track.pregap1 for track 1 is
				   kindof a dirty hack.
				*/

				printf("first track should not have a pregap1\n");
				pt = t;
			} else
				pt = s->track[tnum - 1]; /* ts A70121 */
			for (i = 0; i < 75; i++)
				if (!sector_pregap(o, t->entry->point,
					           pt->entry->control, pt->mode))
					{ ret = 0; goto ex; }
		}
		if (t->pregap2)
			for (i = 0; i < 150; i++)
				if (!sector_pregap(o, t->entry->point,
					           t->entry->control, t->mode))
					{ ret = 0; goto ex; }
	} else {
		o->control = t->entry->control;
		d->send_write_parameters(d, o);

		/* ts A61103 */
		ret = d->get_nwa(d, -1, &lba, &nwa);

		/* ts A70213: CD-TAO: eventually expand size of track to max */
		burn_track_apply_fillup(t, d->media_capacity_remaining, 0);

		/* <<< */
		sprintf(msg, 
	"TAO pre-track %2.2d : get_nwa(%d)=%d, d=%d , demand=%.f , cap=%.f\n",
	tnum+1, nwa, ret, d->nwa, (double) burn_track_get_sectors(t) * 2048.0,
	(double) d->media_capacity_remaining);
		libdax_msgs_submit(libdax_messenger, d->global_index, 0x000002,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
				msg, 0, 0);
		if (nwa > d->nwa)
			d->nwa = nwa;

	}

/* user data */

	sectors = burn_track_get_sectors(t);
	open_ended = burn_track_is_open_ended(t);

	burn_disc_init_track_status(o, s, tnum, sectors);

        burn_print(12, "track %d is %d sectors long\n", tnum, sectors);

	/* ts A61030 : this cannot happen. tnum is always < s->tracks */
	if (tnum == s->tracks)
		tmp = sectors > 150 ? 150 : sectors;

	for (i = 0; open_ended || i < sectors - tmp; i++) {

		/* ts A61023 : http://libburn.pykix.org/ticket/14
                               From time to time inquire drive buffer */
		if ((i%64)==0)
			d->read_buffer_capacity(d);

		if (!sector_data(o, t, 0))
			{ ret = 0; goto ex; }

		/* ts A61031 */
		if (open_ended) {
			d->progress.sectors = sectors = i;
                        if (burn_track_is_data_done(t)) 
	break;
		}

		/* update current progress */
		d->progress.sector++;
	}
	for (; i < sectors; i++) {

		/* ts A61030: program execution never gets to this point */
		fprintf(stderr,"LIBBURN_DEBUG: TNUM=%d  TRACKS=%d  TMP=%d\n",
			tnum, s->tracks, tmp);

		burn_print(1, "last track, leadout prep\n");

		/* ts A61023 */
		if ((i%64)==0)
			d->read_buffer_capacity(d);

		if (!sector_data(o, t, 1))
			{ ret = 0; goto ex; }

		/* update progress */
		d->progress.sector++;
	}

	if (t->postgap)
		for (i = 0; i < 150; i++)
			if (!sector_postgap(o, t->entry->point, t->entry->control,
				            t->mode))
				{ ret = 0; goto ex; }
	i = t->offset;
	if (o->write_type == BURN_WRITE_SAO) {
		if (d->buffer->bytes) {
			int err;
			err = d->write(d, d->nwa, d->buffer);
			if (err == BE_CANCELLED)
				{ ret = 0; goto ex; }

			/* A61101 : probably this is not all payload data */
			/* A61108 : but audio count is short without this */
			t->writecount += d->buffer->bytes;
			t->written_sectors += d->buffer->sectors;
			d->progress.buffered_bytes += d->buffer->bytes;

			d->nwa += d->buffer->sectors;
			d->buffer->bytes = 0;
			d->buffer->sectors = 0;
		}
	}

	/* ts A61103 */
	ret = 1;
ex:;
	if (o->write_type == BURN_WRITE_TAO) {

		/* ts A61103 */
		/* >>> if cancelled: ensure that at least 600 kB get written */

		if (!burn_write_flush(o, t))
			ret = 0;

		/* ts A61030 */
		if (burn_write_close_track(o, s, tnum) <= 0)
			ret = 0;
	}
	return ret;
}

/* ts A61009 */
/* @param flag bit1 = do not libdax_msgs_submit() */
int burn_disc_write_is_ok(struct burn_write_opts *o, struct burn_disc *disc,
			int flag)
{
	int i, t;
	char msg[80];

	for (i = 0; i < disc->sessions; i++)
		for (t = 0; t < disc->session[i]->tracks; t++)
			if (!sector_headers_is_ok(
					o, disc->session[i]->track[t]->mode))
				goto bad_track_mode_found;
	return 1;
bad_track_mode_found:;
	sprintf(msg, "Unsuitable track mode 0x%x in track %d of session %d",
		disc->session[i]->track[t]->mode, i+1, t+1);
	if (!(flag & 2))
		libdax_msgs_submit(libdax_messenger, -1, 0x0002010a,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
	return 0;
}


/* ts A61218 : outsourced from burn_disc_write_sync() */
int burn_disc_init_write_status(struct burn_write_opts *o,
				struct burn_disc *disc)
{
	struct burn_drive *d = o->drive;
	struct burn_track *t = NULL;
	int sx, tx;

	d->cancel = 0;

	/* init progress before showing the state */
	d->progress.session = 0;
	d->progress.sessions = disc->sessions;
	d->progress.track = 0;
	d->progress.tracks = disc->session[0]->tracks;
	/* TODO: handle indices */
	d->progress.index = 0;
	d->progress.indices = disc->session[0]->track[0]->indices;
	/* TODO: handle multissession discs */
	/* XXX: sectors are only set during write track */
	d->progress.start_sector = 0;
	d->progress.sectors = 0;
	d->progress.sector = 0;
	d->progress.track = 0;

	/* ts A61023 */
	d->progress.buffer_capacity = 0;
	d->progress.buffer_available = 0;
	d->progress.buffered_bytes = 0;
	d->progress.buffer_min_fill = 0xffffffff;

	/* ts A70711 */
	d->pessimistic_buffer_free = 0;
	d->pbf_altered = 0;
	d->pessimistic_writes = 0;
	d->waited_writes = 0;
	d->waited_tries = 0;
	d->waited_usec = 0;

	/* Set eventual media fill up for last track only */
	for (sx = 0; sx < disc->sessions; sx++)
		for (tx = 0 ; tx < disc->session[sx]->tracks; tx++) {
			t = disc->session[sx]->track[tx];
			burn_track_set_fillup(t, 0);
		}
	if (o->fill_up_media && t != NULL)
		burn_track_set_fillup(t, 1);

	d->busy = BURN_DRIVE_WRITING;

	return 1;
}


/* ts A70219 : API */
int burn_precheck_write(struct burn_write_opts *o, struct burn_disc *disc,
				 char reasons[BURN_REASONS_LEN], int silent)
{
	enum burn_write_types wt;
	struct burn_drive *d = o->drive;
	char msg[160], *reason_pt;
	int no_media = 0;

	reason_pt= reasons;
	reasons[0] = 0;

	if (d->drive_role == 0) {
		sprintf(reasons,
			 "DRIVE: is a virtual placeholder (null-drive)");
		no_media = 1;
		goto ex;
	}

	/* check write mode against write job */
	wt = burn_write_opts_auto_write_type(o, disc, reasons, 1);
	if (wt == BURN_WRITE_NONE) {
		if (strncmp(reasons, "MEDIA: ", 7)==0)
			no_media = 1;
		goto ex;
	}

	sprintf(reasons, "%s: ", d->current_profile_text);
	reason_pt= reasons + strlen(reasons);
	if (d->status == BURN_DISC_UNSUITABLE)
		goto unsuitable_profile;
	if (d->drive_role == 2 ||
		d->current_profile == 0x1a || d->current_profile == 0x12) { 
		/* DVD+RW , DVD-RAM , emulated drive on stdio file */
		if (o->start_byte >= 0 && (o->start_byte % 2048))
			strcat(reasons,
			 "write start address not properly aligned to 2048, ");
	} else if (d->current_profile == 0x09 || d->current_profile == 0x0a) {
		/* CD-R , CD-RW */
		if (!burn_disc_write_is_ok(o, disc, (!!silent) << 1))
			strcat(reasons, "unsuitable track mode found, ");
		if (o->start_byte >= 0)
			strcat(reasons, "write start address not supported, ");
	} else if (d->current_profile == 0x13) {
		/* DVD-RW Restricted Overwrite */
		if (o->start_byte >= 0 && (o->start_byte % 32768))
			strcat(reasons,
			  "write start address not properly aligned to 32k, ");
	} else if (d->current_profile == 0x11 || d->current_profile == 0x14 ||
	           d->current_profile == 0x15 ||
	           d->current_profile == 0x1b || d->current_profile == 0x2b ) {
		/* DVD-R* Sequential , DVD+R[/DL] */
		if (o->start_byte >= 0)
			strcat(reasons, "write start address not supported, ");
	} else {
unsuitable_profile:;
		sprintf(msg, "Unsuitable media detected. Profile %4.4Xh  %s",
			d->current_profile, d->current_profile_text);
		if (!silent)
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002011e,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
		strcat(reasons, "no suitable media profile detected, ");
		return 0;
	}
ex:;
	if (reason_pt[0]) {
		if (no_media) {
			if (!silent)
				libdax_msgs_submit(libdax_messenger,
				  d->global_index, 0x0002013a,
				  LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				  "No suitable media detected", 0, 0);
			return -1;
		}
		if (!silent)
			libdax_msgs_submit(libdax_messenger,
				  d->global_index, 0x00020139,
				  LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				  "Write job parameters are unsuitable", 0, 0);
		return 0;
	}
	return 1;
}


/* ts A70129 : learned much from dvd+rw-tools-7.0/growisofs_mmc.cpp */
int burn_disc_open_track_dvd_minus_r(struct burn_write_opts *o,
					struct burn_session *s, int tnum)
{
	struct burn_drive *d = o->drive;
	char msg[160];
	int ret, lba, nwa;
	off_t size;

	d->send_write_parameters(d, o);
	ret = d->get_nwa(d, -1, &lba, &nwa);
	sprintf(msg, 
		"DVD pre-track %2.2d : get_nwa(%d), ret= %d , d->nwa= %d",
		tnum+1, nwa, ret, d->nwa);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x000002,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO, msg,0,0);
	if (nwa > d->nwa)
		d->nwa = nwa;
	/* ts A70214 : eventually adjust already expanded size of track */
	burn_track_apply_fillup(s->track[tnum], d->media_capacity_remaining,1);

	if (o->write_type == BURN_WRITE_SAO) { /* DAO */
 		/* Round track size up to 32 KiB and reserve track */
		size = ((off_t) burn_track_get_sectors(s->track[tnum]))
			 * (off_t) 2048;
		size = (size + (off_t) 0x7fff) & ~((off_t) 0x7fff);
		ret = d->reserve_track(d, size);
		if (ret <= 0) {
			sprintf(msg, "Cannot reserve track of %.f bytes",
				(double) size);
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020138,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
			return 0;
		}
	}
	return 1;
}


/* ts A70226 */
int burn_disc_open_track_dvd_plus_r(struct burn_write_opts *o,
					struct burn_session *s, int tnum)
{
	struct burn_drive *d = o->drive;
	char msg[160];
	int ret, lba, nwa;
	off_t size;

	ret = d->get_nwa(d, -1, &lba, &nwa);
	sprintf(msg, 
		"DVD+R pre-track %2.2d : get_nwa(%d), ret= %d , d->nwa= %d",
		tnum+1, nwa, ret, d->nwa);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x000002,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO, msg,0,0);
	if (nwa > d->nwa)
		d->nwa = nwa;
	/* ts A70214 : eventually adjust already expanded size of track */
	burn_track_apply_fillup(s->track[tnum], d->media_capacity_remaining,1);

	if (o->write_type == BURN_WRITE_SAO &&
	    ! burn_track_is_open_ended(s->track[tnum])) {
 		/* Round track size up to 32 KiB and reserve track */
		size = ((off_t) burn_track_get_sectors(s->track[tnum]))
			 * (off_t) 2048;
		size = (size + (off_t) 0x7fff) & ~((off_t) 0x7fff);
		ret = d->reserve_track(d, size);
		if (ret <= 0) {
			sprintf(msg, "Cannot reserve track of %.f bytes",
				(double) size);
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020138,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
			return 0;
		}
	}
	return 1;
}


/* ts A70129 */
int burn_disc_close_track_dvd_minus_r(struct burn_write_opts *o,
					struct burn_session *s, int tnum)
{
	struct burn_drive *d = o->drive;
	char msg[80];

	/* only with Incremental writing */
	if (o->write_type != BURN_WRITE_TAO)
		return 2;

	sprintf(msg, "Closing track %2.2d  (absolute track number %d)",
		tnum + 1, d->last_track_no);
	libdax_msgs_submit(libdax_messenger, o->drive->global_index,0x00020119,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH, msg,0,0);

	d->busy = BURN_DRIVE_CLOSING_SESSION;
	/* Ignoring tnum here and hoping that d->last_track_no is correct */
	d->close_track_session(d, 0, d->last_track_no); /* CLOSE TRACK, 001b */
	d->busy = BURN_DRIVE_WRITING;
	d->last_track_no++;
	return 1;
}


/* ts A70229 */
int burn_disc_finalize_dvd_plus_r(struct burn_write_opts *o)
{
	struct burn_drive *d = o->drive;

	libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00000002,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			"Finalizing DVD+R ...", 0, 0);

	/* CLOSE SESSION, 101b, Finalize with minimal radius */
	d->close_track_session(d, 2, 1);  /* (2<<1)|1 = 5 */

	libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00000002,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			"... finalizing DVD+R done               ", 0, 0);

	return 1;
}


/* ts A70226 */
int burn_disc_close_track_dvd_plus_r(struct burn_write_opts *o,
			struct burn_session *s, int tnum, int is_last_track)
{
	struct burn_drive *d = o->drive;
	char msg[80];

	sprintf(msg,
		"Closing track %2.2d  (absolute track and session number %d)",
		tnum + 1, d->last_track_no);
	libdax_msgs_submit(libdax_messenger, o->drive->global_index,0x00020119,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH, msg,0,0);

	d->busy = BURN_DRIVE_CLOSING_SESSION;
	d->close_track_session(d, 0, d->last_track_no); /* CLOSE TRACK, 001b */

	/* Each session becomes a single logical track. So to distinguish them,
	   it is mandatory to close the session together with each track. */

	if (is_last_track && !o->multi) 
		burn_disc_finalize_dvd_plus_r(o);
	else
 		d->close_track_session(d, 1, 0); /* CLOSE SESSION, 010b */
	d->busy = BURN_DRIVE_WRITING;
	d->last_track_no++;
	return 1;
}


/* ts A61218 - A70129 */
int burn_dvd_write_track(struct burn_write_opts *o,
			struct burn_session *s, int tnum, int is_last_track)
{
	struct burn_track *t = s->track[tnum];
	struct burn_drive *d = o->drive;
	struct buffer *out = d->buffer;
	int sectors;
	int i, open_ended = 0, ret= 0, is_flushed = 0;

	/* ts A70213 : eventually expand size of track to max */
	burn_track_apply_fillup(t, d->media_capacity_remaining, 0);

	if (d->current_profile == 0x11 || d->current_profile == 0x14 ||
	    d->current_profile == 0x15) {
		/* DVD-R, DVD-RW Sequential, DVD-R/DL Sequential */
		ret = burn_disc_open_track_dvd_minus_r(o, s, tnum);
		if (ret <= 0)
			goto ex;
	} else if (d->current_profile == 0x1b || d->current_profile == 0x2b) {
		/* DVD+R , DVD+R/DL */
		ret = burn_disc_open_track_dvd_plus_r(o, s, tnum);
		if (ret <= 0)
			goto ex;
	}

	sectors = burn_track_get_sectors(t);
	open_ended = burn_track_is_open_ended(t);
	/* <<< */
	{
		char msg[160];

		sprintf(msg,
		 "DVD pre-track %2.2d : demand=%.f%s, cap=%.f\n",
			tnum+1, (double) sectors * 2048.0,
			(open_ended ? " (open ended)" : ""),
			(double) d->media_capacity_remaining);
		libdax_msgs_submit(libdax_messenger, d->global_index, 0x000002,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
				msg, 0, 0);
	}


	/* >>> ts A70215 : what about offset padding ? */

	burn_disc_init_track_status(o, s, tnum, sectors);
	for (i = 0; open_ended || i < sectors; i++) {

		/* From time to time inquire drive buffer */
		if ((i%256)==0)
			d->read_buffer_capacity(d);

		/* transact a (CD sized) sector */
		if (!sector_data(o, t, 0))
			{ ret = 0; goto ex; }

		if (open_ended) {
			d->progress.sectors = sectors = i;
                        if (burn_track_is_data_done(t)) 
	break;
		}

		/* update current progress */
		d->progress.sector++;
	}
	
	/* >>> ts A70215 : what about tail padding ? */

	/* Pad up buffer to next full o->obs (usually 32 kB) */
	if (o->obs_pad && out->bytes > 0 && out->bytes < o->obs) {
		memset(out->data + out->bytes, 0, o->obs - out->bytes);
		out->sectors += (o->obs - out->bytes) / 2048;
		out->bytes = o->obs;
	}
	ret = burn_write_flush(o, t);
	if (ret <= 0)
		goto ex;
	is_flushed = 1;

	/* Eventually finalize track */
	if (d->current_profile == 0x11 || d->current_profile == 0x14 ||
	    d->current_profile == 0x15) {
		/* DVD-R, DVD-RW Sequential, DVD-R/DL Sequential */
		ret = burn_disc_close_track_dvd_minus_r(o, s, tnum);
		if (ret <= 0)
			goto ex;
	} else if (d->current_profile == 0x1b || d->current_profile == 0x2b) {
		/* DVD+R , DVD+R/DL */
		ret = burn_disc_close_track_dvd_plus_r(o, s, tnum,
							 is_last_track);
		if (ret <= 0)
			goto ex;
	}
	ret = 1;
ex:;
	if (!is_flushed)
		d->sync_cache(d); /* burn_write_flush() was not called */
	return ret;
}


/* ts A61219 */
int burn_disc_close_session_dvd_plus_rw(struct burn_write_opts *o,
					struct burn_session *s)
{
	struct burn_drive *d = o->drive;

	d->busy = BURN_DRIVE_CLOSING_SESSION;
	/* This seems to be a quick end : "if (!dvd_compat)" */
	/* >>> Stop de-icing (ongoing background format) quickly
	       by mmc_close() (but with opcode[2]=0).
	       Wait for unit to get ready.
	       return 1;
	*/
	/* Else: end eventual background format in a "DVD-RO" compatible way */
	d->close_track_session(d, 1, 0); /* same as CLOSE SESSION for CD */
	d->busy = BURN_DRIVE_WRITING;
	return 1;
}


/* ts A61228 */
int burn_disc_close_session_dvd_minus_rw(struct burn_write_opts *o,
					struct burn_session *s)
{
	struct burn_drive *d = o->drive;

	d->busy = BURN_DRIVE_CLOSING_SESSION;
	if (d->current_profile == 0x13) {
		d->close_track_session(d, 1, 0); /* CLOSE SESSION, 010b */

		/* ??? under what circumstances to use close functiom 011b 
		       "Finalize disc" ? */

	}
	d->busy = BURN_DRIVE_WRITING;
	return 1;
}


/* ts A70129 : for profile 0x11 DVD-R, 0x14 DVD-RW Seq, 0x15 DVD-R/DL Seq */
int burn_disc_close_session_dvd_minus_r(struct burn_write_opts *o,
					struct burn_session *s)
{
	struct burn_drive *d = o->drive;

	/* only for Incremental writing */
	if (o->write_type != BURN_WRITE_TAO)
		return 2;

#ifdef Libburn_dvd_r_dl_multi_no_close_sessioN
	if (d->current_profile == 0x15 && o->multi)
		return 2;
#endif

	libdax_msgs_submit(libdax_messenger, o->drive->global_index,0x00020119,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH,
			"Closing session", 0, 0);

	d->busy = BURN_DRIVE_CLOSING_SESSION;
	d->close_track_session(d, 1, 0); /* CLOSE SESSION, 010b */
	d->busy = BURN_DRIVE_WRITING;
	return 1;
}


/* ts A61218 */
int burn_dvd_write_session(struct burn_write_opts *o,
				struct burn_session *s, int is_last_session)
{
	int i,ret;
        struct burn_drive *d = o->drive;

	/* >>> open_session ? */

	for (i = 0; i < s->tracks; i++) {
		ret = burn_dvd_write_track(o, s, i,
			is_last_session && i == (s->tracks - 1));
		if (ret <= 0)
	break;
	}
	if (d->current_profile == 0x11 || d->current_profile == 0x14 ||
	    d->current_profile == 0x15) {
		/* DVD-R , DVD-RW Sequential, DVD-R/DL Sequential */
		ret = burn_disc_close_session_dvd_minus_r(o, s);
		if (ret <= 0)
			return 0;
	} else if (d->current_profile == 0x12) {
		/* DVD-RAM */
		/* ??? any finalization needed ? */;
	} else if (d->current_profile == 0x13) {
		/* DVD-RW restricted overwrite */
		if (d->needs_close_session) {
			ret = burn_disc_close_session_dvd_minus_rw(o, s);
			if (ret <= 0)
				return 0;
		}
	} else if (d->current_profile == 0x1a) {
		/* DVD+RW */
		if (d->needs_close_session) {
			ret = burn_disc_close_session_dvd_plus_rw(o, s);
			if (ret <= 0)
				return 0;
		}
	} else if (d->current_profile == 0x1b || d->current_profile == 0x2b) {
		/* DVD+R , DVD+R/DL do each track as an own session */;
	}
	return 1;
}


/* ts A61218 : learned much from dvd+rw-tools-7.0/growisofs_mmc.cpp */
int burn_disc_setup_dvd_plus_rw(struct burn_write_opts *o,
				struct burn_disc *disc)
{
	struct burn_drive *d = o->drive;
	int ret;
	char msg[160];

	if (d->bg_format_status==0 || d->bg_format_status==1) {
		d->busy = BURN_DRIVE_FORMATTING;
		/* start or re-start dvd_plus_rw formatting */
		ret = d->format_unit(d, (off_t) 0, 0);
		if (ret <= 0)
			return 0;
		d->busy = BURN_DRIVE_WRITING;
		d->needs_close_session = 1;
	}
	d->nwa = 0;
	if (o->start_byte >= 0) {
		d->nwa = o->start_byte / 2048;

		sprintf(msg, "Write start address is  %d * 2048", d->nwa);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020127,
				LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
	}

	/* >>> perform OPC if needed */;

	/* >>> ? what else ? */;

	return 1;
}


/* ts A61228 : learned much from dvd+rw-tools-7.0/growisofs_mmc.cpp */
int burn_disc_setup_dvd_minus_rw(struct burn_write_opts *o,
				struct burn_disc *disc)
{
	struct burn_drive *d = o->drive;
	char msg[160];
	int ret;

	d->nwa = 0;
	if (o->start_byte >= 0) {
		d->nwa = o->start_byte / 32768; /* align to 32 kB */

		sprintf(msg, "Write start address is  %d * 32768", d->nwa);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020127,
				LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);

		d->nwa *= 16; /* convert to 2048 block units */
	}


	/* ??? mmc5r03c.pdf 7.5.2 :
	"For DVD-RW media ... If a medium is in Restricted overwrite
	 mode, this mode page shall not be used."

	But growisofs composes a page 5 and sends it.
	mmc5r03c.pdf 5.3.16 , table 127 specifies that mode page 5
	shall be supported with feature 0026h Restricted Overwrite.
	5.3.22 describes a feature 002Ch Rigid Restrictive Overwrite
	which seems to apply to DVD-RW and does not mention page 5.

	5.4.14 finally states that profile 0013h includes feature
	002Ch rather than 0026h.
		
		d->send_write_parameters(d, o);
	*/

	d->busy = BURN_DRIVE_FORMATTING;

	/* "quick grow" to at least byte equivalent of d->nwa */
	ret = d->format_unit(d, (off_t) d->nwa * (off_t) 2048,
			     (d->nwa > 0) << 3);
	if (ret <= 0)
		return 0;

	d->busy = BURN_DRIVE_WRITING;

	/* >>> perform OPC if needed */;

	return 1;
}


/* ts A70129 : for DVD-R[W] Sequential Recoding */
int burn_disc_setup_dvd_minus_r(struct burn_write_opts *o,
				struct burn_disc *disc)
{
	struct burn_drive *d = o->drive;

	/* most setup is in burn_disc_setup_track_dvd_minus_r() */;

	d->nwa = 0;
	return 1;
}


/* ts A70226 : for DVD+R , DVD+R/DL */
int burn_disc_setup_dvd_plus_r(struct burn_write_opts *o,
				struct burn_disc *disc)
{
	struct burn_drive *d = o->drive;

	/* most setup is in burn_disc_setup_track_dvd_plus_r() */;

	d->nwa = 0;
	return 1;
}


/* ts A61218 - A70129 */
int burn_dvd_write_sync(struct burn_write_opts *o,
				 struct burn_disc *disc)
{
	int i, ret, o_end;
	off_t default_size = 0;
	struct burn_drive *d = o->drive;
	struct burn_track *t;
	char msg[160];

	d->needs_close_session = 0;

	if (d->current_profile == 0x1a || d->current_profile == 0x12) { 
		/* DVD+RW , DVD-RAM */
		ret = 1;
		if (d->current_profile == 0x1a)
			ret = burn_disc_setup_dvd_plus_rw(o, disc);
		if (ret <= 0) {
			sprintf(msg,
			  "Write preparation setup failed for DVD+RW");
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020121,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
			goto early_failure;
		}
		o->obs_pad = 0; /* no filling-up of track's last 32k buffer */

	} else if (d->current_profile == 0x13) {
		 /* DVD-RW Restricted Overwrite */
		ret = burn_disc_setup_dvd_minus_rw(o, disc);
		if (ret <= 0) {
			sprintf(msg,
			  "Write preparation setup failed for DVD-RW");
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020121,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
			goto early_failure;
		}

		/* _Rigid_ Restricted Overwrite demands this */
		o->obs_pad = 1; /* fill-up track's last 32k buffer */

	} else if (d->current_profile == 0x11 || d->current_profile == 0x14 ||
			d->current_profile == 0x15) {
		/* DVD-R , DVD-RW Sequential , DVD-R/DL Sequential */
		t = disc->session[0]->track[0];
		o_end = ( burn_track_is_open_ended(t) && !o->fill_up_media );
		default_size = burn_track_get_default_size(t);
		if (o->write_type == BURN_WRITE_SAO && o_end) {
			sprintf(msg, "Activated track default size %.f",
				(double) default_size);
			libdax_msgs_submit(libdax_messenger,
				  d->global_index, 0x0002012e,
				  LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_HIGH,
				  msg, 0, 0);
			burn_track_set_size(t, default_size);
		}
		ret = burn_disc_setup_dvd_minus_r(o, disc);
		if (ret <= 0) {
			sprintf(msg,
			  "Write preparation setup failed for DVD-R[W]");
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020121,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
			goto early_failure;
		}
		/* ??? padding needed ??? cowardly doing it for now */
		o->obs_pad = 1; /* fill-up track's last 32k buffer */
		
	} else if (d->current_profile == 0x1b || d->current_profile == 0x2b) {
		/* DVD+R , DVD+R/DL */
		t = disc->session[0]->track[0];
		o_end = ( burn_track_is_open_ended(t) && !o->fill_up_media );
		default_size = burn_track_get_default_size(t);
		if (o->write_type == BURN_WRITE_SAO && o_end) {
			sprintf(msg, "Activated track default size %.f",
				(double) default_size);
			libdax_msgs_submit(libdax_messenger,
				  d->global_index, 0x0002012e,
				  LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_HIGH,
				  msg, 0, 0);
			burn_track_set_size(t, default_size);
		}
		ret = burn_disc_setup_dvd_plus_r(o, disc);
		if (ret <= 0) {
			sprintf(msg,
			  "Write preparation setup failed for DVD+R");
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00020121,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
			goto early_failure;
		}
		/* ??? padding needed ??? cowardly doing it for now */
		o->obs_pad = 1; /* fill-up track's last 32k buffer */
	}
	o->obs = 32*1024; /* buffer flush trigger for sector.c:get_sector() */

	for (i = 0; i < disc->sessions; i++) {
		/* update progress */
		d->progress.session = i;
		d->progress.tracks = disc->session[i]->tracks;

		ret = burn_dvd_write_session(o, disc->session[i],
					i == (disc->sessions - 1));
		if (ret <= 0)
			goto ex;

		/* XXX: currently signs an end of session */
		d->progress.sector = 0;
		d->progress.start_sector = 0;
		d->progress.sectors = 0;
	}
	ret = 1;
ex:;

	/* >>> eventual emergency finalization measures */

	/* update media state records */
	burn_drive_mark_unready(d);
	burn_drive_inquire_media(d);

	d->busy = BURN_DRIVE_IDLE;
	return ret;
early_failure:;
	return 0;
}


/* ts A70904 */
int burn_stdio_open_write(struct burn_drive *d, off_t start_byte, int flag)
{

/* <<< We need _LARGEFILE64_SOURCE defined by the build system.
*/
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

	int fd = -1;
	int mode = O_RDWR | O_CREAT | O_LARGEFILE;
	char msg[160];

	if (d->devname[0] == 0) /* null drives should not come here */
		return -1;
	fd = open(d->devname, mode, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020005,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Failed to open device (a pseudo-drive)", errno, 0);
		return -1;
	} 
	if (start_byte < 0)
		start_byte = 0;
	if (lseek(fd, start_byte, SEEK_SET)==-1) {
		sprintf(msg, "Cannot address start byte %.f",
			 (double) start_byte);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020147,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			msg, errno, 0);
		close(fd);
		fd = -1;
	}
	d->nwa = start_byte / 2048;
	return fd;
}


/* ts A70904 */
int burn_stdio_read_source(struct burn_source *source, char *buf, int bufsize,
			 	struct burn_write_opts *o, int flag)
{
	int count= 0, todo;

	for(todo = bufsize; todo > 0; todo -= count) {
		count = source->read(source,
			    (unsigned char *) (buf + (bufsize - todo)), todo);
		if (count <= 0)
	break;
	}
	return (bufsize - todo);
}


/* ts A70904 */
int burn_stdio_write(int fd, char *buf, int count, struct burn_drive *d, 
			 int flag)
{
	if (write(fd, buf, count) != count) {
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020148,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Cannot write desired amount of data", errno, 0);
		return 0;
	}
	return count;
}


/* ts A70904 */
int burn_stdio_write_track(struct burn_write_opts *o, struct burn_session *s,
				int tnum, int fd, int flag)
{
	int open_ended, bufsize, ret, eof_seen = 0, sectors;
	struct burn_track *t = s->track[tnum];
	struct burn_drive *d = o->drive;
	off_t t_size, w_count;
	char buf[16*2048];

	bufsize = sizeof(buf);

	sectors = burn_track_get_sectors(t);
	burn_disc_init_track_status(o, s, tnum, sectors);

	/* >>> write t->offset zeros */;

	open_ended = burn_track_is_open_ended(t);
	t_size = t->source->get_size(t->source);
	for(w_count = 0; w_count < t_size || open_ended; w_count += ret) {


		if (t_size - w_count < bufsize && ! open_ended)

			/* >>> what about final sector padding ? */

			bufsize = t_size - w_count;
		if (eof_seen)
			ret = 0;
		else
			ret = burn_stdio_read_source(t->source, buf,
							 bufsize, o, 0);
		if (ret < 0)
			return ret;
		if (ret == 0 && open_ended)
	break;
		if (ret < bufsize && !open_ended) {
			memset(buf + ret, 0, bufsize - ret);
			eof_seen = 1;
			ret = bufsize;
		}
		t->sourcecount += ret;
		if (!o->simulate)
			ret = burn_stdio_write(fd, buf, ret, d, 0);
		if (ret <= 0)
			return ret;

		d->progress.sector = (w_count + (off_t) ret) / (off_t) 2048;
		if (open_ended)
			d->progress.sectors = d->progress.sector;
		t->writecount += ret;
		t->written_sectors = t->writecount / 2048;
	}

	/* >>> write t->tail zeros */;

	return 1;
}


/* ts A70904 */
int burn_stdio_write_sync(struct burn_write_opts *o,
				 struct burn_disc *disc)
{
	int ret, fd = -1;
	struct burn_drive *d = o->drive;

	d->needs_close_session = 0;
	o->obs_pad = 0; /* no filling-up of track's last 32k buffer */
	o->obs = 32*1024; /* buffer size */

	if (disc->sessions != 1)
		{ret= 0 ; goto ex;}
	if (disc->session[0]->tracks != 1)
		{ret= 0 ; goto ex;}
	
	/* update progress */
	d->progress.session = 0;
	d->progress.tracks = 1;

	/* open target file */
	fd = burn_stdio_open_write(d, o->start_byte, 0);
	if (fd == -1)
		{ret = 0; goto ex;}

	ret = burn_stdio_write_track(o, disc->session[0], 0, fd, 0);
	if (ret <= 0)
		goto ex;

	/* XXX: currently signs an end of session */
	d->progress.sector = 0;
	d->progress.start_sector = 0;
	d->progress.sectors = 0;
	ret = 1;
ex:;
	if (fd != -1)
		close (fd);
	/* update media state records */
	burn_drive_mark_unready(d);

	d->busy = BURN_DRIVE_IDLE;
	return ret;
}


void burn_disc_write_sync(struct burn_write_opts *o, struct burn_disc *disc)
{
	struct cue_sheet *sheet;
	struct burn_drive *d = o->drive;
	struct buffer buf;
	struct burn_track *lt, *t;
	int first = 1, i, ret, lba, nwa = 0;
	off_t default_size;
	char msg[80];

/* ts A60924 : libburn/message.c gets obsoleted
	burn_message_clear_queue();
*/

	/* ts A61224 */
	burn_disc_init_write_status(o, disc); /* must be done very early */

	d->buffer = &buf;
	memset(d->buffer, 0, sizeof(struct buffer));
	d->rlba = -150;
	d->toc_temp = 9;

	/* ts A70904 */
	if (d->drive_role != 1) {
		ret = burn_stdio_write_sync(o, disc);
		if (ret <= 0)
			goto fail_wo_sync;
		return;
	}
	/* ts A61218 */
	if (! d->current_is_cd_profile) {
		ret = burn_dvd_write_sync(o, disc);
		if (ret <= 0)
			goto fail_wo_sync;
		return;
	}

	/* ts A70218 */
	if (o->write_type == BURN_WRITE_SAO) {
		for (i = 0 ; i < disc->session[0]->tracks; i++) {
			t = disc->session[0]->track[i];
			if (burn_track_is_open_ended(t)) {
				default_size = burn_track_get_default_size(t);
				sprintf(msg,
					"Activated track default size %.f",
					(double) default_size);
				libdax_msgs_submit(libdax_messenger,
					d->global_index, 0x0002012e,
					LIBDAX_MSGS_SEV_NOTE,
					LIBDAX_MSGS_PRIO_HIGH, msg, 0, 0);
				burn_track_set_size(t, default_size);
			}
		}
	}

	burn_print(1, "sync write of %d CD sessions\n", disc->sessions);

/* Apparently some drives require this command to be sent, and a few drives
return crap.  so we send the command, then ignore the result.
*/
	/* ts A61107 : moved up send_write_parameters because LG GSA-4082B
			 seems to dislike get_nwa() in advance */
	d->alba = d->start_lba; /* ts A61114: this looks senseless */
	d->nwa = d->alba;
	if (o->write_type == BURN_WRITE_TAO) {
		nwa = 0; /* get_nwa() will be called in burn_track() */
	} else {

		d->send_write_parameters(d, o);

		ret = d->get_nwa(d, -1, &lba, &nwa);
		sprintf(msg,
			"SAO|RAW: Inquired nwa: %d , ret= %d , cap=%.f\n",
			nwa, ret, (double) d->media_capacity_remaining);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x00000002,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
				msg,0, 0);

		/* >>> ts A70212 : CD-DAO/SAO : eventually expand size of last track to maximum */;

	}

	for (i = 0; i < disc->sessions; i++) {
		/* update progress */
		d->progress.session = i;
		d->progress.tracks = disc->session[i]->tracks;

		/* ts A61114: added parameter nwa */
		sheet = burn_create_toc_entries(o, disc->session[i], nwa);

		/* ts A61009 */
		if (sheet == NULL)
			goto fail;

/*		print_cue(sheet);*/
		if (o->write_type == BURN_WRITE_SAO)
			d->send_cue_sheet(d, sheet);
		free(sheet);

		if (o->write_type == BURN_WRITE_RAW) {
			if (!burn_write_leadin(o, disc->session[i], first))
				goto fail;
		} else {
			if (first) {

				/* ts A61030 : 0 made the burner take data. */
				/* ts A61103 : Meanwhile d->nwa is updated in
						burn_write_track()  */
				if(o->write_type == BURN_WRITE_TAO) {
					d->nwa= d->alba = 0;
				} else {

#ifdef Libburn_sao_can_appenD
					/* ts A61114: address for d->write() */
					if (d->status == BURN_DISC_APPENDABLE
					  && o->write_type == BURN_WRITE_SAO) {
						d->nwa = d->alba = nwa-150;

						sprintf(msg, 
				"SAO appendable d->nwa= %d\n", d->nwa);
						libdax_msgs_submit(
				libdax_messenger, d->global_index, 0x000002,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
				msg, 0, 0);

					} else {
						d->nwa = -150;
						d->alba = -150;
					}
#else
					d->nwa = -150;
					d->alba = -150;
#endif /* ! Libburn_sao_can_appenD */


				}

			} else {
				d->nwa += 4500;
				d->alba += 4500;
			}
		}
		if (!burn_write_session(o, disc->session[i]))
			goto fail;

		lt = disc->session[i]->track[disc->session[i]->tracks - 1];
		if (o->write_type == BURN_WRITE_RAW) {
			if (!burn_write_leadout(o, first, lt->entry->control,
			                        lt->mode))
				goto fail;
		} else {

			/* ts A61030 */
			if (o->write_type != BURN_WRITE_TAO)

				if (!burn_write_flush(o, NULL))
					goto fail;

			d->nwa += first ? 6750 : 2250;
			d->alba += first ? 6750 : 2250;
		}
		if (first)
			first = 0;

		/* XXX: currently signs an end of session */
		d->progress.sector = 0;
		d->progress.start_sector = 0;
		d->progress.sectors = 0;
	}

	/* ts A61030: extended skipping of flush to TAO: session is closed */
	if (o->write_type != BURN_WRITE_SAO && o->write_type != BURN_WRITE_TAO)
		if (!burn_write_flush(o, NULL))
			goto fail;

	sleep(1);

	/* ts A61125 : update media state records */
	burn_drive_mark_unready(d);
	burn_drive_inquire_media(d);

	burn_print(1, "done\n");
	d->busy = BURN_DRIVE_IDLE;

	/* ts A61012 : This return was traditionally missing. I suspect this
			to have caused Cdrskin_eject() failures */
	return;

fail:
	d->sync_cache(d);
fail_wo_sync:;
	usleep(500001); /* ts A61222: to avoid a warning from remove_worker()*/
	burn_print(1, "done - failed\n");
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x0002010b,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Burn run failed", 0, 0);
	d->cancel = 1;
	d->busy = BURN_DRIVE_IDLE;
}

/* ts A70811 : API function */
int burn_random_access_write(struct burn_drive *d, off_t byte_address,
				char *data, off_t data_count, int flag)
{
	int alignment = 0, start, upto, chunksize, err, fd = -1, ret;
	char msg[81], *rpt;
	struct buffer buf;

	if (d->released) {
		libdax_msgs_submit(libdax_messenger,
			d->global_index, 0x00020142,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Drive is not grabbed on random access write", 0, 0);
		return 0;
	}
	if(d->drive_role == 0) {
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020146,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Drive is a virtual placeholder (null-drive)", 0, 0);
		return 0;
	}

	if(d->drive_role != 1)
		alignment = 2 * 1024;
	if (d->current_profile == 0x12) /* DVD-RAM */
		alignment = 2 * 1024;
        if (d->current_profile == 0x13) /* DVD-RW restricted overwrite */
		alignment = 32 * 1024;
	if (d->current_profile == 0x1a) /* DVD+RW */
		alignment = 2 * 1024;
	if (alignment == 0) {
		sprintf(msg, "Write start address not supported");
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020125,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Write start address not supported", 0, 0);
		return 0;
	}
	if ((byte_address % alignment) != 0) {
		sprintf(msg,
			"Write start address not properly aligned (%d bytes)",
			alignment);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020126,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			msg, 0, 0);
		return 0;
	}
	if ((data_count % alignment) != 0) {
		sprintf(msg,
			"Write data count not properly aligned (%ld bytes)",
			(long) alignment);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020141,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			msg, 0, 0);
		return 0;
	}
	if (d->busy != BURN_DRIVE_IDLE) {
		libdax_msgs_submit(libdax_messenger,
			d->global_index, 0x00020140,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Drive is busy on attempt to write random access",0,0);
		return 0;
	}
	if(d->drive_role != 1) {
		fd = burn_stdio_open_write(d, byte_address, 0);
		if (fd == -1)
			return 0;
	}
	d->busy = BURN_DRIVE_WRITING_SYNC;
	d->buffer = &buf;

	start = byte_address / 2048;
	upto = start + data_count / 2048;
	rpt = data;
	for (; start < upto; start += 16) {
		chunksize = upto - start;
		if (chunksize > 16)
			chunksize = 16;
		d->buffer->bytes = chunksize * 2048;
		memcpy(d->buffer->data, rpt, d->buffer->bytes);
		rpt += d->buffer->bytes;
		d->buffer->sectors = chunksize;
		d->nwa = start;
		if(d->drive_role == 1) {
			err = d->write(d, d->nwa, d->buffer);
		} else {
			ret = burn_stdio_write(fd, (char *) d->buffer->data,
						d->buffer->bytes, d, 0);
			err = 0;
			if (ret <= 0)
				err = BE_CANCELLED;
		}
		if (err == BE_CANCELLED) {
			d->busy = BURN_DRIVE_IDLE;
			if(fd != -1)
				close(fd);
			return (-(start * 2048 - byte_address));
		}
	}

	if(d->drive_role == 1 && (flag & 1))
		d->sync_cache(d);
	if(fd != -1)
		close(fd);
	d->buffer = NULL;
	d->busy = BURN_DRIVE_IDLE;
	return 1;
}

