/* vim: set noet ts=8 sts=8 sw=8 : */

#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <assert.h>

#include "ecma119.h"
#include "ecma119_tree.h"
#include "tree.h"
#include "util.h"

static size_t calc_dirent_len(struct ecma119_tree_node *n)
{
	int ret = n->name ? strlen(n->name) + 33 : 34;
	if (ret % 2) ret++;
	return ret;
}

static struct ecma119_tree_node*
create_dir(struct ecma119_write_target *t,
	   struct ecma119_tree_node *parent,
	   struct iso_tree_node *iso)
{
	struct ecma119_tree_node *ret;

	assert(t && (!parent || parent->type == ECMA119_DIR)
			&& iso && S_ISDIR(iso->attrib.st_mode));

	ret = calloc(1, sizeof(struct ecma119_tree_node));
	ret->name = iso->name ? ((t->iso_level == 1) ? iso_1_dirid(iso->name)
						     : iso_2_dirid(iso->name))
			      : NULL;
	ret->dirent_len = calc_dirent_len(ret);
	ret->iso_self = iso;
	ret->target = t;
	ret->type = ECMA119_DIR;
	ret->parent = ret->dir.real_parent = parent;
	ret->dir.depth = parent ? parent->dir.depth + 1 : 1;
	ret->dir.nchildren = iso->nchildren;
	ret->dir.children = calloc(1, sizeof(void*) * iso->nchildren);
	return ret;
}

static struct ecma119_tree_node*
create_file(struct ecma119_write_target *t,
	    struct ecma119_tree_node *parent,
	    struct iso_tree_node *iso)
{
	struct ecma119_tree_node *ret;

	assert(t && iso && parent && parent->type == ECMA119_DIR);

	ret = calloc(1, sizeof(struct ecma119_tree_node));
	ret->name = iso->name ? ((t->iso_level == 1) ? iso_1_fileid(iso->name)
						     : iso_2_fileid(iso->name))
			      : NULL;
	ret->dirent_len = calc_dirent_len(ret);
	ret->parent = parent;
	ret->iso_self = iso;
	ret->target = t;
	ret->type = ECMA119_FILE;
	return ret;
}

static struct ecma119_tree_node*
create_tree(struct ecma119_write_target *t,
	    struct ecma119_tree_node *parent,
	    struct iso_tree_node *iso)
{
	struct ecma119_tree_node *ret;
	size_t i;

	assert(t && iso);

	ret = (S_ISDIR(iso->attrib.st_mode) ? create_dir : create_file)
			(t, parent, iso);
	for (i = 0; i < iso->nchildren; i++) {
		ret->dir.children[i] = create_tree(t, ret, iso->children[i]);
	}
	return ret;
}

void
ecma119_tree_free(struct ecma119_tree_node *root)
{
	size_t i;

	if (root->type == ECMA119_DIR) {
		for (i=0; i < root->dir.nchildren; i++) {
			ecma119_tree_free(root->dir.children[i]);
		}
		free(root->dir.children);
	}
	free(root->name);
	free(root);
}

static size_t
max_child_name_len(struct ecma119_tree_node *root)
{
	size_t ret = 0, i;

	assert(root->type == ECMA119_DIR);
	for (i=0; i < root->dir.nchildren; i++) {
		size_t len = strlen(root->dir.children[i]->name);
		ret = MAX(ret, len);
	}
	return ret;
}

static void
reparent(struct ecma119_tree_node *child,
	 struct ecma119_tree_node *parent)
{
	int found = 0;
	size_t i;
	struct ecma119_tree_node *placeholder;

	assert(child && parent && parent->type == ECMA119_DIR && child->parent);

	/* replace the child in the original parent with a placeholder */
	for (i=0; i < child->parent->dir.nchildren; i++) {
		if (child->parent->dir.children[i] == child) {
			placeholder = create_file(child->target,
						child->parent,
						child->iso_self);
			placeholder->file.real_me = child;
			child->parent->dir.children[i] = placeholder;
			found = 1;
			break;
		}
	}
	assert(found);

	/* add the child to its new parent */
	child->parent = parent;
	parent->dir.nchildren++;
	parent->dir.children = realloc( parent->dir.children,
				sizeof(void*) * parent->dir.nchildren );
	parent->dir.children[parent->dir.nchildren-1] = child;
}

/**
 * Reorder the tree, if necessary, to ensure that
 *  - the depth is at most 8
 *  - each path length is at most 255 characters
 */
static void
reorder_tree(struct ecma119_tree_node *root,
	     struct ecma119_tree_node *cur)
{
	size_t max_path;

	assert(root && cur && cur->type == ECMA119_DIR);

	cur->dir.depth = cur->parent ? cur->parent->dir.depth + 1 : 1;
	cur->dir.path_len = cur->parent ? cur->parent->dir.path_len
					+ strlen(cur->name) : 0;
	max_path = cur->dir.path_len + cur->dir.depth + max_child_name_len(cur);

	if (cur->dir.depth > 8 || max_path > 255) {
		reparent(cur, root);
		/* we are appended to the root's children now, so there is no
		 * need to recurse (the root will hit us again) */
	} else {
		size_t i;

		for (i=0; i < cur->dir.nchildren; i++) {
			if (cur->dir.children[i]->type == ECMA119_DIR)
				reorder_tree(root, cur->dir.children[i]);
		}
	}
}

static int
cmp_node(const void *f1, const void *f2)
{
	struct ecma119_tree_node *f = *((struct ecma119_tree_node**)f1);
	struct ecma119_tree_node *g = *((struct ecma119_tree_node**)f2);
	return strcmp(f->name, g->name);
}

static void
sort_tree(struct ecma119_tree_node *root)
{
	size_t i;

	assert(root && root->type == ECMA119_DIR);

	qsort(root->dir.children, root->dir.nchildren, sizeof(void*), cmp_node);
	for (i=0; i < root->dir.nchildren; i++) {
		if (root->dir.children[i]->type == ECMA119_DIR)
			sort_tree(root->dir.children[i]);
	}
}

/**
 * Change num_change characters of the given filename in order to ensure the
 * name is unique. If the name is short enough (depending on the ISO level),
 * we can append the characters instead of changing them.
 *
 * \p seq_num is the index of this file in the sequence of identical filenames.
 *
 * For example, seq_num=3, num_change=2, name="HELLOTHERE.TXT" changes name to
 * "HELLOTHE03.TXT"
 */
static void
mangle_name(char **name, int num_change, int level, int seq_num)
{
	char *dot = strrchr(*name, '.');
	char *semi = strrchr(*name, ';');
	size_t len = strlen(*name);
	char base[len+1], ext[len+1];
	char fmt[12];
	size_t baselen, extlen;

	if (num_change >= len) {
		return;
	}
	strncpy(base, *name, len+1);
	if (dot) {
		base[dot - *name] = '\0';
		strncpy(ext, dot+1, len+1);
		if (semi) {
			ext[semi - dot - 1] = '\0';
		}
	} else {
		base[len-2] = '\0';
		ext[0] = '\0';
	}
	baselen = strlen(base);
	extlen = strlen(ext);
	if (level == 1 && baselen + num_change > 8) {
		base[8 - num_change] = '\0';
	} else if (level != 1 && baselen + extlen + num_change > 30) {
		base[30 - extlen - num_change] = '\0';
	}

	sprintf(fmt, "%%s%%0%1dd.%%s;1", num_change);
	*name = realloc(*name, baselen + extlen + num_change + 4);
	sprintf(*name, fmt, base, seq_num, ext);
}

static void
mangle_all(struct ecma119_tree_node *dir)
{
	size_t i, j, k;
	struct ecma119_dir_info d = dir->dir;
	size_t n_change;
	int changed;

	assert(dir->type == ECMA119_DIR);
	do {
		changed = 0;
		for (i=0; i < d.nchildren; i++) {
			/* find the number of consecutive equal names */
			j = 1;
			while ( i+j < d.nchildren &&
					!strcmp(d.children[i]->name,
						d.children[i+j]->name) )
				j++;
			if (j == 1) continue;

			/* mangle the names */
			changed = 1;
			n_change = j / 10 + 1;
			for (k=0; k < j; k++) {
				mangle_name(&(d.children[i+k]->name),
						n_change,
						dir->target->iso_level,
						k);
				d.children[i+k]->dirent_len =
					calc_dirent_len(d.children[i+k]);
			}

			/* skip ahead by the number of mangled names */
			i += j - 1;
		}
	} while (changed);

	for (i=0; i < d.nchildren; i++) {
		if (d.children[i]->type == ECMA119_DIR)
			mangle_all(d.children[i]);
	}
}

struct ecma119_tree_node*
ecma119_tree_create(struct ecma119_write_target *t,
		    struct iso_tree_node *iso_root)
{
	t->root = create_tree(t, NULL, iso_root);
	reorder_tree(t->root, t->root);
	sort_tree(t->root);
	mangle_all(t->root);
	return t->root;
}

void
ecma119_tree_print(struct ecma119_tree_node *root, int spaces)
{
	size_t i;
	char sp[spaces+1];

	memset(sp, ' ', spaces);
	sp[spaces] = '\0';

	printf("%s%s\n", sp, root->name);
	if (root->type == ECMA119_DIR)
		for (i=0; i < root->dir.nchildren; i++)
			ecma119_tree_print(root->dir.children[i], spaces+2);
}
