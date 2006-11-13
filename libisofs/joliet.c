/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set noet ts=8 sts=8 sw=8 : */

#include "joliet.h"
#include "ecma119.h"
#include "ecma119_tree.h"
#include "tree.h"
#include "util.h"
#include "volume.h"

#include <assert.h>
#include <string.h>

static struct joliet_tree_node*
create_node(struct ecma119_write_target *t,
	    struct joliet_tree_node *parent,
	    struct iso_tree_node *iso)
{
	struct joliet_tree_node *ret =
		calloc(1, sizeof(struct joliet_tree_node));

	ret->name = iso_j_id(iso->name);
	ret->dirent_len = 34 + (ret->name ? ucslen(ret->name) * 2 : 0);
	ret->len = iso->attrib.st_size; /* for dirs, we'll change this */
	ret->block = iso->block; /* only actually for files, not dirs */
	ret->parent = parent;
	ret->iso_self = iso;
	ret->target = t;
	ret->nchildren = iso->nchildren;
	if (ret->nchildren)
		ret->children = calloc(sizeof(void*), ret->nchildren);
	return ret;
}

static struct joliet_tree_node*
create_tree(struct ecma119_write_target *t,
	    struct joliet_tree_node *parent,
	    struct iso_tree_node *iso_root)
{
	struct joliet_tree_node *root = create_node(t, parent, iso_root);
	size_t i;

	for (i = 0; i < root->nchildren; i++) {
		struct iso_tree_node *iso_ch = iso_root->children[i];
		if (ISO_ISDIR(iso_ch))
			root->children[i] = create_tree(t, root, iso_ch);
		else
			root->children[i] = create_node(t, root, iso_ch);
	}
	return root;
}

static int
cmp_node(const void *f1, const void *f2)
{
	struct joliet_tree_node *f = *((struct joliet_tree_node**)f1);
	struct joliet_tree_node *g = *((struct joliet_tree_node**)f2);
	return ucscmp(f->name, g->name);
}

static void
sort_tree(struct joliet_tree_node *root)
{
	size_t i;

	assert(root && ISO_ISDIR(root->iso_self));

	qsort(root->children, root->nchildren, sizeof(void*), cmp_node);
	for (i = 0; i < root->nchildren; i++)
		if (ISO_ISDIR(root->children[i]->iso_self))
			sort_tree(root->children[i]);
}

void
joliet_prepare_path_tables(struct ecma119_write_target *t)
{
	size_t cur, i, j;

	t->pathlist_joliet[0] = t->joliet_root;
	t->path_table_size_joliet = 10; /* root directory record */
	cur = 1;

	for (i = 0; i < t->dirlist_len; i++) {
		struct joliet_tree_node *dir = t->pathlist_joliet[i];
		for (j = 0; j < dir->nchildren; j++) {
			struct joliet_tree_node *ch = dir->children[j];
			if (ISO_ISDIR(ch->iso_self)) {
				size_t len = 8 + ucslen(ch->name)*2;
				t->pathlist_joliet[cur++] = ch;
				t->path_table_size_joliet += len;
			}
		}
	}
}

/**
 * Calculate the size of each directory.
 */
void
joliet_calc_dir_size(struct ecma119_write_target *t,
		     struct joliet_tree_node *root)
{
	size_t i;
	struct joliet_tree_node *ch;

	assert(root && ISO_ISDIR(root->iso_self));

	root->len = 68; /* for "." and ".." entries */
	for (i = 0; i < root->nchildren; i++) {
		ch = root->children[i];
		root->len += ch->dirent_len;
		if (ISO_ISDIR(ch->iso_self))
			joliet_calc_dir_size(t, ch);
	}
	t->total_dir_size_joliet += round_up (root->len, t->block_size);
}

/**
 * Calculate the position of each directory. Also fill out t->dirlist_joliet.
 */
void
joliet_calc_dir_pos(struct ecma119_write_target *t,
		    struct joliet_tree_node *root)
{
	size_t i;
	struct joliet_tree_node *ch;

	assert(root && ISO_ISDIR(root->iso_self));

	root->block = t->curblock;
	t->curblock += div_up(root->len, t->block_size);

	t->dirlist_joliet[t->curfile++] = root;
	for (i = 0; i < root->nchildren; i++) {
		ch = root->children[i];
		if (ISO_ISDIR(ch->iso_self))
			joliet_calc_dir_pos(t, ch);
	}

	/* reset curfile when we're finished */
	if (!root->parent)
		t->curfile = 0;
}

void
joliet_update_file_pos(struct ecma119_write_target *t,
		       struct joliet_tree_node *dir)
{
	size_t i;

	assert(dir && ISO_ISDIR(dir->iso_self));
	for (i = 0; i < dir->nchildren; i++) {
		struct joliet_tree_node *ch;
		ch = dir->children[i];

		if (!ISO_ISDIR (ch->iso_self)) {
			struct iso_tree_node *iso = ch->iso_self;
			ch->block = iso->block;
		}
		else
			joliet_update_file_pos(t, ch);
	}

	/* reset curfile when we're finished */
	if (!dir->parent)
		t->curfile = 0;
}

struct joliet_tree_node*
joliet_tree_create(struct ecma119_write_target *t,
		   struct iso_tree_node *iso_root)
{
	struct joliet_tree_node *root = create_tree(t, NULL, iso_root);

	sort_tree(root);
	return root;
}

/* ugh. this is mostly C&P */
static void
write_path_table(struct ecma119_write_target *t,
		 int l_type,
		 uint8_t *buf)
{
	void (*write_int)(uint8_t*, uint32_t, int) = l_type ?
		iso_lsb : iso_msb;

       	size_t i;
	struct ecma119_path_table_record *rec;
	struct joliet_tree_node *dir;
	int parent = 0;

	assert (t->joliet);

	for (i = 0; i < t->dirlist_len; i++) {
		dir = t->pathlist_joliet[i];
		while ((i) && t->pathlist_joliet[parent] != dir->parent)
			parent++;
		assert(parent < i || i == 0);

		rec = (struct ecma119_path_table_record*) buf;
		rec->len_di[0] = dir->parent ?
			(uint8_t) ucslen(dir->name) * 2 : 1;
		rec->len_xa[0] = 0;
		write_int(rec->block, dir->block, 4);
		write_int(rec->parent, parent + 1, 2);
		if (dir->parent)
			memcpy(rec->dir_id, dir->name, rec->len_di[0]);
		buf += 8 + rec->len_di[0] + (rec->len_di[0] % 2);
	}

}

/* if file_id is >= 0, we use it instead of the filename. As a magic number,
 * file_id == 3 means that we are writing the root directory record (in order
 * to distinguish it from the "." entry in the root directory) */
static void
write_one_dir_record(struct ecma119_write_target *t,
		     struct joliet_tree_node *node,
		     int file_id,
		     uint8_t *buf)
{
	uint8_t len_dr = (file_id >= 0) ? 34 : node->dirent_len;
	uint8_t len_fi = (file_id >= 0) ? 1 : ucslen(node->name) * 2;
	uint8_t f_id = (uint8_t) ((file_id == 3) ? 0 : file_id);
	uint8_t *name = (file_id >= 0) ? &f_id : (uint8_t*)node->name;
	struct ecma119_dir_record *rec = (struct ecma119_dir_record*)buf;

	if (file_id == 1 && node->parent)
		node = node->parent;

	rec->len_dr[0] = len_dr;
	iso_bb(rec->block, node->block, 4);
	iso_bb(rec->length, node->len, 4);
	iso_datetime_7(rec->recording_time, t->now);
	rec->flags[0] = ISO_ISDIR(node->iso_self) ? 2 : 0;
	iso_bb(rec->vol_seq_number, t->volnum + 1, 2);
	rec->len_fi[0] = len_fi;
	memcpy(rec->file_id, name, len_fi);
}

static void
write_l_path_table(struct ecma119_write_target *t, uint8_t *buf)
{
	write_path_table (t, 1, buf);
}

static void
write_m_path_table(struct ecma119_write_target *t, uint8_t *buf)
{
	write_path_table (t, 0, buf);
}

static void
write_sup_vol_desc(struct ecma119_write_target *t, uint8_t *buf)
{
	struct ecma119_sup_vol_desc *vol = (struct ecma119_sup_vol_desc*)buf;
	struct iso_volume *volume = t->volset->volume[t->volnum];
	uint16_t *vol_id = str2ucs(volume->volume_id);
	uint16_t *pub_id = str2ucs(volume->publisher_id);
	uint16_t *data_id = str2ucs(volume->data_preparer_id);
	uint16_t *volset_id = str2ucs(t->volset->volset_id);
	int vol_id_len = MIN(32, ucslen(vol_id) * 2);
	int pub_id_len = MIN(128, ucslen(pub_id) * 2);
	int data_id_len = MIN(128, ucslen(data_id) * 2);
	int volset_id_len = MIN(128, ucslen(volset_id) * 2);

	vol->vol_desc_type[0] = 2;
	memcpy(vol->std_identifier, "CD001", 5);
	vol->vol_desc_version[0] = 1;
	memcpy(vol->system_id, "SYSID", 5);
	if (vol_id)
		memcpy(vol->volume_id, vol_id, vol_id_len);
	memcpy(vol->esc_sequences, "%/E", 3);
	iso_bb(vol->vol_space_size, t->vol_space_size, 4);
	iso_bb(vol->vol_set_size, t->volset->volset_size, 2);
	iso_bb(vol->vol_seq_number, t->volnum + 1, 2);
	iso_bb(vol->block_size, t->block_size, 2);
	iso_bb(vol->path_table_size, t->path_table_size_joliet, 4);
	iso_lsb(vol->l_path_table_pos, t->l_path_table_pos_joliet, 4);
	iso_msb(vol->m_path_table_pos, t->m_path_table_pos_joliet, 4);

	write_one_dir_record(t, t->joliet_root, 3, vol->root_dir_record);

	memcpy(vol->vol_set_id, volset_id, volset_id_len);
	memcpy(vol->publisher_id, pub_id, pub_id_len);
	memcpy(vol->data_prep_id, data_id, data_id_len);
	/*memcpy(vol->application_id, "APPID", app_id_len);*/

	iso_datetime_17(vol->vol_creation_time, t->now);
	iso_datetime_17(vol->vol_modification_time, t->now);
	iso_datetime_17(vol->vol_effective_time, t->now);
	vol->file_structure_version[0] = 1;

	free(vol_id);
	free(volset_id);
	free(pub_id);
	free(data_id);

}

static void
write_one_dir(struct ecma119_write_target *t,
	      struct joliet_tree_node *dir,
	      uint8_t *buf)
{
	size_t i;
	uint8_t *orig_buf = buf;

	assert(ISO_ISDIR (dir->iso_self));
	/* write the "." and ".." entries first */
	write_one_dir_record(t, dir, 0, buf);
	buf += ((struct ecma119_dir_record*) buf)->len_dr[0];

	write_one_dir_record(t, dir, 1, buf);
	buf += ((struct ecma119_dir_record*) buf)->len_dr[0];

	for (i = 0; i < dir->nchildren; i++) {
		write_one_dir_record(t, dir->children[i], -1, buf);
		buf += ((struct ecma119_dir_record*) buf)->len_dr[0];
	}

	assert (buf - orig_buf == dir->len);
}

static void
write_dirs(struct ecma119_write_target *t, uint8_t *buf)
{
	size_t i;
	struct joliet_tree_node *dir;

	assert (t->curblock == t->dirlist_joliet[0]->block);
	for (i = 0; i < t->dirlist_len; i++) {
		dir = t->dirlist_joliet[i];
		write_one_dir(t, dir, buf);
		buf += round_up(dir->len, t->block_size);
	}
}

void
joliet_wr_sup_vol_desc(struct ecma119_write_target *t,
		       uint8_t *buf)
{
	ecma119_start_chunking(t,
			       write_sup_vol_desc,
			       2048,
			       buf);
}

void
joliet_wr_l_path_table(struct ecma119_write_target *t,
		       uint8_t *buf)
{
	ecma119_start_chunking(t,
			       write_l_path_table,
			       t->path_table_size_joliet,
			       buf);
}

void
joliet_wr_m_path_table(struct ecma119_write_target *t,
		       uint8_t *buf)
{
	ecma119_start_chunking(t,
			       write_m_path_table,
			       t->path_table_size_joliet,
			       buf);
}

void
joliet_wr_dir_records(struct ecma119_write_target *t,
		      uint8_t *buf)
{
	ecma119_start_chunking(t,
			       write_dirs,
			       t->total_dir_size_joliet,
			       buf);
}

