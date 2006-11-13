/* THIS IS NOT A PROPER EXAMPLE */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <libburn/libburn.h>

static struct burn_drive_info *drives;
static unsigned int n_drives;

#warning this example is totally fried
int main()
{
#if 0
	struct burn_drive *drive;
	struct burn_read_opts o;

	burn_initialize();
	o.datafd =
		open("/xp/burn/blah.data", O_CREAT | O_WRONLY,
		     S_IRUSR | S_IWUSR);
	o.subfd =
		open("/xp/burn/blah.sub", O_CREAT | O_WRONLY,
		     S_IRUSR | S_IWUSR);
	o.raw = 1;
	o.c2errors = 0;
	o.subcodes_audio = 1;
	o.subcodes_data = 1;
	o.hardware_error_recovery = 1;
	o.report_recovered_errors = 0;
	o.transfer_damaged_blocks = 1;
	o.hardware_error_retries = 1;

	printf("Scanning for devices...");
	while (!burn_drive_scan(&drives, &n_drives)) ;
	printf("Done\n");
	drive = drives[0].drive;
	burn_drive_set_speed(drive, 0, 0);

	if (!burn_drive_grab(drive, 1)) {
		printf("Unable to open the drive!\n");
		return EXIT_FAILURE;
	}

	while (burn_drive_get_status(drive, NULL))
		usleep(1000);

	burn_disc_read(drive, &o);
#endif
	return EXIT_SUCCESS;
}
