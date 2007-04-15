
/* ddlpa
   Implementation of Delicate Device Locking Protocol level A.
   Copyright (C) 2007 Thomas Schmitt <scdbackup@gmx.net>
   Provided under any of the following licenses: GPL, LGPL, BSD. Choose one.
*/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>

/* No getter functions. This makes the compiled code leaner. */
#define DDLPA_H_NOT_ENCAPSULATED 1

/* All callers of ddlpa must do this */
#include "ddlpa.h"


/* 1 = Enable progress message on stderr, 0 = normal silent operation */
static int ddlpa_debug_mode = 1;


/* ----------------------- private -------------------- */


static int ddlpa_new(struct ddlpa_lock **lck, int o_flags, int ddlpa_flags)
{
	int i;
	struct ddlpa_lock *o;

	o = *lck = (struct ddlpa_lock *) malloc(sizeof(struct ddlpa_lock));
	if (o == NULL)
		return ENOMEM;
	for (i = 0; i < sizeof(struct ddlpa_lock); i++)
		((char *) o)[i] = 0;
	o->path = NULL;
	o->fd = -1;
	for (i = 0; i < DDLPA_MAX_SIBLINGS; i++)
		o->sibling_fds[i] = -1;
	o->errmsg = NULL;

	o->o_flags = o_flags;
	o->ddlpa_flags = ddlpa_flags;
	return 0;
}


static int ddlpa_enumerate(struct ddlpa_lock *o, int *idx,
			char path[DDLPA_MAX_STD_LEN + 1])
{
	if (*idx < 0)
		*idx = 0;

	if (*idx < 26)
		sprintf(path, "/dev/hd%c", 'a' + *idx);
	else if (*idx < 256 + 26)
		sprintf(path, "/dev/sr%d", *idx - 26);
	else if (*idx < 2 * 256 + 26)
		sprintf(path, "/dev/scd%d", *idx - 256 - 26);
	else if (*idx < 3 * 256 + 26)
		sprintf(path, "/dev/sg%d", *idx - 2 * 256 - 26);
	else
		return 1;
	(*idx)++;
	return 0;
}


static int ddlpa_std_by_rdev(struct ddlpa_lock *o)
{
	int idx = 0;
	char try_path[DDLPA_MAX_STD_LEN+1];
	struct stat path_stbuf, try_stbuf;

	if (!o->path_is_valid)
		return EFAULT;
	if (stat(o->path, &path_stbuf) == -1)
		return errno;

	while (ddlpa_enumerate(o, &idx, try_path) == 0) {
		if (stat(try_path, &try_stbuf) == -1)
			continue;
		if (path_stbuf.st_rdev != try_stbuf.st_rdev)
			continue;
		strcpy(o->std_path, try_path);

		if (ddlpa_debug_mode)
			fprintf(stderr,
			    "DDLPA_DEBUG: ddlpa_std_by_rdev(\"%s\") = \"%s\"\n",
			    o->path, o->std_path);

		return 0;
	}
	return ENOENT;
}


/* Caution : these tests are valid only with standard paths */

static int ddlpa_is_scsi(struct ddlpa_lock *o, char *path)
{
	return (strncmp(path, "/dev/s", 6) == 0);
}

static int ddlpa_is_sg(struct ddlpa_lock *o, char *path)
{
	return (strncmp(path, "/dev/sg", 7) == 0);
}

static int ddlpa_is_sr(struct ddlpa_lock *o, char *path)
{
	return (strncmp(path, "/dev/sr", 7) == 0);
}

static int ddlpa_is_scd(struct ddlpa_lock *o, char *path)
{
	return (strncmp(path, "/dev/scd", 8) == 0);
}


static int ddlpa_fcntl_lock(struct ddlpa_lock *o, int fd, int l_type)
{
	struct flock lockthing;
	int ret;

	memset(&lockthing, 0, sizeof(lockthing));
	lockthing.l_type = l_type;
	lockthing.l_whence = SEEK_SET;
	lockthing.l_start = 0;
	lockthing.l_len = 0;
	ret = fcntl(fd, F_SETLK, &lockthing);
	if (ret == -1)
		return EBUSY;
	return 0;
}


static int ddlpa_occupy(struct ddlpa_lock *o, char *path, int *fd,
			int no_o_excl)
{
	int ret, o_flags = O_RDWR | O_NDELAY;

	if(!no_o_excl)
		o_flags |= O_EXCL;
	*fd = open(path, o_flags);
	if (*fd == -1) {
		o->errmsg = malloc(strlen(path)+160);
		if (o->errmsg)
			sprintf(o->errmsg,
				"Failed to open O_RDWR | O_NDELAY %s: '%s'", 
				(no_o_excl ? "" : "| O_EXCL "), path);
		return (errno ? errno : EBUSY);
	}
	ret = ddlpa_fcntl_lock(o, *fd, F_WRLCK);
	if (ret) {
		o->errmsg = malloc(strlen(path)+160);
		if (o->errmsg)
			sprintf(o->errmsg,
				"Failed to lock fcntl(F_WRLCK) : '%s'",path);
		close(*fd);
		*fd = -1;
		return ret;
	}
	if (ddlpa_debug_mode)
		fprintf(stderr, "DDLPA_DEBUG: ddlpa_occupy() %s: '%s'\n",
			(no_o_excl ? "       " : "O_EXCL "), path);
	return 0;
}


static int ddlpa_obtain_scsi_adr(struct ddlpa_lock *o, char *path,
			int *bus, int *host, int *channel, int *id, int *lun)
{
	int fd, ret, open_mode = O_RDONLY | O_NDELAY;
	struct my_scsi_idlun {
		int x;
		int host_unique_id;
	};
	struct my_scsi_idlun idlun;

	if (!ddlpa_is_scsi(o, o->std_path))
                return EFAULT;
	fd = open(path, open_mode);
	if (fd == -1)
		return (errno ? errno : EBUSY);
	if (!(o->ddlpa_flags & DDLPA_ALLOW_ROGUE_BUSIDLUN)) {
		ret = ddlpa_fcntl_lock(o, fd, F_RDLCK);
		if (ret) {
			if (ddlpa_debug_mode)
				fprintf(stderr, 
				"DDLPA_DEBUG: fcntl '%s' errno=%d  ret=%d\n",
				 path, errno, ret);
			return ret;
		}
	}
		
	if (ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, bus) == -1)
		*bus = -1;
	ret = ioctl(fd, SCSI_IOCTL_GET_IDLUN, &idlun);
	close(fd);
	if (ret  == -1)
		return (errno ? errno : EIO);
	*host =    (idlun.x >> 24) & 255;
	*channel = (idlun.x >> 16) & 255;
	*id =      (idlun.x)       & 255;
	*lun =     (idlun.x >> 8 ) & 255;
	return 0;
}


static int ddlpa_collect_siblings(struct ddlpa_lock *o)
{
	int idx = 0, ret, have_sg = 0, have_sr = 0, have_scd = 0;
	dev_t path_dev;
	ino_t path_inode;
	struct stat stbuf;
	char *path, try_path[DDLPA_MAX_STD_LEN+1];
	int p_bus, p_host, p_channel, p_id, p_lun;
	int t_bus, t_host, t_channel, t_id, t_lun;
	
	if (o->ddlpa_flags & DDLPA_OPEN_GIVEN_PATH)
		path = o->path;
	else
		path = o->std_path;
	if (path[0] == 0 || o->num_siblings != 0)
		return EFAULT;
	if (!ddlpa_is_scsi(o, o->std_path))
		return EFAULT;

	if (stat(path, &stbuf) == -1)
		return errno;
	path_inode = stbuf.st_ino;
	path_dev = stbuf.st_dev;
	o->rdev = stbuf.st_rdev;
	o->dev = stbuf.st_dev;
	o->ino = stbuf.st_ino;
	ret = ddlpa_obtain_scsi_adr(o, path,
				&p_bus, &p_host, &p_channel, &p_id, &p_lun);
	if (ret) {
		o->errmsg = strdup(
			"Cannot obtain SCSI parameters host,channel,id,lun");
		return ret;
	}

	while (ddlpa_enumerate(o, &idx, try_path) == 0) {
		if (!ddlpa_is_scsi(o, try_path))
			continue;
		if (stat(try_path, &stbuf) == -1)
			continue;
		ret = ddlpa_obtain_scsi_adr(o, try_path, 
				&t_bus, &t_host, &t_channel, &t_id, &t_lun);
		if (ret) {

			/* >>> interpret error, memorize busy, no permission */

			continue;
		}
		if (t_host != p_host || t_channel != p_channel ||
		    t_id != p_id     || t_lun != p_lun)
			continue;

		if (o->num_siblings >= DDLPA_MAX_SIBLINGS) {
			o->errmsg = 
				strdup("Too many matching device files found");
			return ERANGE;
		}
		if (ddlpa_is_sg(o, try_path))
			have_sg = 1;
		else if (ddlpa_is_sr(o, try_path))
                	have_sr = 1;
		else if (ddlpa_is_scd(o, try_path))
                	have_scd = 1;
		strcpy(o->sibling_paths[o->num_siblings], try_path);
		o->sibling_rdevs[o->num_siblings] = stbuf.st_rdev;
		o->sibling_devs[o->num_siblings] = stbuf.st_dev;
		o->sibling_inodes[o->num_siblings] = stbuf.st_ino;

		if (ddlpa_debug_mode)
			fprintf(stderr,
			"DDLPA_DEBUG: ddlpa_collect_siblings() found \"%s\"\n",
			try_path);

		(o->num_siblings)++;
	}
	if (have_sg && have_sr && have_scd)
		return 0;
	if (o->ddlpa_flags & DDLPA_ALLOW_MISSING_SGRCD)
		return 0;

	o->errmsg = strdup("Did not find enough siblings");

	/* >>> add more info about busy and forbidden paths */

	return EBUSY;
}


static int ddlpa_open_all(struct ddlpa_lock *o)
{
	int i, j, ret, no_o_excl;

	if (ddlpa_is_scsi(o, o->std_path)) {
		ret = ddlpa_collect_siblings(o);
		if (ret)
			return ret;
		for (i = 0; i < o->num_siblings; i++) {

			/* Watch out for the main personality of the drive. */
			/* No need to occupy identical path or softlink path */
			if (o->sibling_devs[i] == o->dev &&
			    o->sibling_inodes[i] == o->ino)
					continue;
			/* There may be the same rdev but different inode. */
			no_o_excl = (o->sibling_rdevs[i] == o->rdev);
		
			/* Look for multiply registered device drivers */	
			for (j = 0; j < i; j++) {
				if (o->sibling_devs[j] == o->sibling_devs[i] &&
			   	  o->sibling_inodes[j] == o->sibling_inodes[i])
					break;
				if (o->sibling_rdevs[j] == o->sibling_rdevs[i])
					no_o_excl = 1;
			}
			if (j < i)
				continue;   /* inode is already occupied */

			ret = ddlpa_occupy(o, o->sibling_paths[i],
					   &(o->sibling_fds[i]), no_o_excl);
			if (ret)
				return ret;
		}
	}

	if (o->ddlpa_flags & DDLPA_OPEN_GIVEN_PATH)
		ret = ddlpa_occupy(o, o->path, &(o->fd), 0);
	else
		ret = ddlpa_occupy(o, o->std_path, &(o->fd), 0);
	if (ret)
		return ret;	

	/* >>> use fcntl() to adjust O_NONBLOCK */;

	return 0;
}


/* ----------------------- public -------------------- */


int ddlpa_destroy(struct ddlpa_lock **lockbundle)
{
	struct ddlpa_lock *o;
	int i;

	o= *lockbundle;
	if (o == NULL)
		return 0;
	for (i = 0; i < o->num_siblings; i++)
		if (o->sibling_fds[i] != -1)
			close(o->sibling_fds[i]);
	if(o->fd != -1)
		close(o->fd);
	if (o->path != NULL)
		free(o->path);
	if (o->errmsg != NULL)
		free(o->errmsg);
	free((char *) o);
	*lockbundle = NULL;
	return 0;
}


int ddlpa_lock_path(char *path, int o_flags, int ddlpa_flags,
			struct ddlpa_lock **lockbundle, char **errmsg)
{
	struct ddlpa_lock *o;
	int ret;

	*errmsg = NULL;
	if (ddlpa_new(&o, o_flags, ddlpa_flags))
		return ENOMEM;
	*lockbundle = o;

	o->path = strdup(path);
	if (o->path == NULL)
		return ENOMEM;
	o->path_is_valid = 1;

	ret = ddlpa_std_by_rdev(o);
	if (ret) {
		*errmsg = strdup(
		  "Cannot find equivalent of given path among standard paths");	
		return ret;
	}
        ret = ddlpa_open_all(o);
	if (ret) {
		*errmsg = o->errmsg;
		o->errmsg = NULL;
		ddlpa_destroy(&o);
	}
	return ret;
}


int ddlpa_lock_btl(int bus, int target, int lun,
			int  o_flags, int ddlpa_flags,
			struct ddlpa_lock **lockbundle, char **errmsg)
{
	/* >>> */

	*errmsg = strdup("Function  ddlpa_lock_btl()  not implemented yet.");
	return ENOSYS;
}


#ifndef DDLPA_H_NOT_ENCAPSULATED

int ddlpa_get_fd(struct ddlpa_lock *lockbundle)
{
	/* >>> */
	return -1;
}


char *ddlpa_get_fd_path(struct ddlpa_lock *lockbundle)
{
	/* >>> */
	return "none";
}


int ddlpa_get_siblings(struct ddlpa_lock *lockbundle, 
			char **std_path, int *num_siblings,
			char ***sibling_paths, int **sibling_fds)
{
	*num_siblings = 0;

	/* >>> */

	return 1;
}

#endif /* ! DDLPA_H_NOT_ENCAPSULATED */


/* ----------------------------- Test / Demo -------------------------- */

#ifdef DDLPA_C_STANDALONE


int main(int argc, char **argv)
{
	struct ddlpa_lock *lck = NULL;
	char *errmsg = NULL, *opened_path = NULL, *my_path = NULL;
	int i, ret, fd = -1, duration = -1;

	if (argc < 3) {
usage:;
		fprintf(stderr, "usage: %s  device_path  duration\n", argv[0]);
		exit(1);
	}
	my_path = argv[1];
	sscanf(argv[2], "%d", &duration);
	if (duration < 0)
		goto usage;

	/* This substitutes for:
		fd = open(my_path, O_RDWR | O_EXCL);
	*/
	ret = ddlpa_lock_path(my_path, O_RDWR, DDLPA_ALLOW_ROGUE_BUSIDLUN,
				 &lck, &errmsg);
	if (ret) {
		fprintf(stderr, "Cannot exclusively open '%s'\n", my_path);
		if (errmsg != NULL)
			fprintf(stderr, "Reason given    : %s\n",
				errmsg);
		free(errmsg);
		fprintf(stderr, "Error condition : %d '%s'\n",
			errno, strerror(errno));
		exit(2);
	}
	fd = lck->fd;

	printf("---------------------------------------------- Lock gained\n");


	/* Use fd for the usual operations on the device depicted by my_path.
	*/


	/* This prints an overview of the impact of the lock */
	if (lck->ddlpa_flags & DDLPA_OPEN_GIVEN_PATH)
		opened_path = lck->path;
	else
        	opened_path = lck->std_path;
	printf("ddlpa: opened           %s", opened_path);

	if (strcmp(opened_path, lck->std_path) != 0)
		printf(" (an alias of '%s')", lck->std_path);
	printf("\n");
	if (lck->num_siblings > 0) {
		printf("ddlpa: opened siblings:");
		for (i = 0; i < lck->num_siblings; i++)
			if (lck->sibling_fds[i] != -1)
				printf(" %s", lck->sibling_paths[i]);
		printf("\n");
	}


	/* This example waits a while. So other lock candidates can collide. */
	for (i = 0; i < duration; i++) {
		sleep(1);
		fprintf(stderr, "\rslept %d seconds of %d", i + 1, duration);
	}
	fprintf(stderr, "\n");


	/* When finally done with the drive, this substitutes for:
		close(fd);
	*/
	if (ddlpa_destroy(&lck)) {
		/* Well, man 2 close says it can fail. */
		exit(3);
	}
	exit(0);
}


#endif /* DDLPA_C_STANDALONE */

