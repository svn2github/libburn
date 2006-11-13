/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set noet ts=8 sts=8 sw=8 : */

#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <err.h>

#include "ecma119.h"
#include "ecma119_tree.h"
#include "susp.h"
#include "rockridge.h"
#include "joliet.h"
#include "volume.h"
#include "tree.h"
#include "util.h"
#include "libisofs.h"
#include "libburn/libburn.h"

/* burn-source compatible stuff */
static int
bs_read(struct burn_source *bs, unsigned char *buf, int size);
static off_t
bs_get_size(struct burn_source *bs);
static void
bs_free_data(struct burn_source *bs);

typedef void (*write_fn)(struct ecma119_write_target*, uint8_t*);

/* return true if the given state is only required for Joliet volumes */
static int
is_joliet_state(enum ecma119_write_state);

static void
next_state(struct ecma119_write_target *t);

/* write t->state_data to the buf, one block at a time */
static void
write_data_chunk(struct ecma119_write_target *t, uint8_t *buf);

/* writing functions. All these functions assume the buf is large enough */
static void
write_pri_vol_desc(struct ecma119_write_target *t, uint8_t *buf);
static void
write_vol_desc_terminator(struct ecma119_write_target *t, uint8_t *buf);
static void
write_path_table(struct ecma119_write_target *t, int l_type, uint8_t *buf);
static void
write_l_path_table(struct ecma119_write_target *t, uint8_t *buf);
static void
write_m_path_table(struct ecma119_write_target *t, uint8_t *buf);
static void
write_one_dir_record(struct ecma119_write_target *t,
		     struct ecma119_tree_node *dir,
		     int file_id,
		     uint8_t *buf);
static void
write_one_dir(struct ecma119_write_target *t,
	      struct ecma119_tree_node *dir,
	      uint8_t *buf);
static void
write_dirs(struct ecma119_write_target *t, uint8_t *buf);

/* wrapper functions for writing */
static void wr_system_area(struct ecma119_write_target*, uint8_t*);
static void wr_pri_vol_desc(struct ecma119_write_target*, uint8_t*);
static void wr_vol_desc_term(struct ecma119_write_target*, uint8_t*);
static void wr_l_path_table(struct ecma119_write_target*, uint8_t*);
static void wr_m_path_table(struct ecma119_write_target*, uint8_t*);
static void wr_dir_records(struct ecma119_write_target*, uint8_t*);
static void wr_files(struct ecma119_write_target*, uint8_t*);

static const write_fn writers[] =
{
	NULL,
	wr_system_area,
	wr_pri_vol_desc,
	joliet_wr_sup_vol_desc,
	wr_vol_desc_term,
	wr_l_path_table,
	wr_m_path_table,
	joliet_wr_l_path_table,
	joliet_wr_m_path_table,
	wr_dir_records,
	joliet_wr_dir_records,
	wr_files
};

/* When a writer is created, we 
 * 1) create an ecma119 tree
 * 2) add SUSP fields (if necessary)
 * 3) calculate the size and position of all nodes in the tree
 * 4) finalize SUSP fields (if necessary)
 */

static void
add_susp_fields_rec(struct ecma119_write_target *t,
		    struct ecma119_tree_node *node)
{
	size_t i;

	if (!node->iso_self)
		return;

	rrip_add_PX(t, node);
	rrip_add_NM(t, node);
	rrip_add_TF(t, node);
	if (node->iso_self->attrib.st_rdev)
		rrip_add_PN(t, node);
	if (S_ISLNK(node->iso_self->attrib.st_mode))
		rrip_add_SL(t, node);
	if (node->type == ECMA119_FILE && node->file.real_me)
		rrip_add_CL(t, node);
	if (node->type == ECMA119_DIR
			&& node->dir.real_parent != node->parent) {
		rrip_add_RE(t, node);
		rrip_add_PL(t, node);
	}
	susp_add_CE(t, node);

	if (node->type == ECMA119_DIR) {
		for (i = 0; i < node->dir.nchildren; i++) {
			add_susp_fields_rec(t, node->dir.children[i]);
		}
	}
}

static void
add_susp_fields(struct ecma119_write_target *t)
{
	susp_add_SP(t, t->root);
	rrip_add_ER(t, t->root);
	add_susp_fields_rec(t, t->root);
}

/**
 * Fill out the dir.len and dir.CE_len fields for each
 * ecma119_tree_node that is a directory. Also calculate the total number of
 * directories and the number of files for which we need to write out data.
 * (dirlist_len and filelist_len)
 */
static void
calc_dir_size(struct ecma119_write_target *t,
	      struct ecma119_tree_node *dir)
{
	size_t i;

	assert(dir->type == ECMA119_DIR);

	t->dirlist_len++;
	dir->dir.len = 34 + dir->dir.self_susp.non_CE_len
			+ 34 + dir->dir.parent_susp.non_CE_len;
	dir->dir.CE_len = dir->dir.self_susp.CE_len
			+ dir->dir.parent_susp.CE_len;
	for (i = 0; i < dir->dir.nchildren; i++) {
		struct ecma119_tree_node *ch = dir->dir.children[i];

		dir->dir.len += ch->dirent_len + ch->susp.non_CE_len;
		dir->dir.CE_len += ch->susp.CE_len;
	}
	t->total_dir_size += round_up(dir->dir.len + dir->dir.CE_len,
				      t->block_size);

	for (i = 0; i < dir->dir.nchildren; i++) {
		struct ecma119_tree_node *ch = dir->dir.children[i];
		struct iso_tree_node *iso = ch->iso_self;
		if (ch->type == ECMA119_DIR) {
			calc_dir_size(t, ch);
		} else if (iso && iso->attrib.st_size
			       && iso->loc.type == LIBISO_FILESYS
			       && iso->loc.path) {
			t->filelist_len++;
		}
	}
}

/**
 * Fill out the block field in each ecma119_tree_node that is a directory and
 * fill out t->dirlist.
 */
static void
calc_dir_pos(struct ecma119_write_target *t,
	     struct ecma119_tree_node *dir)
{
	size_t i;

	assert(dir->type == ECMA119_DIR);

	/* we don't need to set iso_self->block since each tree writes
	 * its own directories */
	dir->block = t->curblock;
	t->curblock += div_up(dir->dir.len + dir->dir.CE_len, t->block_size);
	t->dirlist[t->curfile++] = dir;
	for (i = 0; i < dir->dir.nchildren; i++) {
		struct ecma119_tree_node *ch = dir->dir.children[i];
		if (ch->type == ECMA119_DIR)
			calc_dir_pos(t, ch);
	}

	/* reset curfile when we're finished */
	if (!dir->parent) {
		t->curfile = 0;
	}
}

/**
 * Fill out the block field for each ecma119_tree_node that is a file and fill
 * out t->filelist.
 */
static void
calc_file_pos(struct ecma119_write_target *t,
	      struct ecma119_tree_node *dir)
{
	size_t i;

	assert(dir->type == ECMA119_DIR);

	for (i = 0; i < dir->dir.nchildren; i++) {
		struct ecma119_tree_node *ch = dir->dir.children[i];
		if (ch->type == ECMA119_FILE && ch->iso_self) {
			struct iso_tree_node *iso = ch->iso_self;
			off_t size = iso->attrib.st_size;

			iso->block = ch->block = t->curblock;
			t->curblock += div_up(size, t->block_size);
			if (size && iso->loc.type == LIBISO_FILESYS
				 && iso->loc.path)
				t->filelist[t->curfile++] = ch;
		}
	}

	for (i = 0; i < dir->dir.nchildren; i++) {
		struct ecma119_tree_node *ch = dir->dir.children[i];
		if (ch->type == ECMA119_DIR)
			calc_file_pos(t, ch);
	}

	/* reset curfile when we're finished */
	if (!dir->parent) {
		t->curfile = 0;
	}
}

struct ecma119_write_target*
ecma119_target_new(struct iso_volset *volset,
		   int volnum,
		   int level,
		   int flags)
{
	struct ecma119_write_target *t =
		calloc(1, sizeof(struct ecma119_write_target));
	size_t i, j, cur;
	struct iso_tree_node *iso_root = volset->volume[volnum]->root;

	volset->refcount++;
	t->root = ecma119_tree_create(t, iso_root);
	t->joliet = (flags & ECMA119_JOLIET) ? 1 : 0;
	if (t->joliet)
		t->joliet_root = joliet_tree_create(t, iso_root);
	t->volset = volset;
	t->volnum = volnum;
	t->now = time(NULL);

	t->rockridge = (flags & ECMA119_ROCKRIDGE) ? 1 : 0;
	t->iso_level = level;
	t->block_size = 2048;

	if (t->rockridge)
		add_susp_fields(t);
	calc_dir_size(t, t->root);
	if (t->joliet) {
		joliet_calc_dir_size(t, t->joliet_root);
		t->pathlist_joliet = calloc(1, sizeof(void*) * t->dirlist_len);
		t->dirlist_joliet = calloc(1, sizeof(void*) * t->dirlist_len);
	}

	t->dirlist = calloc(1, sizeof(void*) * t->dirlist_len);
	t->pathlist = calloc(1, sizeof(void*) * t->dirlist_len);
	t->filelist = calloc(1, sizeof(void*) * t->filelist_len);

	/* fill out the pathlist */
	t->pathlist[0] = t->root;
	t->path_table_size = 10; /* root directory record */
	cur = 1;
	for (i = 0; i < t->dirlist_len; i++) {
		struct ecma119_tree_node *dir = t->pathlist[i];
		for (j = 0; j < dir->dir.nchildren; j++) {
			struct ecma119_tree_node *ch = dir->dir.children[j];
			if (ch->type == ECMA119_DIR) {
				size_t len = 8 + strlen(ch->name);
				t->pathlist[cur++] = ch;
				t->path_table_size += len + len % 2;
			}
		}
	}

	t->curblock = 16 /* system area */
		+ 1	/* volume desc */
		+ 1;	/* volume desc terminator */

	if (t->joliet) /* supplementary vol desc */
		t->curblock += div_up (2048, t->block_size);

	t->l_path_table_pos = t->curblock;
	t->curblock += div_up(t->path_table_size, t->block_size);
	t->m_path_table_pos = t->curblock;
	t->curblock += div_up(t->path_table_size, t->block_size);
	if (t->joliet) {
		joliet_prepare_path_tables(t);
		t->l_path_table_pos_joliet = t->curblock;
		t->curblock += div_up(t->path_table_size_joliet, t->block_size);
		t->m_path_table_pos_joliet = t->curblock;
		t->curblock += div_up(t->path_table_size_joliet, t->block_size);
	}

	calc_dir_pos(t, t->root);
	if (t->joliet)
		joliet_calc_dir_pos(t, t->joliet_root);
	calc_file_pos(t, t->root);
	if (t->joliet)
		joliet_update_file_pos (t, t->joliet_root);

	if (t->rockridge) {
		susp_finalize(t, t->root);
		rrip_finalize(t, t->root);
	}

	t->total_size = t->curblock * t->block_size;
	t->vol_space_size = t->curblock;

	/* prepare for writing */
	t->curblock = 0;
	t->state = ECMA119_WRITE_SYSTEM_AREA;

	return t;
}

static int
is_joliet_state(enum ecma119_write_state state)
{
	return state == ECMA119_WRITE_SUP_VOL_DESC_JOLIET
	    || state == ECMA119_WRITE_L_PATH_TABLE_JOLIET
	    || state == ECMA119_WRITE_M_PATH_TABLE_JOLIET
	    || state == ECMA119_WRITE_DIR_RECORDS_JOLIET;
}

static void
next_state(struct ecma119_write_target *t)
{
	t->state++;
	while (!t->joliet && is_joliet_state(t->state))
		t->state++;

	printf ("now in state %d, curblock=%d\n", (int)t->state, (int)t->curblock);
}

static void
wr_system_area(struct ecma119_write_target *t, uint8_t *buf)
{
	memset(buf, 0, t->block_size);
	if (t->curblock == 15) {
		next_state(t);
	}
}
static void
wr_pri_vol_desc(struct ecma119_write_target *t, uint8_t *buf)
{
	ecma119_start_chunking(t, write_pri_vol_desc, 2048, buf);
}

static void
wr_vol_desc_term(struct ecma119_write_target *t, uint8_t *buf)
{
	ecma119_start_chunking(t, write_vol_desc_terminator, 2048, buf);
}

static void
wr_l_path_table(struct ecma119_write_target *t, uint8_t *buf)
{
	ecma119_start_chunking(t, write_l_path_table, t->path_table_size, buf);
}

static void
wr_m_path_table(struct ecma119_write_target *t, uint8_t *buf)
{
	ecma119_start_chunking(t, write_m_path_table, t->path_table_size, buf);
}

static void
wr_dir_records(struct ecma119_write_target *t, uint8_t *buf)
{
	ecma119_start_chunking(t, write_dirs, t->total_dir_size, buf);
}

static void
wr_files(struct ecma119_write_target *t, uint8_t *buf)
{
	struct state_files *f_st = &t->state_files;
	size_t nread;
	struct ecma119_tree_node *f = t->filelist[f_st->file];
	const char *path = f->iso_self->loc.path;

	if (!f_st->fd) {
		f_st->data_len = f->iso_self->attrib.st_size;
		f_st->fd = fopen(path, "r");
		if (!f_st->fd)
			err(1, "couldn't open %s for reading", path);
		assert(t->curblock == f->block);
	}

	nread = fread(buf, 1, t->block_size, f_st->fd);
	f_st->pos += t->block_size;
	if (nread < 0)
		warn("problem reading from %s", path);
	else if (nread != t->block_size && f_st->pos < f_st->data_len)
		warnx("incomplete read from %s", path);
	if (f_st->pos >= f_st->data_len) {
		fclose(f_st->fd);
		f_st->fd = 0;
		f_st->pos = 0;
		f_st->file++;
		if (f_st->file >= t->filelist_len)
			next_state(t);
	}
}

static void
write_pri_vol_desc(struct ecma119_write_target *t, uint8_t *buf)
{
	struct ecma119_pri_vol_desc *vol = (struct ecma119_pri_vol_desc*)buf;
	struct iso_volume *volume = t->volset->volume[t->volnum];
	char *vol_id = str2ascii(volume->volume_id);
	char *pub_id = str2ascii(volume->publisher_id);
	char *data_id = str2ascii(volume->data_preparer_id);
	char *volset_id = str2ascii(t->volset->volset_id);

	vol->vol_desc_type[0] = 1;
	memcpy(vol->std_identifier, "CD001", 5);
	vol->vol_desc_version[0] = 1;
	memcpy(vol->system_id, "SYSID", 5);
	if (vol_id)
		strncpy((char*)vol->volume_id, vol_id, 32);
	iso_bb(vol->vol_space_size, t->vol_space_size, 4);
	iso_bb(vol->vol_set_size, t->volset->volset_size, 2);
	iso_bb(vol->vol_seq_number, t->volnum + 1, 2);
	iso_bb(vol->block_size, t->block_size, 2);
	iso_bb(vol->path_table_size, t->path_table_size, 4);
	iso_lsb(vol->l_path_table_pos, t->l_path_table_pos, 4);
	iso_msb(vol->m_path_table_pos, t->m_path_table_pos, 4);

	write_one_dir_record(t, t->root, 3, vol->root_dir_record);

	strncpy((char*)vol->vol_set_id, volset_id, 128);
	strncpy((char*)vol->publisher_id, pub_id, 128);
	strncpy((char*)vol->data_prep_id, data_id, 128);
	strncpy((char*)vol->application_id, "APPID", 128);

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
write_vol_desc_terminator(struct ecma119_write_target *t, uint8_t *buf)
{
	struct ecma119_vol_desc_terminator *vol =
		(struct ecma119_vol_desc_terminator*) buf;

	vol->vol_desc_type[0] = 255;
	memcpy(vol->std_identifier, "CD001", 5);
	vol->vol_desc_version[0] = 1;
}

static void
write_path_table(struct ecma119_write_target *t, int l_type, uint8_t *buf)
{
	void (*write_int)(uint8_t*, uint32_t, int) = l_type ? iso_lsb
							    : iso_msb;
	size_t i;
	struct ecma119_path_table_record *rec;
	struct ecma119_tree_node *dir;
	int parent = 0;

	for (i = 0; i < t->dirlist_len; i++) {
		dir = t->pathlist[i];
		while ((i) && t->pathlist[parent] != dir->parent)
			parent++;
		assert(parent < i || i == 0);

		rec = (struct ecma119_path_table_record*) buf;
		rec->len_di[0] = dir->parent ? (uint8_t) strlen(dir->name) : 1;
		rec->len_xa[0] = 0;
		write_int(rec->block, dir->block, 4);
		write_int(rec->parent, parent + 1, 2);
		if (dir->parent)
			memcpy(rec->dir_id, dir->name, rec->len_di[0]);
		buf += 8 + rec->len_di[0] + (rec->len_di[0] % 2);
	}
}

static void
write_l_path_table(struct ecma119_write_target *t, uint8_t *buf)
{
	write_path_table(t, 1, buf);
}

static void
write_m_path_table(struct ecma119_write_target *t, uint8_t *buf)
{
	write_path_table(t, 0, buf);
}

/* if file_id is >= 0, we use it instead of the filename. As a magic number,
 * file_id == 3 means that we are writing the root directory record (in order
 * to distinguish it from the "." entry in the root directory) */
static void
write_one_dir_record(struct ecma119_write_target *t,
		     struct ecma119_tree_node *node,
		     int file_id,
		     uint8_t *buf)
{
	uint8_t len_dr = (file_id >= 0) ? 34 : node->dirent_len;
	uint8_t len_fi = (file_id >= 0) ? 1 : strlen(node->name);
	uint8_t f_id = (uint8_t) ((file_id == 3) ? 0 : file_id);
	uint8_t *name = (file_id >= 0) ? &f_id : (uint8_t*)node->name;
	uint32_t len = (node->type == ECMA119_DIR) ? node->dir.len
		: node->file.real_me ? 0 : node->iso_self->attrib.st_size;
	struct ecma119_dir_record *rec = (struct ecma119_dir_record*)buf;

	/* we don't write out susp fields for the root node */
	if (t->rockridge) {
		if (file_id == 0) {
			susp_write(t, &node->dir.self_susp, &buf[len_dr]);
			len_dr += node->dir.self_susp.non_CE_len;
		} else if (file_id == 1) {
			susp_write(t, &node->dir.parent_susp, &buf[len_dr]);
			len_dr += node->dir.parent_susp.non_CE_len;
		} else if (file_id < 0) {
			susp_write(t, &node->susp, &buf[len_dr]);
			len_dr += node->susp.non_CE_len;
		}
	}
	if (file_id == 1 && node->parent)
		node = node->parent;

	rec->len_dr[0] = len_dr;
	iso_bb(rec->block, node->block, 4);
	iso_bb(rec->length, len, 4);
	iso_datetime_7(rec->recording_time, t->now);
	rec->flags[0] = (node->type == ECMA119_DIR) ? 2 : 0;
	iso_bb(rec->vol_seq_number, t->volnum + 1, 2);
	rec->len_fi[0] = len_fi;
	memcpy(rec->file_id, name, len_fi);
}

static void
write_one_dir(struct ecma119_write_target *t,
	      struct ecma119_tree_node *dir,
	      uint8_t *buf)
{
	size_t i;
	uint8_t *orig_buf = buf;

	assert(dir->type == ECMA119_DIR);
	/* write the "." and ".." entries first */
	write_one_dir_record(t, dir, 0, buf);
	buf += ((struct ecma119_dir_record*) buf)->len_dr[0];

	write_one_dir_record(t, dir, 1, buf);
	buf += ((struct ecma119_dir_record*) buf)->len_dr[0];

	for (i = 0; i < dir->dir.nchildren; i++) {
		write_one_dir_record(t, dir->dir.children[i], -1, buf);
		buf += ((struct ecma119_dir_record*) buf)->len_dr[0];
	}

	/* write the susp continuation areas */
	if (t->rockridge) {
		susp_write_CE(t, &dir->dir.self_susp, buf);
		buf += dir->dir.self_susp.CE_len;
		susp_write_CE(t, &dir->dir.parent_susp, buf);
		buf += dir->dir.parent_susp.CE_len;
		for (i = 0; i < dir->dir.nchildren; i++) {
			susp_write_CE(t, &dir->dir.children[i]->susp, buf);
			buf += dir->dir.children[i]->susp.CE_len;
		}
	}
	assert (buf - orig_buf == dir->dir.len + dir->dir.CE_len);
}

static void
write_dirs(struct ecma119_write_target *t, uint8_t *buf)
{
	size_t i;
	struct ecma119_tree_node *dir;
	for (i = 0; i < t->dirlist_len; i++) {
		dir = t->dirlist[i];
		write_one_dir(t, dir, buf);
		buf += round_up(dir->dir.len + dir->dir.CE_len, t->block_size);
	}
}

void
ecma119_start_chunking(struct ecma119_write_target *t,
	       write_fn writer,
	       off_t data_size,
	       uint8_t *buf)
{
	if (data_size != t->state_data_size) {
		data_size = round_up(data_size, t->block_size);
		t->state_data = realloc(t->state_data, data_size);
		t->state_data_size = data_size;
	}
	memset(t->state_data, 0, t->state_data_size);
	t->state_data_off = 0;
	t->state_data_valid = 1;
	writer(t, t->state_data);
	write_data_chunk(t, buf);
}

static void
write_data_chunk(struct ecma119_write_target *t, uint8_t *buf)
{
	memcpy(buf, t->state_data + t->state_data_off, t->block_size);
	t->state_data_off += t->block_size;
	if (t->state_data_off >= t->state_data_size) {
		assert (t->state_data_off <= t->state_data_size);
		t->state_data_valid = 0;
		next_state(t);
	}
}

static int
bs_read(struct burn_source *bs, unsigned char *buf, int size)
{
	struct ecma119_write_target *t = (struct ecma119_write_target*)bs->data;
	if (size != t->block_size) {
		warnx("you must read data in block-sized chunks (%d bytes)",
			(int)t->block_size);
		return 0;
	} else if (t->curblock >= t->vol_space_size) {
		return 0;
	}
	if (t->state_data_valid)
		write_data_chunk(t, buf);
	else
		writers[t->state](t, buf);
	t->curblock++;
	return size;
}

static off_t
bs_get_size(struct burn_source *bs)
{
	struct ecma119_write_target *t = (struct ecma119_write_target*)bs->data;
	return t->total_size;
}

static void
bs_free_data(struct burn_source *bs)
{
	struct ecma119_write_target *t = (struct ecma119_write_target*)bs->data;
	ecma119_tree_free(t->root);
	free(t->dirlist);
	free(t->pathlist);
	free(t->dirlist_joliet);
	free(t->pathlist_joliet);
	free(t->filelist);
	free(t->state_data);
	if (t->state_files.fd)
		fclose(t->state_files.fd);
}

struct burn_source *iso_source_new_ecma119(struct iso_volset *volset,
					   int volnum,
					   int level,
					   int flags)
{
	struct burn_source *ret = calloc(1, sizeof(struct burn_source));
	ret->refcount = 1;
	ret->read = bs_read;
	ret->get_size = bs_get_size;
	ret->free_data = bs_free_data;
	ret->data = ecma119_target_new(volset, volnum, level, flags);
	return ret;
}
