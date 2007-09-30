/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef BURN__FILE_H
#define BURN__FILE_H

struct burn_source_file
{
	int datafd;
	int subfd;
	off_t fixed_size;
};

/* ts A70126 : burn_source_file obsoleted burn_source_fd */


/* ts A70930 */
struct burn_source_fifo {

	/* The fifo stays inactive and unequipped with eventual resources
	   until its read() method is called for the first time.
	   Only then burn_fifo_start() gets called, allocates the complete
           resources, starts a thread with burn_fifo_source_shuffler()
	   which shuffles data and finally destroys the resources.
	   This late start is to stay modest in case of multiple tracks
	   in one disc.
	*/
	int is_started;

	int thread_pid;
	int thread_pid_valid;

	/* the burn_source for which this fifo is acting as proxy */
	struct burn_source *inp;

	/* <<< currently it is only a pipe */
	int outlet[2];

	/* >>> later it will be a ring buffer mechanism */
	int chunksize;
	int chunks;
	char *buf;

	off_t in_counter;
	off_t out_counter;
};


/** The worker behind the fifo thread.
    Gets started from burn_fifo_start() in async.c
*/
int burn_fifo_source_shuffler(struct burn_source *source, int flag);


#endif /* LIBBURN__FILE_H */
