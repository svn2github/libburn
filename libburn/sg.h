/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef __SG
#define __SG


#include "os.h"


/* see os.h for name of particular os-*.h where this is defined */
BURN_OS_DEFINE_DRIVE_ENUMERATOR_T


struct burn_drive;
struct command;


/* ts A60922 ticket 33 */
int sg_give_next_adr(burn_drive_enumerator_t *enm_context,
		     char adr[], int adr_size, int initialize);
int sg_is_enumerable_adr(char *adr);
int sg_obtain_scsi_adr(char *path, int *bus_no, int *host_no, int *channel_no,
                       int *target_no, int *lun_no);

int sg_grab(struct burn_drive *);
int sg_release(struct burn_drive *);
int sg_issue_command(struct burn_drive *, struct command *);

/* ts A61115 : formerly sg_enumerate();ata_enumerate() */
int scsi_enumerate_drives(void);

int sg_drive_is_open(struct burn_drive * d);

int burn_os_stdio_capacity(char *path, off_t *bytes);

#endif /* __SG */
