/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/* scsi block commands */

#include <string.h>
#include <unistd.h>

#include "transport.h"
#include "sbc.h"
#include "spc.h"
#include "options.h"


/* ts A70910
   debug: for tracing calls which might use open drive fds
          or for catching SCSI usage of emulated drives. */
int mmc_function_spy(struct burn_drive *d, char * text);


/* spc command set */
static unsigned char SBC_LOAD[] = { 0x1b, 0, 0, 0, 3, 0 };
static unsigned char SBC_UNLOAD[] = { 0x1b, 0, 0, 0, 2, 0 };
static unsigned char SBC_START_UNIT[] = { 0x1b, 0, 0, 0, 1, 0 };
static unsigned char SBC_STOP_UNIT[] = { 0x1b, 0, 0, 0, 0, 0 };

void sbc_load(struct burn_drive *d)
{
	struct command c;

	if (mmc_function_spy(d, "load") <= 0)
		return;

	scsi_init_command(&c, SBC_LOAD, sizeof(SBC_LOAD));
/*
	memcpy(c.opcode, SBC_LOAD, sizeof(SBC_LOAD));
	c.oplen = sizeof(SBC_LOAD);
	c.page = NULL;
*/
	c.retry = 1;

	/* ts A70921 : Had to revoke Immed because of LG GSA-4082B */
	/* c.opcode[1] |= 1; / * ts A70918 : Immed */

	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
	if (c.error)
		return;
	/* ts A70923 : Needed regardless of Immed bit. Was once 1 minute, now
           5 minutes for loading. If this does not suffice then other commands
	   shall fail righteously. */
	spc_wait_unit_attention(d, 300, "waiting after START UNIT (+ LOAD)",0);
}

void sbc_eject(struct burn_drive *d)
{
	struct command c;

	if (mmc_function_spy(d, "eject") <= 0)
		return;

	scsi_init_command(&c, SBC_UNLOAD, sizeof(SBC_UNLOAD));
/*
	memcpy(c.opcode, SBC_UNLOAD, sizeof(SBC_UNLOAD));
	c.oplen = sizeof(SBC_UNLOAD);
	c.page = NULL;
*/

	c.opcode[1] |= 1; /* ts A70918 : Immed */

	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
	if (c.error)
		return;
	/* ts A70918 : Wait long. A late eject could surprise or hurt user. */
	spc_wait_unit_attention(d, 1800, "STOP UNIT (+ EJECT)", 0);
}

/* ts A61118 : is it necessary to tell the drive to get ready for use ? */
int sbc_start_unit(struct burn_drive *d)
{
	struct command c;
	int ret;

	if (mmc_function_spy(d, "start_unit") <= 0)
		return 0;

	scsi_init_command(&c, SBC_START_UNIT, sizeof(SBC_START_UNIT));
	c.retry = 1;
	c.opcode[1] |= 1; /* ts A70918 : Immed */
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
	if (c.error)
		return 0;
	/* ts A70918 : now asynchronous */
	d->is_stopped = 0;
	ret = spc_wait_unit_attention(d, 1800, "START UNIT", 0);
	return ret;
}

/* ts A90824 : Trying to reduce drive noise */
int sbc_stop_unit(struct burn_drive *d)
{
	struct command c;
	int ret;

	if (mmc_function_spy(d, "stop_unit") <= 0)
		return 0;

	scsi_init_command(&c, SBC_STOP_UNIT, sizeof(SBC_STOP_UNIT));
	c.retry = 1;
	c.opcode[1] |= 1; /* Immed */
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
	if (c.error)
		return 0;
	ret = spc_wait_unit_attention(d, 1800, "STOP UNIT", 0);
	d->is_stopped = 1;
	return ret;
}



/* ts A61021 : the sbc specific part of sg.c:enumerate_common()
*/
int sbc_setup_drive(struct burn_drive *d)
{
	d->eject = sbc_eject;
	d->load = sbc_load;
	d->start_unit = sbc_start_unit;
	d->stop_unit = sbc_stop_unit;
	d->is_stopped = 0;
	return 1;
}

