/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set noet sts=8 ts=8 sw=8 : */

/**
 * Extra declarations for use with the iso_volume structure.
 */

#ifndef LIBISO_VOLUME_H
#define LIBISO_VOLUME_H

#include "libisofs.h"

/**
 * Data volume.
 */
struct iso_volume
{
	int refcount;			/**< Number of used references to this
					     volume. */

	struct iso_tree_node *root;	/**< Root of the directory tree for the
					     volume. */

	char *volume_id;		/**< Volume identifier. */
	char *publisher_id;		/**< Volume publisher. */
	char *data_preparer_id;	/**< Volume data preparer. */
};

/**
 * A set of data volumes.
 */
struct iso_volset
{
	int refcount;

	struct iso_volume **volume;	/**< The volumes belonging to this
					     volume set. */
	int volset_size;		/**< The number of volumes in this
					     volume set. */

	char *volset_id;		/**< The id of this volume set, encoded
					     in the current locale. */
};

#endif /* __ISO_VOLUME */
