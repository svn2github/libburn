/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "source.h"
#include "libburn.h"
#include "file.h"
#include "async.h"

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
	int fd1 = -1, fd2 = -1;

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
		if (fd2 >= 0)
			close(fd2);
		return NULL;
	}

	fs->datafd = fd1;
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
		src->read_sub = file_read_sub;
	src->get_size = file_size;
	src->set_size = file_set_size;
	src->free_data = file_free;
	src->data = fs;
	return src;
}


/* ts A70930 */
/* ----------------------------- fifo ---------------------------- */

/* The fifo mechanism consists of a burn_source proxy which is here,
   a thread management team which is located in async.c,
   and a synchronous shuffler which is here.
*/

static int fifo_read(struct burn_source *source,
		     unsigned char *buffer,
		     int size)
{
	struct burn_source_fifo *fs = source->data;
	int ret;

        if (fs->is_started == 0) {
		ret = burn_fifo_start(source, 0);
		if (ret <= 0) {

			/* >>> cannot start fifo thread */;

			return -1;
		}
		fs->is_started = 1;
	}
	if (size == 0)
		return 0;

	ret = read_full_buffer(fs->outlet[0], buffer, size);
	if (ret > 0)
		fs->out_counter += ret;
	return ret;
}


static off_t fifo_get_size(struct burn_source *source)
{
	struct burn_source_fifo *fs = source->data;

	return fs->inp->get_size(fs->inp);
}


static int fifo_set_size(struct burn_source *source, off_t size)
{
	struct burn_source_fifo *fs = source->data;

	return fs->inp->set_size(fs->inp, size);
}


static void fifo_free(struct burn_source *source)
{
	struct burn_source_fifo *fs = source->data;

	if (fs->outlet[1] >= 0)
		close(fs->outlet[1]);
	free(fs);
}


struct burn_source *burn_fifo_source_new(struct burn_source *inp,
		 		int chunksize, int chunks, int flag)
{
	struct burn_source_fifo *fs;
	struct burn_source *src;
	int ret, outlet[2];

	ret = pipe(outlet);
	if (ret == -1) {
		/* >>> error on pipe creation */;
		return NULL;
	}

	fs = malloc(sizeof(struct burn_source_fifo));
	if (fs == NULL)
		return NULL;
	fs->is_started = 0;
	fs->thread_pid = 0;
	fs->thread_pid_valid = 0;
	fs->inp = inp;
	fs->outlet[0] = outlet[0];
	fs->outlet[1] = outlet[1];
	fs->chunksize = chunksize;
	if (chunksize <= 0)
		fs->chunksize = 2048;
	fs->chunks = chunks;
	fs->buf = NULL;
	fs->in_counter = fs->out_counter = 0;

	src = burn_source_new();
	if (src == NULL) {
		free((char *) fs->buf);
		free((char *) fs);
		return NULL;
	}

	src->read = fifo_read;
	src->read_sub = NULL;
	src->get_size = fifo_get_size;
	src->set_size = fifo_set_size;
	src->free_data = fifo_free;
	src->data = fs;
	return src;
}


int burn_fifo_source_shuffler(struct burn_source *source, int flag)
{
	struct burn_source_fifo *fs = source->data;
	int ret;

	fs->thread_pid = getpid();
	fs->thread_pid_valid = 1;

	while (1) {
		ret = fs->inp->read(fs->inp, (unsigned char *) fs->buf,
					 fs->chunksize);
		if (ret > 0)
			fs->in_counter += ret;
		else if (ret == 0)
	break; /* EOF */
		else {
			/* >>> read error */;
	break;
		}
		ret = write(fs->outlet[1], fs->buf, ret);
		if (ret == -1) {
			/* >>> write error */;
	break;
		}
	}

	/* >>> check and destroy ring buffer */;
	free(fs->buf);
	fs->buf = NULL;

	if (fs->outlet[1] >= 0)
		close(fs->outlet[1]);
	fs->outlet[1] = -1;
	return (ret >= 0);
}


