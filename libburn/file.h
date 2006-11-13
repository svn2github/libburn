/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#ifndef BURN__FILE_H
#define BURN__FILE_H

struct burn_source_file
{
	int datafd;
	int subfd;
};


/* ------ provisory location for the new source subclass fd --------- */

struct burn_source_fd
{
	int datafd;
	int subfd;
	off_t fixed_size;
};

#endif /* LIBBURN__FILE_H */
