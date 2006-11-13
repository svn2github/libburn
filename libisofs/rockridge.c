/* vim: set noet ts=8 sts=8 sw=8 : */

#include "rockridge.h"
#include "util.h"
#include "ecma119.h"
#include "ecma119_tree.h"
#include "tree.h"
#include "susp.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

/* create a PX field from the permissions on the current node. */
uint8_t *rrip_make_PX(struct ecma119_write_target *t,
		      struct ecma119_tree_node *node)
{
	uint8_t *PX = malloc(44);

	PX[0] = 'P';
	PX[1] = 'X';
	PX[2] = 44;
	PX[3] = 1;
	iso_bb(&PX[4], node->iso_self->attrib.st_mode, 4);
	iso_bb(&PX[12], node->iso_self->attrib.st_nlink, 4);
	iso_bb(&PX[20], node->iso_self->attrib.st_uid, 4);
	iso_bb(&PX[28], node->iso_self->attrib.st_gid, 4);
	iso_bb(&PX[36], node->iso_self->attrib.st_ino, 4);
	return PX;
}

/** See IEEE 1282 4.1.1 */
void rrip_add_PX(struct ecma119_write_target *t, struct ecma119_tree_node *node)
{
	susp_append(t, &node->susp, rrip_make_PX(t, node));
	if (node->type == ECMA119_DIR) {
		susp_append(t, &node->dir.self_susp, rrip_make_PX(t, node));
		susp_append(t, &node->dir.parent_susp, rrip_make_PX(t, node));
	}
}

void rrip_add_PN(struct ecma119_write_target *t, struct ecma119_tree_node *node)
{
	uint8_t *PN = malloc(20);

	PN[0] = 'P';
	PN[1] = 'N';
	PN[2] = 20;
	PN[3] = 1;
	iso_bb(&PN[4], node->iso_self->attrib.st_dev >> 32, 4);
	iso_bb(&PN[12], node->iso_self->attrib.st_dev & 0xffffffff, 4);
	susp_append(t, &node->susp, PN);
}

static void rrip_SL_append_comp(int *n, uint8_t ***comps,
				char *s, int size, char fl)
{
	uint8_t *comp = malloc(size + 2);

	(*n)++;
	comp[0] = fl;
	comp[1] = size;
	*comps = realloc(*comps, (*n) * sizeof(void*));
	(*comps)[(*n) - 1] = comp;

	if (size) {
		memcpy(&comp[2], s, size);
	}
}

static void rrip_SL_add_component(char *prev, char *cur, int *n_comp,
				  uint8_t ***comps)
{
	int size = cur - prev;

	if (size == 0) {
		rrip_SL_append_comp(n_comp, comps, prev, 0, 1 << 3);
		return;
	}

	if (size == 1 && prev[0] == '.') {
		rrip_SL_append_comp(n_comp, comps, prev, 0, 1 << 1);
		return;
	}
	if (size == 2 && !strncmp(prev, "..", 2)) {
		rrip_SL_append_comp(n_comp, comps, prev, 0, 1 << 2);
		return;
	}

	/* we can't make a component any bigger than 250 (is this really a
	   problem)? because then it won't fit inside the SL field */
	while (size > 248) {
		size -= 248;
		rrip_SL_append_comp(n_comp, comps, prev, 248, 1 << 0);
	}

	rrip_SL_append_comp(n_comp, comps, prev, size, 0);
}

void rrip_add_SL(struct ecma119_write_target *t, struct ecma119_tree_node *node)
{
	int ret, pathsize = 0;
	char *path = NULL, *cur, *prev;
	int i, j;

	uint8_t **comp = NULL;
	int n_comp = 0;
	int total_comp_len = 0;
	int written = 0, pos;

	uint8_t *SL;

	do {
		pathsize += 128;
		path = realloc(path, pathsize);
		/* FIXME: what if the file is not on the local fs? */
		ret = readlink(node->iso_self->loc.path, path, pathsize);
	} while (ret == pathsize);
	if (ret == -1) {
		fprintf(stderr, "Error: couldn't read symlink: %s\n",
			strerror(errno));
		return;
	}
	path[ret] = '\0';

	prev = path;
	for (cur = strchr(path, '/'); cur && *cur; cur = strchr(cur, '/')) {
		rrip_SL_add_component(prev, cur, &n_comp, &comp);
		cur++;
		prev = cur;
	}

	/* if there was no trailing '/', we need to add the last component. */
	if (prev == path || prev != &path[ret - 1]) {
		rrip_SL_add_component(prev, &path[ret], &n_comp, &comp);
	}

	for (i = 0; i < n_comp; i++) {
		total_comp_len += comp[i][1] + 2;
		if (total_comp_len > 250) {
			total_comp_len -= comp[i][1] + 2;
			SL = malloc(total_comp_len + 5);
			SL[0] = 'S';
			SL[1] = 'L';
			SL[2] = total_comp_len + 5;
			SL[3] = 1;
			SL[4] = 1;	/* CONTINUE */
			pos = 5;
			for (j = written; j < i; j++) {
				memcpy(&SL[pos], comp[j], comp[j][2]);
				pos += comp[j][2];
			}
			susp_append(t, &node->susp, SL);
			written = i - 1;
			total_comp_len = comp[i][1];
		}
	}
	SL = malloc(total_comp_len + 5);
	SL[0] = 'S';
	SL[1] = 'L';
	SL[2] = total_comp_len + 5;
	SL[3] = 1;
	SL[4] = 0;
	pos = 5;

	for (j = written; j < n_comp; j++) {
		memcpy(&SL[pos], comp[j], comp[j][1] + 2);
		pos += comp[j][1] + 2;
	}
	susp_append(t, &node->susp, SL);

	free(path);
	/* free the components */
	for (i = 0; i < n_comp; i++) {
		free(comp[i]);
	}
	free(comp);
}

static void rrip_add_NM_single(struct ecma119_write_target *t,
			       struct susp_info *susp,
			       char *name, int size, int flags)
{
	uint8_t *NM = malloc(size + 5);

	NM[0] = 'N';
	NM[1] = 'M';
	NM[2] = size + 5;
	NM[3] = 1;
	NM[4] = flags;
	if (size) {
		memcpy(&NM[5], name, size);
	}
	susp_append(t, susp, NM);
}

void
rrip_add_NM(struct ecma119_write_target *t, struct ecma119_tree_node *node)
{
	char *name = iso_p_fileid(node->iso_self->name);
	int len = name ? strlen(name) : 0;
	char *pos = name;

	if (!len)
		return;

	if (node->type == ECMA119_DIR) {
		rrip_add_NM_single(t, &node->dir.self_susp, pos, 0, 1 << 1);
		rrip_add_NM_single(t, &node->dir.parent_susp, pos, 0, 1 << 2);
	}

	while (len > 250) {
		rrip_add_NM_single(t, &node->susp, pos, 250, 1);
		len -= 250;
		pos += 250;
	}
	rrip_add_NM_single(t, &node->susp, pos, len, 0);
}

void rrip_add_CL(struct ecma119_write_target *t, struct ecma119_tree_node *node)
{
	uint8_t *CL = calloc(1, 12);

	CL[0] = 'C';
	CL[1] = 'L';
	CL[2] = 12;
	CL[3] = 1;
	susp_append(t, &node->susp, CL);
}

void
rrip_add_PL(struct ecma119_write_target *t, struct ecma119_tree_node *node)
{
	uint8_t *PL = calloc(1, 12);

	PL[0] = 'P';
	PL[1] = 'L';
	PL[2] = 12;
	PL[3] = 1;
	susp_append(t, &node->dir.parent_susp, PL);
}

void
rrip_add_RE(struct ecma119_write_target *t, struct ecma119_tree_node *node)
{
	uint8_t *RE = malloc(4);

	RE[0] = 'R';
	RE[1] = 'E';
	RE[2] = 4;
	RE[3] = 1;
	susp_append(t, &node->susp, RE);
}

void
rrip_add_TF(struct ecma119_write_target *t, struct ecma119_tree_node *node)
{
	uint8_t *TF = malloc(5 + 3 * 7);

	TF[0] = 'T';
	TF[1] = 'F';
	TF[2] = 5 + 3 * 7;
	TF[3] = 1;
	TF[4] = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 7);
	iso_datetime_7(&TF[5], node->iso_self->attrib.st_mtime);
	iso_datetime_7(&TF[12], node->iso_self->attrib.st_atime);
	iso_datetime_7(&TF[19], node->iso_self->attrib.st_ctime);
	susp_append(t, &node->susp, TF);
}

void
rrip_finalize(struct ecma119_write_target *t, struct ecma119_tree_node *dir)
{
	int i;

	assert(dir->type == ECMA119_DIR);

	if (dir->parent != dir->dir.real_parent) {
		uint8_t *PL = susp_find(&dir->dir.parent_susp, "PL");

		assert(PL);
		iso_bb(&PL[4], dir->dir.real_parent->block, 4);
	}

	for (i = 0; i < dir->dir.nchildren; i++) {
		struct ecma119_tree_node *ch = dir->dir.children[i];

		if (ch->type == ECMA119_FILE && ch->file.real_me) {
			uint8_t *CL = susp_find(&ch->susp, "CL");

			assert(CL);
			iso_bb(&CL[4], ch->file.real_me->block, 4);
		} else if (ch->type == ECMA119_DIR) {
			rrip_finalize(t, ch);
		}
	}
}
