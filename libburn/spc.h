/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef __SPC
#define __SPC

#include "libburn.h"

void spc_inquiry(struct burn_drive *);
void spc_prevent(struct burn_drive *);
void spc_allow(struct burn_drive *);
void spc_sense_caps(struct burn_drive *);
void spc_sense_error_params(struct burn_drive *);
void spc_select_error_params(struct burn_drive *,
			     const struct burn_read_opts *);
void spc_getcaps(struct burn_drive *d);
void spc_sense_write_params(struct burn_drive *);
void spc_select_write_params(struct burn_drive *,
			     const struct burn_write_opts *);
void spc_probe_write_modes(struct burn_drive *);
void spc_request_sense(struct burn_drive *d, struct buffer *buf);
int spc_block_type(enum burn_block_types b);
int spc_get_erase_progress(struct burn_drive *d);
int spc_test_unit_ready(struct burn_drive *d);

/* ts A61021 : the spc specific part of sg.c:enumerate_common()
*/
int spc_setup_drive(struct burn_drive *d);

/* ts A61021 : the general SCSI specific part of sg.c:enumerate_common()
   @param flag Bitfield for control purposes
               bit0= do not setup spc/sbc/mmc
*/
int burn_scsi_setup_drive(struct burn_drive *d, int bus_no, int host_no,
			int channel_no, int target_no, int lun_no, int flag);


#endif /*__SPC*/
