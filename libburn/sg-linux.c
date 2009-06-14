/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/*

This is the main operating system dependent SCSI part of libburn. It implements
the transport level aspects of SCSI control and command i/o.

Present implementation: Linux SCSI Generic (sg)


PORTING:

Porting libburn typically will consist of adding a new operating system case
to the following switcher files:
  os.h    Operating system specific libburn definitions and declarations.
  sg.c    Operating system dependent transport level modules.
and of deriving the following system specific files from existing examples:
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

burn_os_stdio_capacity()  estimates the emulated media space of stdio-drives.


Porting hints are marked by the text "PORTING:".
Send feedback to libburn-hackers@pykix.org .

Hint: You should also look into sg-freebsd-port.c, which is a younger and
      in some aspects more straightforward implementation of this interface.

*/


/** PORTING : ------- OS dependent headers and definitions ------ */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/poll.h>
#include <linux/hdreg.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <scsi/scsi.h>
#include <sys/statvfs.h>

/* for ioctl(BLKGETSIZE) */
#include <linux/fs.h>


#include <scsi/sg.h>
/* Values within sg_io_hdr_t indicating success after ioctl(SG_IO) : */
/* .host_status : from http://tldp.org/HOWTO/SCSI-Generic-HOWTO/x291.html */
#define Libburn_sg_host_oK 0
/* .driver_status : from http://tldp.org/HOWTO/SCSI-Generic-HOWTO/x322.html */
#define Libburn_sg_driver_oK 0


/* ts A61211 : to eventually recognize CD devices on /dev/sr* */
#include <limits.h>
#include <linux/cdrom.h>


/** Indication of the Linux kernel this software is running on */
/* -1 = not evaluated , 0 = unrecognizable , 1 = 2.4 , 2 = 2.6 */
static int sg_kernel_age = -1;


/** PORTING : Device file families for bus scanning and drive access.
    Both device families must support the following ioctls:
      SG_IO,
      SG_GET_SCSI_ID
      SCSI_IOCTL_GET_BUS_NUMBER
      SCSI_IOCTL_GET_IDLUN
    as well as mutual exclusively locking with open(O_EXCL).
    If a device family is left empty, then it will not be used.

    To avoid misunderstandings: both families are used via identical
    transport methods as soon as a device file is accepted as CD drive
    by the family specific function <family>_enumerate().
    One difference remains throughout usage: Host,Channel,Id,Lun and Bus
    address parameters of ATA devices are considered invalid.
*/

/* Set this to 1 in order to get on stderr messages from sg_enumerate() */
static int linux_sg_enumerate_debug = 0;


/* The device file family to use for (emulated) generic SCSI transport.
   This must be a printf formatter with one single placeholder for int
   in the range of 0 to 31 . The resulting addresses must provide SCSI
   address parameters Host, Channel, Id, Lun and also Bus.
   E.g.: "/dev/sg%d"
   sr%d is supposed to map only CD-ROM style devices. Additionally a test
   with ioctl(CDROM_DRIVE_STATUS) is made to assert that it is such a drive,

   This initial setting may be overridden in sg_select_device_family() by 
   settings made via burn_preset_device_open().
*/
static char linux_sg_device_family[80] = {"/dev/sg%d"};

/* Set this to 1 if you want the default linux_sg_device_family chosen
   depending on kernel release: sg for <2.6 , sr for >=2.6
*/
static int linux_sg_auto_family = 1;


/* Set this to 1 in order to accept any TYPE_* (see scsi/scsi.h) */
/* But try with 0 first. There is hope via CDROM_DRIVE_STATUS. */
/* !!! DO NOT SET TO 1 UNLESS YOU PROTECTED ALL INDISPENSIBLE DEVICES
       chmod -rw !!! */
static int linux_sg_accept_any_type = 0;


/* The device file family to use for SCSI transport over ATA.
   This must be a printf formatter with one single placeholder for a
   _single_ char in the range of 'a' to 'z'. This placeholder _must_ be
   at the end of the formatter string.
   E.g. "/dev/hd%c"
*/
static char linux_ata_device_family[80] = {"/dev/hd%c"};

/* Set this to 1 in order to get on stderr messages from ata_enumerate()
*/
static int linux_ata_enumerate_verbous = 0;



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

/* ts A51221 */
int burn_drive_is_banned(char *device_address);


/* ------------------------------------------------------------------------ */
/* PORTING:   Private definitions. Port only if needed by public functions. */
/*            (Public functions are listed below)                           */
/* ------------------------------------------------------------------------ */


static void enumerate_common(char *fname, int bus_no, int host_no,
			     int channel_no, int target_no, int lun_no);


/* ts A60813 : storage objects are in libburn/init.c
   wether to use O_EXCL with open(2) of devices
   wether to use fcntl(,F_SETLK,) after open(2) of devices
   what device family to use : 0=default, 1=sr, 2=scd, (3=st), 4=sg
   wether to use O_NOBLOCK with open(2) on devices
   wether to take O_EXCL rejection as fatal error */
extern int burn_sg_open_o_excl;
extern int burn_sg_fcntl_f_setlk;
extern int burn_sg_use_family;
extern int burn_sg_open_o_nonblock;
extern int burn_sg_open_abort_busy;


/* ts A60821
   debug: for tracing calls which might use open drive fds
          or for catching SCSI usage of emulated drives. */
int mmc_function_spy(struct burn_drive *d, char * text);


/* ------------------------------------------------------------------------ */
/* PORTING:   Private functions. Port only if needed by public functions    */
/*            (Public functions are listed below)                           */
/* ------------------------------------------------------------------------ */

/* ts A70413 */
/* This finds out wether the software is running on kernel >= 2.6
*/
static void sg_evaluate_kernel(void)
{
	struct utsname buf;
	if (sg_kernel_age >= 0)
		return;

	sg_kernel_age = 0;
	if (uname(&buf) == -1)
		return;
	sg_kernel_age = 1;
	if (strcmp(buf.release, "2.6") >= 0)
		sg_kernel_age = 2;
}


/* ts A70314 */
/* This installs the device file family if one was chosen explicitely
   by burn_preset_device_open()
*/
static void sg_select_device_family(void)
{

	/* >>> ??? do we need a mutex here ? */
	/* >>> (It might be concurrent but is supposed to have always
	        the same effect. Any race condition should be harmless.) */

	if (burn_sg_use_family == 1)
		strcpy(linux_sg_device_family, "/dev/sr%d");
	else if (burn_sg_use_family == 2)
		strcpy(linux_sg_device_family, "/dev/scd%d");
	else if (burn_sg_use_family == 3)
		strcpy(linux_sg_device_family, "/dev/st%d");
	else if (burn_sg_use_family == 4)
		strcpy(linux_sg_device_family, "/dev/sg%d");
	else if (linux_sg_auto_family) {
		sg_evaluate_kernel();
		if (sg_kernel_age >= 2)
			strcpy(linux_sg_device_family, "/dev/sr%d");
		else
			strcpy(linux_sg_device_family, "/dev/sg%d");
		linux_sg_auto_family = 0;
	}
}


/* ts A80701 */
/* This cares for the case that no /dev/srNN but only /dev/scdNN exists.
   A theoretical case which has its complement in SuSE 10.2 having
   /dev/sr but not /dev/scd.
*/
static int sg_exchange_scd_for_sr(char *fname, int flag)
{
	struct stat stbuf;
	char scd[17], msg[160];

	if (burn_sg_use_family != 0 || strncmp(fname, "/dev/sr", 7)!=0 ||
	    strlen(fname)>9 || strlen(fname)<8)
		return 2;
	if (fname[7] < '0' || fname[7] > '9')
		return 2;
	if (fname [8] != 0 && (fname[7] < '0' || fname[7] > '9'))
		return 2;
	if (stat(fname, &stbuf) != -1)
		return 2;
	strcpy(scd, "/dev/scd");
	strcpy(scd + 8, fname + 7);
	if (stat(scd, &stbuf) == -1)
		return 2;
	sprintf(msg, "%s substitutes for non-existent %s", scd, fname);
	libdax_msgs_submit(libdax_messenger, -1, 0x00000002,
		LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH, msg, 0, 0);
	strcpy(fname, scd);
	return 1;
}


static int sgio_test(int fd)
{
	unsigned char test_ops[] = { 0, 0, 0, 0, 0, 0 };
	sg_io_hdr_t s;

	memset(&s, 0, sizeof(sg_io_hdr_t));
	s.interface_id = 'S';
	s.dxfer_direction = SG_DXFER_NONE;
	s.cmd_len = 6;
	s.cmdp = test_ops;
	s.timeout = 12345;
	return ioctl(fd, SG_IO, &s);
}


/* ts A60924 */
static int sg_handle_busy_device(char *fname, int os_errno)
{
	char msg[4096];
	struct stat stbuf;
	int looks_like_hd= 0, fd, ret;

	/* ts A80713 :
	   check existence of /dev/hdX1 as hint for hard disk rather than CD
	   Hint by Giulio Orsero: check /proc/ide/hdX/media for "disk"
	*/
	if (strncmp(fname, "/dev/hd", 7)==0) {
		sprintf(msg, "%s1", fname);
		if (stat(msg, &stbuf) != -1)
			looks_like_hd= 1;
		sprintf(msg, "/proc/ide/hd%c/media", fname[7]);
		fd = open(msg, O_RDONLY);
		if (fd != -1) {
			ret = read(fd, msg, 10);
			if (ret < 0)
				ret = 0;
			msg[ret]= 0;
			close(fd);
			if (strncmp(msg, "disk\n", 5) == 0 ||
			    strcmp(msg, "disk") == 0)
				looks_like_hd= 2;
			else if (strncmp(msg, "cdrom\n", 6) == 0 ||
			         strcmp(msg, "cdrom") == 0)
				looks_like_hd= 0;
		}
	}

	/* ts A60814 : i saw no way to do this more nicely */ 
	if (burn_sg_open_abort_busy) {
		fprintf(stderr,
	"\nlibburn: FATAL : Application triggered abort on busy device '%s'\n",
			fname);

		/* ts A61007 */
		abort();
		/* a ssert("drive busy" == "non fatal"); */
	}

	/* ts A60924 : now reporting to libdax_msgs */
	if (looks_like_hd == 2) { /* is surely hard disk */
		;
	} else if (looks_like_hd) {
		sprintf(msg, "Could not examine busy device '%s'", fname);
		libdax_msgs_submit(libdax_messenger, -1, 0x0002015a,
				LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_LOW,
				msg, os_errno, 0);
		sprintf(msg,
	"Busy '%s' seems to be a hard disk, as '%s1' exists. But better check.",
				fname, fname);
		libdax_msgs_submit(libdax_messenger, -1, 0x0002015b,
				LIBDAX_MSGS_SEV_HINT, LIBDAX_MSGS_PRIO_LOW,
				msg, 0, 0);

	} else {
		sprintf(msg, "Cannot open busy device '%s'", fname);
		libdax_msgs_submit(libdax_messenger, -1, 0x00020001,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_LOW,
				msg, os_errno, 0);
	}
	return 1;
}


/* ts A60925 : ticket 74 */
static int sg_close_drive_fd(char *fname, int driveno, int *fd, int sorry)
{
	int ret, os_errno, sevno= LIBDAX_MSGS_SEV_DEBUG;
	char msg[4096+100];

	if(*fd < 0)
		return(0);

#ifdef CDROM_MEDIA_CHANGED_disabled_because_not_helpful
#ifdef CDSL_CURRENT
	/* ts A80217 : wondering whether the os knows about our activities */
	ret = ioctl(*fd, CDROM_MEDIA_CHANGED, CDSL_CURRENT);
	sprintf(msg, "ioctl(CDROM_MEDIA_CHANGED) == %d", ret);
	libdax_msgs_submit(libdax_messenger, driveno, 0x00000002,
		LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH, msg, 0, 0);

#ifdef BLKFLSBUF_disabled_because_not_helpful
	ret = ioctl(*fd, BLKFLSBUF, 0);
	sprintf(msg, "ioctl(BLKFLSBUF) == %d", ret);
	os_errno = 0;
	if(ret == -1)
		os_errno = errno;
	libdax_msgs_submit(libdax_messenger, driveno, 0x00000002,
		LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH, msg, os_errno,0);
#endif /* BLKFLSBUF */

#endif /* CDSL_CURRENT */
#endif /* CDROM_MEDIA_CHANGED */

	ret = close(*fd);
	*fd = -1337;
	if(ret != -1) {
		/* ts A70409 : DDLP-B */
		/* >>> release single lock on fname */
		return 1;
	}
	os_errno= errno;

	sprintf(msg, "Encountered error when closing drive '%s'", fname);
	if (sorry)
		sevno = LIBDAX_MSGS_SEV_SORRY;
	libdax_msgs_submit(libdax_messenger, driveno, 0x00020002,
			sevno, LIBDAX_MSGS_PRIO_HIGH, msg, os_errno, 0);
	return 0;	
}


/* ts A70401 : 
   fcntl() has the unappealing property to work only after open().
   So libburn will by default use open(O_EXCL) first and afterwards
   as second assertion will use fcntl(F_SETLK). One lock more should not harm.
*/
static int sg_fcntl_lock(int *fd, char *fd_name, int l_type, int verbous)
{
	struct flock lockthing;
	char msg[81];
	int ret;

	if (!burn_sg_fcntl_f_setlk)
		return 1;

	memset(&lockthing, 0, sizeof(lockthing));
	lockthing.l_type = l_type;
	lockthing.l_whence = SEEK_SET;
	lockthing.l_start = 0;
	lockthing.l_len = 0;
/*
        fprintf(stderr,"LIBBURN_EXPERIMENTAL: fcntl(%d, F_SETLK, %s)\n",
                *fd, l_type == F_WRLCK ? "F_WRLCK" : "F_RDLCK");
*/

	ret = fcntl(*fd, F_SETLK, &lockthing);
	if (ret == -1) {
		if (verbous) {
			sprintf(msg, "Device busy. Failed to fcntl-lock '%s'",
					fd_name);
			libdax_msgs_submit(libdax_messenger, -1, 0x00020008,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				msg, errno, 0);
		}
		close(*fd);
		*fd = -1;

		/* ts A70409 : DDLP-B */
		/* >>> release single lock on fd_name */

		return(0);
	}
	return 1;
}


/* ts A60926 */
static int sg_open_drive_fd(char *fname, int scan_mode)
{
	int open_mode = O_RDWR, fd, tries= 0;
	char msg[81];

	/* ts A70409 : DDLP-B */
	/* >>> obtain single lock on fname */

	/* ts A60813 - A60927
	   O_EXCL with devices is a non-POSIX feature
	   of Linux kernels. Possibly introduced 2002.
	   Mentioned in "The Linux SCSI Generic (sg) HOWTO" */
	if(burn_sg_open_o_excl)
		open_mode |= O_EXCL;
	/* ts A60813
	   O_NONBLOCK was already hardcoded in ata_ but not in sg_.
	   There must be some reason for this. So O_NONBLOCK is
	   default mode for both now. Disable on own risk.
	   ts A70411
           Switched to O_NDELAY for LKML statement 2007/4/11/141 by Alan Cox:
	   "open() has side effects. The CD layer allows you to open
            with O_NDELAY if you want to avoid them." 
	*/
	if(burn_sg_open_o_nonblock)
		open_mode |= O_NDELAY;

/* <<< debugging
	fprintf(stderr,
		"\nlibburn: experimental: o_excl= %d , o_nonblock= %d, abort_on_busy= %d\n",
	burn_sg_open_o_excl,burn_sg_open_o_nonblock,burn_sg_open_abort_busy);
	fprintf(stderr,
		"libburn: experimental: O_EXCL= %d , O_NDELAY= %d\n",
		!!(open_mode&O_EXCL),!!(open_mode&O_NDELAY));
*/

try_open:;
	fd = open(fname, open_mode);
	if (fd == -1) {
/* <<< debugging
		fprintf(stderr,
		"\nlibburn: experimental: fname= %s , errno= %d\n",
			fname,errno);
*/
		if (errno == EBUSY) {
			tries++;

/* <<< debugging
			fprintf(stderr,
				"\nlibburn_DEBUG: EBUSY , tries= %d\n", tries);
*/

			if (tries < 4) {
				usleep(2000000);
				goto try_open;
			}
			sg_handle_busy_device(fname, errno);
			return -1;
			
		}
		if (scan_mode)
			return -1;
		sprintf(msg, "Failed to open device '%s'",fname);
		libdax_msgs_submit(libdax_messenger, -1, 0x00020005,
				LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
				msg, errno, 0);
		return -1;
	}
	sg_fcntl_lock(&fd, fname, F_WRLCK, 1);
	return fd;
}


/* ts A60926 */
static int sg_release_siblings(int sibling_fds[],
				char sibling_fnames[][BURN_OS_SG_MAX_NAMELEN],
				int *sibling_count)
{
	int i;
	char msg[81];

	for(i= 0; i < *sibling_count; i++)
		sg_close_drive_fd(sibling_fnames[i], -1, &(sibling_fds[i]), 0);
	if(*sibling_count > 0) {
		sprintf(msg, "Closed %d O_EXCL scsi siblings", *sibling_count);
		libdax_msgs_submit(libdax_messenger, -1, 0x00020007,
			LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_HIGH, msg, 0,0);
	}
	*sibling_count = 0;
	return 1;
}


/* ts A60926 */
static int sg_close_drive(struct burn_drive *d)
{
	int ret;

	if (!burn_drive_is_open(d))
		return 0;
	sg_release_siblings(d->sibling_fds, d->sibling_fnames,
				&(d->sibling_count));
	ret = sg_close_drive_fd(d->devname, d->global_index, &(d->fd), 0);
	return ret;
}


/* ts A60926 */
static int sg_open_scsi_siblings(char *path, int driveno,
			int sibling_fds[],
			char sibling_fnames[][BURN_OS_SG_MAX_NAMELEN],
			int *sibling_count,
			int host_no, int channel_no, int id_no, int lun_no)
{
	int tld, i, ret, fd, i_bus_no = -1;
	int i_host_no = -1, i_channel_no = -1, i_target_no = -1, i_lun_no = -1;
	char msg[161], fname[81];
	struct stat stbuf;
	dev_t last_rdev = 0, path_rdev;

	static char tldev[][81]= {"/dev/sr%d", "/dev/scd%d", "/dev/sg%d", ""};
					/* ts A70609: removed "/dev/st%d" */

	if(stat(path, &stbuf) == -1)
		return 0;
	path_rdev = stbuf.st_rdev;

        sg_select_device_family();
	if (linux_sg_device_family[0] == 0)
		return 1;

	if(host_no < 0 || id_no < 0 || channel_no < 0 || lun_no < 0)
		return(2);
	if(*sibling_count > 0)
		sg_release_siblings(sibling_fds, sibling_fnames,
					sibling_count);
		
	for (tld = 0; tldev[tld][0] != 0; tld++) {
		if (strcmp(tldev[tld], linux_sg_device_family)==0)
	continue;
		for (i = 0; i < 32; i++) {
			sprintf(fname, tldev[tld], i);
			if(stat(fname, &stbuf) == -1)
		continue;
			if (path_rdev == stbuf.st_rdev)
		continue;
			if (*sibling_count > 0 && last_rdev == stbuf.st_rdev)
		continue;
			ret = sg_obtain_scsi_adr(fname, &i_bus_no, &i_host_no,
				&i_channel_no, &i_target_no, &i_lun_no);
			if (ret <= 0)
		continue;
			if (i_host_no != host_no || i_channel_no != channel_no)
		continue;
			if (i_target_no != id_no || i_lun_no != lun_no)
		continue;

			fd = sg_open_drive_fd(fname, 0);
			if (fd < 0)
				goto failed;

			if (*sibling_count>=BURN_OS_SG_MAX_SIBLINGS) {
				sprintf(msg, "Too many scsi siblings of '%s'",
					path);
				libdax_msgs_submit(libdax_messenger,
					driveno, 0x00020006,
					LIBDAX_MSGS_SEV_FATAL,
					LIBDAX_MSGS_PRIO_HIGH, msg, 0, 0);
				goto failed;
			}
			sprintf(msg, "Opened O_EXCL scsi sibling '%s' of '%s'",
				fname, path);
			libdax_msgs_submit(libdax_messenger, driveno,
				0x00020004,
				LIBDAX_MSGS_SEV_NOTE, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
			sibling_fds[*sibling_count] = fd;
			strcpy(sibling_fnames[*sibling_count], fname);
			(*sibling_count)++;
			last_rdev= stbuf.st_rdev;
		}
	}
	return 1;
failed:;
	sg_release_siblings(sibling_fds, sibling_fnames, sibling_count);
	return 0;
}


#define Libburn_drive_new_deaL 1

#ifdef Libburn_drive_new_deaL

/* ts A80731 */
static int is_ata_drive(char *fname)
{
	int fd;
	struct hd_driveid tm;

	fd = sg_open_drive_fd(fname, 1);
	if (fd == -1) {
		if (linux_ata_enumerate_verbous)
			fprintf(stderr,"open failed, errno=%d  '%s'\n",
				errno, strerror(errno));
		return 0;
	}

	memset(&tm, 0, sizeof(tm));
	ioctl(fd, HDIO_GET_IDENTITY, &tm);

		/* not atapi */
	if (!(tm.config & 0x8000) || (tm.config & 0x4000)) {
		if (linux_ata_enumerate_verbous)
			fprintf(stderr, "not marked as ATAPI\n");
		sg_close_drive_fd(fname, -1, &fd, 0);
		return 0;
	}

	/* if SG_IO fails on an atapi device, we should stop trying to 
	   use hd* devices */
	if (sgio_test(fd) == -1) {
		if (linux_ata_enumerate_verbous)
		  fprintf(stderr,
			 "FATAL: sgio_test() failed: errno=%d  '%s'\n",
			 errno, strerror(errno));
		sg_close_drive_fd(fname, -1, &fd, 0);
		return 0;
	}
	if (sg_close_drive_fd(fname, -1, &fd, 1) <= 0) {
		if (linux_ata_enumerate_verbous)
			fprintf(stderr,
				"cannot close properly, errno=%d  '%s'\n",
				errno, strerror(errno));
		return 0;
	}
	return 1;
}


static int is_scsi_drive(char *fname, int *bus_no, int *host_no,
			 int *channel_no, int *target_no, int *lun_no)
{
	int fd, sid_ret = 0, ret;
	struct sg_scsi_id sid;
	int sibling_fds[BURN_OS_SG_MAX_SIBLINGS], sibling_count= 0;
	char sibling_fnames[BURN_OS_SG_MAX_SIBLINGS][BURN_OS_SG_MAX_NAMELEN];

	fd = sg_open_drive_fd(fname, 1);
	if (fd == -1) {
		if (linux_sg_enumerate_debug)
			fprintf(stderr, "open failed, errno=%d  '%s'\n",
				errno, strerror(errno));
		return 0;
	}

	sid_ret = ioctl(fd, SG_GET_SCSI_ID, &sid);
	if (sid_ret == -1) {
		sid.scsi_id = -1; /* mark SCSI address as invalid */
		if(linux_sg_enumerate_debug) 
			fprintf(stderr,
			"ioctl(SG_GET_SCSI_ID) failed, errno=%d  '%s' , ",
			errno, strerror(errno));

		if (sgio_test(fd) == -1) {
			if (linux_sg_enumerate_debug)
		  		fprintf(stderr,
				 "FATAL: sgio_test() failed: errno=%d  '%s'",
				errno, strerror(errno));

			sg_close_drive_fd(fname, -1, &fd, 0);
			return 0;
		}

#ifdef CDROM_DRIVE_STATUS
		/* http://developer.osdl.org/dev/robustmutexes/
			  src/fusyn.hg/Documentation/ioctl/cdrom.txt */
		sid_ret = ioctl(fd, CDROM_DRIVE_STATUS, 0);
		if(linux_sg_enumerate_debug)
			  fprintf(stderr,
				"ioctl(CDROM_DRIVE_STATUS) = %d , ",
				sid_ret);
		if (sid_ret != -1 && sid_ret != CDS_NO_INFO)
			sid.scsi_type = TYPE_ROM;
		else
			sid_ret = -1;
#endif /* CDROM_DRIVE_STATUS */

	}

#ifdef SCSI_IOCTL_GET_BUS_NUMBER
	/* Hearsay A61005 */
	if (ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, bus_no) == -1)
		*bus_no = -1;
#endif

	if (sg_close_drive_fd(fname, -1, &fd, 
					sid.scsi_type == TYPE_ROM ) <= 0) {
		if (linux_sg_enumerate_debug)
			fprintf(stderr,
				"cannot close properly, errno=%d  '%s'\n",
				errno, strerror(errno)); 
		return 0;
	}
	if ( (sid_ret == -1 || sid.scsi_type != TYPE_ROM)
	     && !linux_sg_accept_any_type) {
		if (linux_sg_enumerate_debug)
			fprintf(stderr, "sid.scsi_type = %d (!= TYPE_ROM)\n",
				sid.scsi_type); 
		return 0;
	}

	if (sid_ret == -1 || sid.scsi_id < 0) {
		/* ts A61211 : employ a more general ioctl */
		ret = sg_obtain_scsi_adr(fname, bus_no, host_no,
					   channel_no, target_no, lun_no);
		if (ret>0) {
			sid.host_no = *host_no;
			sid.channel = *channel_no;
			sid.scsi_id = *target_no;
			sid.lun = *lun_no;
		} else {
			if (linux_sg_enumerate_debug)
				fprintf(stderr,
					"sg_obtain_scsi_adr() failed\n");
			return 0;
		}
	}

	/* ts A60927 : trying to do locking with growisofs */
	if(burn_sg_open_o_excl>1) {
		ret = sg_open_scsi_siblings(
				fname, -1, sibling_fds, sibling_fnames,
				&sibling_count,
				sid.host_no, sid.channel,
				sid.scsi_id, sid.lun);
		if (ret<=0) {
			if (linux_sg_enumerate_debug)
				fprintf(stderr, "cannot lock siblings\n"); 
			sg_handle_busy_device(fname, 0);
			return 0;
		}
		/* the final occupation will be done in sg_grab() */
		sg_release_siblings(sibling_fds, sibling_fnames,
							&sibling_count);
	}
#ifdef SCSI_IOCTL_GET_BUS_NUMBER
	if(*bus_no == -1)
		*bus_no = 1000 * (sid.host_no + 1) + sid.channel;
#else
	*bus_no = sid.host_no;
#endif
	*host_no= sid.host_no;
	*channel_no= sid.channel;
	*target_no= sid.scsi_id;
	*lun_no= sid.lun;
	return 1;
}	

#endif /* Libburn_drive_new_deaL */


/** Speciality of Linux: detect non-SCSI ATAPI (EIDE) which will from
   then on used used via generic SCSI as is done with (emulated) SCSI drives */ 
static void ata_enumerate(void)
{

#ifdef Libburn_drive_new_deaL

	int ret;

#else 
	struct hd_driveid tm;
	int fd;
#endif

	int i;
	char fname[10];

	if (linux_ata_enumerate_verbous)
	  fprintf(stderr, "libburn_debug: linux_ata_device_family = %s\n",
		  linux_ata_device_family);

	if (linux_ata_device_family[0] == 0)
		return;

	for (i = 0; i < 26; i++) {
		sprintf(fname, linux_ata_device_family, 'a' + i);
		if (linux_ata_enumerate_verbous)
		  fprintf(stderr, "libburn_debug: %s : ", fname);

		/* ts A51221 */
		if (burn_drive_is_banned(fname)) {
			if (linux_ata_enumerate_verbous)
				fprintf(stderr, "not in whitelist\n");
	continue;
		}

#ifdef Libburn_drive_new_deaL

		ret = is_ata_drive(fname);
		if (ret < 0)
	break;
		if (ret == 0)
	continue;

#else /* Libburn_drive_new_deaL */

		fd = sg_open_drive_fd(fname, 1);
		if (fd == -1) {
			if (linux_ata_enumerate_verbous)
				fprintf(stderr,"open failed, errno=%d  '%s'\n",
					errno, strerror(errno));
	continue;
		}

		/* found a drive */
		ioctl(fd, HDIO_GET_IDENTITY, &tm);

		/* not atapi */
		if (!(tm.config & 0x8000) || (tm.config & 0x4000)) {
			if (linux_ata_enumerate_verbous)
				fprintf(stderr, "not marked as ATAPI\n");
			sg_close_drive_fd(fname, -1, &fd, 0);
	continue;
		}

		/* if SG_IO fails on an atapi device, we should stop trying to 
		   use hd* devices */
		if (sgio_test(fd) == -1) {
			if (linux_ata_enumerate_verbous)
			  fprintf(stderr,
				 "FATAL: sgio_test() failed: errno=%d  '%s'\n",
				 errno, strerror(errno));
			sg_close_drive_fd(fname, -1, &fd, 0);
			return;
		}
		if (sg_close_drive_fd(fname, -1, &fd, 1) <= 0) {
			if (linux_ata_enumerate_verbous)
			  fprintf(stderr,
				"cannot close properly, errno=%d  '%s'\n",
				errno, strerror(errno));
	continue;
		}

#endif /* Libburn_drive_new_deaL */


		if (linux_ata_enumerate_verbous)
		  fprintf(stderr, "accepting as drive without SCSI address\n");
		enumerate_common(fname, -1, -1, -1, -1, -1);
	}
}


/** Detects (probably emulated) SCSI drives */
static void sg_enumerate(void)
{
#ifdef Libburn_drive_new_deaL

#else 
	struct sg_scsi_id sid;
	int fd, sibling_fds[BURN_OS_SG_MAX_SIBLINGS], sibling_count= 0;
	char sibling_fnames[BURN_OS_SG_MAX_SIBLINGS][BURN_OS_SG_MAX_NAMELEN];
	int sid_ret = 0;
#endif

	int i, ret;
	int bus_no= -1, host_no= -1, channel_no= -1, target_no= -1, lun_no= -1;
	char fname[17];

        sg_select_device_family();

	if (linux_sg_enumerate_debug)
	  fprintf(stderr, "libburn_debug: linux_sg_device_family = %s\n",
		  linux_sg_device_family);

	if (linux_sg_device_family[0] == 0)
		return;

	for (i = 0; i < 32; i++) {
		sprintf(fname, linux_sg_device_family, i);

		/* ts A80702 */
		sg_exchange_scd_for_sr(fname, 0);

		if (linux_sg_enumerate_debug)
		  fprintf(stderr, "libburn_debug: %s : ", fname);

		/* ts A51221 */
		if (burn_drive_is_banned(fname)) {
			if (linux_sg_enumerate_debug)
			  fprintf(stderr, "not in whitelist\n"); 
	continue;
		}


#ifdef Libburn_drive_new_deaL

		ret = is_scsi_drive(fname, &bus_no, &host_no, &channel_no,
							&target_no, &lun_no);
		if (ret < 0)
	break;
		if (ret == 0)
	continue;
		if (linux_sg_enumerate_debug)
		  fprintf(stderr, "accepting as SCSI %d,%d,%d,%d bus=%d\n",
			  host_no, channel_no, target_no, lun_no, bus_no);
		enumerate_common(fname, bus_no, host_no, channel_no, 
				target_no, lun_no);

#else /* Libburn_drive_new_deaL */

		/* ts A60927 */
		fd = sg_open_drive_fd(fname, 1);
		if (fd == -1) {
			if (linux_sg_enumerate_debug)
			  fprintf(stderr, "open failed, errno=%d  '%s'\n",
				  errno, strerror(errno));
	continue;
		}

		/* found a drive */
		sid_ret = ioctl(fd, SG_GET_SCSI_ID, &sid);
		if (sid_ret == -1) {
			sid.scsi_id = -1; /* mark SCSI address as invalid */
			if(linux_sg_enumerate_debug) 
		  	  fprintf(stderr,
			    "ioctl(SG_GET_SCSI_ID) failed, errno=%d  '%s' , ",
			    errno, strerror(errno));

			if (sgio_test(fd) == -1) {
				if (linux_sg_enumerate_debug)
			  		fprintf(stderr,
				 "FATAL: sgio_test() failed: errno=%d  '%s'",
						errno, strerror(errno));

				sg_close_drive_fd(fname, -1, &fd, 0);
	continue;
			}

#ifdef CDROM_DRIVE_STATUS
			/* ts A61211 : not widening old acceptance range */
			if (strcmp(linux_sg_device_family,"/dev/sg%d") != 0) {
				/* http://developer.osdl.org/dev/robustmutexes/
				  src/fusyn.hg/Documentation/ioctl/cdrom.txt */
				sid_ret = ioctl(fd, CDROM_DRIVE_STATUS, 0);
				if(linux_sg_enumerate_debug)
				  fprintf(stderr,
					"ioctl(CDROM_DRIVE_STATUS) = %d , ",
					sid_ret);
				if (sid_ret != -1 && sid_ret != CDS_NO_INFO)
					sid.scsi_type = TYPE_ROM;
				else
					sid_ret = -1;
			}
#endif /* CDROM_DRIVE_STATUS */

		}

#ifdef SCSI_IOCTL_GET_BUS_NUMBER
		/* Hearsay A61005 */
		if (ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, &bus_no) == -1)
			bus_no = -1;
#endif

		if (sg_close_drive_fd(fname, -1, &fd, 
				sid.scsi_type == TYPE_ROM ) <= 0) {
			if (linux_sg_enumerate_debug)
			  fprintf(stderr,
				  "cannot close properly, errno=%d  '%s'\n",
				  errno, strerror(errno)); 
	continue;
		}
		if ( (sid_ret == -1 || sid.scsi_type != TYPE_ROM)
		     && !linux_sg_accept_any_type) {
			if (linux_sg_enumerate_debug)
			  fprintf(stderr, "sid.scsi_type = %d (!= TYPE_ROM)\n",
				sid.scsi_type); 
	continue;
		}

		if (sid_ret == -1 || sid.scsi_id < 0) {
			/* ts A61211 : employ a more general ioctl */
			ret = sg_obtain_scsi_adr(fname, &bus_no, &host_no,
					   &channel_no, &target_no, &lun_no);
			if (ret>0) {
				sid.host_no = host_no;
				sid.channel = channel_no;
				sid.scsi_id = target_no;
				sid.lun = lun_no;
			} else {
				if (linux_sg_enumerate_debug)
				  fprintf(stderr,
					"sg_obtain_scsi_adr() failed\n");
	continue;
			}
                }

		/* ts A60927 : trying to do locking with growisofs */
		if(burn_sg_open_o_excl>1) {
			ret = sg_open_scsi_siblings(
					fname, -1, sibling_fds, sibling_fnames,
					&sibling_count,
					sid.host_no, sid.channel,
					sid.scsi_id, sid.lun);
			if (ret<=0) {
				if (linux_sg_enumerate_debug)
				  fprintf(stderr, "cannot lock siblings\n"); 
				sg_handle_busy_device(fname, 0);
	continue;
			}
			/* the final occupation will be done in sg_grab() */
			sg_release_siblings(sibling_fds, sibling_fnames,
						&sibling_count);
		}
#ifdef SCSI_IOCTL_GET_BUS_NUMBER
		if(bus_no == -1)
			bus_no = 1000 * (sid.host_no + 1) + sid.channel;
#else
		bus_no = sid.host_no;
#endif

		if (linux_sg_enumerate_debug)
		  fprintf(stderr, "accepting as SCSI %d,%d,%d,%d bus=%d\n",
			  sid.host_no, sid.channel, sid.scsi_id, sid.lun,
			  bus_no);
		enumerate_common(fname, bus_no, sid.host_no, sid.channel, 
				 sid.scsi_id, sid.lun);
#endif /* Libburn_drive_new_deaL */

	}
}


#ifdef Libburn_drive_new_deaL


/* ts A80805 : eventually produce the other official name of a device file */
static int fname_other_name(char *fname, char other_name[80], int flag)
{
	if(strncmp(fname, "/dev/sr", 7) == 0 &&
	   (fname[7] >= '0' && fname[7] <= '9') &&
           (fname[8] == 0 ||
	    (fname[8] >= '0' && fname[8] <= '9' && fname[9] == 0))) {
		sprintf(other_name, "/dev/scd%s", fname + 7);
		return 1;
	}
	if(strncmp(fname, "/dev/scd", 8) == 0 &&
	   (fname[8] >= '0' && fname[8] <= '9') &&
           (fname[9] == 0 ||
	    (fname[9] >= '0' && fname[9] <= '9' && fname[10] == 0))) {
		sprintf(other_name, "/dev/sr%s", fname + 8);
		return 1;
	}
	return 0;
}


/* ts A80805 */
static int fname_drive_is_listed(char *fname, int flag)
{
	char other_fname[80];

	if (burn_drive_is_listed(fname, NULL, 0))
		return 1;
	if (fname_other_name(fname, other_fname, 0) > 0)
		if (burn_drive_is_listed(other_fname, NULL, 0))
			return 2;
	return 0;
}


/* ts A80731 : Directly open the given address.
   @param flag bit0= do not complain about missing file
               bit1= do not check whether drive is already listed
*/
static int fname_enumerate(char *fname, int flag)
{
	int is_ata= 0, is_scsi= 0;
	int bus_no= -1, host_no= -1, channel_no= -1, target_no= -1, lun_no= -1;
	char msg[BURN_DRIVE_ADR_LEN + 80];
	struct stat stbuf;

	if (!(flag & 2))
		if (fname_drive_is_listed(fname, 0))
			return 2;
	if (stat(fname, &stbuf) == -1) {
		sprintf(msg, "File object '%s' not found", fname);
		if (!(flag & 1))
			libdax_msgs_submit(libdax_messenger, -1, 0x0002000b,
			  LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
			  msg, 0, 0);
		return -1;
	}
	
	is_ata = is_ata_drive(fname);
	if (is_ata < 0)
		return -1;
	if (!is_ata)
		is_scsi = is_scsi_drive(fname, &bus_no, &host_no, &channel_no,
					&target_no, &lun_no);
	if (is_scsi < 0)
		return -1;
	if (is_ata == 0 && is_scsi == 0)
		return 0;

	if (linux_sg_enumerate_debug)
		  fprintf(stderr,
			"(single) accepting as SCSI %d,%d,%d,%d bus=%d\n",
			host_no, channel_no, target_no, lun_no, bus_no);
	enumerate_common(fname, bus_no, host_no, channel_no, 
				target_no, lun_no);
	return 1;
}


/* ts A80731 : Directly open the given address from a single-item whitlist */
static int single_enumerate(int flag)
{
	int ret, wl_count;
	char *fname, msg[BURN_DRIVE_ADR_LEN + 80];

        wl_count= burn_drive_whitelist_count();
	if (wl_count != 1)
		return 0;
	fname= burn_drive_whitelist_item(0, 0);
	if (fname == NULL)
		return 0;
	ret = fname_enumerate(fname, 2);
	if (ret <= 0) {
		sprintf(msg, "Cannot access '%s' as SG_IO CDROM drive", fname);
		libdax_msgs_submit(libdax_messenger, -1, 0x0002000a,
			LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
			msg, 0, 0);
		ret = -1;
	}
	return ret;
}


/* ts A80801 : looking up drives listed in /proc/sys/dev/cdrom/info line like:
                drive name:             sr1     hdc     hda     sr0
   @parm flag bit0= release list memory and exit
*/
static int proc_sys_dev_cdrom_info(char ***list, int *count, int flag)
{
	FILE *fp;
	char line[1024], fname[1024 + 5], *cpt, *retpt, *list_data;
	int maxl= 0, pass, i;

	if (*list != NULL) {
		if ((*list)[0] != NULL)
			free((*list)[0]);
		free(*list);
		*list = NULL;
		*count = 0;
	}
	if (flag & 1) 
		return 1;

	*count = 0;
	sg_evaluate_kernel();
	if (sg_kernel_age < 2) /* addresses are not suitable for kernel 2.4 */
		return 1;
	fp = fopen("/proc/sys/dev/cdrom/info", "r");
	if (fp == NULL)
		return 0;
	while (1) {
		retpt = fgets(line, sizeof(line), fp);
		if (retpt == NULL)
	break;
		if(strncmp(line, "drive name:", 11) == 0)
	break;
	}
	fclose(fp);
	if (retpt == NULL)
		return 0;
	strcpy(fname, "/dev/");
	for(pass = 0; pass < 2; pass++) {
		*count = 0;
		cpt = line + 11;
		while (*cpt != 0) {
			for(; *cpt == ' ' || *cpt == '\t'; cpt++);
			if (*cpt == 0 || *cpt == '\n')
		break;
			sscanf(cpt, "%s", fname + 5);
			if (strlen(fname) > maxl)
				maxl = strlen(fname);
			if (pass == 1)
				strcpy((*list)[*count], fname);
			(*count)++;
			for(cpt++; *cpt != ' ' && *cpt != '\t'
					 && *cpt != 0 && *cpt != '\n'; cpt++);
		}
		if (pass == 0) {
			list_data = calloc(*count + 1, maxl+1);
			*list = calloc(*count + 1, sizeof(char *));
			if(list_data == NULL || *list == NULL) {
				libdax_msgs_submit(libdax_messenger, -1,
				0x00000003,
				LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				"Out of virtual memory", 0, 0);
				if (list_data != NULL)
					free(list_data);
				if (*list != NULL)
					free((char *) *list);
				return -1;
			}
			for (i = 0; i <= *count; i++)
				(*list)[i] = list_data + i * (maxl + 1);
		}
	}
	return 1;
}


static int add_proc_info_drives(int flag)
{
	int ret, list_count, count = 0, i;
	char **list= NULL;

	ret = proc_sys_dev_cdrom_info(&list, &list_count, 0);
	if (ret <= 0)
		return ret;
	for (i = 0; i < list_count; i++) {
		if (burn_drive_is_banned(list[i]))
	continue;
		ret = fname_enumerate(list[i], 1);
		if (ret == 1)
			count++;
	}
	proc_sys_dev_cdrom_info(&list, &list_count, 1); /* free memory */
	return 1 + count;
}

#endif /* Libburn_drive_new_deaL */


/* ts A61115 */
/* ----------------------------------------------------------------------- */
/* PORTING: Private functions which contain publicly needed functionality. */
/*          Their portable part must be performed. So it is probably best  */
/*          to replace the non-portable part and to call these functions   */
/*          in your port, too.                                             */
/* ----------------------------------------------------------------------- */


/** Wraps a detected drive into libburn structures and hands it over to
    libburn drive list.
*/
/* ts A60923 - A61005 : introduced new SCSI parameters */
/* ts A61021 : moved non os-specific code to spc,sbc,mmc,drive */
static void enumerate_common(char *fname, int bus_no, int host_no,
			     int channel_no, int target_no, int lun_no)
{
	int ret, i;
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

	/* Operating system adapter is Linux Generic SCSI (sg) */
	/* Adapter specific handles and data */
	out.fd = -1337;
	out.sibling_count = 0;
	for(i= 0; i<BURN_OS_SG_MAX_SIBLINGS; i++)
		out.sibling_fds[i] = -1337;

	/* PORTING: ---------------- end of non portable part ------------ */

	/* Adapter specific functions with standardized names */
	out.grab = sg_grab;
	out.release = sg_release;
	out.drive_is_open= sg_drive_is_open;
	out.issue_command = sg_issue_command;

	/* Finally register drive and inquire drive information */
	burn_drive_finish_enum(&out);
}


/* ts A61115 */
/* ------------------------------------------------------------------------ */
/* PORTING:           Public functions. These MUST be ported.               */
/* ------------------------------------------------------------------------ */


/** PORTING:
    In this Linux implementation, this function mirrors the enumeration
    done in sg_enumerate and ata_enumerate(). It would be better to base those
    functions on this sg_give_next_adr() but the situation is not inviting. 
*/
/* ts A60922 ticket 33 : called from drive.c */
/** Returns the next index number and the next enumerated drive address.
    The enumeration has to cover all available and accessible drives. It is
    allowed to return addresses of drives which are not available but under
    some (even exotic) circumstances could be available. It is on the other
    hand allowed, only to hand out addresses which can really be used right
    in the moment of this call. (This implementation chooses the former.)
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
	/* os-linux.h : typedef int burn_drive_enumerator_t; */
	static int sg_limit = 32, ata_limit = 26;
	int baseno = 0, i;
	char other_name[80];

	if (initialize == -1) {
		proc_sys_dev_cdrom_info(&(idx->info_list), &(idx->info_count),
					1);
		return 0;
	}

        sg_select_device_family();
	if (linux_sg_device_family[0] == 0)
		sg_limit = 0;
	if (linux_ata_device_family[0] == 0)
		ata_limit = 0;

	if (initialize  == 1) {
		idx->pos = -1;
		idx->info_count= 0;
		idx->info_list= NULL;
		proc_sys_dev_cdrom_info(&(idx->info_list), &(idx->info_count),
					0);
	}
	(idx->pos)++;
	if (idx->pos >= sg_limit)
		goto next_ata;
	if (adr_size < 11)
		return -1;
	sprintf(adr, linux_sg_device_family, idx->pos);

	sg_exchange_scd_for_sr(adr, 0);
	goto return_1_pre_proc;

next_ata:;
	baseno += sg_limit;
	if (idx->pos - baseno >= ata_limit)
		goto next_proc_info;
	if (adr_size < 9)
		return -1;
	sprintf(adr, linux_ata_device_family, 'a' + (idx->pos - baseno));
	goto return_1_pre_proc;

next_proc_info:;
	baseno += ata_limit;
	for (i = 0; i < idx->info_count; i++) {
		if ((idx->info_list)[i][0] == 0)
	continue;
		if (baseno == idx->pos) {
			if (adr_size < strlen((idx->info_list)[i]) + 1)
				return -1;
			strcpy(adr, (idx->info_list)[i]);
			return 1;
		}
		baseno++;
	}
	return 0;

return_1_pre_proc:;
	for (i = 0; i < idx->info_count; i++) {
		if (strcmp((idx->info_list)[i], adr) == 0)
			(idx->info_list)[i][0] = 0;
	        if (fname_other_name(adr, other_name, 0) > 0)
			if (strcmp((idx->info_list)[i], other_name) == 0)
				(idx->info_list)[i][0] = 0;
	}
	return 1;
}


/** Brings all available, not-whitelist-banned, and accessible drives into
    libburn's list of drives.
*/
/** PORTING:
    If not stricken with an incompletely unified situation like in Linux
    one would rather implement this by a loop calling sg_give_next_adr().
    If needed with your sg_give_next_adr() results, do a test for existence
    and accessability. If burn activities are prone to external interference
    on your system it is also necessary to obtain exclusive access locks on
    the drives.
    Hand over each accepted drive to enumerate_common() resp. its replacement
    within your port.

    See FreeBSD port sketch sg-freebsd-port.c for such an implementation.
*/
/* ts A61115: replacing call to sg-implementation internals from drive.c */
int scsi_enumerate_drives(void)
{

#ifdef Libburn_drive_new_deaL
	int ret;
#endif

#ifdef Libburn_drive_new_deaL
	/* Direct examination of eventually single whitelisted name */
	ret = single_enumerate(0);
	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;
#endif /* Libburn_drive_new_deaL */

	sg_enumerate();
	ata_enumerate();

#ifdef Libburn_drive_new_deaL
	add_proc_info_drives(0);
#endif /* Libburn_drive_new_deaL */

	return 1;
}


/** Tells wether libburn has the given drive in use or exclusively reserved.
    If it is "open" then libburn will eventually call sg_release() on it when
    it is time to give up usage resp. reservation.
*/
/** Published as burn_drive.drive_is_open() */
int sg_drive_is_open(struct burn_drive * d)
{
	/* a bit more detailed case distinction than needed */
	if (d->fd == -1337)
		return 0;
	if (d->fd < 0)
		return 0;
	return 1;
}


/** Opens the drive for SCSI commands and - if burn activities are prone
    to external interference on your system - obtains an exclusive access lock
    on the drive. (Note: this is not physical tray locking.)
    A drive that has been opened with sg_grab() will eventually be handed
    over to sg_release() for closing and unreserving.
*/
int sg_grab(struct burn_drive *d)
{
	int fd, os_errno= 0, ret;

	/* ts A60813 */
	int open_mode = O_RDWR;

/* ts A60821
   <<< debug: for tracing calls which might use open drive fds */
	if (mmc_function_spy(d, "sg_grab") <= 0)
		return 0;


	/* ts A60813 - A60927
	   O_EXCL with devices is a non-POSIX feature
	   of Linux kernels. Possibly introduced 2002.
	   Mentioned in "The Linux SCSI Generic (sg) HOWTO".
	*/
	if(burn_sg_open_o_excl)
		open_mode |= O_EXCL;

	/* ts A60813
	   O_NONBLOCK was hardcoded here. So it should stay default mode.
	   ts A70411
           Switched to O_NDELAY for LKML statement 2007/4/11/141
	*/
	if(burn_sg_open_o_nonblock)
		open_mode |= O_NDELAY;

	/* ts A60813 - A60822
	   After enumeration the drive fd is probably still open.
	   -1337 is the initial value of burn_drive.fd and the value after
	   relase of drive. Unclear why not the official error return
	   value -1 of open(2) war used. */
	if(! burn_drive_is_open(d)) {

		/* ts A60821
   		<<< debug: for tracing calls which might use open drive fds */
		mmc_function_spy(NULL, "sg_grab ----------- opening");

		/* ts A70409 : DDLP-B */
		/* >>> obtain single lock on d->devname */

		/* ts A60926 */
		if(burn_sg_open_o_excl>1) {
			fd = -1;
			ret = sg_open_scsi_siblings(d->devname,
					d->global_index,d->sibling_fds,
					d->sibling_fnames,&(d->sibling_count),
					d->host, d->channel, d->id, d->lun);
			if(ret <= 0)
				goto drive_is_in_use;
		}

		fd = open(d->devname, open_mode);
		os_errno = errno;
		if (fd >= 0) {
			sg_fcntl_lock(&fd, d->devname, F_WRLCK, 1);
			if (fd < 0)
				goto drive_is_in_use;
		}
	} else
		fd= d->fd;

	if (fd >= 0) {
		d->fd = fd;
		fcntl(fd, F_SETOWN, getpid());
		d->released = 0;
		return 1;
	}
	libdax_msgs_submit(libdax_messenger, d->global_index, 0x00020003,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Could not grab drive", os_errno, 0);
	return 0;

drive_is_in_use:;
	libdax_msgs_submit(libdax_messenger, d->global_index,
			0x00020003,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Could not grab drive - already in use", 0, 0);
	sg_close_drive(d);
	d->fd = -1337;
	return 0;
}


/** PORTING: Is mainly about the call to sg_close_drive() and wether it
             implements the demanded functionality.
*/
/** Gives up the drive for SCSI commands and releases eventual access locks.
    (Note: this is not physical tray locking.)
*/
int sg_release(struct burn_drive *d)
{
	/* ts A60821
   	<<< debug: for tracing calls which might use open drive fds */
	if (mmc_function_spy(d, "sg_release") <= 0)
		return 0;

	if (d->fd < 1) {
		burn_print(1, "release an ungrabbed drive.  die\n");
		return 0;
	}

	/* ts A60821
   	<<< debug: for tracing calls which might use open drive fds */
	mmc_function_spy(NULL, "sg_release ----------- closing");

	sg_close_drive(d);
	return 0;
}


/** ts A70518: 
    Debugging log facility. Controlled by existence of macros:
     Libburn_log_sg_commandS          enables logging to file
                                        /tmp/libburn_sg_command_log
     Libburn_fflush_log_sg_commandS   enables fflush after each output line
     Libburn_log_sg_command_stderR    enables additional log to stderr
*/
/*
#define Libburn_log_sg_commandS 1
#define Libburn_fflush_log_sg_commandS 1
#define Libburn_log_sg_command_stderR 1
*/

#ifdef Libburn_log_sg_commandS

/** Logs command (before execution) */
static int sg_log_cmd(struct command *c, FILE *fp, int flag)
{
	int i;

	if (fp != NULL) {
		for(i = 0; i < 16 && i < c->oplen; i++)
  			fprintf(fp,"%2.2x ", c->opcode[i]);
		fprintf(fp, "\n");
#ifdef Libburn_fflush_log_sg_commandS
		fflush(fp);
#endif
	}
	if (fp == stderr)
		return 1;
#ifdef Libburn_log_sg_command_stderR
	sg_log_cmd(c, stderr, flag);
#endif
	return 1;
}


/** logs outcome of a sg command. flag&1 causes an error message */
static int sg_log_err(struct command *c, FILE *fp, 
		sg_io_hdr_t *s,
		int flag)
{
      	if(fp!=NULL) {
		if(flag & 1)
  			fprintf(fp,
			"+++ key=%X  asc=%2.2Xh  ascq=%2.2Xh   (%6d ms)\n",
				s->sbp[2], s->sbp[12], s->sbp[13],s->duration);
		else
			fprintf(fp,"%6d ms\n", s->duration);
#ifdef Libburn_fflush_log_sg_commandS
		fflush(fp);
#endif
	}
	if (fp == stderr)
		return 1;
#ifdef Libburn_log_sg_command_stderR
	sg_log_err(c, stderr, s, flag);
#endif
	return 1;
}


#endif /* Libburn_log_sg_commandS */


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
	int done = 0, no_c_page = 0;
	int err;
	sg_io_hdr_t s;

#ifdef Libburn_log_sg_commandS
	/* ts A61030 */
	static FILE *fp= NULL;
#endif /* Libburn_log_sg_commandS */

	/* <<< ts A60821
	   debug: for tracing calls which might use open drive fds */
	char buf[161];
	sprintf(buf,"sg_issue_command   d->fd= %d  d->released= %d\n",
		d->fd,d->released);
	mmc_function_spy(NULL, buf);

#ifdef Libburn_log_sg_commandS
	/* ts A61030 */
	if(fp==NULL) {
		fp= fopen("/tmp/libburn_sg_command_log","a");
		fprintf(fp,"\n-----------------------------------------\n");
	}
	sg_log_cmd(c,fp,0);
#endif /* Libburn_log_sg_commandS */
	  

	/* ts A61010 : with no fd there is no chance to send an ioctl */
	if (d->fd < 0) {
		c->error = 1;
		return 0;
	}

	c->error = 0;
	memset(&s, 0, sizeof(sg_io_hdr_t));

	s.interface_id = 'S';

	if (c->dir == TO_DRIVE)
		s.dxfer_direction = SG_DXFER_TO_DEV;
	else if (c->dir == FROM_DRIVE)
		s.dxfer_direction = SG_DXFER_FROM_DEV;
	else if (c->dir == NO_TRANSFER) {
		s.dxfer_direction = SG_DXFER_NONE;

		/* ts A61007 */
		/* a ssert(!c->page); */
		no_c_page = 1;
	}
	s.cmd_len = c->oplen;
	s.cmdp = c->opcode;
	s.mx_sb_len = 32;
	s.sbp = c->sense;
	memset(c->sense, 0, sizeof(c->sense));
	s.timeout = 200000;
	if (c->page && !no_c_page) {
		s.dxferp = c->page->data;
		if (c->dir == FROM_DRIVE) {

			/* ts A70519 : kernel 2.4 usb-storage seems to
					expect exact dxfer_len for data
					fetching commands.
			*/
			if (c->dxfer_len >= 0)
				s.dxfer_len = c->dxfer_len;
			else
				s.dxfer_len = BUFFER_SIZE;
/* touch page so we can use valgrind */
			memset(c->page->data, 0, BUFFER_SIZE);
		} else {

			/* ts A61010 */
			/* a ssert(c->page->bytes > 0); */
			if (c->page->bytes <= 0) {
				c->error = 1;
				return 0;
			}

			s.dxfer_len = c->page->bytes;
		}
	} else {
		s.dxferp = NULL;
		s.dxfer_len = 0;
	}
	s.usr_ptr = c;

	do {
		err = ioctl(d->fd, SG_IO, &s);

		/* ts A61010 */
		/* a ssert(err != -1); */
		if (err == -1) {
			libdax_msgs_submit(libdax_messenger,
				 d->global_index, 0x0002010c,
				 LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
				 "Failed to transfer command to drive",
				 errno, 0);
			sg_close_drive(d);
			d->released = 1;
			d->busy = BURN_DRIVE_IDLE;
			c->error = 1;
			return -1;
		}

		if (s.sb_len_wr) {
			if (!c->retry) {
				c->error = 1;

				/* A61106: rather than : return 1 */
				goto ex;
			}
			switch (scsi_error(d, s.sbp, s.sb_len_wr)) {
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

	/* ts A61106 */
ex:;
	if (c->error) {
		scsi_notify_error(d, c, s.sbp, s.sb_len_wr, 0);
	} else if (s.host_status != Libburn_sg_host_oK || 
	    s.driver_status != Libburn_sg_driver_oK) {
		char msg[161];

		sprintf(msg,
			"SCSI command %2.2Xh indicates host or driver error:",
			(unsigned int) c->opcode[0]);
		sprintf(msg+strlen(msg),
			" host_status= %xh , driver_status= %xh",
			(unsigned int) s.host_status,
			(unsigned int) s.driver_status);
		libdax_msgs_submit(libdax_messenger, d->global_index,
				0x0002013b,
				LIBDAX_MSGS_SEV_DEBUG, LIBDAX_MSGS_PRIO_HIGH,
				msg, 0, 0);
	}

#ifdef Libburn_log_sg_commandS
	sg_log_err(c, fp, &s, c->error != 0);
#endif /* Libburn_log_sg_commandS */

	return 1;
}


/* ts A60922 */
/** Tries to obtain SCSI address parameters.
    @return  1 is success , 0 is failure
*/
int sg_obtain_scsi_adr(char *path, int *bus_no, int *host_no, int *channel_no,
                       int *target_no, int *lun_no)
{
	int fd, ret, l, open_mode = O_RDONLY;
	struct my_scsi_idlun {
		int x;
		int host_unique_id;
	};
 	struct my_scsi_idlun idlun;


	l = strlen(linux_ata_device_family) - 2;
	if (l > 0 && strncmp(path, linux_ata_device_family, l) == 0 
	    && path[7] >= 'a' && path[7] <= 'z' && path[8] == 0)
		return 0; /* on RIP 14 all hdx return SCSI adr 0,0,0,0 */

	/* ts A70409 : DDLP-B */
	/* >>> obtain single lock on path */

	if(burn_sg_open_o_nonblock)
		open_mode |= O_NDELAY;
	if(burn_sg_open_o_excl) {
		/* O_EXCL | O_RDONLY does not work with /dev/sg* on 
		   SuSE 9.0 (kernel 2.4) and SuSE 9.3 (kernel 2.6) */
		/* so skip it for now */;
	}
	fd = open(path, open_mode);
	if(fd < 0)
		return 0;
	sg_fcntl_lock(&fd, path, F_RDLCK, 0);
	if(fd < 0)
		return 0;

#ifdef SCSI_IOCTL_GET_BUS_NUMBER
	/* Hearsay A61005 */
	if (ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, bus_no) == -1)
		*bus_no = -1;
#endif

	/* http://www.tldp.org/HOWTO/SCSI-Generic-HOWTO/scsi_g_idlun.html */
	ret = ioctl(fd, SCSI_IOCTL_GET_IDLUN, &idlun);

	sg_close_drive_fd(path, -1, &fd, 0);
	if (ret == -1)
		return(0);
	*host_no= (idlun.x>>24)&255;
	*channel_no= (idlun.x>>16)&255;
	*target_no= (idlun.x)&255;
	*lun_no= (idlun.x>>8)&255;
#ifdef SCSI_IOCTL_GET_BUS_NUMBER
	if(*bus_no == -1)
		*bus_no = 1000 * (*host_no + 1) + *channel_no;
#else
	*bus_no= *host_no;
#endif
	return 1;
}


/* ts A60922 ticket 33 : called from drive.c */
/** Tells wether a text is a persistent address as listed by the enumeration
    functions.
*/
int sg_is_enumerable_adr(char *adr)
{
	char fname[4096];
	int ret = 0, first = 1;
	burn_drive_enumerator_t idx;

	while (1) {
		ret= sg_give_next_adr(&idx, fname, sizeof(fname), first);
		if(ret <= 0)
	break;
		first = 0;
		if (strcmp(adr, fname) == 0) {
			sg_give_next_adr(&idx, fname, sizeof(fname), -1);
			return 1;
		}
	}
	if (first == 0)
		sg_give_next_adr(&idx, fname, sizeof(fname), -1);
	return(0);
}


/* ts A70909 */
/** Estimate the potential payload capacity of a file address.
    @param path  The address of the file to be examined. If it does not
                 exist yet, then the directory will be inquired.
    @param bytes The pointed value gets modified, but only if an estimation is
                 possible.
    @return      -2 = cannot perform necessary operations on file object
                 -1 = neither path nor dirname of path exist
                  0 = could not estimate size capacity of file object
                  1 = estimation has been made, bytes was set
*/
int burn_os_stdio_capacity(char *path, off_t *bytes)
{
	struct stat stbuf;
	struct statvfs vfsbuf;
	char testpath[4096], *cpt;
	long blocks;
	int open_mode = O_RDONLY, fd, ret;
	off_t add_size = 0;

	testpath[0] = 0;
	blocks = *bytes / 512;
	if (stat(path, &stbuf) == -1) {
		strcpy(testpath, path);
		cpt = strrchr(testpath, '/');
		if(cpt == NULL)
			strcpy(testpath, ".");
		else if(cpt == testpath)
			testpath[1] = 0;
		else
			*cpt = 0;
		if (stat(testpath, &stbuf) == -1)
			return -1;
	} else if(S_ISBLK(stbuf.st_mode)) {
		fd = open(path, open_mode);
		if (fd == -1)
			return -2;
		ret = ioctl(fd, BLKGETSIZE, &blocks);
		close(fd);
		if (ret == -1)
			return -2;
		*bytes = ((off_t) blocks) * (off_t) 512;
	} else if(S_ISREG(stbuf.st_mode)) {
		add_size = stbuf.st_blocks * (off_t) 512;
		strcpy(testpath, path);
	} else
		return 0;

	if (testpath[0]) {	
		if (statvfs(testpath, &vfsbuf) == -1)
			return -2;
		*bytes = add_size + ((off_t) vfsbuf.f_bsize) *
						(off_t) vfsbuf.f_bavail;
	}
	return 1;
}

