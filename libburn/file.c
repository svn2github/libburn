/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "source.h"
#include "libburn.h"
#include "file.h"

/* main channel data can be padded on read, but 0 padding the subs will make
an unreadable disc */


/* This is a generic OS oriented function wrapper which compensates
   shortcommings of read() in respect to a guaranteed amount of return data.
   See  man 2 read , paragraph "RETURN VALUE".
   Possibly libburn/file.c is not the right storage location for this.
   To make it ready for a move, this function is not declared static.
*/
static int read_full_buffer(int fd, unsigned char *buffer, int size)
{
	int ret,summed_ret = 0;

	/* make safe against partial buffer returns */
	while (1) {
		ret = read(fd, buffer + summed_ret, size - summed_ret);
		if (ret <= 0)
	break;
		summed_ret += ret;
		if (summed_ret >= size)
	break;
	}
	if (ret < 0)		 /* error encountered. abort immediately */
		return ret;
	return summed_ret;
}


static int file_read(struct burn_source *source,
		     unsigned char *buffer,
		     int size)
{
	struct burn_source_fd *fs = source->data;

	return read_full_buffer(fs->datafd, buffer, size);
}

static int file_read_sub(struct burn_source *source,
			 unsigned char *buffer,
			 int size)
{
	struct burn_source_file *fs = source->data;

	return read_full_buffer(fs->subfd, buffer, size);
}

static void file_free(struct burn_source *source)
{
	struct burn_source_file *fs = source->data;

	close(fs->datafd);
	if (source->read_sub)
		close(fs->subfd);
	free(fs);
}

static off_t file_size(struct burn_source *source)
{
	struct stat buf;
	struct burn_source_file *fs = source->data;

	if (fstat(fs->datafd, &buf) == -1)
		return (off_t) 0;
	/* for now we keep it compatible to the old (int) return value */
	if(buf.st_size >= 1308622848)  /* 2 GB - 800 MB to prevent rollover */ 
		return (off_t) 1308622848;
	return (off_t) buf.st_size;
}

struct burn_source *burn_file_source_new(const char *path, const char *subpath)
{
	struct burn_source_file *fs;
	struct burn_source *src;
	int fd1, fd2 = 0;

	if (!path)
		return NULL;
	fd1 = open(path, O_RDONLY);
	if (fd1 == -1)
		return NULL;
	if (subpath) {
		fd2 = open(subpath, O_RDONLY);
		if (fd2 == -1) {
			close(fd1);
			return NULL;
		}
	}
	fs = malloc(sizeof(struct burn_source_file));
	fs->datafd = fd1;

	if (subpath)
		fs->subfd = fd2;

	src = burn_source_new();
	src->read = file_read;
	if (subpath)
		src->read_sub = file_read_sub;

	src->get_size = file_size;
	src->free_data = file_free;
	src->data = fs;
	return src;
}


/* ------ provisory location for the new source subclass fd --------- */

static off_t fd_get_size(struct burn_source *source)
{
	struct stat buf;
	struct burn_source_fd *fs = source->data;

	if (fs->fixed_size > 0)
		return fs->fixed_size;
	if (fstat(fs->datafd, &buf) == -1)
		return (off_t) 0;
	/* for now we keep it compatible to the old (int) return value */
	if (buf.st_size >= 1308622848) /* 2 GB - 800 MB to prevent rollover */
		return (off_t) 1308622848;
	return buf.st_size;
}

static int fd_read(struct burn_source *source,
		     unsigned char *buffer,
		     int size)
{
	struct burn_source_fd *fs = source->data;

	return read_full_buffer(fs->datafd, buffer, size);
}


static int fd_read_sub(struct burn_source *source,
		     unsigned char *buffer,
		     int size)
{
	struct burn_source_fd *fs = source->data;

	return read_full_buffer(fs->subfd, buffer, size);
}


static void fd_free_data(struct burn_source *source)
{
	struct burn_source_fd *fs = source->data;

	close(fs->datafd);
	if (source->read_sub)
		close(fs->subfd);
	free(fs);
}


struct burn_source *burn_fd_source_new(int datafd, int subfd, off_t size)
{
	struct burn_source_fd *fs;
	struct burn_source *src;

	if (datafd == -1)
		return NULL;
	fs = malloc(sizeof(struct burn_source_fd));
	fs->datafd = datafd;
	fs->subfd = subfd;
	fs->fixed_size = size;

	src = burn_source_new();
	src->read = fd_read;
	if(subfd != -1)
		src->read = fd_read_sub;
	src->get_size = fd_get_size;
	src->free_data = fd_free_data;
	src->data = fs;
	return src;
}

