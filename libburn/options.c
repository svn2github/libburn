#include "libburn.h"
#include "options.h"
#include "transport.h"

/* ts A61007 */
/* #include <a ssert.h> */

#include <stdlib.h>
#include <string.h>

#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;


struct burn_write_opts *burn_write_opts_new(struct burn_drive *drive)
{
	struct burn_write_opts *opts;

	opts = malloc(sizeof(struct burn_write_opts));
	if (opts == NULL) {
		libdax_msgs_submit(libdax_messenger, -1, 0x00020111,
			LIBDAX_MSGS_SEV_FATAL, LIBDAX_MSGS_PRIO_HIGH,
			"Could not allocate new auxiliary object", 0, 0);
		return NULL;
	}
	opts->drive = drive;
	opts->refcount = 1;
	opts->write_type = BURN_WRITE_TAO;
	opts->block_type = BURN_BLOCK_MODE1;
	opts->toc_entry = NULL;
	opts->toc_entries = 0;
	opts->simulate = 0;
	opts->underrun_proof = drive->mdata->underrun_proof;
	opts->perform_opc = 1;
	opts->obs = -1;
	opts->obs_pad = 0;
	opts->start_byte = 0;
	opts->has_mediacatalog = 0;
	opts->format = BURN_CDROM;
	opts->multi = 0;
	opts->control = 0;
	return opts;
}

void burn_write_opts_free(struct burn_write_opts *opts)
{
	if (--opts->refcount <= 0)
		free(opts);
}

struct burn_read_opts *burn_read_opts_new(struct burn_drive *drive)
{
	struct burn_read_opts *opts;

	opts = malloc(sizeof(struct burn_read_opts));
	opts->drive = drive;
	opts->refcount = 1;
	opts->raw = 0;
	opts->c2errors = 0;
	opts->subcodes_audio = 0;
	opts->subcodes_data = 0;
	opts->hardware_error_recovery = 0;
	opts->report_recovered_errors = 0;
	opts->transfer_damaged_blocks = 0;
	opts->hardware_error_retries = 3;

	return opts;
}

void burn_read_opts_free(struct burn_read_opts *opts)
{
	if (--opts->refcount <= 0)
		free(opts);
}

int burn_write_opts_set_write_type(struct burn_write_opts *opts,
				   enum burn_write_types write_type,
				   int block_type)
{
	int sector_get_outmode(enum burn_write_types write_type,
				enum burn_block_types block_type);
	int spc_block_type(enum burn_block_types b);
	
	/* ts A61007 */
	if (! ( (write_type == BURN_WRITE_SAO && block_type == BURN_BLOCK_SAO)
		 || (opts->drive->block_types[write_type] & block_type) ) ) {
bad_combination:;
		libdax_msgs_submit(libdax_messenger, -1, 0x00020112,
			LIBDAX_MSGS_SEV_SORRY, LIBDAX_MSGS_PRIO_HIGH,
			"Bad combination of write_type and block_type", 0, 0);
		return 0;
	}
	/*  ts A61007 : obsoleting Assert in sector.c:get_outmode() */
	if (sector_get_outmode(write_type, (enum burn_block_types) block_type)
		 == -1)
		goto bad_combination;
	/*  ts A61007 : obsoleting Assert in spc.c:spc_block_type() */
	if (spc_block_type((enum burn_block_types) block_type) == -1)
		goto bad_combination;

	opts->write_type = write_type;
	opts->block_type = block_type;
	return 1;

	/* a ssert(0); */
}

void burn_write_opts_set_toc_entries(struct burn_write_opts *opts, int count,
				     struct burn_toc_entry *toc_entries)
{
	opts->toc_entries = count;
	opts->toc_entry = malloc(count * sizeof(struct burn_toc_entry));
	memcpy(opts->toc_entry, &toc_entries,
	       sizeof(struct burn_toc_entry) * count);
}

void burn_write_opts_set_format(struct burn_write_opts *opts, int format)
{
	opts->format = format;
}

int burn_write_opts_set_simulate(struct burn_write_opts *opts, int sim)
{
	if (opts->drive->mdata->simulate) {
		opts->simulate = sim;
		return 1;
	}
	return 0;
}

int burn_write_opts_set_underrun_proof(struct burn_write_opts *opts,
				       int underrun_proof)
{
	if (opts->drive->mdata->underrun_proof) {
		opts->underrun_proof = underrun_proof;
		return 1;
	}
	return 0;
}

void burn_write_opts_set_perform_opc(struct burn_write_opts *opts, int opc)
{
	opts->perform_opc = opc;
}

void burn_write_opts_set_has_mediacatalog(struct burn_write_opts *opts,
					  int has_mediacatalog)
{
	opts->has_mediacatalog = has_mediacatalog;
}

void burn_write_opts_set_mediacatalog(struct burn_write_opts *opts,
				      unsigned char mediacatalog[13])
{
	memcpy(opts->mediacatalog, &mediacatalog, 13);
}


/* ts A61106 */
void burn_write_opts_set_multi(struct burn_write_opts *opts, int multi)
{
	opts->multi = !!multi;
}


/* ts A61222 */
void burn_write_opts_set_start_byte(struct burn_write_opts *opts, off_t value)
{
	opts->start_byte = value;
}


void burn_read_opts_set_raw(struct burn_read_opts *opts, int raw)
{
	opts->raw = raw;
}

void burn_read_opts_set_c2errors(struct burn_read_opts *opts, int c2errors)
{
	opts->c2errors = c2errors;
}

void burn_read_opts_read_subcodes_audio(struct burn_read_opts *opts,
					int subcodes_audio)
{
	opts->subcodes_audio = subcodes_audio;
}

void burn_read_opts_read_subcodes_data(struct burn_read_opts *opts,
				       int subcodes_data)
{
	opts->subcodes_data = subcodes_data;
}

void burn_read_opts_set_hardware_error_recovery(struct burn_read_opts *opts,
						int hardware_error_recovery)
{
	opts->hardware_error_recovery = hardware_error_recovery;
}

void burn_read_opts_report_recovered_errors(struct burn_read_opts *opts,
					    int report_recovered_errors)
{
	opts->report_recovered_errors = report_recovered_errors;
}

void burn_read_opts_transfer_damaged_blocks(struct burn_read_opts *opts,
					    int transfer_damaged_blocks)
{
	opts->transfer_damaged_blocks = transfer_damaged_blocks;
}

void burn_read_opts_set_hardware_error_retries(struct burn_read_opts *opts,
					       unsigned char
					       hardware_error_retries)
{
	opts->hardware_error_retries = hardware_error_retries;
}

