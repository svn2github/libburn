/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef BURN__INIT_H
#define BURN__INIT_H

extern int burn_running;

/** Indicator for burn_drive_get_status() wether a signal hit parts of the
    thread team. 
    0= all works well ,
    1 to 5 = waiting for eventual signal on control thread
    > 5 = do abort now
    -1 = control thread has been informed
*/
extern volatile int burn_global_abort_level;
extern int burn_global_abort_signum;
extern void *burn_global_signal_handle;
extern burn_abort_handler_t burn_global_signal_handler;


#endif /* BURN__INIT_H */
