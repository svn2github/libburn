/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

/* Copyright (c) 2004 - 2006 Derek Foreman, Ben Jansens
   Copyright (c) 2006 - 2017 Thomas Schmitt <scdbackup@gmx.net>
   Provided under GPL version 2 or later.
*/

#ifndef BURN__ASYNC_H
#define BURN__ASYNC_H

void burn_async_join_all(void);
struct burn_write_opts;

/* ts A70930 */
/* To be called when the first read() call comes to a fifo */
int burn_fifo_start(struct burn_source *source, int flag);

/* ts A81108 */
/* To abort a running fifo thread before the fifo object gets deleted */
int burn_fifo_abort(struct burn_source_fifo *fs, int flag);

/* ts B70126 */
#define BURN_ASYNC_LOCK_RELEASE 0
#define BURN_ASYNC_LOCK_OBTAIN  1
#define BURN_ASYNC_LOCK_INIT    2
int burn_async_manage_lock(int mode);

#endif /* BURN__ASYNC_H */
