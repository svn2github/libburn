/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/* scsi block commands */

#include <string.h>

#include "transport.h"
#include "sbc.h"
#include "options.h"

/* spc command set */
static char SBC_LOAD[] = { 0x1b, 0, 0, 0, 3, 0 };
static char SBC_UNLOAD[] = { 0x1b, 0, 0, 0, 2, 0 };
static char SBC_START_UNIT[] = { 0x1b, 0, 0, 0, 1, 0 };

void sbc_load(struct burn_drive *d)
{
	struct command c;

	memcpy(c.opcode, SBC_LOAD, sizeof(SBC_LOAD));
	c.retry = 1;
	c.oplen = sizeof(SBC_LOAD);
	c.dir = NO_TRANSFER;
	c.page = NULL;
	d->issue_command(d, &c);
}

void sbc_eject(struct burn_drive *d)
{
	struct command c;

	c.page = NULL;
	memcpy(c.opcode, SBC_UNLOAD, sizeof(SBC_UNLOAD));
	c.oplen = 1;
	c.oplen = sizeof(SBC_UNLOAD);
	c.page = NULL;
	c.dir = NO_TRANSFER;
	d->issue_command(d, &c);
}

/* ts A61118 : is it necessary to tell the drive to get ready for use ? */
int sbc_start_unit(struct burn_drive *d)
{
	struct command c;

	memcpy(c.opcode, SBC_START_UNIT, sizeof(SBC_START_UNIT));
	c.retry = 1;
	c.oplen = sizeof(SBC_START_UNIT);
	c.dir = NO_TRANSFER;
	c.page = NULL;
	d->issue_command(d, &c);
	return (c.error==0);
}


/* ts A61021 : the sbc specific part of sg.c:enumerate_common()
*/
int sbc_setup_drive(struct burn_drive *d)
{
	d->eject = sbc_eject;
	d->load = sbc_load;
	d->start_unit = sbc_start_unit;
	return 1;
}

