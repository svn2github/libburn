/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef BURN__WRITE_H
#define BURN__WRITE_H

struct cue_sheet;
struct burn_session;
struct burn_write_opts;
struct burn_disc;

struct cue_sheet *burn_create_toc_entries(struct burn_write_opts *o,
					  struct burn_session *session,
					  int nwa);
int burn_sector_length(int trackmode);
int burn_subcode_length(int trackmode);

/* ts A61009 */
int burn_disc_write_is_ok(struct burn_write_opts *o, struct burn_disc *disc,
			int flag);

void burn_disc_write_sync(struct burn_write_opts *o, struct burn_disc *disc);
int burn_write_leadin(struct burn_write_opts *o,
		       struct burn_session *s, int first);
int burn_write_leadout(struct burn_write_opts *o,
			int first, unsigned char control, int mode);
int burn_write_session(struct burn_write_opts *o, struct burn_session *s);
int burn_write_track(struct burn_write_opts *o, struct burn_session *s,
		      int tnum);
int burn_write_flush(struct burn_write_opts *o, struct burn_track *track);

/* ts A61030 : necessary for TAO */
int burn_write_close_track(struct burn_write_opts *o, struct burn_session *s,
                           int tnum);
int burn_write_close_session(struct burn_write_opts *o,struct burn_session *s);



/* mmc5r03c.pdf 6.3.3.3.3: DVD-R DL: Close Function 010b: Close Session
     "When the recording mode is Incremental Recording,
      the disc is single session."
   Enable this macro to get away from growisofs which uses Close Session
   but also states "// DVD-R DL Seq has no notion of multi-session".

     #define Libburn_dvd_r_dl_multi_no_close_sessioN 1

*/

#endif /* BURN__WRITE_H */
