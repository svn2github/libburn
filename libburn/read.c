/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/* #include <m alloc.h>  ts A61013 : not in Linux man 3 malloc */

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

/* ts A61007 */
/* #include <a ssert.h> */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#include "sector.h"
#include "libburn.h"
#include "drive.h"
#include "transport.h"

/* ts A60925 : obsoleted by libdax_msgs.h
#include "message.h"
*/

#include "crc.h"
#include "debug.h"
#include "init.h"
#include "lec.h"
#include "toc.h"
#include "util.h"
#include "sg.h"
#include "read.h"
#include "options.h"

/* ts A70812 */
#include "error.h"
#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;


void burn_disc_read(struct burn_drive *d, const struct burn_read_opts *o)
{
#if 0
	int i, end, maxsects, finish;
	int seclen;
	int drive_lba;
	unsigned short crc;
	unsigned char fakesub[96];
	struct buffer page;
	int speed;

	/* ts A61007 : if this function gets revived, then these
			tests have to be done more graceful */
	a ssert((o->version & 0xfffff000) == (OPTIONS_VERSION & 0xfffff000));
	a ssert(!d->busy);
	a ssert(d->toc->valid);
	a ssert(o->datafd != -1);

	/* moved up from spc_select_error_params alias d->send_parameters() */
	a ssert(d->mdata->valid);

/* XXX not sure this is a good idea.  copy it? */
/* XXX also, we have duplicated data now, do we remove the fds from struct 
drive, or only store a subset of the _opts structs in drives */

	/* set the speed on the drive */
	speed = o->speed > 0 ? o->speed : d->mdata->max_read_speed;
	d->set_speed(d, speed, 0);

	d->params.retries = o->hardware_error_retries;

	d->send_parameters(d, o);

	d->cancel = 0;
	d->busy = BURN_DRIVE_READING;
	d->currsession = 0;
/*	drive_lba = 232000;
	d->currtrack = 18;
*/
	d->currtrack = 0;
	drive_lba = 0;
/* XXX removal of this line obviously breaks *
   d->track_end = burn_track_end(d, d->currsession, d->currtrack);*/
	printf("track ends at %d\n", d->track_end);
	page.sectors = 0;
	page.bytes = 0;

	if (o->subfd != -1) {
		memset(fakesub, 0xFF, 12);
		memset(fakesub + 12, 0, 84);
		fakesub[13] = 1;
		fakesub[14] = 1;
		fakesub[20] = 2;
		fakesub[12] = (d->toc->toc_entry[0].control << 4) +
			d->toc->toc_entry[0].adr;
		crc = crc_ccitt(fakesub + 12, 10);
		fakesub[22] = crc >> 8;
		fakesub[23] = crc & 0xFF;
		write(o->subfd, fakesub, 96);
	}
	while (1) {
		seclen = burn_sector_length_read(d, o);

		burn_print(12, "received %d blocks\n", page.sectors);
		for (i = 0; i < page.sectors; i++) {
			burn_packet_process(d, page.data + seclen * i, o);
			d->track_end--;
			drive_lba++;
		}

		if ((d->cancel) || (drive_lba == LAST_SESSION_END(d))) {
			burn_print(1, "finished or cancelled\n");
			d->busy = BURN_DRIVE_IDLE;
			if (!d->cancel)
				d->toc->complete = 1;
			return;
		}
/* XXX: removal of this line obviously breaks *
		end = burn_track_end(d, d->currsession, d->currtrack); */

		if (drive_lba == end) {
			d->currtrack++;
			if (d->currtrack >
			    d->toc->session[d->currsession].lasttrack) {
				d->currsession++;
				burn_print(12, "session switch to %d\n",
					   d->currsession);
				burn_print(12, "skipping a lead out\n");
				drive_lba = CURRENT_SESSION_START(d);
				burn_print(12, "new lba %d\n", drive_lba);
/* XXX more of the same
				end = burn_track_end(d, d->currsession,
							d->currtrack);
*/ }
			burn_print(12, "track switch to %d\n", d->currtrack);
		}

		page.sectors = 0;
		page.bytes = 0;

		maxsects = BUFFER_SIZE / seclen;
		finish = end - drive_lba;

		d->track_end = finish;

		page.sectors = (finish < maxsects) ? finish : maxsects;
		printf("reading %d sectors from %d\n", page.sectors,
		       drive_lba);

		/* >>> ts A61009 : ensure page.sectors >= 0 before calling */
		d->r ead_sectors(d, drive_lba, page.sectors, o, &page);

		printf("Read %d\n", page.sectors);
	}
#endif
}
int burn_sector_length_read(struct burn_drive *d,
			    const struct burn_read_opts *o)
{
	int dlen = 2352;
	int data;

/*XXX how do we handle this crap now?*/
/*	data = d->toc->track[d->currtrack].toc_entry->control & 4;*/
	data = 1;
	if (o->report_recovered_errors)
		dlen += 294;
	if ((o->subcodes_data) && data)
		dlen += 96;
	if ((o->subcodes_audio) && !data)
		dlen += 96;
	return dlen;
}

static int bitcount(unsigned char *data, int n)
{
	int i, j, count = 0;
	unsigned char tem;

	for (i = 0; i < n; i++) {
		tem = data[i];
		for (j = 0; j < 8; j++) {
			count += tem & 1;
			tem >>= 1;
		}
	}
	return count;
}

void burn_packet_process(struct burn_drive *d, unsigned char *data,
			 const struct burn_read_opts *o)
{
	unsigned char sub[96];
	unsigned short crc;
	int ptr = 2352, i, j, code, fb;
	int audio = 1;

	if (o->c2errors) {
		fb = bitcount(data + ptr, 294);
		if (fb) {
			burn_print(1, "%d damaged bits\n",
				   bitcount(data + ptr, 294));
			burn_print(1, "sending error on %s %s\n",
				   d->idata->vendor, d->idata->product);
			/* XXX send a burn_message! burn_message_error(d,
			   something); */
		}
		ptr += 294;
	}
/*
	if (d->toc->track[d->currtrack].mode == BURN_MODE_UNINITIALIZED) {
		if ((d->toc->track[d->currtrack].toc_entry->control & 4) == 0)
			d->toc->track[d->currtrack].mode = BURN_MODE_AUDIO;
		else
			switch (data[15]) {
			case 0:
				d->toc->track[d->currtrack].mode = BURN_MODE0;
				break;
			case 1:
				d->toc->track[d->currtrack].mode = BURN_MODE1;
				break;
			case 2:
				d->toc->track[d->currtrack].mode =
					BURN_MODE2_FORMLESS;
				break;
			}
	}
*/
	if ((audio && o->subcodes_audio)
	    || (!audio && o->subcodes_data)) {
		memset(sub, 0, sizeof(sub));
		for (i = 0; i < 12; i++) {
			for (j = 0; j < 8; j++) {
				for (code = 0; code < 8; code++) {
					sub[code * 12 + i] <<= 1;
					if (data[ptr + j + i * 8] &
					    (1 << (7 - code)))
						sub[code * 12 + i]++;
				}
			}
		}
		crc = (*(sub + 22) << 8) + *(sub + 23);
		if (crc != crc_ccitt(sub + 12, 10)) {
			burn_print(1, "sending error on %s %s\n",
				   d->idata->vendor, d->idata->product);
/*			e = burn_error();
			e->drive = d;
*/
			burn_print(1, "crc mismatch in Q\n");
		}
		/* else process_q(d, sub + 12); */
		/* 
		   if (o->subfd != -1) write(o->subfd, sub, 96); */
	}
/*
	if ((d->track_end <= 150)
	    && (drive_lba + 150 < CURRENT_SESSION_END(d))
	    && (TOC_ENTRY(d->toc, d->currtrack).control == 4)
	    && (TOC_ENTRY(d->toc, d->currtrack + 1).control == 0)) {
		burn_print(12, "pregap : %d\n", d->track_end);
		write(o->binfd, zeros, 2352);

#warning XXX WHERE ARE MY SUBCODES
				} else
*//* write(o->datafd, data, 2352); */
}

/*  so yeah, when you uncomment these, make them write zeros insted of crap
static void write_empty_sector(int fd)
{
	char sec[2352];

	burn_print(1, "writing an 'empty' sector\n");
	write(fd, sec, 2352);
}

static void write_empty_subcode(int fd)
{
	char sub[96];

	write(fd, sub, 96);
}

static void flipq(unsigned char *sub)
{
	*(sub + 12 + 10) = ~*(sub + 12 + 10);
	*(sub + 12 + 11) = ~*(sub + 12 + 11);
}
*/


/* ts A70904 */
/** @param flag bit0=be silent on data shortage */
int burn_stdio_read(int fd, char *buf, int bufsize, struct burn_drive *d,
			int flag)
{
	int todo, count = 0;

	for(todo = bufsize; todo > 0; ) {
		count = read(fd, buf + (bufsize - todo), todo);
		if(count <= 0)
	break;
		todo -= count;
	}
	if(todo > 0 && !(flag & 1)) {
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x0002014a,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Cannot read desired amount of data", errno, 0);
	}
	if (count < 0)
		return -1;
	return (bufsize - todo);
}


/* ts A70812 : API function */
int burn_read_data(struct burn_drive *d, off_t byte_address,
                   char data[], off_t data_size, off_t *data_count, int flag)
{
	int alignment = 2048, start, upto, chunksize = 1, err, cpy_size, i;
	int sose_mem = 0, fd = -1, ret;
	char msg[81], *wpt;
	struct buffer buf, *buffer_mem = d->buffer;

/*
#define Libburn_read_data_adr_logginG 1
*/
#ifdef Libburn_read_data_adr_logginG
	static FILE *log_fp= NULL;

	if(log_fp == NULL)
		log_fp = fopen("/tmp/burn_read_data_log", "a");
	if(log_fp!=NULL)
		fprintf(log_fp, "%d\n", (int) (byte_address / 2048));
#endif /* Libburn_read_data_logginG */


	*data_count = 0;
	sose_mem = d->silent_on_scsi_error;

	if (d->released) {
		libdax_msgs_submit(libdax_messenger,
			d->global_index, 0x00020142,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Drive is not grabbed on random access read", 0, 0);
		return 0;
	}
	if (d->drive_role == 0) {
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020146,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Drive is a virtual placeholder (null-drive)", 0, 0);
		return 0;
	} else if (d->drive_role == 3) {
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020151,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Read attempt on write-only drive", 0, 0);
		return 0;
	}
	if ((byte_address % alignment) != 0) {
		sprintf(msg,
			"Read start address not properly aligned (%d bytes)",
			alignment);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020143,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			msg, 0, 0);
		return 0;
	}

	if (d->busy != BURN_DRIVE_IDLE) {
		libdax_msgs_submit(libdax_messenger,
			d->global_index, 0x00020145,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Drive is busy on attempt to read data", 0, 0);
		return 0;
	}

	if (d->drive_role != 1) {

/* <<< We need _LARGEFILE64_SOURCE defined by the build system.
*/
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

		fd = d->stdio_fd;
		if (fd < 0)
			d->stdio_fd = fd =
				open(d->devname, O_RDONLY | O_LARGEFILE);
		if (fd == -1) {
			if (errno != ENOENT  || !(flag & 2))
				libdax_msgs_submit(libdax_messenger,
				  d->global_index,
				  0x00020005,
				  LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			  "Failed to open device (a pseudo-drive) for reading",
				   errno, 0);
			ret = 0; goto ex;
		}
		if (lseek(fd, byte_address, SEEK_SET) == -1) {
			if (!(flag & 2))
				libdax_msgs_submit(libdax_messenger,
				  d->global_index,
				  0x00020147,
				  LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				  "Cannot address start byte", errno, 0);
			ret = 0; goto ex;
		}
	}

	d->busy = BURN_DRIVE_READING_SYNC;
	d->buffer = &buf;

	start = byte_address / 2048;
	upto = start + data_size / 2048;
	if (data_size % 2048)
		upto++;
	wpt = data;
	for (; start < upto; start += chunksize) {
		chunksize = upto - start;
		if (chunksize > 16) {
			chunksize = 16;
			cpy_size = 16 * 2048;
		} else
			cpy_size = data_size - *data_count;
		if (flag & 2)
			d->silent_on_scsi_error = 1;
		if (d->drive_role == 1) {
			err = d->read_10(d, start, chunksize, d->buffer);
		} else {
			ret = burn_stdio_read(fd, (char *) d->buffer->data,
						 cpy_size, d, !!(flag & 2));
			err = 0;
			if (ret <= 0)
				err = BE_CANCELLED;
		}
		if (flag & 2) 
			d->silent_on_scsi_error = sose_mem;
		if (err == BE_CANCELLED) {
			/* Try to read a smaller part of the chunk */
			if(!(flag & 4))
			  for (i = 0; i < chunksize - 1; i++) {
				if (flag & 2)
					d->silent_on_scsi_error = 1;
				if (d->drive_role == 1) {
					err = d->read_10(d, start + i, 1,
							 d->buffer);
				} else {
					ret = burn_stdio_read(fd,
						(char *) d->buffer->data,
						2048, d, 1);
					if (ret <= 0)
						err = BE_CANCELLED;
				}
				if (flag & 2) 
					d->silent_on_scsi_error = sose_mem;
				if (err == BE_CANCELLED)
			break;
				memcpy(wpt, d->buffer->data, 2048);
				wpt += 2048;
				*data_count += 2048;
			}
			if (!(flag & 2))
				libdax_msgs_submit(libdax_messenger,
				  d->global_index,
				  0x00020000,
				  LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH,
				  "burn_read_data() returns 0",
				  0, 0);
			ret = 0; goto ex;
		}
		memcpy(wpt, d->buffer->data, cpy_size);
		wpt += cpy_size;
		*data_count += cpy_size;
	}

	ret = 1;
ex:;
/* <<< let it open until drive is given up or writing shall happen
	if (fd != -1)
		close(fd);
*/
	d->buffer = buffer_mem;
	d->busy = BURN_DRIVE_IDLE;
	return ret;
}
