/* vim: set noet ts=8 sts=8 sw=8 : */

/**
 * \file tree.h
 *
 * Declare the structure of a libisofs filesystem tree. The files in this
 * tree can come from either the local filesystem or from another .iso image
 * (for multisession).
 *
 * This tree preserves as much information as it can about the files; names
 * are stored in wchar_t and we preserve POSIX attributes. This tree does
 * *not* include information that is necessary for writing out, for example,
 * an ISO level 1 tree. That information will go in a different tree because
 * the structure is sufficiently different.
 */

#ifndef LIBISO_TREE_H
#define LIBISO_TREE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <wchar.h>

#include "libisofs.h"

enum file_location {
	LIBISO_FILESYS,
	LIBISO_PREVSESSION,
	LIBISO_NONE		/**< for files/dirs that were added with
				  * iso_tree_add_new_XXX. */
};

/**
 * This tells us where to read the data from a file. Either we read from the
 * local filesystem or we just point to the block on a previous session.
 */
struct iso_file_location
{
	enum file_location type;
  /*	union {*/
		char *path;	/* in the current locale */
		uint32_t block;
  /*	};*/
};

/**
 * A node in the filesystem tree.
 */
struct iso_tree_node
{
	struct iso_volume *volume;
	struct iso_tree_node *parent;
	char *name;
	struct stat attrib;	/**< The POSIX attributes of this node as
				  * documented in "man 2 stat". */
	struct iso_file_location loc;
				/**< Only used for regular files and symbolic
				 * links (ie. files for which we might have to
				 * copy data). */

	size_t nchildren;	/**< The number of children of this
				  * directory (if this is a directory). */
	struct iso_tree_node **children;

	size_t block;		/**< The block at which this file will
				  * reside on disk. We store this here as
				  * well as in the various mangled trees
				  * because many different trees might point
				  * to the same file and they need to share the
				  * block location. */
};

/**
 * Create a new root directory for a volume.
 *
 * \param vol The volume for which to create a new root directory.
 *
 * \pre \p vol is non-NULL.
 * \post \p vol has a non-NULL, empty root directory with permissions 777.
 * \return \p vol's new non-NULL, empty root directory.
 */
struct iso_tree_node *iso_tree_new_root(struct iso_volume *vol);

/**
 * Create a new, empty, file.
 *
 * \param parent The parent directory of the new file. If this is null, create
 *        and return a new file node without adding it to any tree.
 * \param name The name of the new file, encoded in the current locale.
 * \pre \p name is non-NULL and it does not match any other file or directory
 *      name in \p parent.
 * \post \p parent (if non-NULL) contains a file with the following properties:
 *      - the file's name is \p name (converted to wchar_t)
 *      - the file's POSIX permissions are the same as \p parent's
 *      - the file is a regular file
 *      - the file is empty
 *
 * \return \p parent's newly created file.
 */
struct iso_tree_node *iso_tree_add_new_file(struct iso_tree_node *parent,
					    const char *name);

/**
 * Recursively free a directory.
 *
 * \param root The root of the directory heirarchy to free.
 *
 * \pre \p root is non-NULL.
 */
void iso_tree_free(struct iso_tree_node *root);

/**
 * A function that prints verbose information about a directory.
 *
 * \param dir The directory about which to print information.
 * \param data Unspecified function-dependent data.
 * \param spaces The number of spaces to prepend to the output.
 *
 * \see iso_tree_print_verbose
 */
typedef void (*print_dir_callback) (const struct iso_tree_node *dir,
				    void *data,
				    int spaces);
/**
 * A function that prints verbose information about a file.
 *
 * \param dir The file about which to print information.
 * \param data Unspecified function-dependent data.
 * \param spaces The number of spaces to prepend to the output.
 *
 * \see iso_tree_print_verbose
 */
typedef void (*print_file_callback) (const struct iso_tree_node *file,
				     void *data,
				     int spaces);

/**
 * Recursively print a directory heirarchy. For each node in the directory
 * heirarchy, call a callback function to print information more verbosely.
 *
 * \param root The root of the directory heirarchy to print.
 * \param dir The callback function to call for each directory in the tree.
 * \param file The callback function to call for each file in the tree.
 * \param callback_data The data to pass to the callback functions.
 * \param spaces The number of spaces to prepend to the output.
 *
 * \pre \p root is not NULL.
 * \pre Neither of the callback functions modifies the directory heirarchy.
 */
void iso_tree_print_verbose(const struct iso_tree_node *root,
			    print_dir_callback dir,
			    print_file_callback file,
			    void *callback_data,
			    int spaces);

#define ISO_ISDIR(n) S_ISDIR(n->attrib.st_mode)

#endif /* LIBISO_TREE_H */
