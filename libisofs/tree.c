/* vim: set noet ts=8 sts=8 sw=8 : */

/**
 * \file tree.c
 *
 * Implement filesystem trees.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <stdio.h>

#include "tree.h"
#include "util.h"
#include "volume.h"
#include "exclude.h"

static void
set_default_stat(struct stat *s)
{
	time_t now = time(NULL);

	memset(s, 0, sizeof(struct stat));
	s->st_mode = 0777 | S_IFREG;
	s->st_atime = s->st_mtime = s->st_ctime = now;
}

static struct stat
get_attrib(const struct iso_tree_node *node)
{
	struct stat st;

	if (node) {
		return node->attrib;
	}
	set_default_stat(&st);
	return st;
}

static void
append_node(struct iso_tree_node *parent,
	    struct iso_tree_node *child)
{
	assert((!parent || S_ISDIR(parent->attrib.st_mode)) && child);
	if (!parent)
		return;

	parent->nchildren++;
	parent->children =
		realloc(parent->children, parent->nchildren * sizeof(void*));
	parent->children[parent->nchildren-1] = child;
}

struct iso_tree_node*
iso_tree_new_root(struct iso_volume *vol)
{
	assert(vol);

	if (vol->root) {
		iso_tree_free(vol->root);
	}

	vol->root = calloc(1, sizeof(struct iso_tree_node));
	vol->root->volume = vol;
	set_default_stat(&vol->root->attrib);
	vol->root->attrib.st_mode = S_IFDIR | 0777;
	vol->root->loc.type = LIBISO_NONE;
	return vol->root;
}

struct iso_tree_node*
iso_tree_add_new_file(struct iso_tree_node *parent, const char *name)
{
	struct iso_tree_node *f = calloc(1, sizeof(struct iso_tree_node));

	assert((!parent || S_ISDIR(parent->attrib.st_mode)) && name);

	f->volume = parent ? parent->volume : NULL;
	f->parent = parent;
	f->name = parent ? strdup(name) : NULL;
	f->attrib = get_attrib(parent);
	f->attrib.st_mode = 0777 | S_IFREG;
	f->loc.type = LIBISO_NONE;
	append_node(parent, f);
	return f;
}

struct iso_tree_node*
iso_tree_add_new_dir(struct iso_tree_node *parent, const char *name)
{
	struct iso_tree_node *d = iso_tree_add_new_file(parent, name);

	assert((!parent || S_ISDIR(parent->attrib.st_mode)) && name);

	d->attrib.st_mode = (d->attrib.st_mode & ~S_IFMT) | S_IFDIR;
	return d;
}

struct iso_tree_node*
iso_tree_add_node(struct iso_tree_node *parent, const char *path)
{
	char *p;
	struct stat st;
	struct iso_tree_node *ret;

	assert((!parent || S_ISDIR(parent->attrib.st_mode)) && path);

	if (lstat(path, &st) == -1)
		return NULL;

	p = strdup(path); /* because basename() might modify its arg */

	/* it doesn't matter if we add a file or directory since we modify
	 * attrib anyway. */
	ret = iso_tree_add_new_file(parent, basename(p));
	ret->attrib = st;
	ret->loc.type = LIBISO_FILESYS;
	ret->loc.path = strdup(path);
	free(p);

	return ret;
}

struct iso_tree_node*
iso_tree_radd_dir (struct iso_tree_node *parent, const char *path)
{
	struct iso_tree_node *new;
	DIR *dir;
	struct dirent *ent;

	assert((!parent || S_ISDIR(parent->attrib.st_mode)) && path);

	new = iso_tree_add_node(parent, path);
	if (!new || !S_ISDIR(new->attrib.st_mode)) {
		return new;
	}

	dir = opendir(path);
	if (!dir) {
		warn("couldn't open directory %s: %s\n", path, strerror(errno));
		return new;
	}

	while ((ent = readdir(dir))) {
		char child[strlen(ent->d_name) + strlen(path) + 2];

		if (strcmp(ent->d_name, ".") == 0 ||
				strcmp(ent->d_name, "..") == 0)
			continue;

		sprintf(child, "%s/%s", path, ent->d_name);

		/* see if this child is excluded. */
		if (iso_exclude_lookup(child))
			continue;

		iso_tree_radd_dir(new, child);
	}
	closedir(dir);

	return new;
}

void
iso_tree_free(struct iso_tree_node *root)
{
	size_t i;

	for (i=0; i < root->nchildren; i++) {
		iso_tree_free(root->children[i]);
	}
	free(root->name);
	free(root->children);
	free(root);
}

void
iso_tree_print(const struct iso_tree_node *root, int spaces)
{
	size_t i;
	char sp[spaces+1];

	memset(sp, ' ', spaces);
	sp[spaces] = '\0';

	printf("%s%sn", sp, root->name);
	for (i=0; i < root->nchildren; i++) {
		iso_tree_print(root->children[i], spaces+2);
	}
}

void
iso_tree_print_verbose(const struct iso_tree_node *root,
		       print_dir_callback dir,
		       print_file_callback file,
		       void *callback_data,
		       int spaces)
{
	size_t i;

	(S_ISDIR(root->attrib.st_mode) ? dir : file)
		(root, callback_data, spaces);
	for (i=0; i < root->nchildren; i++) {
		iso_tree_print_verbose(root->children[i], dir,
				file, callback_data, spaces+2);
	}
}

void
iso_tree_node_set_name(struct iso_tree_node *file, const char *name)
{
	free(file->name);
	file->name = strdup(name);
}
