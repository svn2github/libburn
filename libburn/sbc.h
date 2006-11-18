/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef __SBC
#define __SBC

struct burn_drive;

void sbc_load(struct burn_drive *);
void sbc_eject(struct burn_drive *);

/* ts A61118 */
int sbc_start_unit(struct burn_drive *);

/* ts A61021 : the sbc specific part of sg.c:enumerate_common()
*/
int sbc_setup_drive(struct burn_drive *d);

#endif /* __SBC */
