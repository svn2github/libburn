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
	struct burn_source_file *fs = source->data;

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

	if (fs->fixed_size > 0)
		return fs->fixed_size;
	if (fstat(fs->datafd, &buf) == -1)
		return (off_t) 0;
	return (off_t) buf.st_size;
}


/* ts A70125 */
static int file_set_size(struct burn_source *source, off_t size)
{
	struct burn_source_file *fs = source->data;

	fs->fixed_size = size;
	return 1;
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
	if (subpath != NULL) {
		fd2 = open(subpath, O_RDONLY);
		if (fd2 == -1) {
			close(fd1);
			return NULL;
		}
	}
	fs = malloc(sizeof(struct burn_source_file));

	/* ts A70825 */
	if (fs == NULL) {
failure:;
		close(fd1);
		if (subpath != NULL)
			close(fd2);
		return NULL;
	}

	fs->datafd = fd1;

	if (subpath)
		fs->subfd = fd2;

	/* ts A70125 */
	fs->fixed_size = 0;

	src = burn_source_new();

	/* ts A70825 */
	if (src == NULL) {
		free((char *) fs);
		goto failure;
	}

	src->read = file_read;
	if (subpath)
		src->read_sub = file_read_sub;

	src->get_size = file_size;
	src->set_size = file_set_size;
	src->free_data = file_free;
	src->data = fs;
	return src;
}


/* ts A70126 : removed class burn_source_fd in favor of burn_source_file */

struct burn_source *burn_fd_source_new(int datafd, int subfd, off_t size)
{
	struct burn_source_file *fs;
	struct burn_source *src;

	if (datafd == -1)
		return NULL;
	fs = malloc(sizeof(struct burn_source_file));
	if (fs == NULL) /* ts A70825 */
		return NULL;
	fs->datafd = datafd;
	fs->subfd = subfd;
	fs->fixed_size = size;

	src = burn_source_new();

	/* ts A70825 */
	if (src == NULL) {
		free((char *) fs);
		return NULL;
	}

	src->read = file_read;
	if(subfd != -1)
		src->read = file_read_sub;
	src->get_size = file_size;
	src->set_size = file_set_size;
	src->free_data = file_free;
	src->data = fs;
	return src;
}

