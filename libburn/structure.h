#ifndef BURN__STRUCTURE_H
#define BURN__STRUCTURE_H

struct isrc
{
	int has_isrc;
	char country[2];	/* each must be 0-9, A-Z */
	char owner[3];		/* each must be 0-9, A-Z */
	unsigned char year;	/* must be 0-99 */
	unsigned int serial;	/* must be 0-99999 */
};

struct burn_track
{
	int refcnt;
	struct burn_toc_entry *entry;
	unsigned char indices;
	/* lba address of the index */
	unsigned int index[99];
	/** number of 0 bytes to write before data */
	int offset;
	/** how much offset has been used */
	int offsetcount;
	/** Number of zeros to write after data */
	int tail;
	/** how much tail has been used */
	int tailcount;
	/** 1 means Pad with zeros, 0 means start reading the next track */
	int pad;

	/* ts A70213 : wether to expand this track to full available media */
	int fill_up_media;

	/* ts A70218 : a track size to use if it is mandarory to have some */
	off_t default_size;

	/** Data source */
	struct burn_source *source;
	/** End of Source flag */
	int eos;

	/* ts A61101 */
	off_t sourcecount;
	off_t writecount;
	off_t written_sectors;

	/* ts A61031 */
	/** Source is of undefined length */
	int open_ended;
	/** End of open ended track flag : offset+payload+tail are delivered */
	int track_data_done;

	/** The audio/data mode for the entry. Derived from control and
	    possibly from reading the track's first sector. */
	int mode;
	/** The track contains interval one of a pregap */
	int pregap1;
	/** The track contains interval two of a pregap */
	int pregap2;
	/** The track contains a postgap */
	int postgap;
	struct isrc isrc;

	/* ts A61024 */
	/** Byte swapping on source data stream : 0=none , 1=pairwise */
	int swap_source_bytes;
};

struct burn_session
{
	unsigned char firsttrack;
	unsigned char lasttrack;
	int hidefirst;
	unsigned char start_m;
	unsigned char start_s;
	unsigned char start_f;
	struct burn_toc_entry *leadout_entry;

	int tracks;
	struct burn_track **track;
	int refcnt;
};

struct burn_disc
{
	int sessions;
	struct burn_session **session;
	int refcnt;
};

int burn_track_get_shortage(struct burn_track *t);


/* ts A61031 : might go to libburn.h */
int burn_track_is_open_ended(struct burn_track *t);
int burn_track_is_data_done(struct burn_track *t);

/* ts A70125 : sets overall sectors of a track: offset+payload+padding */
int burn_track_set_sectors(struct burn_track *t, int sectors);

/* ts A70218 : sets the payload size alone */
int burn_track_set_size(struct burn_track *t, off_t size);

/* ts A70213 */
int burn_track_set_fillup(struct burn_track *t, int fill_up_media);
int burn_track_apply_fillup(struct burn_track *t, off_t max_size, int flag);

/* ts A70218 */
off_t burn_track_get_default_size(struct burn_track *t);


/* ts A80808 : Enhance CD toc to DVD toc */
int burn_disc_cd_toc_extensions(struct burn_disc *d, int flag);


#endif /* BURN__STRUCTURE_H */
