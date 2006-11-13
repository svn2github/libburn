/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
#include <libburn/libburn.h>

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static struct burn_drive_info *drives;
static unsigned int n_drives;

void burn_files(struct burn_drive *drive, struct burn_disc *disc)
{
	struct burn_write_opts *o;
	enum burn_disc_status s;

	if (!burn_drive_grab(drive, 1)) {
		printf("Unable to open the drive!\n");
		return;
	}
	while (burn_drive_get_status(drive, NULL))
		usleep(1000);

	while ((s = burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
		usleep(1000);

	if (s != BURN_DISC_BLANK) {
		burn_drive_release(drive, 0);
		printf("Please insert blank media in the drive\n");
		return;
	}
	o = burn_write_opts_new(drive);
	burn_drive_set_speed(drive, 0, 2816);
	burn_write_opts_set_perform_opc(o, 0);
	burn_write_opts_set_write_type(o, BURN_WRITE_TAO, BURN_BLOCK_MODE1);
	burn_write_opts_set_simulate(o, 1);
/* want failure on seggy while debugging :) */
	burn_write_opts_set_underrun_proof(o, 0);
	burn_structure_print_disc(disc);
	burn_disc_write(o, disc);
	burn_write_opts_free(o);

	while (burn_drive_get_status(drive, NULL)) {
		sleep(1);
	}
	printf("\n");
	burn_drive_release(drive, 0);
	burn_disc_free(disc);
}

void parse_args(int argc, char **argv, int *drive, struct burn_disc **disc)
{
	int i, tmode = BURN_AUDIO;
	int help = 1;
	struct burn_session *session;
	struct burn_track *tr;
	struct burn_source *src;

	*disc = burn_disc_create();
	session = burn_session_create();
	burn_disc_add_session(*disc, session, BURN_POS_END);
	for (i = 1; i < argc; ++i) {
		if (!strcmp(argv[i], "--drive")) {
			++i;
			if (i >= argc)
				printf("--drive requires an argument\n");
			else
				*drive = atoi(argv[i]);
		} else if (!strcmp(argv[i], "--audio")) {
			tmode = BURN_AUDIO;
		} else if (!strcmp(argv[i], "--data")) {
			tmode = BURN_MODE1;
		} else if (!strcmp(argv[i], "--verbose")) {
			++i;
			if (i >= argc)
				printf("--verbose requires an argument\n");
			else
				burn_set_verbosity(atoi(argv[i]));
		} else if (!strcmp(argv[i], "--help")) {
			help = 1;	/* who cares */
		} else {
			help = 0;
			printf("%s is a track\n", argv[i]);

			tr = burn_track_create();
			src = burn_file_source_new(argv[i], NULL);
			burn_track_set_source(tr, src);
			burn_source_free(src);
			burn_track_define_data(tr, 0, 0, 1, tmode);
			burn_session_add_track(session, tr, BURN_POS_END);
		}
	}
	if (help) {
		printf("Usage: %s [--drive <num>] [--verbose <level>] files\n",
		       argv[0]);
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	int drive = 0;
	struct burn_disc *disc;

	parse_args(argc, argv, &drive, &disc);
	printf("Initializing library...");
	if (burn_initialize())
		printf("Success\n");
	else {
		printf("Failed\n");
		return 1;
	}

	printf("Scanning for devices...");
	while (!burn_drive_scan(&drives, &n_drives)) ;
	printf("Done\n");

	burn_files(drives[drive].drive, disc);
	burn_drive_info_free(drives);
	burn_finish();
	return 0;
}
