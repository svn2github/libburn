/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set noet ts=8 sts=8 sw=8 : */

/**
 * \file joliet.h
 *
 * Declare the filesystems trees that are Joliet-compatible and the public
 * functions for tying them into an ecma119 volume.
 */

#ifndef LIBISO_JOLIET_H
#define LIBISO_JOLIET_H

#include <stdint.h>
#include <stdlib.h>

struct ecma119_write_target;
struct iso_tree_node;

struct joliet_tree_node
{
	uint16_t *name;			/**< In UCS-2BE. */
	size_t dirent_len;
	size_t len;
	size_t block;

	struct joliet_tree_node *parent;
	struct iso_tree_node *iso_self;
	struct ecma119_write_target *target;

	struct joliet_tree_node **children;
	size_t nchildren;
};

/**
 * Create a new joliet_tree that corresponds to the tree represented by
 * \p iso_root.
 */
struct joliet_tree_node*
joliet_tree_create(struct ecma119_write_target *target,
		   struct iso_tree_node *iso_root);

/**
 * Calculate the size of each directory in the joliet heirarchy.
 */
void
joliet_calc_dir_size(struct ecma119_write_target *t, struct joliet_tree_node*);

/**
 * Calculate the position of each directory in the joliet heirarchy.
 */
void
joliet_calc_dir_pos(struct ecma119_write_target *t, struct joliet_tree_node*);

/**
 * Update the position of each file in the joliet hierarchy (to be called
 * AFTER the file positions in the iso tree have been set).
 */
void
joliet_update_file_pos(struct ecma119_write_target *t, struct joliet_tree_node*);

/**
 * Calculate the size of the joliet path table and fill in the list of
 * directories.
 */
void
joliet_prepare_path_tables(struct ecma119_write_target *t);

void
joliet_tree_free(struct joliet_tree_node *root);

void
joliet_wr_sup_vol_desc(struct ecma119_write_target *t, uint8_t *buf);

void
joliet_wr_l_path_table(struct ecma119_write_target *t, uint8_t *buf);

void
joliet_wr_m_path_table(struct ecma119_write_target *t, uint8_t *buf);

void
joliet_wr_dir_records(struct ecma119_write_target *t, uint8_t *buf);

#endif /* LIBISO_JOLIET_H */
