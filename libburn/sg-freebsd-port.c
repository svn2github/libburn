/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/*

This is the main operating system dependent SCSI part of libburn. It implements
the transport level aspects of SCSI control and command i/o.

Present implementation: FreeBSD CAM (untested)


PORTING:

Porting libburn typically will consist of adding a new operating system case
to the following switcher files:
  os.h    Operating system specific libburn definitions and declarations.
  sg.c    Operating system dependent transport level modules.
and to derive the following system specific files from existing examples:
  os-*.h  Included by os.h. You will need some general system knowledge
          about signals and knowledge about the storage object needs of your
          transport level module sg-*.c.

  sg-*.c  This source module. You will need special system knowledge about
          how to detect all potentially available drives, how to open them,
          eventually how to exclusively reserve them, how to perform
          SCSI transactions, how to inquire the (pseudo-)SCSI driver.
          You will not need to care about CD burning, MMC or other high-level
          SCSI aspects.

Said sg-*.c operations are defined by a public function interface, which has
to be implemented in a way that provides libburn with the desired services:
 
sg_give_next_adr()      iterates over the set of potentially useful drive 
                        address strings.

scsi_enumerate_drives() brings all available, not-whitelist-banned, and
                        accessible drives into libburn's list of drives.

sg_drive_is_open()      tells wether libburn has the given drive in use.

sg_grab()               opens the drive for SCSI commands and ensures
                        undisturbed access.

sg_release()            closes a drive opened by sg_grab()

sg_issue_command()      sends a SCSI command to the drive, receives reply,
                        and evaluates wether the command succeeded or shall
                        be retried or finally failed.

sg_obtain_scsi_adr()    tries to obtain SCSI address parameters.


Porting hints are marked by the text "PORTING:".
Send feedback to libburn-hackers@pykix.org .

*/


/** PORTING : ------- OS dependent headers and definitions ------ */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <camlib.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_pass.h>

#include <err.h> /* XXX */


/** PORTING : ------ libburn portable headers and definitions ----- */

#include "transport.h"
#include "drive.h"
#include "sg.h"
#include "spc.h"
#include "mmc.h"
#include "sbc.h"
#include "debug.h"
#include "toc.h"
#include "util.h"

#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;


/* is in portable part of libburn */
int burn_drive_is_banned(char *device_address);



/* ------------------------------------------------------------------------ */
/* ts A61115:  Private functions. Port only if needed by public functions   */
/*            (Public functions are listed below)                           */
/* ------------------------------------------------------------------------ */


/* Helper function for scsi_give_next_adr() */
static int sg_init_enumerator(burn_drive_enumerator_t *idx)
{
	idx->skip_device = 0;

	if ((idx->fd = open(XPT_DEVICE, O_RDWR)) == -1) {
		warn("couldn't open %s", XPT_DEVICE);
		return -1;
	}

	bzero(&(idx->ccb), sizeof(union ccb));

	idx->ccb.ccb_h.path_id = CAM_XPT_PATH_ID;
	idx->ccb.ccb_h.target_id = CAM_TARGET_WILDCARD;
	idx->ccb.ccb_h.target_lun = CAM_LUN_WILDCARD;

	idx->ccb.ccb_h.func_code = XPT_DEV_MATCH;
	idx->bufsize = sizeof(struct dev_match_result) * 100;
	idx->ccb.cdm.match_buf_len = idx->bufsize;
	idx->ccb.cdm.matches = (struct dev_match_result *)malloc(idx->bufsize);
	if (idx->ccb.cdm.matches == NULL) {
		warnx("can't malloc memory for matches");
		close(idx->fd);
		return -1;
	}
	idx->ccb.cdm.num_matches = 0;
	idx->i = idx->ccb.cdm.num_matches; /* to trigger buffer load */

	/*
	 * We fetch all nodes, since we display most of them in the default
	 * case, and all in the verbose case.
	 */
	idx->ccb.cdm.num_patterns = 0;
	idx->ccb.cdm.pattern_buf_len = 0;

	return 1;
}


/* Helper function for scsi_give_next_adr() */
static int sg_next_enumeration_buffer(burn_drive_enumerator_t *idx)
{
	/*
	 * We do the ioctl multiple times if necessary, in case there are
	 * more than 100 nodes in the EDT.
	 */
	if (ioctl(idx->fd, CAMIOCOMMAND, &(idx->ccb)) == -1) {
		warn("error sending CAMIOCOMMAND ioctl");
		return -1;
	}

	if ((idx->ccb.ccb_h.status != CAM_REQ_CMP)
	    || ((idx->ccb.cdm.status != CAM_DEV_MATCH_LAST)
		&& (idx->ccb.cdm.status != CAM_DEV_MATCH_MORE))) {
		warnx("got CAM error %#x, CDM error %d\n",
		      idx->ccb.ccb_h.status, idx->ccb.cdm.status);
		return -1;
	}
	return 1;
}


static int sg_close_drive(struct burn_drive * d)
{
	if (d->cam != NULL) {
		cam_close_device(d->cam);
		d->cam = NULL;
	}
	return 0;
}


/* ----------------------------------------------------------------------- */
/* PORTING: Private functions which contain publicly needed functionality. */
/*          Their portable part must be performed. So it is probably best  */
/*          to replace the non-portable part and to call these functions   */
/*          in your port, too.                                             */
/* ----------------------------------------------------------------------- */


/** Wraps a detected drive into libburn structures and hands it over to
    libburn drive list.
*/
static void enumerate_common(char *fname, int bus_no, int host_no,
			     int channel_no, int target_no, int lun_no)
{
	int ret;
	struct burn_drive out;

	/* General libburn drive setup */
	burn_setup_drive(&out, fname);

	/* This transport adapter uses SCSI-family commands and models
	   (seems the adapter would know better than its boss, if ever) */
	ret = burn_scsi_setup_drive(&out, bus_no, host_no, channel_no,
                                 target_no, lun_no, 0);
        if (ret<=0)
                return;

	/* PORTING: ------------------- non portable part --------------- */

	/* Operating system adapter is CAM */
	/* Adapter specific handles and data */
	out.cam = NULL;

	/* PORTING: ---------------- end of non portable part ------------ */

	/* Adapter specific functions with standardized names */
	out.grab = sg_grab;
	out.release = sg_release;
	out.drive_is_open = sg_drive_is_open;
	out.issue_command = sg_issue_command;
	/* Finally register drive and inquire drive information */
	burn_drive_finish_enum(&out);
}


/* ts A61115 */
/* ------------------------------------------------------------------------ */
/* PORTING:           Public functions. These MUST be ported.               */
/* ------------------------------------------------------------------------ */


/** Returns the next index number and the next enumerated drive address.
    The enumeration has to cover all available and accessible drives. It is
    allowed to return addresses of drives which are not available but under
    some (even exotic) circumstances could be available. It is on the other
    hand allowed, only to hand out addresses which can really be used right
    in the moment of this call. (This implementation chooses the latter.)
    @param idx An opaque handle. Make no own theories about it.
    @param adr Takes the reply
    @param adr_size Gives maximum size of reply including final 0
    @param initialize  1 = start new,
                       0 = continue, use no other values for now
                      -1 = finish
    @return 1 = reply is a valid address , 0 = no further address available
           -1 = severe error (e.g. adr_size too small)
*/
int sg_give_next_adr(burn_drive_enumerator_t *idx,
		     char adr[], int adr_size, int initialize)
{
	int ret;

	if (initialize == 1) {
		ret = sg_init_enumerator(idx);
		if (ret<=0)
			return ret;
	} else if (initialize == -1) {
		if(idx->fd != -1)
			close(idx->fd);
		idx->fd = -1;
		return 0;
	}


try_item:; /* This spaghetti loop keeps the number of tabs small  */

	/* Loop content from old scsi_enumerate_drives() */

	while (idx->i >= idx->ccb.cdm.num_matches) {
		ret = sg_next_enumeration_buffer(idx);
		if (ret<=0)
			return -1;
		if (!((idx->ccb.ccb_h.status == CAM_REQ_CMP)
			&& (idx->ccb.cdm.status == CAM_DEV_MATCH_MORE)) )
			return 0;
		idx->i = 0;
	}

	switch (idx->ccb.cdm.matches[idx->i].type) {
	case DEV_MATCH_BUS:
		break;
	case DEV_MATCH_DEVICE: {
		struct device_match_result* result;

		result = &(idx->ccb.cdm.matches[i].result.device_result);
		if (result->flags & DEV_RESULT_UNCONFIGURED)
			idx->skip_device = 1;
		else
			idx->skip_device = 0;
		break;
	}
	case DEV_MATCH_PERIPH: {
		struct periph_match_result* result;
		char buf[64];

		result = &(idx->ccb.cdm.matches[i].result.periph_result);
		if (idx->skip_device || 
		    strcmp(result->periph_name, "pass") == 0)
			break;
		snprintf(buf, sizeof (buf), "/dev/%s%d",
			 result->periph_name, result->unit_number);
		if(adr_size <= strlen(buf)
			return -1;
		strcpy(adr, buf);

		/* Found next enumerable address */
		return 1;

	}
	default:
		/* printf(stderr, "unknown match type\n"); */
		break;
	}

	(idx->i)++;
	goto try_item; /* Regular function exit is return 1 above  */
}


/** Brings all available, not-whitelist-banned, and accessible drives into
    libburn's list of drives.
*/
int scsi_enumerate_drives(void)
{
	burn_drive_enumerator_t idx;
	int initialize = 1;
	char buf[64];

	while(1) {
		ret = sg_give_next_adr(&idx, buf, sizeof(buf), initialize);
		initialize = 0;
		if (ret <= 0)
	break;
		if (burn_drive_is_banned(buf))
	continue; 
		enumerate_common(buf, idx.result->path_id, idx.result->path_id,
				0, idx.result->target_id, 
				idx.result->target_lun);
	}
	sg_give_next_adr(&idx, buf, sizeof(buf), -1);
}


/** Tells wether libburn has the given drive in use or exclusively reserved.
    If it is "open" then libburn will eventually call sg_release() on it when
    it is time to give up usage resp. reservation.
*/
/** Published as burn_drive.drive_is_open() */
int sg_drive_is_open(struct burn_drive * d)
{
	return (d->cam != NULL);
}


/** Opens the drive for SCSI commands and - if burn activities are prone
    to external interference on your system - obtains an exclusive access lock
    on the drive. (Note: this is not physical tray locking.)
    A drive that has been opened with sg_grab() will eventually be handed
    over to sg_release() for closing and unreserving. 
*/  
int sg_grab(struct burn_drive *d)
{
	int count;
	struct cam_device *cam;

	if(d->cam != NULL)
		return 0;

	cam = cam_open_device(d->devname, O_RDWR);
	if (cam == NULL) {
		libdax_msgs_submit(libdax_messenger, d->global_index,
		0x00020003,
		LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
		"Could not grab drive", 0/*os_errno*/, 0);
		return 0;
	}
	d->cam = cam;
	fcntl(cam->fd, F_SETOWN, getpid());
	d->released = 0;
	return 1;
}


/** PORTING: Is mainly about the call to sg_close_drive() and wether it
             implements the demanded functionality.
*/
/** Gives up the drive for SCSI commands and releases eventual access locks.
    (Note: this is not physical tray locking.) 
*/
int sg_release(struct burn_drive *d)
{
	if (d->cam == NULL) {
		burn_print(1, "release an ungrabbed drive.  die\n");
		return 0;
	}
	sg_close_drive(d);
	return 0;
}


/** Sends a SCSI command to the drive, receives reply and evaluates wether
    the command succeeded or shall be retried or finally failed.
    Returned SCSI errors shall not lead to a return value indicating failure.
    The callers get notified by c->error. An SCSI failure which leads not to
    a retry shall be notified via scsi_notify_error().
    The Libburn_log_sg_commandS facility might be of help when problems with
    a drive have to be examined. It shall stay disabled for normal use.
    @return: 1 success , <=0 failure
*/
int sg_issue_command(struct burn_drive *d, struct command *c)
{
	int done = 0;
	int err;
	union ccb *ccb;

	if (d->cam == NULL) {
		c->error = 0;
		return 0;
	}

	c->error = 0;

	ccb = cam_getccb(d->cam);
	cam_fill_csio(&ccb->csio,
				  1,                              /* retries */
				  NULL,                           /* cbfncp */
				  CAM_DEV_QFRZDIS,                /* flags */
				  MSG_SIMPLE_Q_TAG,               /* tag_action */
				  NULL,                           /* data_ptr */
				  0,                              /* dxfer_len */
				  sizeof (ccb->csio.sense_data),  /* sense_len */
				  0,                              /* cdb_len */
				  30*1000);                       /* timeout */
	switch (c->dir) {
	case TO_DRIVE:
		ccb->csio.ccb_h.flags |= CAM_DIR_OUT;
		break;
	case FROM_DRIVE:
		ccb->csio.ccb_h.flags |= CAM_DIR_IN;
		break;
	case NO_TRANSFER:
		ccb->csio.ccb_h.flags |= CAM_DIR_NONE;
		break;
	}

	ccb->csio.cdb_len = c->oplen;
	memcpy(&ccb->csio.cdb_io.cdb_bytes, &c->opcode, c->oplen);
	
	memset(&ccb->csio.sense_data, 0, sizeof (ccb->csio.sense_data));

	if (c->page) {
		ccb->csio.data_ptr  = c->page->data;
		if (c->dir == FROM_DRIVE) {
			ccb->csio.dxfer_len = BUFFER_SIZE;
/* touch page so we can use valgrind */
			memset(c->page->data, 0, BUFFER_SIZE);
		} else {

			/* ts A61115: removed a ssert() */
			if(c->page->bytes <= 0)
				return 0;

			ccb->csio.dxfer_len = c->page->bytes;
		}
	} else {
		ccb->csio.data_ptr  = NULL;
		ccb->csio.dxfer_len = 0;
	}

	do {
		err = cam_send_ccb(d->cam, ccb);
		if (err == -1) {
			libdax_msgs_submit(libdax_messenger,
				d->global_index, 0x0002010c,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				"Failed to transfer command to drive",
				errno, 0);
			cam_freeccb(ccb);
			sg_close_drive(d);
			d->released = 1;
			d->busy = BURN_DRIVE_IDLE;
			c->error = 1;
			return -1;
		}
		/* XXX */
		memcpy(c->sense, &ccb->csio.sense_data, ccb->csio.sense_len);
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			if (!c->retry) {
				c->error = 1;
				cam_freeccb(ccb);
				return 1;
			}
			switch (scsi_error(d, c->sense, 0)) {
			case RETRY:
				done = 0;
				break;
			case FAIL:
				done = 1;
				c->error = 1;
				break;
			}
		} else {
			done = 1;
		}
	} while (!done);
	cam_freeccb(ccb);
	return 1;
}


/** Tries to obtain SCSI address parameters.
    @return  1 is success , 0 is failure
*/
int sg_obtain_scsi_adr(char *path, int *bus_no, int *host_no, int *channel_no,
                       int *target_no, int *lun_no)
{
	burn_drive_enumerator_t idx;
	int initialize = 1;
	char buf[64];
	struct periph_match_result* result;

	while(1) {
		ret = sg_give_next_adr(&idx, buf, sizeof(buf), initialize);
		initialize = 0;
		if (ret <= 0)
	break;
		if (strcmp(adr, buf) != 0)
	continue;
		result = &(idx->ccb.cdm.matches[i].result.periph_result);
		*bus_no = result->path_id;
		*host_no = result->path_id;
		*channel_no = 0;
		*target_no = result->target_id
		*lun_no = result->target_lun;
		sg_give_next_adr(&idx, buf, sizeof(buf), -1);
		return 1;
	}
	sg_give_next_adr(&idx, buf, sizeof(buf), -1);
	return (0);
}


/** Tells wether a text is a persistent address as listed by the enumeration
    functions.
*/
int sg_is_enumerable_adr(char* adr)
{
	burn_drive_enumerator_t idx;
	int initialize = 1;
	char buf[64];

	while(1) {
		ret = sg_give_next_adr(&idx, buf, sizeof(buf), initialize);
		initialize = 0;
		if (ret <= 0)
	break;
		if (strcmp(adr, buf) == 0) {
			sg_give_next_adr(&idx, buf, sizeof(buf), -1);
			return 1;
		}
	}
	sg_give_next_adr(&idx, buf, sizeof(buf), -1);
	return (0);
}

