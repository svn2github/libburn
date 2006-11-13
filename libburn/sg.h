/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef __SG
#define __SG

#ifdef __FreeBSD__

/* >>> To hold all state information of BSD device enumeration
       which are now local in sg_enumerate() . So that sg_give_next_adr()
       can work in BSD and sg_enumerate() can use it. */
struct burn_drive_enumeration_state {

#ifdef Scsi_freebsd_old_sg_enumeratE
	int dummy;
#else
	union ccb ccb;
	int bufsize, fd;
	unsigned int i;
	int skip_device;
#endif /* ! Scsi_freebsd_old_sg_enumeratE */

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

enum response
{ RETRY, FAIL };

/* ts A60925 : ticket 74 */
int sg_close_drive_fd(char *fname, int driveno, int *fd, int sorry);

/* ts A60922 ticket 33 */
int sg_give_next_adr(burn_drive_enumerator_t *enm_context,
		     char adr[], int adr_size, int initialize);
int sg_is_enumerable_adr(char *adr);
int sg_obtain_scsi_adr(char *path, int *bus_no, int *host_no, int *channel_no,
                       int *target_no, int *lun_no);

/* ts A60926 : ticket 33 ++ */
int sg_open_scsi_siblings(char *fname, int driveno,
                          int sibling_fds[], int *sibling_count,
                          int host_no, int channel_no, int id_no, int lun_no);
int sg_release_siblings(int sibling_fds[], int *sibling_count);
int sg_close_drive(struct burn_drive *d);

void sg_enumerate(void);
void ata_enumerate(void);
int sg_grab(struct burn_drive *);
int sg_release(struct burn_drive *);
int sg_issue_command(struct burn_drive *, struct command *);
enum response scsi_error(struct burn_drive *, unsigned char *, int);

/* ts A61030 */
/* @param flag bit0=do also report TEST UNIT READY failures */
int scsi_notify_error(struct burn_drive *, struct command *c,
                      unsigned char *sense, int senselen, int flag);

#endif /* __SG */
