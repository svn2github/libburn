
/* ts A61008 */
/* #include <a ssert.h> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libburn.h"
#include "structure.h"
#include "write.h"
#include "debug.h"

#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;


/* ts A61008 : replaced Assert by if and return 0 */
/* 	a ssert(!(pos > BURN_POS_END)); */

#define RESIZE(TO, NEW, pos) {\
	void *tmp;\
\
	if (pos > BURN_POS_END)\
		return 0;\
	if (pos == BURN_POS_END)\
		pos = TO->NEW##s;\
	if (pos > TO->NEW##s)\
		return 0;\
\
	tmp = realloc(TO->NEW, sizeof(struct NEW *) * (TO->NEW##s + 1));\
	if (!tmp)\
		return 0;\
	TO->NEW = tmp;\
	memmove(TO->NEW + pos + 1, TO->NEW + pos,\
	        sizeof(struct NEW *) * (TO->NEW##s - pos));\
	TO->NEW##s++;\
}

struct burn_disc *burn_disc_create(void)
{
	struct burn_disc *d;
	d = calloc(1, sizeof(struct burn_disc));
	if (d == NULL) /* ts A70825 */
		return NULL;
	d->refcnt = 1;
	d->sessions = 0;
	d->session = NULL;
	return d;
}

void burn_disc_free(struct burn_disc *d)
{
	d->refcnt--;
	if (d->refcnt == 0) {
		/* dec refs on all elements */
		int i;

		for (i = 0; i < d->sessions; i++)
			burn_session_free(d->session[i]);
		free(d->session);
		free(d);
	}
}

struct burn_session *burn_session_create(void)
{
	struct burn_session *s;
	s = calloc(1, sizeof(struct burn_session));
	if (s == NULL) /* ts A70825 */
		return NULL;
	s->refcnt = 1;
	s->tracks = 0;
	s->track = NULL;
	s->hidefirst = 0;
	return s;
}

void burn_session_hide_first_track(struct burn_session *s, int onoff)
{
	s->hidefirst = onoff;
}

void burn_session_free(struct burn_session *s)
{
	s->refcnt--;
	if (s->refcnt == 0) {
		/* dec refs on all elements */
		int i;

		for (i = 0; i < s->tracks; i++)
			burn_track_free(s->track[i]);
		free(s->track);
		free(s);
	}

}

int burn_disc_add_session(struct burn_disc *d, struct burn_session *s,
			  unsigned int pos)
{
	RESIZE(d, session, pos);
	d->session[pos] = s;
	s->refcnt++;
	return 1;
}


/* ts A81202: this function was in the API but not implemented.
*/
int burn_disc_remove_session(struct burn_disc *d, struct burn_session *s)
{
	int i, skip = 0;

	if (d->session == NULL)
		return 0;
	for (i = 0; i < d->sessions; i++) {
		if (s == d->session[i]) {
			skip++;
	continue;
		}
		d->session[i - skip] = d->session[i];
	}
	if (!skip)
		return 0;
	burn_session_free(s);
	d->sessions--;
	return 1;
}


struct burn_track *burn_track_create(void)
{
	struct burn_track *t;
	t = calloc(1, sizeof(struct burn_track));
	if (t == NULL) /* ts A70825 */
		return NULL;
	t->refcnt = 1;
	t->indices = 0;
	t->offset = 0;
	t->offsetcount = 0;
	t->tail = 0;
	t->tailcount = 0;
	t->mode = BURN_MODE1;
	t->isrc.has_isrc = 0;
	t->pad = 1;

	/* ts A70213 */
	t->fill_up_media = 0;
	/* ts A70218 */
	t->default_size = 0;

	t->entry = NULL;
	t->source = NULL;
	t->eos = 0;

	/* ts A61101 */
	t->sourcecount = 0;
	t->writecount = 0;
	t->written_sectors = 0;

	/* ts A61031 */
	t->open_ended = 0;
	t->track_data_done = 0;

	t->postgap = 0;
	t->pregap1 = 0;
	t->pregap2 = 0;

	/* ts A61024 */
	t->swap_source_bytes = 0;
	return t;
}

void burn_track_free(struct burn_track *t)
{
	t->refcnt--;
	if (t->refcnt == 0) {
		/* dec refs on all elements */
		if (t->source)
			burn_source_free(t->source);
		free(t);
	}
}

int burn_session_add_track(struct burn_session *s, struct burn_track *t,
			   unsigned int pos)
{
	RESIZE(s, track, pos);
	s->track[pos] = t;
	t->refcnt++;
	return 1;
}

int burn_session_remove_track(struct burn_session *s, struct burn_track *t)
{
	struct burn_track **tmp;
	int i, pos = -1;

	/* ts A61008 */
	/* a ssert(s->track != NULL); */
	if (s->track == NULL)
		return 0;

	burn_track_free(t);

	/* Find the position */
	for (i = 0; i < s->tracks; i++) {
		if (t == s->track[i]) {
			pos = i;
			break;
		}
	}

	if (pos == -1)
		return 0;

	/* Is it the last track? */
	if (pos != s->tracks - 1) {
		memmove(&s->track[pos], &s->track[pos + 1],
			sizeof(struct burn_track *) * (s->tracks - (pos + 1)));
	}

	s->tracks--;
	tmp = realloc(s->track, sizeof(struct burn_track *) * s->tracks);
	if (tmp)
		s->track = tmp;
	return 1;
}

void burn_structure_print_disc(struct burn_disc *d)
{
	int i;

	burn_print(12, "This disc has %d sessions\n", d->sessions);
	for (i = 0; i < d->sessions; i++) {
		burn_structure_print_session(d->session[i]);
	}
}
void burn_structure_print_session(struct burn_session *s)
{
	int i;

	burn_print(12, "    Session has %d tracks\n", s->tracks);
	for (i = 0; i < s->tracks; i++) {
		burn_structure_print_track(s->track[i]);
	}
}
void burn_structure_print_track(struct burn_track *t)
{
	burn_print(12, "(%p)  track size %d sectors\n", t,
		   burn_track_get_sectors(t));
}

void burn_track_define_data(struct burn_track *t, int offset, int tail,
			    int pad, int mode)
{
	int type_to_form(int mode, unsigned char *ctladr, int *form);
	int burn_sector_length(int tracktype);
	unsigned char ctladr;
	int form = -1; /* unchanged form will be considered an error too */

	type_to_form(mode, &ctladr, &form);
	if (form == -1 || burn_sector_length(mode) <= 0) {
		char msg[160];

		sprintf(msg, "Attempt to set track mode to unusable value %d",
			mode);
		libdax_msgs_submit(libdax_messenger, -1, 0x00020115,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			msg, 0, 0);
		return;
	}

	t->offset = offset;
	t->pad = pad;
	t->mode = mode;
	t->tail = tail;
}


/* ts A61024 */
int burn_track_set_byte_swap(struct burn_track *t, int swap_source_bytes)
{
	if(swap_source_bytes!=0 && swap_source_bytes!=1)
		return 0;
	t->swap_source_bytes = swap_source_bytes;
	return 1;
}


void burn_track_set_isrc(struct burn_track *t, char *country, char *owner,
			 unsigned char year, unsigned int serial)
{
	int i;

	for (i = 0; i < 2; ++i) {

		/* ts A61008 : This is always true */
		/* a ssert((country[i] >= '0' || country[i] < '9') &&
		       (country[i] >= 'a' || country[i] < 'z') &&
		       (country[i] >= 'A' || country[i] < 'Z')); */
		/* ts A61008 : now coordinated with sector.c: char_to_isrc() */
		if (! ((country[i] >= '0' && country[i] <= '9') ||
		       (country[i] >= 'a' && country[i] <= 'z') ||
		       (country[i] >= 'A' && country[i] <= 'Z')   ) )
			goto is_not_allowed;

		t->isrc.country[i] = country[i];
	}
	for (i = 0; i < 3; ++i) {

		/* ts A61008 : This is always true */
		/* a ssert((owner[i] >= '0' || owner[i] < '9') &&
		       (owner[i] >= 'a' || owner[i] < 'z') &&
		       (owner[i] >= 'A' || owner[i] < 'Z')); */
		/* ts A61008 : now coordinated with sector.c: char_to_isrc() */
		if (! ((owner[i] >= '0' && owner[i] <= '9') ||
		       (owner[i] >= 'a' && owner[i] <= 'z') ||
		       (owner[i] >= 'A' && owner[i] <= 'Z')   ) )
			goto is_not_allowed;

		t->isrc.owner[i] = owner[i];
	}

	/* ts A61008 */
	/* a ssert(year <= 99); */
	if (year > 99)
		goto is_not_allowed;

	t->isrc.year = year;

	/* ts A61008 */
	/* a ssert(serial <= 99999); */
	if (serial > 99999)
		goto is_not_allowed;

	t->isrc.serial = serial;

	/* ts A61008 */
	t->isrc.has_isrc = 1;
	return;
is_not_allowed:;
	libdax_msgs_submit(libdax_messenger, -1, 0x00020114,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Attempt to set ISRC with bad data", 0, 0);
	return;
}

void burn_track_clear_isrc(struct burn_track *t)
{
	t->isrc.has_isrc = 0;
}

int burn_track_get_sectors(struct burn_track *t)
{
	/* ts A70125 : was int */
	off_t size = 0;
	int sectors, seclen;

	seclen = burn_sector_length(t->mode);
	if (t->source != NULL)                /* ts A80808 : mending sigsegv */
		size = t->offset + t->source->get_size(t->source) + t->tail;
	else if(t->entry != NULL) {
		/* ts A80808 : all burn_toc_entry of track starts should now
			have (extensions_valid & 1), even those from CD.
		*/
		if (t->entry->extensions_valid & 1)
			size = ((off_t) t->entry->track_blocks) * (off_t) 2048;
	}
	sectors = size / seclen;
	if (size % seclen)
		sectors++;
	burn_print(1, "%d sectors of %d length\n", sectors, seclen);
	return sectors;
}


/* ts A70125 */
int burn_track_set_sectors(struct burn_track *t, int sectors)
{
	off_t size, seclen;
	int ret;

	seclen = burn_sector_length(t->mode);
	size = seclen * (off_t) sectors - (off_t) t->offset - (off_t) t->tail;
	if (size < 0)
		return 0;
	ret = t->source->set_size(t->source, size);
	t->open_ended = (t->source->get_size(t->source) <= 0);
	return ret;
}


/* ts A70218 , API since A70328 */
int burn_track_set_size(struct burn_track *t, off_t size)
{
	if (t->source == NULL)
		return 0;
	if (t->source->set_size == NULL)
		return 0;
	t->open_ended = (size <= 0);
	return t->source->set_size(t->source, size);
}


/* ts A70213 */
int burn_track_set_fillup(struct burn_track *t, int fill_up_media)
{
	t->fill_up_media = fill_up_media;
	if (fill_up_media)
		t->open_ended = 0;
	return 1;
}


/* ts A70213 */
/**
  @param flag bit0= force new size even if existing track size is larger
*/
int burn_track_apply_fillup(struct burn_track *t, off_t max_size, int flag)
{
	int max_sectors, ret = 2;
	char msg[160];

	if (t->fill_up_media <= 0)
		return 2;
	max_sectors = max_size / 2048;
	if (burn_track_get_sectors(t) < max_sectors || (flag & 1)) {
		sprintf(msg, "Setting total track size to %ds (payload %ds)\n",
		    max_sectors, (int) (t->source->get_size(t->source)/2048));
		libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
			LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_ZERO,
			msg, 0, 0);
		ret = burn_track_set_sectors(t, max_sectors);
		t->open_ended = 0;
	}
	return ret;
}


/* ts A61031 */
int burn_track_is_open_ended(struct burn_track *t)
{
	return !!t->open_ended;
}


/* ts A70218 : API */
int burn_track_set_default_size(struct burn_track *t, off_t size)
{
	t->default_size = size;
	return 1;
}


/* ts A70218 */
off_t burn_track_get_default_size(struct burn_track *t)
{
	return t->default_size;
}


/* ts A61101 : API function */
int burn_track_get_counters(struct burn_track *t, 
                            off_t *read_bytes, off_t *written_bytes)
{
/*
	fprintf(stderr, "libburn_experimental: sizeof(off_t)=%d\n",
		sizeof(off_t));
*/
	*read_bytes = t->sourcecount;
	*written_bytes = t->writecount;
	return 1;
}

/* ts A61031 */
int burn_track_is_data_done(struct burn_track *t)
{
        return !!t->track_data_done;
}

int burn_track_get_shortage(struct burn_track *t)
{
	int size;
	int seclen;

	seclen = burn_sector_length(t->mode);
	size = t->offset + t->source->get_size(t->source) + t->tail;
	if (size % seclen)
		return seclen - size % seclen;
	return 0;
}

int burn_session_get_sectors(struct burn_session *s)
{
	int sectors = 0, i;

	for (i = 0; i < s->tracks; i++)
		sectors += burn_track_get_sectors(s->track[i]);
	return sectors;
}


int burn_disc_get_sectors(struct burn_disc *d)
{
	int sectors = 0, i;

	for (i = 0; i < d->sessions; i++)
		sectors += burn_session_get_sectors(d->session[i]);
	return sectors;
}

void burn_track_get_entry(struct burn_track *t, struct burn_toc_entry *entry)
{
	if (t->entry == NULL)
		memset(entry, 0, sizeof(struct burn_toc_entry));
	else
		memcpy(entry, t->entry, sizeof(struct burn_toc_entry));
}

void burn_session_get_leadout_entry(struct burn_session *s,
				    struct burn_toc_entry *entry)
{
	if (s->leadout_entry == NULL)
		memset(entry, 0, sizeof(struct burn_toc_entry));
	else
		memcpy(entry, s->leadout_entry, sizeof(struct burn_toc_entry));
}

struct burn_session **burn_disc_get_sessions(struct burn_disc *d, int *num)
{
	*num = d->sessions;
	return d->session;
}

struct burn_track **burn_session_get_tracks(struct burn_session *s, int *num)
{
	*num = s->tracks;
	return s->track;
}

int burn_track_get_mode(struct burn_track *track)
{
	return track->mode;
}

int burn_session_get_hidefirst(struct burn_session *session)
{
	return session->hidefirst;
}


/* ts A80808 : Enhance CD toc to DVD toc */
int burn_disc_cd_toc_extensions(struct burn_disc *d, int flag)
{
	int sidx= 0, tidx= 0;
	struct burn_toc_entry *entry, *prev_entry= NULL;
	/* ts A81126 : ticket 146 : There was a SIGSEGV in here */
	char msg_data[321], *msg;

	strcpy(msg_data,
		"Damaged CD table-of-content detected and truncated.");
	strcat(msg_data, " In burn_disc_cd_toc_extensions: ");
        msg = msg_data + strlen(msg_data);
	if (d->session == NULL) {
		strcpy(msg, "d->session == NULL");
		goto failure;
	}
	for (sidx = 0; sidx < d->sessions; sidx++) {
		if (d->session[sidx] == NULL) {
			sprintf(msg, "d->session[%d of %d] == NULL",
				sidx, d->sessions);
			goto failure;
		}
		if (d->session[sidx]->track == NULL) {
			sprintf(msg, "d->session[%d of %d]->track == NULL",
				sidx, d->sessions);
			goto failure;
		}
		if (d->session[sidx]->leadout_entry == NULL) {
			sprintf(msg,
				" Session %d of %d: Leadout entry missing.",
			  	sidx, d->sessions);
			goto failure;
		}
		for (tidx = 0; tidx < d->session[sidx]->tracks + 1; tidx++) {
			if (tidx < d->session[sidx]->tracks) {
				if (d->session[sidx]->track[tidx] == NULL) {
					sprintf(msg,
			  "d->session[%d of %d]->track[%d of %d] == NULL",
			   sidx, d->sessions, tidx,  d->session[sidx]->tracks);
					goto failure;
				}
				entry = d->session[sidx]->track[tidx]->entry;
				if (entry == NULL) {
					sprintf(msg,
			  "session %d of %d, track %d of %d, entry == NULL",
			  			sidx, d->sessions, tidx,
						d->session[sidx]->tracks);
					goto failure;
				}
			} else
				entry = d->session[sidx]->leadout_entry;
			entry->session_msb = 0;
			entry->point_msb = 0;
			entry->start_lba = burn_msf_to_lba(entry->pmin,
						entry->psec, entry->pframe);
			if (tidx > 0) {
				prev_entry->track_blocks =
					entry->start_lba
					- prev_entry->start_lba;
				prev_entry->extensions_valid |= 1;
			}
			if (tidx == d->session[sidx]->tracks) {
				entry->session_msb = 0;
				entry->point_msb = 0;
				entry->track_blocks = 0;
				entry->extensions_valid |= 1;
			}
			prev_entry = entry;
		}
	}
	return 1;
failure:
	libdax_msgs_submit(libdax_messenger, -1, 0x0002015f,
		LIBDAX_MSGS_SEV_MISHAP, LIBDAX_MSGS_PRIO_HIGH, msg_data, 0, 0);
	d->sessions= sidx;
	return 0;
}


