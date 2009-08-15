/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef __MMC
#define __MMC

struct burn_drive;
struct burn_write_opts;
struct command;
struct buffer;
struct cue_sheet;

/* MMC commands */

void mmc_read(struct burn_drive *);

/* ts A61009 : removed redundant parameter d in favor of o->drive */
/* void mmc_close_session(struct burn_drive *, struct burn_write_opts *); */
/* void mmc_close_disc(struct burn_drive *, struct burn_write_opts *); */
void mmc_close_session(struct burn_write_opts *o);
void mmc_close_disc(struct burn_write_opts *o);

void mmc_close(struct burn_drive *, int session, int track);
void mmc_get_event(struct burn_drive *);
int mmc_write(struct burn_drive *, int start, struct buffer *buf);
void mmc_write_12(struct burn_drive *d, int start, struct buffer *buf);
void mmc_sync_cache(struct burn_drive *);
void mmc_load(struct burn_drive *);
void mmc_eject(struct burn_drive *);
void mmc_erase(struct burn_drive *, int);
void mmc_read_toc(struct burn_drive *);
void mmc_read_disc_info(struct burn_drive *);
void mmc_read_atip(struct burn_drive *);
void mmc_read_sectors(struct burn_drive *,
		      int,
		      int, const struct burn_read_opts *, struct buffer *);
void mmc_set_speed(struct burn_drive *, int, int);
void mmc_read_lead_in(struct burn_drive *, struct buffer *);
void mmc_perform_opc(struct burn_drive *);
void mmc_get_configuration(struct burn_drive *);

/* ts A61110 : added parameters trackno, lba, nwa. Redefined return value.
   @return 1=nwa is valid , 0=nwa is not valid , -1=error */
int mmc_get_nwa(struct burn_drive *d, int trackno, int *lba, int *nwa);

void mmc_send_cue_sheet(struct burn_drive *, struct cue_sheet *);

/* ts A61023 : get size and free space of drive buffer */
int mmc_read_buffer_capacity(struct burn_drive *d);

/* ts A61021 : the mmc specific part of sg.c:enumerate_common()
*/
int mmc_setup_drive(struct burn_drive *d);

/* ts A61219 : learned much from dvd+rw-tools-7.0: plus_rw_format()
               and mmc5r03c.pdf, 6.5 FORMAT UNIT */
int mmc_format_unit(struct burn_drive *d, off_t size, int flag);

/* ts A61225 : obtain write speed descriptors via ACh GET PERFORMANCE */
int mmc_get_write_performance(struct burn_drive *d);


/* ts A61229 : outsourced from spc_select_write_params() */
/* Note: Page data is not zeroed here to allow preset defaults. Thus
           memset(pd, 0, 2 + d->mdata->write_page_length); 
         is the eventual duty of the caller.
*/
int mmc_compose_mode_page_5(struct burn_drive *d,
                            const struct burn_write_opts *o,
                            unsigned char *pd);

/* ts A70812 : return 0 = ok , return BE_CANCELLED = error occured */
int mmc_read_10(struct burn_drive *d, int start, int amount,
                struct buffer *buf);

/* ts A81210 : Determine the upper limit of readable data size */
int mmc_read_capacity(struct burn_drive *d);

/* ts A61201 */
char *mmc_obtain_profile_name(int profile_number);


/* mmc5r03c.pdf 4.3.4.4.1 d) "The maximum number of RZones is 2 302." */
#define BURN_MMC_FAKE_TOC_MAX_SIZE 2302

#endif /*__MMC*/
