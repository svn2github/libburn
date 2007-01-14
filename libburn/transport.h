/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef __TRANSPORT
#define __TRANSPORT

#include "libburn.h"
#include "os.h"

#include <pthread.h>
/* sg data structures */
#include <sys/types.h>


/* see os.h for name of particular os-*.h where this is defined */
#define BUFFER_SIZE BURN_OS_TRANSPORT_BUFFER_SIZE


enum transfer_direction
{ TO_DRIVE, FROM_DRIVE, NO_TRANSFER };

/* end of sg data structures */

/* generic 'drive' data structures */

struct cue_sheet
{
	int count;
	unsigned char *data;
};

struct params
{
	int speed;
	int retries;
};

struct buffer
{
	/* ts A61219: 
	   Added 4096 bytes reserve against possible buffer overflows.
	   (Changed in sector.c buffer flush test from >= to > BUFFER_SIZE .
	    This can at most cause a 1 sector overlap. Sometimes an offset
	    of 16 byte is applied to the output data (in some RAW mode). ) */
	unsigned char data[BUFFER_SIZE + 4096];
	int sectors;
	int bytes;
};

struct command
{
	unsigned char opcode[16];
	int oplen;
	int dir;
	unsigned char sense[128];
	int error;
	int retry;
	struct buffer *page;
};

struct burn_scsi_inquiry_data
{
	char vendor[9];
	char product[17];
	char revision[5];
	int valid;
};


struct scsi_mode_data
{
	int buffer_size;
	int dvdram_read;
	int dvdram_write;
	int dvdr_read;
	int dvdr_write;
	int dvdrom_read;
	int cdrw_read;
	int cdrw_write;
	int cdr_read;
	int cdr_write;
	int simulate;
	int max_read_speed;
	int max_write_speed;

	/* ts A61021 */
	int min_write_speed;

	/* ts A61225 : Results from ACh GET PERFORMANCE, Type 03h
	               Speed values go into *_*_speed */
	int min_end_lba;
	int max_end_lba;
	struct burn_speed_descriptor *speed_descriptors;

	int cur_read_speed;
	int cur_write_speed;
	int retry_page_length;
	int retry_page_valid;
	int write_page_length;
	int write_page_valid;
	int c2_pointers;
	int valid;
	int underrun_proof;
};


/* ts A70112 : represents a single Formattable Capacity Descriptor as of
               mmc5r03c.pdf 6.24.3.3 . There can at most be 32 of them. */
struct burn_format_descr {
	/* format type: e.g 0x00 is "Full", 0x15 is "Quick" */
	int type;

	/* the size in bytes derived from Number of Blocks */
	off_t size;

	/* the Type Dependent Parameter (usually the write alignment size) */
	unsigned tdp;
};


#define LIBBURN_SG_MAX_SIBLINGS 16

/** Gets initialized in enumerate_common() and burn_drive_register() */
struct burn_drive
{
	int bus_no;
	int host;
	int id;
	int channel;
	int lun;
	char *devname;


	/* see os.h for name of particular os-*.h where this is defined */
	BURN_OS_TRANSPORT_DRIVE_ELEMENTS	


	/* ts A60904 : ticket 62, contribution by elmom */
	/**
	    Tells the index in scanned burn_drive_info array.
	    -1 if fallen victim to burn_drive_info_forget()
	*/
	int global_index;

	pthread_mutex_t access_lock;

	enum burn_disc_status status;
	int erasable;

	/* ts A61201 from 46h GET CONFIGURATION  */
	int current_profile;
	char current_profile_text[80];
	int current_is_cd_profile;
	int current_is_supported_profile;

	/* ts A70114 : wether a DVD-RW media holds an incomplete session
	               (which could need closing after write) */
	int dvd_minus_rw_incomplete;

	/* ts A61218 from 46h GET CONFIGURATION  */
	int bg_format_status; /* 0=needs format start, 1=needs format restart*/

	/* ts A70108 from 23h READ FORMAT CAPACITY mmc5r03c.pdf 6.24 */
	int format_descr_type;      /* 1=unformatted, 2=formatted, 3=unclear */
	off_t format_curr_max_size;  /* meaning depends on format_descr_type */
	unsigned format_curr_blsas;  /* meaning depends on format_descr_type */
	int best_format_type;
	off_t best_format_size;

	/* The complete list of format descriptors as read with 23h */
	int num_format_descr;
	struct burn_format_descr format_descriptors[32];
	

	volatile int released;

	/* ts A61106 */
	int silent_on_scsi_error;

	int nwa;		/* next writeable address */
	int alba;		/* absolute lba */
	int rlba;		/* relative lba in section */
	int start_lba;
	int end_lba;
	int toc_temp;
	struct burn_disc *disc;	/* disc structure */
	int block_types[4];
	struct buffer *buffer;
	struct burn_progress progress;

	volatile int cancel;
	volatile enum burn_drive_status busy;
/* transport functions */
	int (*grab) (struct burn_drive *);
	int (*release) (struct burn_drive *);

	/* ts A61021 */
	int (*drive_is_open) (struct burn_drive *);

	int (*issue_command) (struct burn_drive *, struct command *);

/* lower level functions */
	void (*erase) (struct burn_drive *, int);
	void (*getcaps) (struct burn_drive *);

	/* ts A61021 */
	void (*read_atip) (struct burn_drive *);

	int (*write) (struct burn_drive *, int, struct buffer *);
	void (*read_toc) (struct burn_drive *);
	void (*lock) (struct burn_drive *);
	void (*unlock) (struct burn_drive *);
	void (*eject) (struct burn_drive *);
	void (*load) (struct burn_drive *);
	int (*start_unit) (struct burn_drive *);
	void (*read_disc_info) (struct burn_drive *);
	void (*read_sectors) (struct burn_drive *,
			      int start,
			      int len,
			      const struct burn_read_opts *, struct buffer *);
	void (*perform_opc) (struct burn_drive *);
	void (*set_speed) (struct burn_drive *, int, int);
	void (*send_parameters) (struct burn_drive *,
				 const struct burn_read_opts *);
	void (*send_write_parameters) (struct burn_drive *,
				       const struct burn_write_opts *);
	void (*send_cue_sheet) (struct burn_drive *, struct cue_sheet *);
	void (*sync_cache) (struct burn_drive *);
	int (*get_erase_progress) (struct burn_drive *);
	int (*get_nwa) (struct burn_drive *, int trackno, int *lba, int *nwa);

	/* ts A61009 : removed d in favor of o->drive */
	/* void (*close_disc) (struct burn_drive * d,
				 struct burn_write_opts * o);
	   void (*close_session) (struct burn_drive * d,
			       struct burn_write_opts * o);
	*/
	void (*close_disc) (struct burn_write_opts * o);
	void (*close_session) ( struct burn_write_opts * o);

	/* ts A61029 */
	void (*close_track_session) ( struct burn_drive *d,
				int session, int track);

	int (*test_unit_ready) (struct burn_drive * d);
	void (*probe_write_modes) (struct burn_drive * d);
	struct params params;
	struct burn_scsi_inquiry_data *idata;
	struct scsi_mode_data *mdata;
	int toc_entries;
	struct burn_toc_entry *toc_entry;

	/* ts A61023 : get size and free space of drive buffer */
	int (*read_buffer_capacity) (struct burn_drive *d);

	/* ts A61220 : format media (e.g. DVD+RW) */
	int (*format_unit) (struct burn_drive *d, off_t size, int flag);

	/* ts A70108 */
	/* mmc5r03c.pdf 6.24 : get list of available formats */
	int (*read_format_capacities) (struct burn_drive *d, int top_wanted);

};

/* end of generic 'drive' data structures */

#endif /* __TRANSPORT */
