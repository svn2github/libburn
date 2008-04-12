#ifndef BURN__OPTIONS_H
#define BURN__OPTIONS_H

#include "libburn.h"

/** Options for disc writing operations. This should be created with
    burn_write_opts_new() and freed with burn_write_opts_free(). */
struct burn_write_opts
{
	/** Drive the write opts are good for */
	struct burn_drive *drive;

	/** For internal use. */
	int refcount;

	/** The method/style of writing to use. */
	enum burn_write_types write_type;
	/** format of the data to send to the drive */
	enum burn_block_types block_type;

	/** Number of toc entries.  if this is 0, they will be auto generated*/
	int toc_entries;
	/** Toc entries for the disc */
	struct burn_toc_entry *toc_entry;

	/** Simulate the write so that the disc is not actually written */
	unsigned int simulate:1;
	/** If available, enable a drive feature which prevents buffer
	    underruns if not enough data is available to keep up with the
	    drive. */
	unsigned int underrun_proof:1;
	/** Perform calibration of the drive's laser before beginning the
	    write. */
	unsigned int perform_opc:1;

	/* ts A61219 : Output block size to trigger buffer flush if hit.
			 -1 with CD, 32 kB with DVD */
	int obs;
	int obs_pad; /* 1=pad up last block to obs */

	/* ts A61222 : Start address for media which allow a choice */
	off_t start_byte;

	/* ts A70213 : Wether to fill up the available space on media */
	int fill_up_media;

	/* ts A70303 : Wether to override conformance checks:
	   - the check wether CD write+block type is supported by the drive 
	*/
	int force_is_set;

	/* ts A80412 : whether to use WRITE12 with Streaming bit set
	   rather than WRITE10. Speeds up DVD-RAM. Might help with BD-RE.
	   This gets transferred to burn_drive.do_stream_recording */
	int do_stream_recording;

	/** A disc can have a media catalog number */
	int has_mediacatalog;
	unsigned char mediacatalog[13];
	/** Session format */
	int format;
	/* internal use only */
	unsigned char control;
	unsigned char multi;
};

/** Options for disc reading operations. This should be created with
    burn_read_opts_new() and freed with burn_read_opts_free(). */
struct burn_read_opts
{
	/** Drive the read opts are good for */
	struct burn_drive *drive;

	/** For internal use. */
	int refcount;

	/** Read in raw mode, so that everything in the data tracks on the
	    disc is read, including headers. Not needed if just reading a
	    filesystem off a disc, but it should usually be used when making a
	    disc image or copying a disc. */
	unsigned int raw:1;
	/** Report c2 errors. Useful for statistics reporting */
	unsigned int c2errors:1;
	/** Read subcodes from audio tracks on the disc */
	unsigned int subcodes_audio:1;
	/** Read subcodes from data tracks on the disc */
	unsigned int subcodes_data:1;
	/** Have the drive recover errors if possible */
	unsigned int hardware_error_recovery:1;
	/** Report errors even when they were recovered from */
	unsigned int report_recovered_errors:1;
	/** Read blocks even when there are unrecoverable errors in them */
	unsigned int transfer_damaged_blocks:1;

	/** The number of retries the hardware should make to correct
	    errors. */
	unsigned char hardware_error_retries;
};

#endif /* BURN__OPTIONS_H */
