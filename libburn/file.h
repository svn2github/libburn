/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef BURN__FILE_H
#define BURN__FILE_H

/* ts A70125 : 
   Looks like burn_source_file and burn_source_fd become identical because
   of the need to set a fixed_size of at least 600 kB.
   I will try to unify both classes.
*/
struct burn_source_file
{
	int datafd;
	int subfd;
	off_t fixed_size;
};


/* ------ provisory location for the new source subclass fd --------- */

struct burn_source_fd
{
	int datafd;
	int subfd;
	off_t fixed_size;
};

#endif /* LIBBURN__FILE_H */
