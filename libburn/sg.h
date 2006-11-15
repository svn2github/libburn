/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef __SG
#define __SG

#ifdef __FreeBSD__

/* To hold all state information of BSD device enumeration
   which are now local in sg_enumerate() . So that sg_give_next_adr()
   can work in BSD and sg_enumerate() can use it. */
struct burn_drive_enumeration_state {
	union ccb ccb;
	int bufsize, fd;
	unsigned int i;
	int skip_device;
};
typedef struct burn_drive_enumeration_state burn_drive_enumerator_t;

#else /* __FreeBSD__ */

/* <<< just for testing the C syntax */
struct burn_drive_enumeration_state {
        int dummy;
};
typedef struct burn_drive_enumeration_state burn_drive_enumerator_tX;

typedef int burn_drive_enumerator_t;

#endif /* ! __FreeBSD__ */

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

#endif /* __SG */
