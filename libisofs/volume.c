/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set ts=8 sts=8 sw=8 noet : */

#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "libisofs.h"
#include "tree.h"
#include "util.h"
#include "volume.h"

struct iso_volset*
iso_volset_new(struct iso_volume *vol, const char *id)
{
	struct iso_volset *volset = calloc(1, sizeof(struct iso_volset));

	volset->volset_size = 1;
	volset->refcount = 1;
	volset->volume = malloc(sizeof(void *));
	volset->volume[0] = vol;
	volset->volset_id = strdup(id);

	vol->refcount++;
	return volset;
}

void
iso_volset_free(struct iso_volset *volset)
{
	if (--volset->refcount < 1) {
		int i;
		for (i = 0; i < volset->volset_size; i++) {
			iso_volume_free(volset->volume[i]);
		}
		free(volset->volume);
		free(volset->volset_id);
	}
}

struct iso_volume*
iso_volume_new(const char *volume_id,
	       const char *publisher_id,
	       const char *data_preparer_id)
{
	return iso_volume_new_with_root(volume_id,
					publisher_id,
					data_preparer_id,
					NULL);
}

struct iso_volume*
iso_volume_new_with_root(const char *volume_id,
			 const char *publisher_id,
			 const char *data_preparer_id,
			 struct iso_tree_node *root)
{
	struct iso_volume *volume;

	volume = calloc(1, sizeof(struct iso_volume));
	volume->refcount = 1;

	volume->root = root ? root : iso_tree_new_root(volume);

	if (volume_id != NULL)
		volume->volume_id = strdup(volume_id);
	if (publisher_id != NULL)
		volume->publisher_id = strdup(publisher_id);
	if (data_preparer_id != NULL)
		volume->data_preparer_id = strdup(data_preparer_id);
	return volume;
}

void
iso_volume_free(struct iso_volume *volume)
{
	/* Only free if no references are in use. */
	if (--volume->refcount < 1) {
		iso_tree_free(volume->root);

		free(volume->volume_id);
		free(volume->publisher_id);
		free(volume->data_preparer_id);

		free(volume);
	}
}

struct iso_tree_node *
iso_volume_get_root(const struct iso_volume *volume)
{
	return volume->root;
}

struct iso_tree_node *
iso_tree_volume_path_to_node(struct iso_volume *volume, const char *path)
{
	struct iso_tree_node *node;
	char *ptr, *brk_info, *component;

	/* get the first child at the root of the volume
	 * that is "/" */
	node=iso_volume_get_root(volume);
	if (!strcmp (path, "/"))
		return node;

	if (!node->nchildren)
		return NULL;

	/* the name of the nodes is in wide characters so first convert path
	 * into wide characters. */
	ptr = strdup(path);

	/* get the first component of the path */
	component=strtok_r(ptr, "/", &brk_info);
	while (component) {
		size_t max;
		size_t i;

		/* search among all the children of this directory if this path component exists */
		max=node->nchildren;
		for (i=0; i < max; i++) {
			if (!strcmp(component, node->children[i]->name)) {
				node=node->children[i];
				break;
			}
		}

		/* see if a node could be found */
		if (i==max) {
			node=NULL;
			break;
		}

		component=strtok_r(NULL, "/", &brk_info);
	}

	free(ptr);
	return node;	
}

struct iso_tree_node *
iso_tree_volume_add_path(struct iso_volume *volume,
					 const char *disc_path,
					 const char *path)
{
	char *tmp;
	struct iso_tree_node *node;
	struct iso_tree_node *parent_node;

	tmp=strdup(disc_path);
	parent_node = iso_tree_volume_path_to_node(volume, dirname(tmp));
	free(tmp);

	if (!parent_node)
		return NULL;

	node = iso_tree_radd_dir(parent_node, path);
	if (!node)
		return NULL;

	tmp=strdup(disc_path);
	iso_tree_node_set_name(node, basename(tmp));
	free(tmp);

	return node;
}

struct iso_tree_node *
iso_tree_volume_add_new_dir(struct iso_volume *volume,
	 				      const char *disc_path)
{
	char *tmp;
	struct iso_tree_node *node;
	struct iso_tree_node *parent_node;

	tmp=strdup(disc_path);
	parent_node = iso_tree_volume_path_to_node(volume, dirname(tmp));
	free(tmp);

	if (!parent_node)
		return NULL;

	tmp=strdup(disc_path);
	node = iso_tree_add_new_dir(parent_node, basename(tmp));
	free(tmp);

	return node;
}
