/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set noet ts=8 sts=8 sw=8 : */

/**
 * Create an ISO-9660 data volume with Rock Ridge and Joliet extensions.
 * Usage is easy:
 *  - Create a new volume.
 *  - Add files and directories.
 *  - Write the volume to a file or create a burn source for use with Libburn.
 */

#ifndef LIBISO_LIBISOFS_H
#define LIBISO_LIBISOFS_H

/* #include <libburn.h> */
struct burn_source;

/**
 * Data volume.
 * @see volume.h for details.
 */
struct iso_volume;

/**
 * A set of data volumes.
 * @see volume.h for details.
 */
struct iso_volset;

/**
 * A node in the filesystem tree.
 * \see tree.h
 */
struct iso_tree_node;

enum ecma119_extension_flag {
	ECMA119_ROCKRIDGE	= (1<<0),
	ECMA119_JOLIET		= (1<<1)
};

/**
 * Create a new volume.
 * The parameters can be set to NULL if you wish to set them later.
 */
struct iso_volume *iso_volume_new(const char *volume_id,
				  const char *publisher_id,
				  const char *data_preparer_id);

struct iso_volume *iso_volume_new_with_root(const char *volume_id,
					    const char *publisher_id,
					    const char *data_preparer_id,
					    struct iso_tree_node *root);

/**
 * Free a volume.
 */
void iso_volume_free(struct iso_volume *volume);

/**
 * Free a set of data volumes.
 */
void iso_volset_free(struct iso_volset *volume);

/**
 * Get the root directory for a volume.
 */
struct iso_tree_node *iso_volume_get_root(const struct iso_volume *volume);

/**
 * Fill in the volume identifier for a volume.
 */
void iso_volume_set_volume_id(struct iso_volume *volume,
			      const char *volume_id);

/**
 * Fill in the publisher for a volume.
 */
void iso_volume_set_publisher_id(struct iso_volume *volume,
				 const char *publisher_id);

/**
 * Fill in the data preparer for a volume.
 */
void iso_volume_set_data_preparer_id(struct iso_volume *volume,
				     const char *data_preparer_id);

/**
 * Locate a node by its path on disc.
 * 
 * \param volume The volume to search in.
 * \param path The path, in the image, of the file.
 *
 * \return The node found or NULL.
 *
 */
struct iso_tree_node *iso_tree_volume_path_to_node(struct iso_volume *volume, const char *path);

/**
 * Add a file or a directory (recursively) to a volume by specifying its path on the volume.
 *
 * \param volume The volume to add the file to.
 * \param disc_path The path on the disc at which to add the disc.
 * \param path The path, on the local filesystem, of the file.
 *
 * \return The node for the file or NULL if the parent doesn't exists on the disc.
 */
struct iso_tree_node *iso_tree_volume_add_path(struct iso_volume *volume,
				 					const char *disc_path,
									const char *path);

/**
 * Creates a new, empty directory on the volume.
 *
 * \param volume The volume to add the directory to.
 * \param disc_path The path on the volume at which to add the directory.
 *
 * \return A pointer to the newly created directory.
 */
struct iso_tree_node *iso_tree_volume_add_new_dir(struct iso_volume *volume,
				 					const char *disc_path);

/**
 * Create a new Volume Set consisting of only one volume.
 * @param volume The first and only volume for the volset to contain.
 * @param volset_id The Volume Set ID.
 * @return A new iso_volset.
 */
struct iso_volset *iso_volset_new(struct iso_volume *volume,
                                  const char *volset_id);

/**
 * Add a file to a directory.
 *
 * \param path The path, on the local filesystem, of the file.
 *
 * \pre \p parent is NULL or is a directory.
 * \pre \p path is non-NULL and is a valid path to a non-directory on the local
 *	filesystem.
 * \return An iso_tree_node whose path is \p path and whose parent is \p parent.
 */
struct iso_tree_node *iso_tree_add_node(struct iso_tree_node *parent,
					const char *path);

/**
 * Recursively add an existing directory to the tree.
 * Warning: when using this, you'll lose pointers to files or subdirectories.
 * If you want to have pointers to all files and directories,
 * use iso_tree_add_file and iso_tree_add_dir.
 *
 * \param path The path, on the local filesystem, of the directory to add.
 *
 * \pre \p parent is NULL or is a directory.
 * \pre \p path is non-NULL and is a valid path to a directory on the local
 *	filesystem.
 * \return a pointer to the newly created directory.
 */
struct iso_tree_node *iso_tree_radd_dir(struct iso_tree_node *parent,
					const char *path);


/**
 * Add the path of a file or directory to ignore when adding a directory recursively.
 *
 * \param path The path, on the local filesystem, of the file.
 */
void iso_exclude_add_path(const char *path);

/**
 * Remove a path that was set to be ignored when adding a directory recusively.
 * 
 * \param path The path, on the local filesystem, of the file.
 */
void iso_exclude_remove_path(const char *path);

/**
 * Remove all paths that were set to be ignored when adding a directory recusively.
 */
void iso_exclude_empty(void);

/**
 * Creates a new, empty directory on the volume.
 *
 * \pre \p parent is NULL or is a directory.
 * \pre \p name is unique among the children and files belonging to \p parent.
 *	Also, it doesn't contain '/' characters.
 *
 * \post \p parent contains a child directory whose name is \p name and whose
 *	POSIX attributes are the same as \p parent's.
 * \return a pointer to the newly created directory.
 */
struct iso_tree_node *iso_tree_add_new_dir(struct iso_tree_node *parent,
					   const char *name);

/**
 * Set the name of a file (using the current locale).
 */
void iso_tree_node_set_name(struct iso_tree_node *file, const char *name);

/**
 * Recursively print a directory to stdout.
 * \param spaces The initial number of spaces on the left. Set to 0 if you
 *	supply a root directory.
 */
void iso_tree_print(const struct iso_tree_node *root, int spaces);

/** Create a burn_source which can be used as a data source for a track
 *
 * The volume set used to create the libburn_source can _not_ be modified
 * until the libburn_source is freed.
 *
 * \param volumeset The volume set from which you want to write
 * \param volnum The volume in the set which you want to write (usually 0)
 * \param level ISO level to write at.
 * \param flags Which extensions to support.
 *
 * \pre \p volumeset is non-NULL
 * \pre \p volnum is less than \p volset->volset_size.
 * \return A burn_source to be used for the data source for a track
 */
struct burn_source* iso_source_new_ecma119 (struct iso_volset *volumeset,
					    int volnum,
					    int level,
					    int flags);

#endif /* LIBISO_LIBISOFS_H */
