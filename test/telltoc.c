
/*  test/telltoc.c , API illustration of obtaining media status info */
/*  Copyright (C) 2006 Thomas Schmitt <scdbackup@gmx.net> Provided under GPL */

/**                               Overview 
  
  telltoc is a minimal demo application for the library libburn as provided
  on  http://libburn.pykix.org . It can list the available devices, can display
  some drive properties, the type of media, eventual table of content and
  multisession info for mkisofs option -C . 
  It's main purpose, nevertheless, is to show you how to use libburn and also
  to serve the libburn team as reference application. telltoc.c does indeed
  define the standard way how above gestures can be implemented and stay upward
  compatible for a good while.
  
  Before you can do anything, you have to initialize libburn by
     burn_initialize()
  as it is done in main() at the end of this file. Then you aquire a
  drive in an appropriate way conforming to the API. The two main
  approaches are shown here in application functions:
     telltoc_aquire_by_adr()       demonstrates usage as of cdrecord traditions
     telltoc_aquire_by_driveno()   demonstrates a scan-and-choose approach
  With that aquired drive you can call
     telltoc_media()   prints some information about the media in a drive
     telltoc_toc()     prints a table of content (if there is content)
     telltoc_msinfo()  prints parameters for mkisofs option -C
  When everything is done, main() releases the drive and shuts down libburn:
     burn_drive_release();
     burn_finish()
  
*/

/** See this for the decisive API specs . libburn.h is The Original */
/*  For using the installed header file :  #include <libburn/libburn.h> */
/*  This program insists in the own headerfile. */
#include "../libburn/libburn.h"

/* libburn is intended for Linux systems with kernel 2.4 or 2.6 for now */
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>


/** For simplicity i use global variables to represent the drives.
    Drives are systemwide global, so we do not give away much of good style.
*/

/** This list will hold the drives known to libburn. This might be all CD
    drives of the system and thus might impose severe impact on the system.
*/
static struct burn_drive_info *drive_list;

/** If you start a long lasting operation with drive_count > 1 then you are
    not friendly to the users of other drives on those systems. Beware. */
static unsigned int drive_count;

/** This variable indicates wether the drive is grabbed and must be
    finally released */
static int drive_is_grabbed = 0;


/* Some in-advance definitions to allow a more comprehensive ordering
   of the functions and their explanations in here */
int telltoc_aquire_by_adr(char *drive_adr);
int telltoc_aquire_by_driveno(int *drive_no, int silent);


/* ------------------------------- API gestures ---------------------------- */

/** You need to aquire a drive before burning. The API offers this as one
    compact call and alternatively as application controllable gestures of
    whitelisting, scanning for drives and finally grabbing one of them.

    If you have a persistent address of the drive, then the compact call is
    to prefer because it only touches one drive. On modern Linux kernels,
    there should be no fatal disturbance of ongoing burns of other libburn
    instances with any of our approaches. We use open(O_EXCL) by default.
    On /dev/hdX it should cooperate with growisofs and some cdrecord variants.
    On /dev/sgN versus /dev/scdM expect it not to respect other programs.
*/
int telltoc_aquire_drive(char *drive_adr, int *driveno, int silent_drive)
{
	int ret;

	if(drive_adr != NULL && drive_adr[0] != 0)
		ret = telltoc_aquire_by_adr(drive_adr);
	else
		ret = telltoc_aquire_by_driveno(driveno, silent_drive);
	return ret;
}


/** If the persistent drive address is known, then this approach is much
    more un-obtrusive to the systemwide livestock of drives. Only the
    given drive device will be opened during this procedure.
*/
int telltoc_aquire_by_adr(char *drive_adr)
{
	int ret;
	
	fprintf(stderr,"Aquiring drive '%s' ...\n",drive_adr);
	ret = burn_drive_scan_and_grab(&drive_list,drive_adr,1);
	if (ret <= 0) {
		fprintf(stderr,"FAILURE with persistent drive address  '%s'\n",
			drive_adr);
	} else {
		fprintf(stderr,"Done\n");
		drive_is_grabbed = 1;
	}
	return ret;
}


/** This method demonstrates how to use libburn without knowing a persistent
    drive address in advance. It has to make sure that after assessing the list
    of available drives, all unwanted drives get closed again. As long as they
    are open, no other libburn instance can see them. This is an intended
    locking feature. The application is responsible for giving up the locks
    by either burn_drive_release() (only after burn_drive_grab() !),
    burn_drive_info_forget(), burn_drive_info_free(), or burn_finish().
    @param driveno the index number in libburn's drive list. This will get
                   set to 0 on success and will then be the drive index to
                   use in the further dourse of processing.
    @param silent_drive 1=do not print "Drive found  :" line with *driveno >= 0
    @return 1 success , <= 0 failure
*/
int telltoc_aquire_by_driveno(int *driveno, int silent_drive)
{
	char adr[BURN_DRIVE_ADR_LEN];
	int ret, i;

	fprintf(stderr, "Beginning to scan for devices ...\n");
	while (!burn_drive_scan(&drive_list, &drive_count))
		usleep(1002);
	if (drive_count <= 0 && *driveno >= 0) {
		fprintf(stderr, "FAILED (no drives found)\n");
		return 0;
	}
	fprintf(stderr, "Done\n");

	for (i = 0; i < drive_count; i++) {
		if (*driveno >= 0 && (silent_drive || *driveno != i))
	continue;
		if (burn_drive_get_adr(&(drive_list[i]), adr) <=0)
			strcpy(adr, "-get_adr_failed-");
		printf("Drive found  : %d  --drive '%s'  : ", i,adr);
		printf("%-8s  %-16s  (%4s)\n",
			drive_list[i].vendor,drive_list[i].product,
			drive_list[i].revision);
	}
	if (*driveno < 0) {
		fprintf(stderr, 
			"Pseudo-drive \"-\" given : bus scanning done.\n");
		return 2; /* the program will end after this */
	}

	/* We already made our choice via command line. (default is 0)
	   So we just have to keep our desired drive and drop all others.
	 */
	if (drive_count <= *driveno) {
		fprintf(stderr,
			"Found only %d drives. Number %d not available.\n",
			drive_count, *driveno);
		return 0; /* the program will end after this */
	}

	/* Drop all drives which we do not want to use */
	for (i = 0; i < drive_count; i++) {
		if (i == *driveno) /* the one drive we want to keep */
	continue;
		ret = burn_drive_info_forget(&(drive_list[i]),0);
		if (ret != 1)
			fprintf(stderr, "Cannot drop drive %d. Please report \"ret=%d\" to libburn-hackers@pykix.org\n",
				i, ret);
		else
			fprintf(stderr, "Dropped unwanted drive %d\n",i);
	}
	/* Make the one we want ready for inquiry */
	ret= burn_drive_grab(drive_list[*driveno].drive, 1);
	if (ret != 1)
		return 0;
	drive_is_grabbed = 1;
	return 1;
}



/** This gesture is necessary to get my NEC DVD_RW ND-4570A out of a state
    of noisy overexcitement after it was inquired for Next Writeable Address.
    The noise then still lasts 20 seconds. Same with cdrecord -toc, btw.
    It opens a small gap for losing the drive to another libburn instance.
    Not a problem in telltoc. This is done as very last drive operation.
    Eventually the other libburn instance will have the same sanitizing effect.
*/
int telltoc_regrab(struct burn_drive *drive) {
	int ret;

	if (drive_is_grabbed)
		burn_drive_release(drive, 0);
	drive_is_grabbed = 0;
	ret = burn_drive_grab(drive, 0);
	if (ret != 0) {
		drive_is_grabbed = 1;
	}
	return !!ret;
}


int telltoc_media(struct burn_drive *drive)
{
	int ret, media_found = 0;
	double max_speed = 0.0, min_speed = 0.0;
	enum burn_disc_status s;

	while (burn_drive_get_status(drive, NULL) != BURN_DRIVE_IDLE)
		usleep(100001);
	while ((s = burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
		usleep(100001);

	printf("Media status : ");
	if (s==BURN_DISC_FULL) {
		printf("is written , is closed\n");
		media_found = 1;
	} else if (s==BURN_DISC_APPENDABLE) {
		printf("is written, is appendable\n");
		media_found = 1;
	} else if (s==BURN_DISC_BLANK) {
		printf("is blank\n");
		media_found = 1;
	} else if (s==BURN_DISC_EMPTY)
		printf("is not present\n");
	else
		printf("is not recognizable\n");

	printf("Media type   : ");
	if (media_found) {
		if (burn_disc_erasable(drive))
			printf("is erasable\n");
		else
			printf("is not erasable\n");	
	} else
		printf("is not recognizable\n");

	ret= burn_disc_read_atip(drive);
	if(ret>0) {
		ret= burn_drive_get_min_write_speed(drive);
		min_speed = ((double ) ret) / 176.0;
	}
	ret= burn_drive_get_write_speed(drive);
	max_speed = ((double ) ret) / 176.0;
	if (!media_found)
		printf("Drive speed  : max=%.f\n", max_speed);
	else if (min_speed<=0)
		printf("Media speed  : max=%.f\n", max_speed);
	else
		printf("Media speed  : max=%.f , min=%.f\n",
			max_speed, min_speed);

	return 1;
}


int telltoc_toc(struct burn_drive *drive)
{
	int num_sessions = 0 , num_tracks = 0 , lba = 0;
	int track_count = 0;
	int session_no, track_no;
	enum burn_disc_status s;
	struct burn_disc *disc= NULL;
	struct burn_session **sessions;
	struct burn_track **tracks;
	struct burn_toc_entry toc_entry;

	while (burn_drive_get_status(drive, NULL) != BURN_DRIVE_IDLE)
		usleep(100001);
	while ((s = burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
		usleep(100001);

	disc = burn_drive_get_disc(drive);
	if (disc==NULL) {
		fprintf(stderr, "SORRY: Cannot obtain Table Of Content\n");
		return 2;
	}
	sessions = burn_disc_get_sessions(disc, &num_sessions);
	for (session_no = 0; session_no<num_sessions; session_no++) {
		tracks = burn_session_get_tracks(sessions[session_no],
						&num_tracks);
		if (tracks==NULL)
	continue;
		for(track_no= 0; track_no<num_tracks; track_no++) {
			track_count++;
			burn_track_get_entry(tracks[track_no], &toc_entry);
			lba= burn_msf_to_lba(toc_entry.pmin, toc_entry.psec,
						toc_entry.pframe);
			printf("Media content: session %2d  ", session_no+1);
			printf("track    %2d %s  lba: %9d  %2.2u:%2.2u:%2.2u\n",
				track_count,
				((toc_entry.control&7)<4?"audio":"data "),
				lba,
				toc_entry.pmin,
				toc_entry.psec,
				toc_entry.pframe);
		}
		burn_session_get_leadout_entry(sessions[session_no],
						&toc_entry);
		lba = burn_msf_to_lba(toc_entry.pmin,
					toc_entry.psec, toc_entry.pframe);
		printf("Media content: session %2d  ", session_no+1);
		printf("leadout            lba: %9d  %2.2u:%2.2u:%2.2u\n",
			lba,
			toc_entry.pmin,
			toc_entry.psec,
			toc_entry.pframe);
	}
	if (disc!=NULL)
		burn_disc_free(disc);
	return 1;
}


int telltoc_msinfo(struct burn_drive *drive, 
			int msinfo_explicit, int msinfo_alone)
{
	int num_sessions, session_no, ret, num_tracks;
	int nwa = -123456789, lba = -123456789, aux_lba, lout_lba;
	enum burn_disc_status s;
	struct burn_disc *disc= NULL;
	struct burn_session **sessions;
	struct burn_track **tracks;
	struct burn_toc_entry toc_entry;
	struct burn_write_opts *o= NULL;

	while (burn_drive_get_status(drive, NULL) != BURN_DRIVE_IDLE)
		usleep(100001);
	while ((s = burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
		usleep(100001);
	if (s!=BURN_DISC_APPENDABLE) {
		if (!msinfo_explicit)
			return 2;
		fprintf(stderr,
		   "SORRY: --msinfo can only operate on appendable media.\n");
		return 0;
	}

	/* man mkisofs , option -C :
	   The first number is the sector number of the first sector in
	   the last session of the disk that should be appended to.
	*/
	disc = burn_drive_get_disc(drive);
	if (disc==NULL) {
		fprintf(stderr,"SORRY: Cannot obtain info about CD content\n");
		return 2;
	}
	sessions = burn_disc_get_sessions(disc, &num_sessions);
	for (session_no = 0; session_no<num_sessions; session_no++) {
		tracks = burn_session_get_tracks(sessions[session_no],
						&num_tracks);
		if (tracks==NULL || num_tracks<=0)
	continue;
		burn_track_get_entry(tracks[0], &toc_entry);
		lba= burn_msf_to_lba(toc_entry.pmin, toc_entry.psec,
						toc_entry.pframe);
	}
	if(lba==-123456789) {
		fprintf(stderr,"SORRY: Cannot find any track on media\n");
		{ ret = 0; goto ex; }
	}
	/* Prepare a qualified guess as fallback for nwa inquiry */
	burn_session_get_leadout_entry(sessions[num_sessions-1], &toc_entry);
	lout_lba= burn_msf_to_lba(toc_entry.pmin,toc_entry.psec,
					toc_entry.pframe);

	/* man mkisofs , option -C :
	   The second  number is the starting sector number of the new session.
	*/
	/* Set some write opts to be sent to drive. LG GSA-4082B needs it. */
	o= burn_write_opts_new(drive);
 	if(o!=NULL) {
   		burn_write_opts_set_perform_opc(o, 0);
   		burn_write_opts_set_write_type(o,
					BURN_WRITE_TAO, BURN_BLOCK_MODE1);
	}
	/* Now try to inquire nwa from drive */
	ret= burn_disc_track_lba_nwa(drive,o,0,&aux_lba,&nwa);
	telltoc_regrab(drive); /* necessary to calm down my NEC drive */
	if(ret<=0) {
		fprintf(stderr,
		   "NOTE: Guessing next writeable address from leadout\n");
		if(num_sessions>0)
			nwa= lout_lba+6900;
		else
			nwa= lout_lba+11400;
	}

	if (!msinfo_alone)
		printf("Media msinfo : mkisofs ... -C ");
	printf("%d,%d\n",lba,nwa);

	ret = 1;
ex:;
	if (disc!=NULL)
		burn_disc_free(disc);
	if (o!=NULL)
		burn_write_opts_free(o);
	return ret;
}


/** The setup parameters of telltoc */
static char drive_adr[BURN_DRIVE_ADR_LEN] = {""};
static int driveno = 0;
static int do_media = 0;
static int do_toc = 0;
static int do_msinfo = 0;
static int print_help = 0;


/** Converts command line arguments into above setup parameters.
    drive_adr[] must provide at least BURN_DRIVE_ADR_LEN bytes.
    source_adr[] must provide at least 4096 bytes.
*/
int telltoc_setup(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--drive")) {
            ++i;
            if (i >= argc) {
                fprintf(stderr,"--drive requires an argument\n");
                return 1;
            } else if (strcmp(argv[i], "-") == 0) {
                drive_adr[0] = 0;
                driveno = -1;
            } else if (isdigit(argv[i][0])) {
                drive_adr[0] = 0;
                driveno = atoi(argv[i]);
            } else {
                if(strlen(argv[i]) >= BURN_DRIVE_ADR_LEN) {
                    fprintf(stderr,"--drive address too long (max. %d)\n",
                            BURN_DRIVE_ADR_LEN-1);
                    return 2;
                }
                strcpy(drive_adr, argv[i]);
            }
        } else if (strcmp(argv[i],"--media")==0) {
	    do_media = 1;

        } else if (!strcmp(argv[i], "--msinfo")) {
	    do_msinfo = 1;

        } else if (!strcmp(argv[i], "--toc")) {
	    do_toc = 1;

        } else if (!strcmp(argv[i], "--help")) {
            print_help = 1;

        } else  {
            fprintf(stderr, "Unidentified option: %s\n", argv[i]);
            return 7;
        }
    }
    if (argc==1)
	print_help = 1;
    if (print_help) {
        printf("Usage: %s\n", argv[0]);
        printf("       [--drive <address>|<driveno>|\"-\"]\n");
        printf("       [--media]  [--toc]  [--msinfo]\n");
        printf("Examples\n");
        printf("A bus scan (needs rw-permissions to see a drive):\n");
        printf("  %s --drive -\n",argv[0]);
	printf("Obtain info about the type of loaded media:\n");
        printf("  %s --drive /dev/hdc --media\n",argv[0]);
	printf("Obtain table of content:\n");
        printf("  %s --drive /dev/hdc --toc\n",argv[0]);
	printf("Obtain parameters for option -C of program mkisofs:\n");
        printf("  msinfo=$(%s --drive /dev/hdc --msinfo 2>/dev/null)\n",
		argv[0]);
        printf("  mkisofs ... -C \"$msinfo\" ...\n");
	printf("Obtain what is available about drive 0 and its media\n");
	printf("  %s --drive 0\n",argv[0]);
    }
    return 0;
}


int main(int argc, char **argv)
{
	int ret, toc_failed = 0, msinfo_alone = 0, msinfo_explicit = 0;
	int full_default = 0;

	ret = telltoc_setup(argc, argv);
	if (ret)
		exit(ret);

	/* Behavior shall be different if --msinfo is only option */
	if (do_msinfo) {
		msinfo_explicit = 1;
		if (!(do_media || do_toc))
			msinfo_alone = 1;
	}
	/* Default option is to do everything if possible */
    	if (do_media==0 && do_msinfo==0 && do_toc==0 && driveno!=-1) {
		if(print_help)
			exit(0);
		full_default = do_media = do_msinfo = do_toc = 1;
	}

	fprintf(stderr, "Initializing libburn.pykix.org ...\n");
	if (burn_initialize())
		fprintf(stderr, "Done\n");
	else {
		fprintf(stderr,"\nFATAL: Failed to initialize.\n");
		exit(33);
	}

	/* Print messages of severity SORRY or more directly to stderr */
	burn_msgs_set_severities("NEVER", "SORRY", "telltoc : ");

	/** Note: driveno might change its value in this call */
	ret = telltoc_aquire_drive(drive_adr, &driveno, !full_default);
	if (ret<=0) {
		fprintf(stderr,"\nFATAL: Failed to aquire drive.\n");
		{ ret = 34; goto finish_libburn; }
	}
	if (ret == 2)
		{ ret = 0; goto release_drive; }

	if (do_media) {
		ret = telltoc_media(drive_list[driveno].drive);
		if (ret<=0)
			{ret = 36; goto release_drive; }
	}
	if (do_toc) {
		ret = telltoc_toc(drive_list[driveno].drive);
		if (ret<=0)
			{ret = 37; goto release_drive; }
		if (ret==2)
			toc_failed = 1;
	}
	if (do_msinfo) {
		ret = telltoc_msinfo(drive_list[driveno].drive,
					msinfo_explicit, msinfo_alone);
		if (ret<=0)
			{ret = 38; goto release_drive; }
	}

	ret = 0;
	if (toc_failed)
		ret = 37;
release_drive:;
	if (drive_is_grabbed)
		burn_drive_release(drive_list[driveno].drive, 0);

finish_libburn:;
	/* This app does not bother to know about exact scan state. 
	   Better to accept a memory leak here. We are done anyway. */
	/* burn_drive_info_free(drive_list); */

	burn_finish();
	exit(ret);
}

/*  License and copyright aspects:
    See libburner.c
*/

