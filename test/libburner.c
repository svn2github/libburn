
/*  test/libburner.c , API illustration of burning a single data track to CD */
/*  Copyright (C) 2005 - 2006 Thomas Schmitt <scdbackup@gmx.net> */
/*  Provided under GPL, see also "License and copyright aspects" at file end */


/**                               Overview 
  
  libburner is a minimal demo application for the library libburn as provided
  on  http://libburn.pykix.org . It can list the available devices, can
  blank a CD-RW and can burn to CD-R or CD-RW.
  It's main purpose, nevertheless, is to show you how to use libburn and also
  to serve the libburn team as reference application. libburner.c does indeed
  define the standard way how above three gestures can be implemented and
  stay upward compatible for a good while.
  
  Before you can do anything, you have to initialize libburn by
     burn_initialize()
  and provide some signal and abort handling, e.g. by the builtin handler, by
     burn_set_signal_handling() 
  as it is done in main() at the end of this file. Then you aquire a
  drive in an appropriate way conforming to the API. The two main
  approaches are shown here in application functions:
     libburner_aquire_by_adr()     demonstrates usage as of cdrecord traditions
     libburner_aquire_by_driveno()      demonstrates a scan-and-choose approach
  With that aquired drive you can blank a CD-RW
     libburner_blank_disc()
  Between blanking and burning one eventually has to reload the drive status
     libburner_regrab()
  With the aquired drive you can burn to CD-R or blank CD-RW
     libburner_payload()
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
int libburner_aquire_by_adr(char *drive_adr);
int libburner_aquire_by_driveno(int *drive_no);


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
int libburner_aquire_drive(char *drive_adr, int *driveno)
{
	int ret;

	if(drive_adr != NULL && drive_adr[0] != 0)
		ret = libburner_aquire_by_adr(drive_adr);
	else
		ret = libburner_aquire_by_driveno(driveno);
	return ret;
}


/** If the persistent drive address is known, then this approach is much
    more un-obtrusive to the systemwide livestock of drives. Only the
    given drive device will be opened during this procedure.
*/
int libburner_aquire_by_adr(char *drive_adr)
{
	int ret;
	
	printf("Aquiring drive '%s' ...\n",drive_adr);
	ret = burn_drive_scan_and_grab(&drive_list,drive_adr,1);
	if (ret <= 0) {
		fprintf(stderr,"FAILURE with persistent drive address  '%s'\n",
			drive_adr);
		if (strncmp(drive_adr,"/dev/sg",7) != 0 &&
		    strncmp(drive_adr,"/dev/hd",7) != 0)
			fprintf(stderr,"\nHINT: Consider addresses like  '/dev/hdc'  or  '/dev/sg0'\n");
	} else {
		printf("Done\n");
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
    @return 1 success , <= 0 failure
*/
int libburner_aquire_by_driveno(int *driveno)
{
	char adr[BURN_DRIVE_ADR_LEN];
	int ret, i;

	printf("Beginning to scan for devices ...\n");
	while (!burn_drive_scan(&drive_list, &drive_count))
		usleep(1002);
	if (drive_count <= 0 && *driveno >= 0) {
		printf("FAILED (no drives found)\n");
		return 0;
	}
	printf("Done\n");

	/*
	Interactive programs may choose the drive number at this moment.

	drive[0] to drive[drive_count-1] are struct burn_drive_info
	as defined in  libburn/libburn.h  . This structure is part of API
	and thus will strive for future compatibility on source level.
	Have a look at the info offered.
	Caution: do not take .location for drive address. Always use
		burn_drive_get_adr() or you might become incompatible
		in future.
	Note: bugs with struct burn_drive_info - if any - will not be
		easy to fix. Please report them but also strive for
		workarounds on application level.
	*/
	printf("\nOverview of accessible drives (%d found) :\n",
		drive_count);
	printf("-----------------------------------------------------------------------------\n");
	for (i = 0; i < drive_count; i++) {
		if (burn_drive_get_adr(&(drive_list[i]), adr) <=0)
			strcpy(adr, "-get_adr_failed-");
		printf("%d  --drive '%s'  :  '%s'  '%s'\n",
			i,adr,drive_list[i].vendor,drive_list[i].product);
	}
	printf("-----------------------------------------------------------------------------\n\n");


	/*
	On multi-drive systems save yourself from sysadmins' revenge.

	Be aware that you hold reserved all available drives at this point.
	So either make your choice quick enough not to annoy other system
	users, or set free the drives for a while.

	The tested way of setting free all drives is to shutdown the library
	and to restart when the choice has been made. The list of selectable
	drives should also hold persistent drive addresses as obtained
	above by burn_drive_get_adr(). By such an address one may use
	burn_drive_scan_and_grab() to finally aquire exactly one drive.

	A not yet tested shortcut should be to call burn_drive_info_free()
	and to call either burn_drive_scan() or burn_drive_scan_and_grab()
	before accessing any drives again.

	In both cases you have to be aware that the desired drive might get
	aquired in the meantime by another user resp. libburn process.
	*/

	/* We already made our choice via command line. (default is 0)
	   So we just have to keep our desired drive and drop all others.
	   No other libburn instance will have a chance to steal our drive.
	 */
	if (*driveno < 0) {
		printf("Pseudo-drive \"-\" given : bus scanning done.\n");
		return 2; /* the program will end after this */
	}
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
			printf("Dropped unwanted drive %d\n",i);
	}
	/* Make the one we want ready for blanking or burning */
	ret= burn_drive_grab(drive_list[*driveno].drive, 1);
	if (ret != 1)
		return 0;
	drive_is_grabbed = 1;
	return 1;
}


/** Makes a previously used CD-RW ready for thorough re-usal.

    To our knowledge it is hardly possible to abort an ongoing blank operation
    because after start it is entirely handled by the drive.
    So expect signal handling to wait the normal blanking timespan until it
    can allow the process to end. External kill -9 will not help the drive.
*/
int libburner_blank_disc(struct burn_drive *drive, int blank_fast)
{
	enum burn_disc_status disc_state;
	struct burn_progress p;
	int percent = 1;

	while (burn_drive_get_status(drive, NULL) != BURN_DRIVE_IDLE)
		usleep(1001);

	while ((disc_state = burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
		usleep(1001);
	printf(
	    "Drive media status:  %d  (see  libburn/libburn.h  BURN_DISC_*)\n",
	    disc_state);
	if (disc_state == BURN_DISC_BLANK) {
		fprintf(stderr,
		  "IDLE: Blank CD media detected. Will leave it untouched\n");
		return 2;
	} else if (disc_state == BURN_DISC_FULL ||
		   disc_state == BURN_DISC_APPENDABLE) {
		; /* this is what libburn is willing to blank */
	} else if (disc_state == BURN_DISC_EMPTY) {
		fprintf(stderr,"FATAL: No media detected in drive\n");
		return 0;
	} else {
		fprintf(stderr,
			"FATAL: Cannot recognize drive and media state\n");
		return 0;
	}
	if(!burn_disc_erasable(drive)) {
		fprintf(stderr,
			"FATAL : Media is not of erasable type\n");
		return 0;
	}
	printf(
	      "Beginning to %s-blank CD media.\n", (blank_fast?"fast":"full"));
	burn_disc_erase(drive, blank_fast);
	sleep(1);
	while (burn_drive_get_status(drive, &p) != BURN_DRIVE_IDLE) {
		if(p.sectors>0 && p.sector>=0) /* display 1 to 99 percent */
			percent = 1.0 + ((double) p.sector+1.0)
					 / ((double) p.sectors) * 98.0;
		printf("Blanking  ( %d%% done )\n", percent);
		sleep(1);
	}
	printf("Done\n");
	return 1;
}


/** This gesture is necessary to get the drive info after blanking.
    It opens a small gap for losing the drive to another libburn instance.
    We will work on closing this gap.
*/
int libburner_regrab(struct burn_drive *drive) {
	int ret;

	printf("Releasing and regrabbing drive ...\n");
	if (drive_is_grabbed)
		burn_drive_release(drive, 0);
	drive_is_grabbed = 0;
	ret = burn_drive_grab(drive, 0);
	if (ret != 0) {
		drive_is_grabbed = 1;
	 	printf("Done\n");
	} else
	 	printf("FAILED\n");
	return !!ret;
}


/** Brings preformatted track images (ISO 9660, audio, ...) onto media.
    To make sure a data image is fully readable on any Linux machine, this
    function adds 300 kB of padding to the (usualy single) track.
    Audio tracks get padded to complete their last sector.

    In case of external signals expect abort handling of an ongoing burn to
    last up to a minute. Wait the normal burning timespan before any kill -9.
*/
int libburner_payload(struct burn_drive *drive, 
		      char source_adr[][4096], int source_adr_count,
		      int simulate_burn, int all_tracks_type)
{
	struct burn_source *data_src;
	struct burn_disc *target_disc;
	struct burn_session *session;
	struct burn_write_opts *burn_options;
	enum burn_disc_status disc_state;
	struct burn_track *track, *tracklist[99];
	struct burn_progress progress;
	time_t start_time;
	int last_sector = 0, padding = 0, trackno, write_mode_tao = 0, fd;
	off_t fixed_size;
	char *adr;
	struct stat stbuf;

	if (all_tracks_type != BURN_AUDIO) {
		all_tracks_type = BURN_MODE1;
		/* a padding of 300 kB helps to avoid the read-ahead bug */
		padding = 300*1024;
	}

	target_disc = burn_disc_create();
	session = burn_session_create();
	burn_disc_add_session(target_disc, session, BURN_POS_END);

	for (trackno = 0 ; trackno < source_adr_count; trackno++) {
	  tracklist[trackno] = track = burn_track_create();
	  burn_track_define_data(track, 0, padding, 1, all_tracks_type);

	  adr = source_adr[trackno];
	  fixed_size = 0;
	  if (adr[0] == '-' && adr[1] == 0) {
		fd = 0;
	  } else {
		fd = open(adr, O_RDONLY);
		if (fd>=0)
			if (fstat(fd,&stbuf)!=-1)
				if((stbuf.st_mode&S_IFMT)==S_IFREG)
					fixed_size = stbuf.st_size;
	  }
	  if (fixed_size==0)
		write_mode_tao = 1;
	  data_src = NULL;
	  if (fd>=0)
	  	data_src = burn_fd_source_new(fd, -1, fixed_size);
	  if (data_src == NULL) {
		fprintf(stderr,
		       "FATAL: Could not open data source '%s'.\n",adr);
		if(errno!=0)
			fprintf(stderr,"(Most recent system error: %s )\n",
				strerror(errno));
		return 0;
	  }
	  if (burn_track_set_source(track, data_src) != BURN_SOURCE_OK) {
		printf("FATAL: Cannot attach source object to track object\n");
		return 0;
	  }

	  burn_session_add_track(session, track, BURN_POS_END);
	  printf("Track %d : source is '%s'\n", trackno, adr);
	  burn_source_free(data_src);
        } /* trackno loop end */

	while (burn_drive_get_status(drive, NULL) != BURN_DRIVE_IDLE)
		usleep(100001);

	/* Evaluate drive and media */
	while ((disc_state = burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
		usleep(100001);
	if (disc_state != BURN_DISC_BLANK) {
		if (disc_state == BURN_DISC_FULL ||
		    disc_state == BURN_DISC_APPENDABLE) {
			fprintf(stderr, "FATAL: Media with data detected. Need blank media.\n");
			if (burn_disc_erasable(drive))
				fprintf(stderr, "HINT: Try --blank_fast\n\n");
		} else if (disc_state == BURN_DISC_EMPTY) 
			fprintf(stderr,"FATAL: No media detected in drive\n");
		else
			fprintf(stderr,
			 "FATAL: Cannot recognize state of drive and media\n");
		return 0;
	}

	burn_options = burn_write_opts_new(drive);
	burn_write_opts_set_perform_opc(burn_options, 0);
	if (write_mode_tao)
		burn_write_opts_set_write_type(burn_options,
					BURN_WRITE_TAO, BURN_BLOCK_MODE1);
	else
		burn_write_opts_set_write_type(burn_options,
					BURN_WRITE_SAO, BURN_BLOCK_SAO);
	if(simulate_burn)
		printf("\n*** Will TRY to SIMULATE burning ***\n\n");
	burn_write_opts_set_simulate(burn_options, simulate_burn);
	burn_structure_print_disc(target_disc);
	burn_drive_set_speed(drive, 0, 0);
	burn_write_opts_set_underrun_proof(burn_options, 1);

	printf("Burning starts. With e.g. 4x media expect up to a minute of zero progress.\n");
	start_time = time(0);
	burn_disc_write(burn_options, target_disc);

	burn_write_opts_free(burn_options);
	while (burn_drive_get_status(drive, NULL) == BURN_DRIVE_SPAWNING)
		usleep(1002);
	while (burn_drive_get_status(drive, &progress) != BURN_DRIVE_IDLE) {
		if( progress.sectors <= 0 || progress.sector == last_sector)
			printf(
			     "Thank you for being patient since %d seconds.\n",
			     (int) (time(0) - start_time));
		else if(write_mode_tao)
			printf("Track %d : sector %d\n", progress.track,
				progress.sector);
		else
			printf("Track %d : sector %d of %d\n", progress.track,
				progress.sector, progress.sectors);
		last_sector = progress.sector;
		sleep(1);
	}
	printf("\n");

	for (trackno = 0 ; trackno < source_adr_count; trackno++)
		burn_track_free(tracklist[trackno]);
	burn_session_free(session);
	burn_disc_free(target_disc);
	if(simulate_burn)
		printf("\n*** Did TRY to SIMULATE burning ***\n\n");
	return 0;
}


/** The setup parameters of libburn */
static char drive_adr[BURN_DRIVE_ADR_LEN] = {""};
static int driveno = 0;
static int do_blank = 0;
static char source_adr[99][4096];
static int source_adr_count = 0;
static int simulate_burn = 0;
static int all_tracks_type = BURN_MODE1;


/** Converts command line arguments into above setup parameters.
    drive_adr[] must provide at least BURN_DRIVE_ADR_LEN bytes.
    source_adr[] must provide at least 4096 bytes.
*/
int libburner_setup(int argc, char **argv)
{
    int i, insuffient_parameters = 0, print_help = 0;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--audio")) {
            all_tracks_type = BURN_AUDIO;

        } else if (!strcmp(argv[i], "--blank_fast")) {
            do_blank = 1;

        } else if (!strcmp(argv[i], "--blank_full")) {
            do_blank = 2;

        } else if (!strcmp(argv[i], "--burn_for_real")) {
            simulate_burn = 0;

        } else if (!strcmp(argv[i], "--drive")) {
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
	} else if (!strcmp(argv[i], "--stdin_size")) { /* obsoleted */
	    i++;

        } else if (!strcmp(argv[i], "--try_to_simulate")) {
            simulate_burn = 1;

        } else if (!strcmp(argv[i], "--help")) {
            print_help = 1;

        } else if (!strncmp(argv[i], "--",2)) {
            fprintf(stderr, "Unidentified option: %s\n", argv[i]);
            return 7;
        } else {
            if(strlen(argv[i]) >= 4096) {
                fprintf(stderr, "Source address too long (max. %d)\n", 4096-1);
                return 5;
            }
            if(source_adr_count >= 99) {
                fprintf(stderr, "Too many tracks (max. 99)\n");
                return 6;
            }
            strcpy(source_adr[source_adr_count], argv[i]);
            source_adr_count++;
        }
    }
    insuffient_parameters = 1;
    if (driveno < 0)
        insuffient_parameters = 0;
    if (source_adr_count > 0)
        insuffient_parameters = 0; 
    if (do_blank)
        insuffient_parameters = 0;
    if (print_help || insuffient_parameters ) {
        printf("Usage: %s\n", argv[0]);
        printf("       [--drive <address>|<driveno>|\"-\"]  [--audio]\n");
        printf("       [--blank_fast|--blank_full]  [--try_to_simulate]\n");
        printf("       [<one or more imagefiles>|\"-\"]\n");
        printf("Examples\n");
        printf("A bus scan (needs rw-permissions to see a drive):\n");
        printf("  %s --drive -\n",argv[0]);
        printf("Burn a file to drive chosen by number:\n");
        printf("  %s --drive 0 my_image_file\n",
            argv[0]);
        printf("Burn a file to drive chosen by persistent address:\n");
        printf("  %s --drive /dev/hdc my_image_file\n", argv[0]);
        printf("Blank a used CD-RW (is combinable with burning in one run):\n");
        printf("  %s --drive /dev/hdc --blank_fast\n",argv[0]);
        printf("Burn two audio tracks\n");
        printf("  lame --decode -t /path/to/track1.mp3 track1.cd\n");
        printf("  test/dewav /path/to/track2.wav -o track2.cd\n");
        printf("  %s --drive /dev/hdc --audio track1.cd track2.cd\n", argv[0]);
        printf("Burn a compressed afio archive on-the-fly, pad up to 700 MB:\n");
        printf("  ( cd my_directory ; find . -print | afio -oZ - ) | \\\n");
        printf("  %s --drive /dev/hdc -\n", argv[0]);
        printf("To be read from *not mounted* CD via: afio -tvZ /dev/hdc\n");
        printf("Program tar would need a clean EOF which our padded CD cannot deliver.\n");
        if (insuffient_parameters)
            return 6;
    }
    return 0;
}


int main(int argc, char **argv)
{
	int ret;

	ret = libburner_setup(argc, argv);
	if (ret)
		exit(ret);

	printf("Initializing libburn.pykix.org ...\n");
	if (burn_initialize())
		printf("Done\n");
	else {
		printf("FAILED\n");
		fprintf(stderr,"\nFATAL: Failed to initialize.\n");
		exit(33);
	}

	/* Print messages of severity SORRY or more directly to stderr */
	burn_msgs_set_severities("NEVER", "SORRY", "libburner : ");

	/* Activate the default signal handler which eventually will try to
	   properly shutdown drive and library on aborting events. */
	burn_set_signal_handling("libburner : ", NULL, 0);

	/** Note: driveno might change its value in this call */
	ret = libburner_aquire_drive(drive_adr, &driveno);
	if (ret<=0) {
		fprintf(stderr,"\nFATAL: Failed to aquire drive.\n");
		{ ret = 34; goto finish_libburn; }
	}
	if (ret == 2)
		{ ret = 0; goto release_drive; }
	if (do_blank) {
		ret = libburner_blank_disc(drive_list[driveno].drive,
					  do_blank == 1);
		if (ret<=0)
			{ ret = 36; goto release_drive; }
		if (ret != 2 && source_adr_count > 0)
			ret = libburner_regrab(drive_list[driveno].drive);
		if (ret<=0) {
			fprintf(stderr,
	        "FATAL: Cannot release and grab again drive after blanking\n");
			{ ret = 37; goto finish_libburn; }
		}
	}
	if (source_adr_count > 0) {
		ret = libburner_payload(drive_list[driveno].drive,
				source_adr, source_adr_count,
				simulate_burn, all_tracks_type);
		if (ret<=0)
			{ ret = 38; goto release_drive; }
	}
	ret = 0;
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

This all is provided under GPL.
Read. Try. Think. Play. Write yourself some code. Be free of my copyright.

Be also invited to study the code of cdrskin/cdrskin.c et al.


Clarification in my name and in the name of Mario Danic, copyright holder
on toplevel of libburn. To be fully in effect after the remaining other
copyrighted code has been replaced by ours and by copyright-free contributions
of our friends:

We, the copyright holders, agree on the interpretation that
dynamical linking of our libraries constitutes "use of" and
not "derivation from" our work in the sense of GPL, provided
those libraries are compiled from our unaltered code.

Thus you may link our libraries dynamically with applications
which are not under GPL. You may distribute our libraries and
application tools in binary form, if you fulfill the usual
condition of GPL to offer a copy of the source code -altered
or unaltered- under GPL.

We ask you politely to use our work in open source spirit
and with the due reference to the entire open source community.

If there should really arise the case where above clarification
does not suffice to fulfill a clear and neat request in open source
spirit that would otherwise be declined for mere formal reasons,
only in that case we will duely consider to issue a special license
covering only that special case.
It is the open source idea of responsible freedom which will be
decisive and you will have to prove that you exhausted all own
means to qualify for GPL.

For now we are firmly committed to maintain one single license: GPL.

History:
libburner is a compilation of my own contributions to test/burniso.c and
fresh code which replaced the remaining parts under copyright of
Derek Foreman.
My respect and my thanks to Derek for providing me a start back in 2005.

*/

