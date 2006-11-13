/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#include <libburn/libburn.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static struct burn_drive_info *drives;
static unsigned int n_drives;

static void show_tocs()
{
	struct burn_session **sessions;
	struct burn_track **tracks;
	struct burn_disc *disc;
	int nses, ntracks, hidefirst;
	unsigned int i, j, k;
	struct burn_toc_entry e;
	enum burn_disc_status s;

	for (i = 0; i < n_drives; ++i) {
		fprintf(stderr, "TOC for disc in %s - %s:\n",
			drives[i].vendor, drives[i].product);

		if (!burn_drive_grab(drives[i].drive, 1)) {
			fprintf(stderr, "Unable to open the drive!\n");
			continue;
		}

		while (burn_drive_get_status(drives[i].drive, NULL))
			usleep(1000);

		while ((s = burn_disc_get_status(drives[i].drive))
		       == BURN_DISC_UNREADY)
			usleep(1000);
		if (s != BURN_DISC_FULL) {
			burn_drive_release(drives[i].drive, 0);
			fprintf(stderr, "No disc found!\n");
			continue;
		}

		disc = burn_drive_get_disc(drives[i].drive);

		sessions = burn_disc_get_sessions(disc, &nses);
		for (k = 0; k < nses; ++k) {
			tracks = burn_session_get_tracks(sessions[k],
							 &ntracks);
			hidefirst = burn_session_get_hidefirst(sessions[k]);
			if (hidefirst)
				fprintf(stderr,
					"track: GAP (%2d) lba: %9d (%9d) %02d:%02d:%02d adr: X control: X mode: %d\n",
					k + 1, 0, 0, 0, 2, 0,
					burn_track_get_mode(tracks[0]));

			for (j = !!hidefirst; j < ntracks; ++j) {
				burn_track_get_entry(tracks[j], &e);
				fprintf(stderr,
					"track: %3d (%2d) lba: %9d (%9d) %02d:%02d:%02d "
					"adr: %d control: %d mode: %d\n",
					e.point, e.session,
					burn_msf_to_lba(e.pmin, e.psec,
							e.pframe),
					burn_msf_to_lba(e.pmin, e.psec,
							e.pframe) * 4,
					e.pmin, e.psec, e.pframe, e.adr,
					e.control,
					burn_track_get_mode(tracks[j]));
			}
			burn_session_get_leadout_entry(sessions[k], &e);
			fprintf(stderr,
				"track:lout (%2d) lba: %9d (%9d) %02d:%02d:%02d "
				"adr: %d control: %d mode: %d\n",
				k + 1, burn_msf_to_lba(e.pmin, e.psec,
						       e.pframe),
				burn_msf_to_lba(e.pmin, e.psec,
						e.pframe) * 4, e.pmin,
				e.psec, e.pframe, e.adr, e.control, -1);
		}
		burn_disc_free(disc);
		burn_drive_release(drives[i].drive, 0);
	}
}

int main()
{
	fprintf(stderr, "Initializing library...");
	if (burn_initialize())
		fprintf(stderr, "Success\n");
	else {
		printf("Failed\n");
		return 1;
	}

	fprintf(stderr, "Scanning for devices...");
	while (!burn_drive_scan(&drives, &n_drives)) ;
	fprintf(stderr, "Done\n");

	show_tocs();
	burn_drive_info_free(drives);
	burn_finish();
	return 0;
}
