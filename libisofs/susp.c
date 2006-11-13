/* vim: set noet ts=8 sts=8 sw=8 : */

#include "susp.h"
#include "util.h"
#include "ecma119.h"
#include "ecma119_tree.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

void susp_insert(struct ecma119_write_target *t,
		 struct susp_info *susp,
		 uint8_t *data,
		 int pos)
{
	int i;

	if (pos < 0) {
		pos = susp->n_susp_fields;
	}

	assert(pos <= susp->n_susp_fields);
	susp->n_susp_fields++;
	susp->susp_fields = realloc(susp->susp_fields,
				    sizeof(void*) * susp->n_susp_fields);

	for (i = susp->n_susp_fields-1; i > pos; i--) {
		susp->susp_fields[i] = susp->susp_fields[i - 1];
	}
	susp->susp_fields[pos] = data;
}

void susp_append(struct ecma119_write_target *t,
		 struct susp_info *susp,
		 uint8_t *data)
{
	susp_insert(t, susp, data, susp->n_susp_fields);
}

uint8_t *susp_find(struct susp_info *susp, const char *name)
{
	int i;

	for (i = 0; i < susp->n_susp_fields; i++) {
		if (!strncmp((char *)susp->susp_fields[i], name, 2)) {
			return susp->susp_fields[i];
		}
	}
	return NULL;
}

/** Utility function for susp_add_CE because susp_add_CE needs to act 3 times
 * on directories (for the "." and ".." entries.
 *
 * \param len The amount of space available for the System Use area.
 */
#define CE_LEN 28
static unsigned char *susp_add_single_CE(struct ecma119_write_target *t,
					 struct susp_info *susp,
					 int len)
{
	int susp_length = 0, tmp_len;
	int i;
	unsigned char *CE;

	for (i = 0; i < susp->n_susp_fields; i++) {
		susp_length += susp->susp_fields[i][2];
	}
	if (susp_length <= len) {
		/* no need for a CE field */
		susp->non_CE_len = susp_length;
		susp->n_fields_fit = susp->n_susp_fields;
		return NULL;
	}

	tmp_len = susp_length;
	for (i = susp->n_susp_fields - 1; i >= 0; i--) {
		tmp_len -= susp->susp_fields[i][2];
		if (tmp_len + CE_LEN <= len) {
			susp->non_CE_len = tmp_len + CE_LEN;
			susp->CE_len = susp_length - tmp_len;

			/* i+1 because we have to count the CE field */
			susp->n_fields_fit = i + 1;

			CE = calloc(1, CE_LEN);
			/* we don't fill in the BLOCK LOCATION or OFFSET
			   fields yet. */
			CE[0] = 'C';
			CE[1] = 'E';
			CE[2] = (char)CE_LEN;
			CE[3] = (char)1;
			iso_bb(&CE[20], susp_length - tmp_len, 4);

			return CE;
		}
	}
	assert(0);
	return NULL;
}

static void
try_add_CE(struct ecma119_write_target *t,
	   struct susp_info *susp,
	   size_t dirent_len)
{
	uint8_t *CE = susp_add_single_CE(t, susp, 255 - dirent_len);
	if (CE)
		susp_insert(t, susp, CE, susp->n_fields_fit - 1);
}

/** See IEEE P1281 Draft Version 1.12/5.2. Because this function depends on the
 * length of the other SUSP fields, it should always be calculated last. */
void
susp_add_CE(struct ecma119_write_target *t, struct ecma119_tree_node *node)
{
	try_add_CE(t, &node->susp, node->dirent_len);
	if (node->type == ECMA119_DIR) {
		try_add_CE(t, &node->dir.self_susp, 34);
		try_add_CE(t, &node->dir.parent_susp, 34);
	}
}

/** See IEEE P1281 Draft Version 1.12/5.3 */
void
susp_add_SP(struct ecma119_write_target *t, struct ecma119_tree_node *dir)
{
	unsigned char *SP = malloc(7);

	assert(dir->type == ECMA119_DIR);

	SP[0] = 'S';
	SP[1] = 'P';
	SP[2] = (char)7;
	SP[3] = (char)1;
	SP[4] = 0xbe;
	SP[5] = 0xef;
	SP[6] = 0;
	susp_append(t, &dir->dir.self_susp, SP);
}

#if 0
/** See IEEE P1281 Draft Version 1.12/5.4 */
static void susp_add_ST(struct ecma119_write_target *t,
			struct iso_tree_node *node)
{
	unsigned char *ST = malloc(4);

	ST[0] = 'S';
	ST[1] = 'T';
	ST[2] = (char)4;
	ST[3] = (char)1;
	susp_append(t, node, ST);
}
#endif

/** See IEEE P1281 Draft Version 1.12/5.5 FIXME: this is rockridge */
void
rrip_add_ER(struct ecma119_write_target *t, struct ecma119_tree_node *dir)
{
	unsigned char *ER = malloc(182);

	assert(dir->type == ECMA119_DIR);

	ER[0] = 'E';
	ER[1] = 'R';
	ER[2] = 182;
	ER[3] = 1;
	ER[4] = 9;
	ER[5] = 72;
	ER[6] = 93;
	ER[7] = 1;
	memcpy(&ER[8], "IEEE_1282", 9);
	memcpy(&ER[17], "THE IEEE 1282 PROTOCOL PROVIDES SUPPORT FOR POSIX "
	       "FILE SYSTEM SEMANTICS.", 72);
	memcpy(&ER[89], "PLEASE CONTACT THE IEEE STANDARDS DEPARTMENT, "
	       "PISCATAWAY, NJ, USA FOR THE 1282 SPECIFICATION.", 93);
	susp_append(t, &dir->dir.self_susp, ER);
}

/* calculate the location of the CE areas. Since CE areas don't need to be
 * aligned to a block boundary, we contatenate all CE areas from a single
 * directory and dump them immediately after all the directory records.
 *
 * Requires that the following be known:
 *  - position of the current directory (dir->block)
 *  - length of the current directory (dir->dir.len)
 *  - sum of the children's CE lengths (dir->dir.CE_len)
 */
static void
susp_fin_1_CE(struct ecma119_write_target *t,
		   struct susp_info *susp,
		   size_t block,
		   size_t *offset)
{
	uint8_t *CE = susp->susp_fields[susp->n_fields_fit - 1];

	if (!susp->CE_len) {
		return;
	}
	iso_bb(&CE[4], block + (*offset) / t->block_size, 4);
	iso_bb(&CE[12], (*offset) % t->block_size, 4);
	*offset += susp->CE_len;
}

static void susp_fin_CE(struct ecma119_write_target *t,
			struct ecma119_tree_node *dir)
{
	int i;
	size_t CE_offset = dir->dir.len;

	assert(dir->type == ECMA119_DIR);

	susp_fin_1_CE(t, &dir->dir.self_susp, dir->block, &CE_offset);
	susp_fin_1_CE(t, &dir->dir.parent_susp, dir->block, &CE_offset);

	for (i = 0; i < dir->dir.nchildren; i++) {
		struct ecma119_tree_node *ch = dir->dir.children[i];
		susp_fin_1_CE(t, &ch->susp, dir->block, &CE_offset);
	}
	assert(CE_offset == dir->dir.len + dir->dir.CE_len);
}

void
susp_finalize(struct ecma119_write_target *t, struct ecma119_tree_node *dir)
{
	int i;

	assert(dir->type = ECMA119_DIR);

	if (dir->dir.depth != 1) {
		susp_fin_CE(t, dir);
	}

	for (i = 0; i < dir->dir.nchildren; i++) {
		if (dir->dir.children[i]->type == ECMA119_DIR)
			susp_finalize(t, dir->dir.children[i]);
	}
}

void susp_write(struct ecma119_write_target *t,
		struct susp_info *susp,
		unsigned char *buf)
{
	int i;
	int pos = 0;

	for (i = 0; i < susp->n_fields_fit; i++) {
		memcpy(&buf[pos], susp->susp_fields[i],
		       susp->susp_fields[i][2]);
		pos += susp->susp_fields[i][2];
	}
}

void susp_write_CE(struct ecma119_write_target *t, struct susp_info *susp,
		   unsigned char *buf)
{
	int i;
	int pos = 0;

	for (i = susp->n_fields_fit; i < susp->n_susp_fields; i++) {
		memcpy(&buf[pos], susp->susp_fields[i],
		       susp->susp_fields[i][2]);
		pos += susp->susp_fields[i][2];
	}
}

void susp_free_fields(struct susp_info *susp)
{
	int i;

	for (i=0; i<susp->n_susp_fields; i++) {
		free(susp->susp_fields[i]);
	}
	if (susp->susp_fields) {
		free(susp->susp_fields);
	}
	memset(susp, 0, sizeof(struct susp_info));
}
