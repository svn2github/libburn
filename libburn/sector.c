/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#include <stdio.h>

/* ts A61010 */
/* #include <a ssert.h> */

#include <unistd.h>
#include <string.h>
#include "error.h"
#include "options.h"
#include "transport.h"
#include "libburn.h"
#include "drive.h"
#include "sector.h"
#include "crc.h"
#include "debug.h"
#include "lec.h"
#include "toc.h"
#include "write.h"


#ifdef Libburn_log_in_and_out_streaM
/* <<< ts A61031 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif /* Libburn_log_in_and_out_streaM */


/*static unsigned char isrc[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";*/

#define sector_common(X) d->alba++; d->rlba X;

static void uncook_subs(unsigned char *dest, unsigned char *source)
{
	int i, j, code;

	memset(dest, 0, 96);

	for (i = 0; i < 12; i++) {
		for (j = 0; j < 8; j++) {
			for (code = 0; code < 8; code++) {
				if (source[code * 12 + i] & 0x80)
					dest[j + i * 8] |= (1 << (7 - code));
				source[code * 12 + i] <<= 1;
			}
		}
	}
}

/* @return >=0 : valid , <0 invalid */
int sector_get_outmode(enum burn_write_types write_type,
		    enum burn_block_types block_type)
{
	/* ts A61103 : extended SAO condition to TAO */
	if (write_type == BURN_WRITE_SAO || write_type == BURN_WRITE_TAO)
		return 0;
	else
		switch (block_type) {
		case BURN_BLOCK_RAW0:
			return BURN_MODE_RAW;
		case BURN_BLOCK_RAW16:
			return BURN_MODE_RAW | BURN_SUBCODE_P16;
		case BURN_BLOCK_RAW96P:
			return BURN_MODE_RAW | BURN_SUBCODE_P96;
		case BURN_BLOCK_RAW96R:
			return BURN_MODE_RAW | BURN_SUBCODE_R96;
		case BURN_BLOCK_MODE1:
			return BURN_MODE1;
		default:
			return -1;
		}

	/* ts A61007 : now handled in burn_write_opts_set_write_type() */
	/* a ssert(0); */	/* return BURN_MODE_UNIMPLEMENTED :) */
}

/* 0 means "same as inmode" */
static int get_outmode(struct burn_write_opts *o)
{
	/* ts A61007 */
	return sector_get_outmode(o->write_type, o->block_type);

	/* -1 is prevented by check in burn_write_opts_set_write_type() */
	/* a ssert(0); */		/* return BURN_MODE_UNIMPLEMENTED :) */
}


static void get_bytes(struct burn_track *track, int count, unsigned char *data)
{
	int valid, shortage, curr, i, tr;

#ifdef Libburn_log_in_and_out_streaM 
        /* <<< ts A61031 */
        static int tee_fd= -1;
        if(tee_fd==-1)
                tee_fd= open("/tmp/libburn_sg_readin",
                                O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
#endif /* Libburn_log_in_and_out_streaM */


/* no track pointer means we're just generating 0s */
	if (!track) {
		memset(data, 0, count);
		return;
	}

/* first we use up any offset */
	valid = track->offset - track->offsetcount;
	if (valid > count)
		valid = count;

	if (valid) {
		track->offsetcount += valid;
		memset(data, 0, valid);
	}
	shortage = count - valid;

	if (!shortage)
		goto ex;

/* Next we use source data */
	curr = valid;
	if (!track->eos) {
		if (track->source->read != NULL)
			valid = track->source->read(track->source,
						data + curr, count - curr);
		else
			valid = track->source->read_xt(track->source,
						data + curr, count - curr);
	} else valid = 0;

	if (valid <= 0) { /* ts A61031 : extended from (valid == -1) */
		track->eos = 1;
		valid = 0;
	}
	track->sourcecount += valid;

#ifdef Libburn_log_in_and_out_streaM
	/* <<< ts A61031 */
        if(tee_fd!=-1 && valid>0) {
                write(tee_fd, data + curr, valid);
        }
#endif /* Libburn_log_in_and_out_streaM */

	curr += valid;
	shortage = count - curr;

	if (!shortage)
		goto ex;

/* Before going to the next track, we run through any tail */

	valid = track->tail - track->tailcount;
	if (valid > count - curr)
		valid = count - curr;

	if (valid) {
		track->tailcount += valid;
		memset(data + curr, 0, valid);
	}
	curr += valid;
	shortage -= valid;

	if (!shortage)
		goto ex;

	/* ts A61031 */
	if (shortage >= count)
		track->track_data_done = 1;
	if (track->open_ended)
		goto ex;

/* If we're still short, and there's a "next" pointer, we pull from that.
   if that depletes, we'll just fill with 0s.
*/
	if (track->source->next) {
		struct burn_source *src;
		printf("pulling from next track\n");
		src = track->source->next;
		valid = src->read(src, data + curr, shortage);
		if (valid > 0) {
			shortage -= valid;
			curr += valid;
		}
	}
ex:;
	/* ts A61024 : general finalizing processing */ 
	if(shortage)
		memset(data + curr, 0, shortage); /* this is old icculus.org */
	if (track->swap_source_bytes == 1) {
		for (i = 1; i < count; i += 2) {
			tr = data[i];
			data[i] = data[i-1];
			data[i-1] = tr;
		}
	}
}

/* ts A61009 : seems to hand out sector start pointer in opts->drive->buffer
		and to count hand outs as well as reserved bytes */
/* ts A61101 : added parameter track for counting written bytes */
static unsigned char *get_sector(struct burn_write_opts *opts, 
				struct burn_track *track, int inmode)
{
	struct burn_drive *d = opts->drive;
	struct buffer *out = d->buffer;
	int outmode;
	int seclen;
	unsigned char *ret;

	outmode = get_outmode(opts);
	if (outmode == 0)
		outmode = inmode;

	/* ts A61009 : react on eventual failure of burn_sector_length()
			(should not happen if API tested properly).
			Ensures out->bytes >= out->sectors  */
	seclen = burn_sector_length(outmode);
	if (seclen <= 0)
		return NULL;
	seclen += burn_subcode_length(outmode);

	/* ts A61219 : opts->obs is eventually a 32k trigger for DVD */
	if (out->bytes + seclen > BUFFER_SIZE ||
	    (opts->obs > 0 && out->bytes + seclen > opts->obs)) {
		int err;
		err = d->write(d, d->nwa, out);
		if (err == BE_CANCELLED)
			return NULL;

		/* ts A61101 */
		if(track != NULL) {
			track->writecount += out->bytes;
			track->written_sectors += out->sectors;
		}
		/* ts A61119 */
		d->progress.buffered_bytes += out->bytes;

		d->nwa += out->sectors;
		out->bytes = 0;
		out->sectors = 0;
	}

	ret = out->data + out->bytes;
	out->bytes += seclen;
	out->sectors++;

	return ret;
}

/* ts A61031 */
/* Revoke the counting of the most recent sector handed out by get_sector() */
static void unget_sector(struct burn_write_opts *opts, int inmode)
{
	struct burn_drive *d = opts->drive;
	struct buffer *out = d->buffer;
	int outmode;
	int seclen;

	outmode = get_outmode(opts);
	if (outmode == 0)
		outmode = inmode;

	/* ts A61009 : react on eventual failure of burn_sector_length()
			(should not happen if API tested properly).
			Ensures out->bytes >= out->sectors  */
	seclen = burn_sector_length(outmode);
	if (seclen <= 0)
		return;
	seclen += burn_subcode_length(outmode);

	out->bytes -= seclen;
	out->sectors--;
}


/* either inmode == outmode, or outmode == raw.  anything else is bad news */
/* ts A61010 : changed type to int in order to propagate said bad news */
/** @return 1 is ok, <= 0 is failure */
static int convert_data(struct burn_write_opts *o, struct burn_track *track,
			 int inmode, unsigned char *data)
{
	int outlen, inlen;
	int offset = -1;
	int outmode;

	outmode = get_outmode(o);
	if (outmode == 0)
		outmode = inmode;

	outlen = burn_sector_length(outmode);
	inlen = burn_sector_length(inmode);

	/* ts A61010 */
	/* a ssert(outlen >= inlen); */
	if (outlen < inlen)
		return 0;

	if ((outmode & BURN_MODE_BITS) == (inmode & BURN_MODE_BITS)) {
		get_bytes(track, inlen, data);
		return 1;
	}

	/* ts A61010 */
	/* a ssert(outmode & BURN_MODE_RAW); */
	if (!(outmode & BURN_MODE_RAW))
		return 0;

	if (inmode & BURN_MODE1)
		offset = 16;
	if (inmode & BURN_MODE_RAW)
		offset = 0;
	if (inmode & BURN_AUDIO)
		offset = 0;

	/* ts A61010 */
	/* a ssert(offset != -1); */
	if (offset == -1)
		return 0;

	get_bytes(track, inlen, data + offset);
	return 1;
}

static void convert_subs(struct burn_write_opts *o, int inmode,
			 unsigned char *subs, unsigned char *sector)
{
	unsigned char *out;
	int outmode;

	outmode = get_outmode(o);
	if (outmode == 0)
		outmode = inmode;
	sector += burn_sector_length(outmode);
/* XXX for sao with subs, we'd need something else... */

	switch (o->block_type) {
	case BURN_BLOCK_RAW96R:
		uncook_subs(sector, subs);
		break;

	case BURN_BLOCK_RAW16:
		memcpy(sector, subs + 12, 12);
		out = sector + 12;
		out[0] = 0;
		out[1] = 0;
		out[2] = 0;
/*XXX find a better way to deal with partially damaged P channels*/
		if (subs[2] != 0)
			out[3] = 0x80;
		else
			out[3] = 0;
		out = sector + 10;

		out[0] = ~out[0];
		out[1] = ~out[1];
		break;
	/* ts A61119 : to silence compiler warnings */
	default:;
	}
}

static void subcode_toc(struct burn_drive *d, int mode, unsigned char *data)
{
	unsigned char *q;
	int track;
	int crc;
	int min, sec, frame;

	track = d->toc_temp / 3;
	memset(data, 0, 96);
	q = data + 12;

	burn_lba_to_msf(d->rlba, &min, &sec, &frame);
/*XXX track numbers are BCD
a0 - 1st track ctrl
a1 - last track ctrl
a2 - lout ctrl
*/
	q[0] = (d->toc_entry[track].control << 4) + 1;
	q[1] = 0;
	if (d->toc_entry[track].point < 100)
		q[2] = dec_to_bcd(d->toc_entry[track].point);
	else
		q[2] = d->toc_entry[track].point;
	q[3] = dec_to_bcd(min);
	q[4] = dec_to_bcd(sec);
	q[5] = dec_to_bcd(frame);
	q[6] = 0;
	q[7] = dec_to_bcd(d->toc_entry[track].pmin);
	q[8] = dec_to_bcd(d->toc_entry[track].psec);
	q[9] = dec_to_bcd(d->toc_entry[track].pframe);
	crc = crc_ccitt(q, 10);
	q[10] = crc >> 8;
	q[11] = crc & 0xFF;
	d->toc_temp++;
	d->toc_temp %= (d->toc_entries * 3);
}

int sector_toc(struct burn_write_opts *o, int mode)
{
	struct burn_drive *d = o->drive;
	unsigned char *data;
	unsigned char subs[96];

	data = get_sector(o, NULL, mode);
	if (data == NULL)
		return 0;
	/* ts A61010 */
	if (convert_data(o, NULL, mode, data) <= 0)
		return 0;
	subcode_toc(d, mode, subs);
	convert_subs(o, mode, subs, data);
	sector_headers(o, data, mode, 1);
	sector_common(++)
	return 1;
}

int sector_pregap(struct burn_write_opts *o,
		   unsigned char tno, unsigned char control, int mode)
{
	struct burn_drive *d = o->drive;
	unsigned char *data;
	unsigned char subs[96];

	data = get_sector(o, NULL, mode);
	if (data == NULL)
		return 0;
	/* ts A61010 */
	if (convert_data(o, NULL, mode, data) <= 0)
		return 0;
	subcode_user(o, subs, tno, control, 0, NULL, 1);
	convert_subs(o, mode, subs, data);
	sector_headers(o, data, mode, 0);
	sector_common(--)
	return 1;
}

int sector_postgap(struct burn_write_opts *o,
		    unsigned char tno, unsigned char control, int mode)
{
	struct burn_drive *d = o->drive;
	unsigned char subs[96];
	unsigned char *data;

	data = get_sector(o, NULL, mode);
	if (data == NULL)
		return 0;
	/* ts A61010 */
	if (convert_data(o, NULL, mode, data) <= 0)
		return 0;;
/* use last index in track */
	subcode_user(o, subs, tno, control, 1, NULL, 1);
	convert_subs(o, mode, subs, data);
	sector_headers(o, data, mode, 0);
	sector_common(++)
	return 1;
}

static void subcode_lout(struct burn_write_opts *o, unsigned char control,
			 unsigned char *data)
{
	struct burn_drive *d = o->drive;
	unsigned char *q;
	int crc;
	int rmin, min, rsec, sec, rframe, frame;

	memset(data, 0, 96);
	q = data + 12;

	burn_lba_to_msf(d->alba, &min, &sec, &frame);
	burn_lba_to_msf(d->rlba, &rmin, &rsec, &rframe);

	if (((rmin == 0) && (rsec == 0) && (rframe == 0)) ||
	    ((rsec >= 2) && !((rframe / 19) % 2)))
		memset(data, 0xFF, 12);
	q[0] = (control << 4) + 1;
	q[1] = 0xAA;
	q[2] = 0x01;
	q[3] = dec_to_bcd(rmin);
	q[4] = dec_to_bcd(rsec);
	q[5] = dec_to_bcd(rframe);
	q[6] = 0;
	q[7] = dec_to_bcd(min);
	q[8] = dec_to_bcd(sec);
	q[9] = dec_to_bcd(frame);
	crc = crc_ccitt(q, 10);
	q[10] = crc >> 8;
	q[11] = crc & 0xFF;
}

static char char_to_isrc(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'Z')
		return 0x11 + (c - 'A');
	if (c >= 'a' && c <= 'z')
		return 0x11 + (c - 'a');

	/* ts A61008 : obsoleted by test in burn_track_set_isrc() */
	/* a ssert(0); */
	return 0;
}

void subcode_user(struct burn_write_opts *o, unsigned char *subcodes,
		  unsigned char tno, unsigned char control,
		  unsigned char indx, struct isrc *isrc, int psub)
{
	struct burn_drive *d = o->drive;
	unsigned char *p, *q;
	int crc;
	int m, s, f, c, qmode;	/* 1, 2 or 3 */

	memset(subcodes, 0, 96);

	p = subcodes;
	if ((tno == 1) && (d->rlba == -150))
		memset(p, 0xFF, 12);

	if (psub)
		memset(p, 0xFF, 12);
	q = subcodes + 12;

	qmode = 1;
	/* every 1 in 10 we can do something different */
	if (d->rlba % 10 == 0) {
		/* each of these can occur 1 in 100 */
		if ((d->rlba / 10) % 10 == 0) {
			if (o->has_mediacatalog)
				qmode = 2;
		} else if ((d->rlba / 10) % 10 == 1) {
			if (isrc && isrc->has_isrc)
				qmode = 3;
		}
	}

	/* ts A61010 : this cannot happen. Assert for fun ? */
	/* a ssert(qmode == 1 || qmode == 2 || qmode == 3); */

	switch (qmode) {
	case 1:
		q[1] = dec_to_bcd(tno);	/* track number */
		q[2] = dec_to_bcd(indx);	/* index XXX read this shit
						   from the track array */
		burn_lba_to_msf(d->rlba, &m, &s, &f);
		q[3] = dec_to_bcd(m);	/* rel min */
		q[4] = dec_to_bcd(s);	/* rel sec */
		q[5] = dec_to_bcd(f);	/* rel frame */
		q[6] = 0;	/* zero */
		burn_lba_to_msf(d->alba, &m, &s, &f);
		q[7] = dec_to_bcd(m);	/* abs min */
		q[8] = dec_to_bcd(s);	/* abs sec */
		q[9] = dec_to_bcd(f);	/* abs frame */
		break;
	case 2:
		/* media catalog number */
		q[1] = (o->mediacatalog[0] << 4) + o->mediacatalog[1];
		q[2] = (o->mediacatalog[2] << 4) + o->mediacatalog[3];
		q[3] = (o->mediacatalog[4] << 4) + o->mediacatalog[5];
		q[4] = (o->mediacatalog[6] << 4) + o->mediacatalog[7];
		q[5] = (o->mediacatalog[8] << 4) + o->mediacatalog[9];
		q[6] = (o->mediacatalog[10] << 4) + o->mediacatalog[11];
		q[7] = o->mediacatalog[12] << 4;

		q[8] = 0;
		burn_lba_to_msf(d->alba, &m, &s, &f);
		q[9] = dec_to_bcd(f);	/* abs frame */
		break;
	case 3:
		c = char_to_isrc(isrc->country[0]);
		/* top 6 bits of [1] is the first country code */
		q[1] = c << 2;
		c = char_to_isrc(isrc->country[1]);
		/* bottom 2 bits of [1] is part of the second country code */
		q[1] += (c >> 4);
		/* top 4 bits if [2] is the rest of the second country code */
		q[2] = c << 4;

		c = char_to_isrc(isrc->owner[0]);
		/* bottom 4 bits of [2] is part of the first owner code */
		q[2] += (c >> 2);
		/* top 2 bits of [3] is the rest of the first owner code */
		q[3] = c << 6;
		c = char_to_isrc(isrc->owner[1]);
		/* bottom 6 bits of [3] is the entire second owner code */
		q[3] += c;
		c = char_to_isrc(isrc->owner[2]);
		/* top 6 bits of [4] are the third owner code */
		q[4] = c << 2;

		/* [5] is the year in 2 BCD numbers */
		q[5] = dec_to_bcd(isrc->year % 100);
		/* [6] is the first 2 digits in the serial */
		q[6] = dec_to_bcd(isrc->serial % 100);
		/* [7] is the next 2 digits in the serial */
		q[7] = dec_to_bcd((isrc->serial / 100) % 100);
		/* the top 4 bits of [8] is the last serial digit, the rest is 
		   zeros */
		q[8] = dec_to_bcd((isrc->serial / 10000) % 10) << 4;
		burn_lba_to_msf(d->alba, &m, &s, &f);
		q[9] = dec_to_bcd(f);	/* abs frame */
		break;
	}
	q[0] = (control << 4) + qmode;

	crc = crc_ccitt(q, 10);
	q[10] = crc >> 8;
	q[11] = crc & 0xff;
}

int sector_lout(struct burn_write_opts *o, unsigned char control, int mode)
{
	struct burn_drive *d = o->drive;
	unsigned char subs[96];
	unsigned char *data;

	data = get_sector(o, NULL, mode);
	if (!data)
		return 0;
	/* ts A61010 */
	if (convert_data(o, NULL, mode, data) <= 0)
		return 0;
	subcode_lout(o, control, subs);
	convert_subs(o, mode, subs, data);
	sector_headers(o, data, mode, 0);
	sector_common(++)
	return 1;
}

int sector_data(struct burn_write_opts *o, struct burn_track *t, int psub)
{
	struct burn_drive *d = o->drive;
	unsigned char subs[96];
	unsigned char *data;

	data = get_sector(o, t, t->mode);
	if (!data)
		return 0;
	/* ts A61010 */
	if (convert_data(o, t, t->mode, data) <= 0)
		return 0;

	/* ts A61031 */
	if (t->open_ended && t->track_data_done) {
		unget_sector(o, t->mode);
		return 2;
	}

	/* ts A61219 : allow track without .entry */
	if (t->entry == NULL)
		;
	else if (!t->source->read_sub)
		subcode_user(o, subs, t->entry->point,
			     t->entry->control, 1, &t->isrc, psub);
	else if (!t->source->read_sub(t->source, subs, 96))
		subcode_user(o, subs, t->entry->point,
			     t->entry->control, 1, &t->isrc, psub);
	convert_subs(o, t->mode, subs, data);

	sector_headers(o, data, t->mode, 0);
	sector_common(++)
	return 1;
}

int burn_msf_to_lba(int m, int s, int f)
{
	if (m < 90)
		return (m * 60 + s) * 75 + f - 150;
	else
		return (m * 60 + s) * 75 + f - 450150;
}

void burn_lba_to_msf(int lba, int *m, int *s, int *f)
{
	if (lba >= -150) {
		*m = (lba + 150) / (60 * 75);
		*s = (lba + 150 - *m * 60 * 75) / 75;
		*f = lba + 150 - *m * 60 * 75 - *s * 75;
	} else {
		*m = (lba + 450150) / (60 * 75);
		*s = (lba + 450150 - *m * 60 * 75) / 75;
		*f = lba + 450150 - *m * 60 * 75 - *s * 75;
	}
}

int dec_to_bcd(int d)
{
	int top, bottom;

	top = d / 10;
	bottom = d - (top * 10);
	return (top << 4) + bottom;
}

int sector_headers_is_ok(struct burn_write_opts *o, int mode)
{
	if (mode & BURN_AUDIO)	/* no headers for "audio" */
		return 1;
	if (o->write_type == BURN_WRITE_SAO)
		return 1;

	/* ts A61031 */
	if (o->write_type == BURN_WRITE_TAO)
		return 1;

	if (mode & BURN_MODE1)
		return 2;
	return 0;
}

void sector_headers(struct burn_write_opts *o, unsigned char *out,
		    int mode, int leadin)
{
	struct burn_drive *d = o->drive;
	unsigned int crc;
	int min, sec, frame;
	int modebyte = -1;

	/* ts A61009 */
#if 1
	int ret;

	ret = sector_headers_is_ok(o, mode);
	if (ret != 2)
		return;
	modebyte = 1;

#else

	if (mode & BURN_AUDIO)	/* no headers for "audio" */
		return;
	if (o->write_type == BURN_WRITE_SAO)
		return;

	/* ts A61031 */
	if (o->write_type == BURN_WRITE_TAO)
		return;

	if (mode & BURN_MODE1)
		modebyte = 1;

#endif

	/* ts A61009 : now ensured by burn_disc_write_is_ok() */
	/* a ssert(modebyte == 1); */

	out[0] = 0;
	memset(out + 1, 0xFF, 10);	/* sync */
	out[11] = 0;

	if (leadin) {
		burn_lba_to_msf(d->rlba, &min, &sec, &frame);
		out[12] = dec_to_bcd(min) + 0xA0;
		out[13] = dec_to_bcd(sec);
		out[14] = dec_to_bcd(frame);
		out[15] = modebyte;
	} else {
		burn_lba_to_msf(d->alba, &min, &sec, &frame);
		out[12] = dec_to_bcd(min);
		out[13] = dec_to_bcd(sec);
		out[14] = dec_to_bcd(frame);
		out[15] = modebyte;
	}
	if (mode & BURN_MODE1) {
		crc = crc_32(out, 2064);
		out[2064] = crc & 0xFF;
		crc >>= 8;
		out[2065] = crc & 0xFF;
		crc >>= 8;
		out[2066] = crc & 0xFF;
		crc >>= 8;
		out[2067] = crc & 0xFF;
	}
	if (mode & BURN_MODE1) {
		memset(out + 2068, 0, 8);
		parity_p(out);
		parity_q(out);
	}
	scramble(out);
}

#if 0
void process_q(struct burn_drive *d, unsigned char *q)
{
	unsigned char i[5];
	int mode;

	mode = q[0] & 0xF;
/*      burn_print(12, "mode: %d : ", mode);*/
	switch (mode) {
	case 1:
/*              burn_print(12, "tno = %d : ", q[1]);
                burn_print(12, "index = %d\n", q[2]);
*/
		/* q[1] is the track number (starting at 1) q[2] is the index
		   number (starting at 0) */
#warning this is totally bogus
		if (q[1] - 1 > 99)
			break;
		if (q[2] > d->toc->track[q[1] - 1].indices) {
			burn_print(12, "new index at %d\n", d->alba);
			d->toc->track[q[1] - 1].index[q[2]] = d->alba;
			d->toc->track[q[1] - 1].indices++;
		}
		break;
	case 2:
		/* XXX dont ignore these */
		break;
	case 3:
/*              burn_print(12, "ISRC data in mode 3 q\n");*/
		i[0] = isrc[(q[1] << 2) >> 2];
/*              burn_print(12, "0x%x 0x%x 0x%x 0x%x 0x%x\n", q[1], q[2], q[3], q[4], q[5]);
                burn_print(12, "ISRC - %c%c%c%c%c\n", i[0], i[1], i[2], i[3], i[4]);
*/
		break;
	default:

		/* ts A61009 : if reactivated then witout Assert */
		a ssert(0);
	}
}
#endif

/* this needs more info.  subs in the data? control/adr? */

/* ts A61119 : One should not use inofficial compiler extensions.
   >>> Some day this function needs to be implemented. At least for now
       the result does not match the "mode" of cdrecord -toc.
 */
/*
#warning sector_identify needs to be written
*/
int sector_identify(unsigned char *data)
{
	scramble(data);
/*
check mode byte for 1 or 2
test parity to see if it's a valid sector
if invalid, return BURN_MODE_AUDIO;
else return mode byte  (what about mode 2 formless?  heh)
*/
	return BURN_MODE1;
}
