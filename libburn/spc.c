/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/* Copyright (c) 2004 - 2006 Derek Foreman, Ben Jansens
   Copyright (c) 2006 - 2011 Thomas Schmitt <scdbackup@gmx.net>
   Provided under GPL version 2 or later.
*/

/* scsi primary commands */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

/* ts A61008 */
/* #include <a ssert.h> */

#include <stdlib.h>

#include "libburn.h"
#include "transport.h"
#include "spc.h"
#include "mmc.h"
#include "sbc.h"
#include "drive.h"
#include "debug.h"
#include "options.h"
#include "init.h"

#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;

/* ts A91111 :
   whether to log SCSI commands:
   bit0= log in /tmp/libburn_sg_command_log
   bit1= log to stderr
   bit2= flush every line
*/
extern int burn_sg_log_scsi;


/* spc command set */
/* ts A70519 : allocation length byte 3+4 was 0,255 */
static unsigned char SPC_INQUIRY[] = { 0x12, 0, 0, 0, 36, 0 };

/*static char SPC_TEST[]={0,0,0,0,0,0};*/
static unsigned char SPC_PREVENT[] = { 0x1e, 0, 0, 0, 1, 0 };
static unsigned char SPC_ALLOW[] = { 0x1e, 0, 0, 0, 0, 0 };
static unsigned char SPC_MODE_SENSE[] = { 0x5a, 0, 0, 0, 0, 0, 0, 16, 0, 0 };
static unsigned char SPC_MODE_SELECT[] = { 0x55, 16, 0, 0, 0, 0, 0, 0, 0, 0 };
static unsigned char SPC_REQUEST_SENSE[] = { 0x03, 0, 0, 0, 18, 0 };
static unsigned char SPC_TEST_UNIT_READY[] = { 0x00, 0, 0, 0, 0, 0 };


/* ts A70519 : An initializer for the abstract SCSI command structure */
int scsi_init_command(struct command *c, unsigned char *opcode, int oplen)
{
	if (oplen > 16)
		return 0;
	memset(c, 0, sizeof(struct command));
	memcpy(c->opcode, opcode, oplen);
	c->oplen = oplen;
	c->dir = NO_TRANSFER;
	c->dxfer_len = -1;
	memset(c->sense, 0, sizeof(c->sense));
	c->error = 0;
	c->retry = 0;
	c->page = NULL;
	return 1;
}


/* ts B00728 */
int spc_decode_sense(unsigned char *sense, int senselen,
                     int *key, int *asc, int *ascq)
{
	*key = *asc = *ascq = 0;
	if ((sense[0] & 0x7f) == 0x72 || (sense[0] & 0x7f) == 0x73) {
		if (senselen <= 0 || senselen > 1)
			*key = sense[1] & 0x0f;
		if (senselen <= 0 || senselen > 2)
			*asc = sense[2];
		if (senselen <= 0 || senselen > 3)
			*ascq = sense[3];
		return 1;
	}
	if (senselen <= 0 || senselen > 2)
		*key = sense[2] & 0x0f;
	if (senselen <= 0 || senselen > 12)
		*asc = sense[12];
	if (senselen <= 0 || senselen > 13)
		*ascq = sense[13];
	return 1;
}


int spc_test_unit_ready_r(struct burn_drive *d, int *key, int *asc, int *ascq)
{
	struct command *c;

 	c = &(d->casual_command);
	if (mmc_function_spy(d, "test_unit_ready") <= 0)
		return 0;

	scsi_init_command(c, SPC_TEST_UNIT_READY,sizeof(SPC_TEST_UNIT_READY));
	c->retry = 0;
	c->dir = NO_TRANSFER;
	d->issue_command(d, c);
	*key = *asc = *ascq = 0;
	if (c->error) {
		spc_decode_sense(c->sense, 0, key, asc, ascq);
		return (key == 0);
	}
	return 1;
}


int spc_test_unit_ready(struct burn_drive *d)
{
	int key,asc,ascq;

	return spc_test_unit_ready_r(d, &key, &asc, &ascq);
}


/* ts A70315 */
/** @param flag bit0=do not wait 0.1 seconds before first test unit ready
                bit1=do not issue success message
 */
/** Wait until the drive state becomes clear or until max_usec elapsed */
int spc_wait_unit_attention(struct burn_drive *d, int max_sec, char *cmd_text,
				int flag)
{
	int i, ret = 1, key = 0, asc = 0, ascq = 0, clueless_start = 0;
	static double tests_per_second = 2.0;
	int sleep_usecs, loop_limit, clueless_timeout;
	char *msg = NULL;
	unsigned char sense[14];

	BURN_ALLOC_MEM(msg, char, 320);
	clueless_timeout = 5 * tests_per_second + 1;
	loop_limit = max_sec * tests_per_second + 1;
	sleep_usecs = 1000000 / tests_per_second;

	if (!(flag & 1))
		usleep(sleep_usecs);

	for(i = !(flag & 1); i < loop_limit; i++) {
		ret = spc_test_unit_ready_r(d, &key, &asc, &ascq);
		if (ret > 0) /* ready */
	break;
		if (key!=0x2 || asc!=0x4) {
			if (key == 0x2 && asc == 0x3A) {
				ret = 1; /* medium not present = ok */
/* <<<
  ts A70912 :
  My LG GSA-4082B on asynchronous load:
    first it reports no media 2,3A,00,
    then it reports not ready 2,04,00,
    further media inquiry retrieves wrong data

				if(i<=100)
					goto slumber;
*/
	break;
			}
			if (key == 0x6 && asc == 0x28 && ascq == 0x00)
				/* media change notice = try again */
				goto slumber;

handle_error:;
			/* ts A90213 */
			sprintf(msg,
				"Asynchronous SCSI error on %s: ", cmd_text);
			sense[0] = 0x70; /* Fixed format sense data */
			sense[2] = key;
			sense[12] = asc;
			sense[13] = ascq;
			scsi_error_msg(d, sense, 14, msg + strlen(msg),
						 &key, &asc, &ascq);
			libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002014d,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
			d->cancel = 1;
	break;
		} else if (ascq == 0x00) { /* CAUSE NOT REPORTABLE */
			/* Might be a clueless system adapter */
			if (clueless_start == 0)
				clueless_start = i;
			if (i - clueless_start > clueless_timeout) {
				libdax_msgs_submit(libdax_messenger,
				  d->global_index,
				  0x00000002,
				  LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH,
				  "Ended clueless NOT READY cycle",
				  0, 0);
				ret = 1; /* medium not present = ok */
	break;
			}
		} else if (ascq == 0x02 || ascq == 0x03) 
			goto handle_error;

slumber:;
		usleep(sleep_usecs);
	}
	if (ret <= 0 || !(flag & 2)) {
		sprintf(msg, "Async %s %s after %d.%d seconds",
			cmd_text, (ret > 0 ? "succeeded" : "failed"),
			i / 10, i % 10);
		libdax_msgs_submit(libdax_messenger, d->global_index,
			 0x00020150, LIBDAX_MSGS_SEV_DEBUG,
			 LIBDAX_MSGS_PRIO_LOW, msg, 0, 0);
	}

	if (i < max_sec * 10)
		{ret = (ret > 0); goto ex;}

	sprintf(msg, "Timeout (%d s) with asynchronous SCSI command %s\n",
	 	max_sec, cmd_text);
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x0002014f,
		LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH, msg, 0, 0);
	ret = 0;
ex:;
	BURN_FREE_MEM(msg);
	return ret;
}


void spc_request_sense(struct burn_drive *d, struct buffer *buf)
{
	struct command *c;

	c = &(d->casual_command);
	if (mmc_function_spy(d, "request_sense") <= 0)
		return;

	scsi_init_command(c, SPC_REQUEST_SENSE, sizeof(SPC_REQUEST_SENSE));
	c->retry = 0;
	c->dxfer_len= c->opcode[4];
	c->retry = 0;
	c->page = buf;
	c->page->sectors = 0;
	c->page->bytes = 0;
	c->dir = FROM_DRIVE;
	d->issue_command(d, c);
}

int spc_get_erase_progress(struct burn_drive *d)
{
	struct buffer *b = NULL;
	int ret;

	if (mmc_function_spy(d, "get_erase_progress") <= 0)
		{ret = 0; goto ex;}

	BURN_ALLOC_MEM(b, struct buffer, 1);
	spc_request_sense(d, b);
	ret = (b->data[16] << 8) | b->data[17];
ex:;
	BURN_FREE_MEM(b);
	return ret;
}

void spc_inquiry(struct burn_drive *d)
{
	struct buffer *buf = NULL;
	struct burn_scsi_inquiry_data *id;
	struct command *c = NULL;

	if (mmc_function_spy(d, "inquiry") <= 0)
		return;

	BURN_ALLOC_MEM_VOID(buf, struct buffer, 1);
	BURN_ALLOC_MEM_VOID(c, struct command, 1);
	scsi_init_command(c, SPC_INQUIRY, sizeof(SPC_INQUIRY));
	c->dxfer_len = (c->opcode[3] << 8) | c->opcode[4];
	c->retry = 1;
	c->page = buf;
	c->page->bytes = 0;
	c->page->sectors = 0;
	c->dir = FROM_DRIVE;
	d->issue_command(d, c);
	id = (struct burn_scsi_inquiry_data *)d->idata;
	memset(id->vendor, 0, 9);
	memset(id->product, 0, 17);
	memset(id->revision, 0, 5);
	if (c->error) {
		id->valid = -1;
		goto ex;
	}
	memcpy(id->vendor, c->page->data + 8, 8);
	memcpy(id->product, c->page->data + 16, 16);
	memcpy(id->revision, c->page->data + 32, 4);
	id->valid = 1;
ex:;
	BURN_FREE_MEM(buf);
	BURN_FREE_MEM(c);
	return;
}

void spc_prevent(struct burn_drive *d)
{
	struct command *c;

	c = &(d->casual_command);
	if (mmc_function_spy(d, "prevent") <= 0)
		return;

	scsi_init_command(c, SPC_PREVENT, sizeof(SPC_PREVENT));
	c->retry = 1;
	c->dir = NO_TRANSFER;
	d->issue_command(d, c);
	
#ifdef Libburn_pioneer_dvr_216d_get_evenT
        mmc_get_event(d);
#endif

}

void spc_allow(struct burn_drive *d)
{
	struct command *c;

	c = &(d->casual_command);
	if (mmc_function_spy(d, "allow") <= 0)
		return;

	scsi_init_command(c, SPC_ALLOW, sizeof(SPC_ALLOW));
	c->retry = 1;
	c->dir = NO_TRANSFER;
	d->issue_command(d, c);
}

/*
ts A70518 - A90603 : Do not call with *alloc_len < 10
*/
/** flag&1= do only inquire alloc_len */
static int spc_sense_caps_al(struct burn_drive *d, int *alloc_len, int flag)
{
	struct buffer *buf = NULL;
	struct scsi_mode_data *m;
	int page_length, num_write_speeds = 0, i, speed, ret;
	int old_alloc_len, was_error = 0;
	unsigned char *page;
	struct command *c = NULL;
	struct burn_speed_descriptor *sd;
	char *msg = NULL;

	/* ts A61225 : 1 = report about post-MMC-1 speed descriptors */
	static int speed_debug = 0;

	if (*alloc_len < 10)
		{ret = 0; goto ex;}

	BURN_ALLOC_MEM(msg, char, BURN_DRIVE_ADR_LEN + 160);
	BURN_ALLOC_MEM(buf, struct buffer, 1);
	BURN_ALLOC_MEM(c, struct command, 1);

	/* ts A90602 : Clearing mdata before command execution */
	m = d->mdata;
	m->valid = 0;
	burn_mdata_free_subs(m);

	memset(buf, 0, sizeof(struct buffer));
	scsi_init_command(c, SPC_MODE_SENSE, sizeof(SPC_MODE_SENSE));
/*
	memcpy(c->opcode, SPC_MODE_SENSE, sizeof(SPC_MODE_SENSE));
	c->oplen = sizeof(SPC_MODE_SENSE);
*/
	c->dxfer_len = *alloc_len;
	c->opcode[7] = (c->dxfer_len >> 8) & 0xff;
	c->opcode[8] = c->dxfer_len & 0xff;
	c->retry = 1;
	c->opcode[2] = 0x2A;
	c->page = buf;
	c->page->bytes = 0;
	c->page->sectors = 0;
	c->dir = FROM_DRIVE;
	d->issue_command(d, c);
	if (c->error) {
		memset(buf, 0, sizeof(struct buffer));
		m->valid = -1;
		was_error = 1;
	}

	page = c->page->data + 8;

	/* ts A61225 :
	   Although MODE SENSE indeed belongs to SPC, the returned code page
	   2Ah is part of MMC-1 to MMC-3. In MMC-1 5.2.3.4. it has 22 bytes,
	   in MMC-3 6.3.11 there are at least 28 bytes plus a variable length
	   set of speed descriptors. In MMC-5 E.11 it is declared "legacy".
	*/
	/* ts A90603 :
	   SPC-1 8.3.3 enumerates mode page format bytes from 0 to n and
	   defines Page Length as (n-1).
	*/
	page_length = page[1];
	old_alloc_len = *alloc_len;
	*alloc_len = page_length + 10;
	if (flag & 1)
		{ret = !was_error; goto ex;}
	if (page_length + 10 > old_alloc_len)
		page_length = old_alloc_len - 10;

	/* ts A90602 : 20 asserts page[21]. (see SPC-1 8.3.3) */
	if (page_length < 20) {
		m->valid = -1;
		sprintf(msg, "MODE SENSE page 2A too short: %s : %d",
			d->devname, page_length);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002016e, LIBDAX_MSGS_SEV_DEBUG,
				LIBDAX_MSGS_PRIO_LOW, msg, 0, 0);
		{ret = 0; goto ex;}
	}

	m->buffer_size = page[12] * 256 + page[13];
	m->dvdram_read = page[2] & 32;
	m->dvdram_write = page[3] & 32;
	m->dvdr_read = page[2] & 16;
	m->dvdr_write = page[3] & 16;
	m->dvdrom_read = page[2] & 8;
	m->simulate = page[3] & 4;
	m->cdrw_read = page[2] & 2;
	m->cdrw_write = page[3] & 2;
	m->cdr_read = page[2] & 1;
	m->cdr_write = page[3] & 1;

	m->c2_pointers = page[5] & 16;
	m->underrun_proof = page[4] & 128;

	/* ts A61021 : these fields are marked obsolete in MMC 3 */
	m->max_read_speed = page[8] * 256 + page[9];
	m->cur_read_speed = page[14] * 256 + page[15];

	m->max_write_speed = page[18] * 256 + page[19];
	m->cur_write_speed = page[20] * 256 + page[21];

	/* ts A61021 : New field to be set by atip (or following MMC-3 info) */
	m->min_write_speed = m->max_write_speed;

	/* ts A61225 : for ACh GET PERFORMANCE, Type 03h */
	m->min_end_lba = 0x7fffffff;
	m->max_end_lba = 0;

	if (!was_error)
		m->valid = 1;

	mmc_get_configuration(d);

	/* ts A61225 : end of MMC-1 , begin of MMC-3 */
	if (page_length < 30) /* no write speed descriptors ? */
		goto try_mmc_get_performance;

	m->cur_write_speed = page[28] * 256 + page[29];

	if (speed_debug) 
	fprintf(stderr, "LIBBURN_DEBUG: cur_write_speed = %d\n",
		m->cur_write_speed);

	num_write_speeds = page[30] * 256 + page[31];
	m->max_write_speed = m->min_write_speed = m->cur_write_speed;

        if (32 + 4 * num_write_speeds > page_length + 2) {
		sprintf(msg, "Malformed capabilities page 2Ah received (len=%d, #speeds=%d)", page_length, num_write_speeds);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002013c,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
		{ret = 0; goto ex;}
	}

	for (i = 0; i < num_write_speeds; i++) {
		speed = page[32 + 4 * i + 2] * 256 + page[32 + 4 * i + 3];

		if (speed_debug) 
		fprintf(stderr,
			"LIBBURN_DEBUG: write speed #%d = %d kB/s  (rc %d)\n",
			i, speed, page[32 + 4 * i + 1] & 7);

		/* ts A61226 */
		ret = burn_speed_descriptor_new(&(d->mdata->speed_descriptors),
				NULL, d->mdata->speed_descriptors, 0);
		if (ret > 0) {
			sd = d->mdata->speed_descriptors;
			sd->source = 1;
			if (d->current_profile > 0) {
				sd->profile_loaded = d->current_profile;
				strcpy(sd->profile_name,
					d->current_profile_text);
			}
			sd->wrc = (( page[32 + 4 * i + 1] & 7 ) == 1 );
			sd->write_speed = speed;
		}

		if (speed > m->max_write_speed)
			m->max_write_speed = speed;
		if (speed < m->min_write_speed)
			m->min_write_speed = speed;
	}

	if (speed_debug) 
	fprintf(stderr,
	"LIBBURN_DEBUG: 5Ah,2Ah min_write_speed = %d , max_write_speed = %d\n",
		m->min_write_speed, m->max_write_speed);

try_mmc_get_performance:;
	if (m->cdrw_write || page_length >= 32) {
		/* ts A90823:
		   One has to avoid U3 enhanced memory sticks here. On my
		   SuSE 10.2 a SanDisk Cruzer 4GB stalls at the second occasion
		   of ACh GET PERFORMANCE. (The first one is obviously called
		   by the OS at plug time.)
		   This pseudo drive returns no write capabilities and a page
		   length of 28. MMC-3 describes page length 32. Regrettably
		   MMC-2 prescribes a page length of 26. Here i have to trust
		   m->cdrw_write to reliably indicate any MMC-2 burner.
		*/
		ret = mmc_get_write_performance(d);
		if (ret > 0 && speed_debug)
			fprintf(stderr,
	  "LIBBURN_DEBUG: ACh min_write_speed = %d , max_write_speed = %d\n",
			m->min_write_speed, m->max_write_speed);
	}

	ret = !was_error;
ex:
	BURN_FREE_MEM(msg);
	BURN_FREE_MEM(buf);
	BURN_FREE_MEM(c);
	return ret;
}


void spc_sense_caps(struct burn_drive *d)
{
	int alloc_len, start_len = 30, ret;

	mmc_start_if_needed(d, 1);
	if (mmc_function_spy(d, "sense_caps") <= 0)
		return;

	/* first command execution to learn Allocation Length */
	alloc_len = start_len;
	ret = spc_sense_caps_al(d, &alloc_len, 1);
/*
	fprintf(stderr,"LIBBURN_DEBUG: 5Ah alloc_len = %d , ret = %d\n",
			alloc_len, ret);
*/
	if (alloc_len >= start_len && ret > 0)
		/* second execution with announced length */
		spc_sense_caps_al(d, &alloc_len, 0);
}


void spc_sense_error_params(struct burn_drive *d)
{
	struct buffer *buf = NULL;
	struct scsi_mode_data *m;
	int alloc_len = 12 ;
	unsigned char *page;
	struct command *c = NULL;

	mmc_start_if_needed(d, 1);
	if (mmc_function_spy(d, "sense_error_params") <= 0)
		goto ex;

	BURN_ALLOC_MEM_VOID(buf, struct buffer, 1);
	BURN_ALLOC_MEM_VOID(c, struct command, 1);

	scsi_init_command(c, SPC_MODE_SENSE, sizeof(SPC_MODE_SENSE));
	c->dxfer_len = alloc_len;
	c->opcode[7] = (c->dxfer_len >> 8) & 0xff;
	c->opcode[8] = c->dxfer_len & 0xff;
	c->retry = 1;
	c->opcode[2] = 0x01;
	c->page = buf;
	c->page->bytes = 0;
	c->page->sectors = 0;
	c->dir = FROM_DRIVE;
	d->issue_command(d, c);

	m = d->mdata;
	page = c->page->data + 8;
	d->params.retries = page[3];
	m->retry_page_length = page[1];
	m->retry_page_valid = 1;
ex:;
	BURN_FREE_MEM(buf);
	BURN_FREE_MEM(c);
}

void spc_select_error_params(struct burn_drive *d,
			     const struct burn_read_opts *o)
{
	struct buffer *buf = NULL;
	struct command *c = NULL;

	mmc_start_if_needed(d, 1);
	if (mmc_function_spy(d, "select_error_params") <= 0)
		goto ex;

	BURN_ALLOC_MEM_VOID(buf, struct buffer, 1);
	BURN_ALLOC_MEM_VOID(c, struct command, 1);
	
	scsi_init_command(c, SPC_MODE_SELECT, sizeof(SPC_MODE_SELECT));
	c->retry = 1;
	c->opcode[8] = 8 + 2 + d->mdata->retry_page_length;
	c->page = buf;
	c->page->bytes = 0;
	c->page->sectors = 0;

	memset(c->page->data, 0, 8 + 2 + d->mdata->retry_page_length);
	c->page->bytes = 8 + 2 + d->mdata->retry_page_length;
	c->page->data[8] = 1;
	c->page->data[9] = d->mdata->retry_page_length;
	if (o->transfer_damaged_blocks)
		c->page->data[10] |= 32;
	if (o->report_recovered_errors)
		c->page->data[10] |= 4;
	if (!o->hardware_error_recovery)
		c->page->data[10] |= 1;
	c->page->data[11] = d->params.retries;
	c->dir = TO_DRIVE;
	d->issue_command(d, c);
ex:;
	BURN_FREE_MEM(buf);
	BURN_FREE_MEM(c);
}

void spc_sense_write_params(struct burn_drive *d)
{
	struct buffer *buf = NULL;
	struct scsi_mode_data *m;
	int dummy1, dummy2, alloc_len = 10;
	unsigned char *page;
	struct command *c = NULL;

	mmc_start_if_needed(d, 1);
	if (mmc_function_spy(d, "sense_write_params") <= 0)
		goto ex;

	BURN_ALLOC_MEM_VOID(buf, struct buffer, 1);
	BURN_ALLOC_MEM_VOID(c, struct command, 1);

	/* ts A61007 : Done in soft at only caller burn_drive_grab() */
	/* a ssert(d->mdata->cdr_write || d->mdata->cdrw_write ||
	       d->mdata->dvdr_write || d->mdata->dvdram_write); */

	scsi_init_command(c, SPC_MODE_SENSE, sizeof(SPC_MODE_SENSE));
	c->dxfer_len = alloc_len;
	c->opcode[7] = (c->dxfer_len >> 8) & 0xff;
	c->opcode[8] = c->dxfer_len & 0xff;
	c->retry = 1;
	c->opcode[2] = 0x05;
	c->page = buf;
	c->page->bytes = 0;
	c->page->sectors = 0;
	c->dir = FROM_DRIVE;
	d->issue_command(d, c);

	/* ts A71128 : do not interpret reply if error */
	m = d->mdata;
	if (!c->error) {
		page = c->page->data + 8;
		m->write_page_length = page[1];
		m->write_page_valid = 1;
	} else
		m->write_page_valid = 0;
	mmc_read_disc_info(d);

	/* ts A70212 : try to setup d->media_capacity_remaining */
	if (d->current_profile == 0x1a || d->current_profile == 0x13 ||
	    d->current_profile == 0x12 || d->current_profile == 0x43)
		d->read_format_capacities(d, -1);
	else if (d->status == BURN_DISC_BLANK ||
	     (d->current_is_cd_profile && d->status == BURN_DISC_APPENDABLE)) {
		burn_drive_send_default_page_05(d, 0);
		d->get_nwa(d, -1, &dummy1, &dummy2);
	}
	/* others are hopefully up to date from mmc_read_disc_info() */

/*
        fprintf(stderr, "LIBBURN_DEBUG: media_capacity_remaining = %.f\n",
                (double) d->media_capacity_remaining);
*/

ex:;
	BURN_FREE_MEM(buf);
	BURN_FREE_MEM(c);
}


/* remark ts A61104 :
Although command MODE SELECT is SPC, the content of the
Write Parameters Mode Page (05h) is MMC (Table 108 in MMC-1). 
Thus the filling of the mode page is done by mmc_compose_mode_page_5().
*/
void spc_select_write_params(struct burn_drive *d,
			     const struct burn_write_opts *o)
{
	struct buffer *buf = NULL;
	struct command *c = NULL;
	int alloc_len;

	mmc_start_if_needed(d, 1);
	if (mmc_function_spy(d, "select_write_params") <= 0)
		goto ex;

	BURN_ALLOC_MEM_VOID(buf, struct buffer, 1);
	BURN_ALLOC_MEM_VOID(c, struct command, 1);

	/* ts A61007 : All current callers are safe. */
	/* a ssert(o->drive == d); */

	/* <<< A61030
	fprintf(stderr,"libburn_debug: write_type=%d  multi=%d  control=%d\n",
		o->write_type,o->multi,o->control);
	fprintf(stderr,"libburn_debug: block_type=%d  spc_block_type=%d\n",
		o->block_type,spc_block_type(o->block_type));
	*/

	alloc_len = 8 + 2 + d->mdata->write_page_length;
	memset(&(buf->data), 0, alloc_len);

#ifdef Libburn_pioneer_dvr_216d_load_mode5

	scsi_init_command(c, SPC_MODE_SENSE, sizeof(SPC_MODE_SENSE));
	c->dxfer_len = alloc_len;
	c->opcode[7] = (alloc_len >> 8) & 0xff;
	c->opcode[8] = alloc_len & 0xff;
	c->retry = 1;
	c->opcode[2] = 0x05;
	c->page = buf;
	c->page->bytes = 0;
	c->page->sectors = 0;
	c->dir = FROM_DRIVE;
	d->issue_command(d, c);

	if (c->error) 
		memset(&(buf->data), 0,
				8 + 2 + d->mdata->write_page_length);

#endif /* Libburn_pioneer_dvr_216d_load_mode5 */

	scsi_init_command(c, SPC_MODE_SELECT, sizeof(SPC_MODE_SELECT));
	c->retry = 1;
	c->opcode[7] = (alloc_len >> 8) & 0xff;
	c->opcode[8] = alloc_len & 0xff;
	c->page = buf;
	c->page->bytes = 0;
	c->page->sectors = 0;

	c->page->bytes = alloc_len;

	/* ts A61229 */
	if (mmc_compose_mode_page_5(d, o, c->page->data + 8) <= 0)
		goto ex;

	c->dir = TO_DRIVE;
	d->issue_command(d, c);
ex:;
	BURN_FREE_MEM(buf);
	BURN_FREE_MEM(c);
}

void spc_getcaps(struct burn_drive *d)
{
	if (mmc_function_spy(d, "getcaps") <= 0)
		return;

	burn_speed_descriptor_destroy(&(d->mdata->speed_descriptors), 1);
	spc_inquiry(d);
	spc_sense_caps(d);
	spc_sense_error_params(d);
}

/*
don't check totally stupid modes (raw/raw0)
some drives say they're ok, and they're not.
*/

void spc_probe_write_modes(struct burn_drive *d)
{
	struct buffer *buf = NULL;
	int try_write_type = 1;
	int try_block_type = 0;
	int key, asc, ascq, useable_write_type = -1, useable_block_type = -1;
	int last_try = 0;
	struct command *c = NULL;

	mmc_start_if_needed(d, 1);
	if (mmc_function_spy(d, "spc_probe_write_modes") <= 0)
		goto ex;

	BURN_ALLOC_MEM_VOID(buf, struct buffer, 1);
	BURN_ALLOC_MEM_VOID(c, struct command, 1);

	/* ts A70213 : added pseudo try_write_type 4 to set a suitable mode */
	while (try_write_type != 5) {
		/* ts A70213 */
		if (try_write_type == 4) {
			/* Pseudo write type NONE . Set a useable write mode */
			if (useable_write_type == -1)
	break;
			try_write_type = useable_write_type;
			try_block_type = useable_block_type;
			last_try= 1;
		}

		scsi_init_command(c, SPC_MODE_SELECT,sizeof(SPC_MODE_SELECT));
		c->retry = 1;
		c->opcode[8] = 8 + 2 + 0x32;
		c->page = buf;

		memset(c->page->data, 0, 8 + 2 + 0x32);
		c->page->bytes = 8 + 2 + 0x32;

		c->page->data[8] = 5;
		c->page->data[9] = 0x32;
		c->page->data[10] = try_write_type;
		if (try_block_type > 4)
			c->page->data[11] = 4;
		else
			c->page->data[11] = 0;
		c->page->data[12] = try_block_type;
		c->page->data[23] = 150;
		c->dir = TO_DRIVE;

		d->silent_on_scsi_error = 1;
		d->issue_command(d, c);
		d->silent_on_scsi_error = 0;

		if (last_try)
	break;

		spc_decode_sense(c->sense, 0, &key, &asc, &ascq);
		if (key)
			/* try_block_type not supported */;
		else {
			/* try_write_type, try_block_type is supported mode */
			if (try_write_type == 2)	/* sao */
				d->block_types[try_write_type] =
					BURN_BLOCK_SAO;
			else
				d->block_types[try_write_type] |=
					1 << try_block_type;

			/* ts A70213 */
			if ((useable_write_type < 0 && try_write_type > 0) ||
			    (try_write_type == 1 && try_block_type == 8)) {
			 	/* Packet is not supported yet.
				   Prefer TAO MODE_1. */
				useable_write_type = try_write_type;
				useable_block_type = try_block_type;
			} 
		}
		switch (try_block_type) {
		case 0:
		case 1:
		case 2:
			try_block_type++;
			break;
		case 3:
			try_block_type = 8;
			break;
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			try_block_type++;
			break;
		case 13:
			try_block_type = 0;
			try_write_type++;
			break;
		default:
			goto ex;
		}
	}
ex:;
	BURN_FREE_MEM(buf);
	BURN_FREE_MEM(c);
}

/* ( ts A61229 : shouldn't this go to mmc.c too ?) */

/** @return -1 = error */
int spc_block_type(enum burn_block_types b)
{
	switch (b) {
	case BURN_BLOCK_SAO:
		return 0;	/* ignored bitz */
	case BURN_BLOCK_RAW0:
		return 0;
	case BURN_BLOCK_RAW16:
		return 1;
	case BURN_BLOCK_RAW96P:
		return 2;
	case BURN_BLOCK_RAW96R:
		return 3;
	case BURN_BLOCK_MODE1:
		return 8;
	case BURN_BLOCK_MODE2R:
		return 9;
	case BURN_BLOCK_MODE2_PATHETIC:
		return 10;
	case BURN_BLOCK_MODE2_LAME:
		return 11;
	case BURN_BLOCK_MODE2_OBSCURE:
		return 12;
	case BURN_BLOCK_MODE2_OK:
		return 13;
	default:
		return -1;
	}
	/* ts A61007 : already prevented in burn_write_opts_set_write_type() */
	/* a ssert(0); */;
}

/* ts A61021 : the spc specific part of sg.c:enumerate_common()
*/
int spc_setup_drive(struct burn_drive *d)
{
	d->getcaps = spc_getcaps;
	d->lock = spc_prevent;
	d->unlock = spc_allow;
	d->read_disc_info = spc_sense_write_params;
	d->get_erase_progress = spc_get_erase_progress;
	d->test_unit_ready = spc_test_unit_ready;
	d->probe_write_modes = spc_probe_write_modes;
	d->send_parameters = spc_select_error_params;
	d->send_write_parameters = spc_select_write_params;
	return 1;	
}

/* ts A61021 : the general SCSI specific part of sg.c:enumerate_common()
   @param flag Bitfiled for control purposes
               bit0= do not setup spc/sbc/mmc
*/
int burn_scsi_setup_drive(struct burn_drive *d, int bus_no, int host_no,
			int channel_no, int target_no, int lun_no, int flag)
{
	int ret;

        /* ts A60923 */
	d->bus_no = bus_no;
	d->host = host_no;
	d->id = target_no;
	d->channel = channel_no;
	d->lun = lun_no;

	/* ts A61106 */
	d->silent_on_scsi_error = 0;


	d->idata = calloc(1, sizeof(struct burn_scsi_inquiry_data));
	d->mdata = calloc(1, sizeof(struct scsi_mode_data));

	/* ts A61007 : obsolete Assert in drive_getcaps() */
	if (d->idata == NULL || d->mdata == NULL) {
	        libdax_msgs_submit(libdax_messenger, -1, 0x00020108,
	                LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
	                "Could not allocate new drive object", 0, 0);
		return -1;
	}
	d->idata->valid = 0;
	d->mdata->valid = 0;
	d->mdata->speed_descriptors = NULL;
	if (!(flag & 1)) {
		ret = spc_setup_drive(d);
		if (ret<=0)
			return ret;
		ret = sbc_setup_drive(d);
		if (ret<=0)
			return ret;
		ret = mmc_setup_drive(d);
		if (ret<=0)
			return ret;
	}
	return 1;
}


/* ts A61122 - A80829 */
enum response scsi_error_msg(struct burn_drive *d, unsigned char *sense,
			     int senselen, char msg_data[161],
			     int *key, int *asc, int *ascq)
{
	int ret;
	char *msg;
	static char key_def[16][40] = {
		"(no specific error)",
		"Recovered error",
		"Drive not ready",
		"Medium error",
		"Drive error",
		"Illegal request",
		"Drive event",
		"Data protected",
		"Blank/Nonblank",
		"Vendor specific code",
		"Copy aborted",
		"Command aborted",
		"(obsolete error code)",
		"Volume overflow",
		"Miscompare",
		"(reserved error code)",
	};

	msg= msg_data;
	*key= *asc= *ascq= -1;

	ret = spc_decode_sense(sense, senselen, key, asc, ascq);
	if (ret <= 0)
		*key= *asc= *ascq= -1;

	sprintf(msg, "[%X %2.2X %2.2X] ", *key, *asc, *ascq);
	msg= msg + strlen(msg);

	switch (*asc) {
	case 0x00:
		if (*key > 0 || *ascq > 0) 
			break; /* Fall through to unknown error */
		sprintf(msg, "(No error reported by SCSI transaction)");
		return GO_ON;

	case 0x02:
		sprintf(msg, "Not ready");
		return RETRY;
	case 0x04:
		if (*ascq == 1)
			sprintf(msg,
			"Logical unit is in the process of becoming ready");
		else
			sprintf(msg, "Logical unit is not ready");
		return RETRY;
	case 0x08:
		if (*key != 4)
			break;
		if (*ascq == 0)
			sprintf(msg, "Logical unit communication failure");
		else if (*ascq == 1)
			sprintf(msg, "Logical unit communication timeout");
		else if (*ascq == 2)
			sprintf(msg, "Logical unit communication parity error");
		else if (*ascq == 3)
			sprintf(msg, "Logical unit communication crc error");
		return RETRY;
	case 0x09:
		if (*key != 4)
			break;
		if (*ascq == 0)
			sprintf(msg, "Track following error");
		else if (*ascq == 1)
			sprintf(msg, "Tracking servo failure");
		else if (*ascq == 2)
			sprintf(msg, "Focus servo failure");
		else if (*ascq == 3)
			sprintf(msg, "Spindle servo failure");
		else if (*ascq == 4)
			sprintf(msg, "Head select fault");
		else
			break;
		return FAIL;
	case 0x0C:
		if (*key == 2 && *ascq == 7)
			sprintf(msg, "Write error, recovery needed"); 
		else if (*key == 2 && *ascq == 0x0f)
			sprintf(msg, "Defects in error window"); 
		else if (*key == 3 && *ascq == 2)
			sprintf(msg, "Write error, auto reallocation failed");
		else if (*key == 3 && *ascq == 9)
			sprintf(msg, "Write error, loss of streaming");
		else if (*key == 3)
			sprintf(msg, "Write error");
		else
			break;
		return FAIL;
	case 0x11:
		if (*key != 3)
			break;
		if (*ascq == 0)
			sprintf(msg, "Unrecovered read error");
		else if (*ascq == 1)
			sprintf(msg, "Read retries exhausted");
		else if (*ascq == 2)
			sprintf(msg, "Error too long to correct");
		else if (*ascq == 5)
			sprintf(msg, "L-EC uncorrectable error");
		else if (*ascq == 6)
			sprintf(msg, "CIRC uncorrectable error");
		else
			break;
		return FAIL;
	case 0x15:
		if (*key != 3 && *key != 4)
			break;
		sprintf(msg, "Random positioning error");
		return FAIL;
	case 0x1a:
		if (*key != 5)
			break;
		sprintf(msg, "Parameter list length error");
		return FAIL;
	case 0x1b:
		if (*key != 4)
			break;
		sprintf(msg, "Synchronous data transfer error");
		return FAIL;
	case 0x20:
		if (*key != 5)
			break;
		sprintf(msg, "Invalid command operation code");
		return FAIL;
	case 0x21:
		if (*key != 5)
			break;
		if (*ascq == 0)
			sprintf(msg, "Lba out of range");
		else if (*ascq == 3)
			sprintf(msg, "Invalid write crossing layer jump");
		else
			sprintf(msg, "Invalid address");
		return FAIL;
	case 0x24:
		if (*key != 5)
			break;
		sprintf(msg, "Invalid field in cdb");
		return FAIL;
	case 0x26:
		if (*key != 5)
			break;
		if (*ascq == 1)
			sprintf(msg, "Parameter not supported");
		else if (*ascq == 2)
			sprintf(msg, "Parameter value invalid");
		else
			sprintf(msg, "Invalid field in parameter list");
		return FAIL;
	case 0x27:
		if (*key != 7)
			break;
		sprintf(msg, "Write protected");
		return FAIL;
	case 0x28:
		if (*key != 6)
			break;
		if (*ascq == 0)
			sprintf(msg, "Medium may have changed");
		else if (*ascq == 2)
			sprintf(msg, "Format layer may have changed");
		else
			break;
		return RETRY;
	case 0x29:
		if (*key != 6)
			break;
		if (*ascq == 0)
			sprintf(msg,
                               "Power on, reset, or bus device reset occured");
		else if (*ascq == 1)
			sprintf(msg, "Power on occured");
		else if (*ascq == 2)
			sprintf(msg, "Bus reset occured");
		else if (*ascq == 3)
			sprintf(msg, "Bus device reset function occured");
		else if (*ascq == 4)
			sprintf(msg, "Device internal reset");
		else
			break;
		return RETRY;
	case 0x2c:
		if (*key != 5)
			break;
		if (*ascq == 0)
			sprintf(msg, "Command sequence error");
		else 
			break;
		return FAIL;
	case 0x2e:
		if (*key != 6)
			break;
		if (*ascq == 0)
			sprintf(msg,
                               "Insufficient time for operation");
		else 
			break;
		return FAIL;
	case 0x30:
		if (*key != 2 && *key != 5)
			break;
		if (*ascq == 1)
			sprintf(msg, "Cannot read medium, unknown format");
		else if (*ascq == 2)
			sprintf(msg,
				"Cannot read medium, incompatible format");
		else if (*ascq == 4)
			sprintf(msg, "Cannot write medium, unknown format");
		else if (*ascq == 5)
			sprintf(msg,
				"Cannot write medium, incompatible format");
		else if (*ascq == 6)
			sprintf(msg,
				"Cannot format medium, incompatible medium");
		else if (*ascq == 7)
			sprintf(msg, "Cleaning failure");
		else
			sprintf(msg, "Incompatible medium installed");
		return FAIL;
	case 0x31:
		if (*key != 3)
			break;
		if (*ascq == 0)
			sprintf(msg, "Medium unformatted or format corrupted");
		else if (*ascq == 1)
			sprintf(msg, "Format command failed");
		return FAIL;
	case 0x3A:
		if (*key != 2)
			break;
		if (*ascq == 1)
			sprintf(msg, "Medium not present, tray closed");
		else if (*ascq == 2)
			sprintf(msg, "Medium not present, tray open");
		else if (*ascq == 3)
			sprintf(msg, "Medium not present, loadable");
		else
			sprintf(msg, "Medium not present");
		d->status = BURN_DISC_EMPTY;
		return FAIL;
	case 0x3E:
		if (*ascq == 1)
			sprintf(msg, "Logical unit failure");
		else if (*ascq == 2)
			sprintf(msg, "Timeout on logical unit");
		return FAIL;
	case 0x44:
		if (*ascq == 0)
			sprintf(msg, "Internal target failure");
		return FAIL;
	case 0x57:
		if (*key != 3 || *ascq != 0)
			break;
		sprintf(msg, "Unable to recover Table-of-Content");
		return FAIL;
	case 0x63:
		if (*key != 5)
			break;
		if (*ascq == 0)
			sprintf(msg,
				"End of user area encountered on this track");
		else if (*ascq == 1)
			sprintf(msg, "Packet does not fit in available space");
		else
			break;
		return FAIL;
	case 0x64:
		if (*key != 5)
			break;
		if (*ascq == 0)
			sprintf(msg, "Illegal mode for this track");
		else if (*ascq == 1)
			sprintf(msg, "Invalid packet size");
		else
			break;
		return FAIL;
	case 0x72:
		if (*key == 3)
			sprintf(msg, "Session fixation error");
		else if (*key == 5 && *ascq == 3)
			sprintf(msg,
			"Session fixation error, incomplete track in session");
		else if (*key == 5 && *ascq == 4)
			sprintf(msg,
			"Empty or partially written reserved track");
		else if (*key == 5 && *ascq == 5)
			sprintf(msg,
			"No more track reservations allowed");
		else
			break;
		return FAIL;
	case 0x73:
		if (*key == 3 && *ascq == 0)
			sprintf(msg, "CD control error");
		else if (*key == 3 && *ascq == 2)
			sprintf(msg, "Power calibration area is full");
		else if (*key == 3 && *ascq == 3)
			sprintf(msg, "Power calibration area error");
		else if (*key == 3 && *ascq == 4)
			sprintf(msg, "Program memory area update failure");
		else if (*key == 3 && *ascq == 5)
			sprintf(msg, "Program memory area is full");
		else
			break;
		return FAIL;
	}
	sprintf(msg_data,
		"See MMC specs: Sense Key %X \"%s\", ASC %2.2X ASCQ %2.2X",
		*key & 0xf, key_def[(*key) & 0xf], *asc, *ascq);
	return FAIL;
}


/* ts A61115 moved from sg-*.c */
/* ts A61122 made it frontend to scsi_error_msg() */
enum response scsi_error(struct burn_drive *d, unsigned char *sense,
			 int senselen)
{
	int key, asc, ascq, ret = 0;
	char *msg = NULL;
	enum response resp;

	BURN_ALLOC_MEM(msg, char, 160);
	resp = scsi_error_msg(d, sense, senselen, msg, &key, &asc, &ascq);
ex:;
	if (ret == -1)
		resp = FAIL;
	BURN_FREE_MEM(msg);
	return resp;
}


static char *scsi_command_name(unsigned int c, int flag)
{
	switch (c) {
        case 0x00:
		return "TEST UNIT READY";
        case 0x03:
		return "REQUEST SENSE";
        case 0x04:
		return "FORMAT UNIT";
        case 0x1b:
		return "START/STOP UNIT";
	case 0x12:
		return "INQUIRY";
	case 0x1e:
		return "PREVENT/ALLOW MEDIA REMOVAL";
        case 0x23:
		return "READ FORMAT CAPACITIES";
        case 0x25:
		return "READ CAPACITY";
        case 0x28:
		return "READ(10)";
        case 0x2a:
		return "WRITE(10)";
        case 0x35:
		return "SYNCHRONIZE CACHE";
        case 0x43:
		return "READ TOC/PMA/ATIP";
        case 0x46:
		return "GET CONFIGURATION";
        case 0x4a:
		return "GET EVENT STATUS NOTIFICATION";
        case 0x51:
		return "READ DISC INFORMATION";
        case 0x52:
		return "READ TRACK INFORMATION";
        case 0x53:
		return "RESERVE TRACK";
        case 0x54:
		return "SEND OPC INFORMATION";
        case 0x55:
		return "MODE SELECT";
	case 0x5a:
		return "MODE SENSE";
        case 0x5b:
		return "CLOSE TRACK/SESSION";
        case 0x5c:
		return "READ BUFFER CAPACITY";
        case 0x5d:
		return "SEND CUE SHEET";
        case 0xa1:
		return "BLANK";
        case 0xaa:
		return "WRITE(12)";
        case 0xac:
		return "GET PERFORMANCE";
        case 0xad:
		return "READ DISC STRUCTURE";
        case 0xb6:
		return "SET STREAMING";
        case 0xbb:
		return "SET CD SPEED";
        case 0xbe:
		return "READ CD";
	}
	return "(NOT IN COMMAND LIST)";
}


/* ts A61030 - A61115 */
/* @param flag bit0= do report conditions which are considered not an error
               bit1= report with severity FAILURE rather than DEBUG
 */
int scsi_notify_error(struct burn_drive *d, struct command *c,
                      unsigned char *sense, int senselen, int flag)
{
	int key= -1, asc= -1, ascq= -1, ret;
	char *msg = NULL, *scsi_msg = NULL;

	if (d->silent_on_scsi_error)
		{ret = 1; goto ex;}

	BURN_ALLOC_MEM(msg, char, 320);
	BURN_ALLOC_MEM(scsi_msg, char, 160);
	scsi_error_msg(d, sense, senselen, scsi_msg, &key, &asc, &ascq);

	if (!(flag & 1)) {
		/* SPC : TEST UNIT READY command */
		if (c->opcode[0] == 0)
			{ret = 1; goto ex;}
		/* MMC : READ DISC INFORMATION command */
		if (c->opcode[0] == 0x51)
			if (key == 0x2 && asc == 0x3A &&
			    ascq>=0 && ascq <= 0x02) /* MEDIUM NOT PRESENT */
				{ret = 1; goto ex;}
		if (key == 0 && asc == 0 && ascq == 0)
			{ret = 1; goto ex;}
	}

	sprintf(msg, "SCSI error condition on command %2.2Xh %s: ",
		c->opcode[0],
		scsi_command_name((unsigned int) c->opcode[0], 0));
	strcat(msg, scsi_msg);
	ret = libdax_msgs_submit(libdax_messenger, d->global_index, 0x0002010f,
		flag & 2 ? LIBDAX_MSGS_SEV_FAILURE : LIBDAX_MSGS_SEV_DEBUG,
		LIBDAX_MSGS_PRIO_HIGH, msg,0,0);
ex:;
	BURN_FREE_MEM(msg);
	BURN_FREE_MEM(scsi_msg);
	return ret;
}


/* ts A91106 */
/* @param flag bit0= do not show eventual data payload sent to the drive
                     (never with WRITE commands)
               bit1= show write length and target LBA in decimal
*/
int scsi_show_cmd_text(struct command *c, void *fp_in, int flag)
{
	int i;
	FILE *fp = fp_in;

	fprintf(fp, "\n%s\n",
		 scsi_command_name((unsigned int) c->opcode[0], 0));
	for(i = 0; i < 16 && i < c->oplen; i++)
		fprintf(fp, "%2.2x ", c->opcode[i]);
	if (i > 0)
		fprintf(fp, "\n");
	if (flag & 1)
		return 1;
	if (c->opcode[0] == 0x2A) { /* WRITE 10 */
		if (flag & 2)
			fprintf(fp, "%d -> %d\n",
				(c->opcode[7] << 8) | c->opcode[8], 
				mmc_four_char_to_int(c->opcode + 2));
	} else if (c->opcode[0] == 0xAA) { /* WRITE 12 */
		if (flag & 2)
			fprintf(fp, "%d -> %d\n",
				mmc_four_char_to_int(c->opcode + 6),
				mmc_four_char_to_int(c->opcode + 2));	
	} else if (c->dir == TO_DRIVE) {
		fprintf(fp, "To drive: %db\n", c->page->bytes);
		for (i = 0; i < c->page->bytes; i++) 
			fprintf(fp, "%2.2x%c", c->page->data[i],
				((i % 20) == 19 ? '\n' : ' '));
		if (i % 20)
			fprintf(fp, "\n");
	}
	return 1;
}

/* ts A91106 */
int scsi_show_cmd_reply(struct command *c, void *fp_in, int flag)
{
	int i;
	FILE *fp = fp_in;

	if (c->dir != FROM_DRIVE)
		return 2;
	if (c->opcode[0] == 0x28 || c->opcode[0] == 0x3C ||
	    c->opcode[0] == 0xA8 || c->opcode[0] == 0xBE) {
							/* READ commands */
		/* >>> report amount of data */;

		return 2;
	}
	fprintf(fp, "From drive: %db\n", c->dxfer_len);
	for (i = 0; i < c->dxfer_len; i++)
		fprintf(fp, "%2.2x%c", c->page->data[i],
			((i % 20) == 19 ? '\n' : ' '));
	if (i % 20)
		fprintf(fp, "\n");
	return 1;
}


/* ts A91218 (former sg_log_cmd ts A70518) */ 
/** Logs command (before execution) */
int scsi_log_cmd(struct command *c, void *fp_in, int flag)
{
	FILE *fp = fp_in;

	if (fp != NULL && (fp == stderr || (burn_sg_log_scsi & 1))) {
		scsi_show_cmd_text(c, fp, 0);
		if (burn_sg_log_scsi & 4)
			fflush(fp);
	}
	if (fp == stderr || !(burn_sg_log_scsi & 2))
		return 1;
	scsi_log_cmd(c, stderr, flag);
	return 1;
}


/* ts A91221 (former sg_log_err ts A91108) */
/** Logs outcome of a sg command.
    @param flag  bit0 causes an error message
                 bit1 do not print duration
*/
int scsi_log_err(struct command *c, void *fp_in, unsigned char sense[18],
		 int sense_len, int duration, int flag)
{
	char durtxt[20];
	FILE *fp = fp_in;
	int key, asc, ascq, i, l;

      	if (fp != NULL && (fp == stderr || (burn_sg_log_scsi & 1))) {
		if (flag & 1) {
			l = 18;
			if ((sense[0] & 0x7f) == 0x72 ||
			    (sense[0] & 0x7f) == 0x73)
				l = sense[7] + 7 + 1; /* SPC-5 4.5.2. */
			if (l > sense_len)
				l = sense_len;
			fprintf(fp, "+++ sense data =");
			for (i = 0 ; i < l; i++)
				fprintf(fp, " %2.2X", sense[i]);
			fprintf(fp, "\n");
			durtxt[0] = 0;
			if (!(flag & 2))
  				sprintf(durtxt, "   (%6d ms)", duration);
			spc_decode_sense(sense, 0, &key, &asc, &ascq);
  			fprintf(fp, "+++ key=%X  asc=%2.2Xh  ascq=%2.2Xh%s\n",
				(unsigned int) key, (unsigned int) asc,
				(unsigned int) ascq, durtxt);
		} else {
			scsi_show_cmd_reply(c, fp, 0);
			if (!(flag & 2))
				fprintf(fp,"%6d ms\n", duration);
		}
		if (burn_sg_log_scsi & 4)
			fflush(fp);
	}
	if (fp == stderr || !(burn_sg_log_scsi & 2))
		return 1;
	scsi_log_err(c, stderr, sense, sense_len, duration, flag);
	return 1;
}


/* ts B00808 */
/*
    @param flag    bit0 = do not retry
                   bit1 = do not print duration
    @return 0 = not yet done , 1 = done , -1 = error
*/
int scsi_eval_cmd_outcome(struct burn_drive *d, struct command *c, void *fp,
			unsigned char *sense, int sense_len,
			int duration, time_t start_time, int timeout_ms,
			int loop_count, int flag)
{
	enum response outcome;
	int done = -1, usleep_time;

	if (burn_sg_log_scsi & 3)
		scsi_log_err(c, fp, sense, sense_len, duration,
				 (sense_len > 0) | (flag & 2));
	if (sense_len <= 0)
		return 1;
		
	outcome = scsi_error(d, sense, sense_len);
	if (outcome == RETRY && c->retry && !(flag & 1)) {
		/* Calming down retries and breaking up endless cycle
		*/
		usleep_time = Libburn_scsi_retry_usleeP +
				loop_count * Libburn_scsi_retry_incR;
		if (time(NULL) + usleep_time / 1000000 - start_time >
		    timeout_ms / 1000 + 1) {
			done = 1;
			goto ex;
		}
		usleep(usleep_time);
		if (burn_sg_log_scsi & 3) 
			scsi_log_cmd(c, fp, 0);
		return 0;
	} else if (outcome == RETRY) {
		done = 1;
	} else if (outcome == GO_ON) {
		return 1;
	} else if (outcome == FAIL) {
		done = 1;
	}
ex:;
	c->error = 1;
	scsi_notify_error(d, c, sense, sense_len, 0);
	return done;
}
