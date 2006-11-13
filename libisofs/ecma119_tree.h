/* vim: set noet ts=8 sts=8 sw=8 : */

/**
 * \file ecma119_tree.h
 *
 * Declarations for creating, modifying and printing filesystem trees that
 * are compatible with ecma119.
 */

#ifndef LIBISO_ECMA119_TREE_H
#define LIBISO_ECMA119_TREE_H

struct ecma119_write_target;

enum {
	ECMA119_FILE,
	ECMA119_DIR
};

struct ecma119_dir_info {
	struct susp_info self_susp;	/**< susp entries for "." */
	struct susp_info parent_susp;	/**< susp entries for ".." */

	size_t len;			/**< sum of the lengths of children's
					  * Directory Records (including SU) */
	size_t CE_len;			/**< sum of the lengths of children's
					  * SUSP CE areas */

	int depth;
	size_t path_len;		/**< The length of a path up to, and
					  * including, this directory. This
					  * cannot exceed 255. */
	size_t nchildren;
	struct ecma119_tree_node **children;

	struct ecma119_tree_node *real_parent;
					/**< The parent before relocation */
};

struct ecma119_file_info
{
	struct ecma119_tree_node *real_me;
					/**< If this is non-NULL, the file is
					  *  a placeholder for a relocated
					  *  directory and this field points to
					  *  that relocated directory.
					  */
};

/**
 * A node for a tree containing all the information necessary for writing
 * an ISO9660 volume.
 */
struct ecma119_tree_node
{
	char *name;			/**< in ASCII, conforming to the
					  * current ISO level. */
	size_t dirent_len;		/**< Length of the directory record,
					  * not including SU. */
	size_t block;

	struct ecma119_tree_node *parent;
	struct iso_tree_node *iso_self;
	struct ecma119_write_target *target;

	struct susp_info susp;

	int type;			/**< file or directory */
  /*	union {*/
		struct ecma119_dir_info dir;
		struct ecma119_file_info file;
  /*	};*/
};

/**
 * Create a new ecma119_tree that corresponds to the tree represented by
 * \p iso_root.
 */
struct ecma119_tree_node*
ecma119_tree_create(struct ecma119_write_target *target,
		    struct iso_tree_node *iso_root);

/**
 * Free an ecma119 tree.
 */
void
ecma119_tree_free(struct ecma119_tree_node *root);

/**
 * Print an ecma119 tree.
 */
void
ecma119_tree_print(struct ecma119_tree_node *root, int spaces);

#endif /* LIBISO_ECMA119_TREE_H */
