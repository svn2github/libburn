/* vim: set noet ts=8 sts=8 sw=8 : */

/** Functions and structures used for SUSP (IEEE 1281).
 */

#ifndef __ISO_SUSP
#define __ISO_SUSP

#include <stdint.h>

/* SUSP is only present in standard ecma119 */
struct ecma119_write_target;
struct ecma119_tree_node;

/** This contains the information that needs to go in the SUSP area of a file.
 */
struct susp_info
{
	int n_susp_fields;		/**< Number of SUSP fields */
	uint8_t **susp_fields;		/**< Data for each SUSP field */

	/* the next 3 relate to CE and are filled out by susp_add_CE. */
	int n_fields_fit;	/**< How many of the above SUSP fields fit
				  *  within this node's dirent. */
	int non_CE_len;		/**< Length of the part of the SUSP area that
				  *  fits in the dirent. */
	int CE_len;		/**< Length of the part of the SUSP area that
				  *  will go in a CE area. */
};

void susp_add_CE(struct ecma119_write_target *, struct ecma119_tree_node *);

/* these next 2 are special because they don't modify the susp fields of the
 * directory; they modify the susp fields of the
 * "." entry in the directory. */
void susp_add_SP(struct ecma119_write_target *, struct ecma119_tree_node *);
void rrip_add_ER(struct ecma119_write_target *, struct ecma119_tree_node *);

/** Once all the directories and files are laid out, recurse through the tree
 *  and finalize all SUSP CE entries. */
void susp_finalize(struct ecma119_write_target *, struct ecma119_tree_node *);

void susp_append(struct ecma119_write_target *,
		 struct susp_info *,
		 uint8_t *);
void susp_insert(struct ecma119_write_target *,
		 struct susp_info *,
		 uint8_t *,
		 int pos);
uint8_t *susp_find(struct susp_info *,
		   const char *);

void susp_write(struct ecma119_write_target *,
		struct susp_info *,
		uint8_t *);
void susp_write_CE(struct ecma119_write_target *,
		   struct susp_info *,
		   uint8_t *);

void susp_free_fields(struct susp_info *);

#endif /* __ISO_SUSP */
