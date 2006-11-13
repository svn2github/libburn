/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set noet ts=8 sts=8 sw=8 : */

/** 
 * \file ecma119.h
 *
 * Structures and definitions used for writing an emca119 (ISO9660) compatible
 * volume.
 */

#ifndef LIBISO_ECMA119_H
#define LIBISO_ECMA119_H

#include <sys/time.h>
#include <stdint.h>
#include <stdio.h> /* for FILE */
#include <sys/types.h>
#include "susp.h"

struct ecma119_tree_node;
struct joliet_tree_node;

/**
 * The possible states that the ecma119 writer can be in.
 */
enum ecma119_write_state
{
	ECMA119_WRITE_BEFORE,

	ECMA119_WRITE_SYSTEM_AREA,
	ECMA119_WRITE_PRI_VOL_DESC,
	ECMA119_WRITE_SUP_VOL_DESC_JOLIET,
	ECMA119_WRITE_VOL_DESC_TERMINATOR,
	ECMA119_WRITE_L_PATH_TABLE,
	ECMA119_WRITE_M_PATH_TABLE,
	ECMA119_WRITE_L_PATH_TABLE_JOLIET,
	ECMA119_WRITE_M_PATH_TABLE_JOLIET,
	ECMA119_WRITE_DIR_RECORDS,
	ECMA119_WRITE_DIR_RECORDS_JOLIET,
	ECMA119_WRITE_FILES,

	ECMA119_WRITE_DONE
};

/**
 * Data describing the state of the ecma119 writer. Everything here should be
 * considered private!
 */
struct ecma119_write_target
{
	struct ecma119_tree_node *root;
	struct joliet_tree_node *joliet_root;
	struct iso_volset *volset;
	int volnum;

	time_t now;		/**< Time at which writing began. */
	off_t total_size;	/**< Total size of the output. This only
				  *  includes the current volume. */
	uint32_t vol_space_size;

	unsigned int rockridge:1;
	unsigned int joliet:1;
	unsigned int iso_level:2;

	int curblock;
	uint16_t block_size;
	uint32_t path_table_size;
	uint32_t path_table_size_joliet;
	uint32_t l_path_table_pos;
	uint32_t m_path_table_pos;
	uint32_t l_path_table_pos_joliet;
	uint32_t m_path_table_pos_joliet;
	uint32_t total_dir_size;
	uint32_t total_dir_size_joliet;

	struct ecma119_tree_node **dirlist;
					/**< A pre-order list of directories
					 * (this is the order in which we write
					 * out directory records).
					 */
	struct ecma119_tree_node **pathlist;
					/**< A breadth-first list of
					 * directories. This is used for
					 * writing out the path tables.
					 */
	size_t dirlist_len;		/**< The length of the previous 2 lists.
					 */

	struct ecma119_tree_node **filelist;
					/**< A pre-order list of files with
					 *  non-NULL paths and non-zero sizes.
					 */
	size_t filelist_len;		/* Length of the previous list. */

	int curfile;			/**< Used as a helper field for writing
					   out filelist and dirlist */

	/* Joliet versions of the above lists. Since Joliet doesn't require
	 * directory relocation, the order of these lists might be different
	 * from the lists above (but they will be the same length).
	 */
	struct joliet_tree_node **dirlist_joliet;
	struct joliet_tree_node **pathlist_joliet;

	enum ecma119_write_state state;	/* The current state of the writer. */

	/* Most writers work by
	 * 1) making sure state_data is big enough for their data
	 * 2) writing _all_ their data into state_data
	 * 3) relying on write_data_chunk to write the data block
	 *    by block.
	 */
	uint8_t *state_data;
	off_t state_data_size;
	off_t state_data_off;
	int state_data_valid;

	/* for writing out files */
	struct state_files {
		off_t pos;	/* The number of bytes we have written
				 * so far in the current file.
				 */
		off_t data_len;/* The number of bytes in the currently
				 * open file.
				 */
		FILE *fd;	/* The currently open file. */
		int file;	/* The index in filelist that we are
				 * currently writing (or about to write). */
	} state_files;
};

/**
 * Create a new ecma119_write_target from the given volume number of the
 * given volume set.
 *
 * \pre \p volnum is less than \p volset-\>volset_size.
 * \post For each node in the tree, writer_data has been allocated.
 * \post The directory heirarchy has been reorganised to be ecma119-compatible.
 */
struct ecma119_write_target *ecma119_target_new(struct iso_volset *volset,
						int volnum,
						int level,
						int flags);

#define BP(a,b) [(b) - (a) + 1]

struct ecma119_pri_vol_desc
{
	uint8_t vol_desc_type		BP(1, 1);
	uint8_t std_identifier		BP(2, 6);
	uint8_t vol_desc_version	BP(7, 7);
	uint8_t unused1			BP(8, 8);
	uint8_t system_id		BP(9, 40);
	uint8_t volume_id		BP(41, 72);
	uint8_t unused2			BP(73, 80);
	uint8_t vol_space_size		BP(81, 88);
	uint8_t unused3			BP(89, 120);
	uint8_t vol_set_size		BP(121, 124);
	uint8_t vol_seq_number		BP(125, 128);
	uint8_t block_size		BP(129, 132);
	uint8_t path_table_size		BP(133, 140);
	uint8_t l_path_table_pos	BP(141, 144);
	uint8_t opt_l_path_table_pos	BP(145, 148);
	uint8_t m_path_table_pos	BP(149, 152);
	uint8_t opt_m_path_table_pos	BP(153, 156);
	uint8_t root_dir_record		BP(157, 190);
	uint8_t	vol_set_id		BP(191, 318);
	uint8_t publisher_id		BP(319, 446);
	uint8_t data_prep_id		BP(447, 574);
	uint8_t application_id		BP(575, 702);
	uint8_t copyright_file_id	BP(703, 739);
	uint8_t abstract_file_id	BP(740, 776);
	uint8_t bibliographic_file_id	BP(777, 813);
	uint8_t vol_creation_time	BP(814, 830);
	uint8_t vol_modification_time	BP(831, 847);
	uint8_t vol_expiration_time	BP(848, 864);
	uint8_t vol_effective_time	BP(865, 881);
	uint8_t file_structure_version	BP(882, 882);
	uint8_t reserved1		BP(883, 883);
	uint8_t app_use			BP(884, 1395);
	uint8_t reserved2		BP(1396, 2048);
};

struct ecma119_sup_vol_desc
{
	uint8_t vol_desc_type		BP(1, 1);
	uint8_t std_identifier		BP(2, 6);
	uint8_t vol_desc_version	BP(7, 7);
	uint8_t vol_flags		BP(8, 8);
	uint8_t system_id		BP(9, 40);
	uint8_t volume_id		BP(41, 72);
	uint8_t unused2			BP(73, 80);
	uint8_t vol_space_size		BP(81, 88);
	uint8_t esc_sequences		BP(89, 120);
	uint8_t vol_set_size		BP(121, 124);
	uint8_t vol_seq_number		BP(125, 128);
	uint8_t block_size		BP(129, 132);
	uint8_t path_table_size		BP(133, 140);
	uint8_t l_path_table_pos	BP(141, 144);
	uint8_t opt_l_path_table_pos	BP(145, 148);
	uint8_t m_path_table_pos	BP(149, 152);
	uint8_t opt_m_path_table_pos	BP(153, 156);
	uint8_t root_dir_record		BP(157, 190);
	uint8_t	vol_set_id		BP(191, 318);
	uint8_t publisher_id		BP(319, 446);
	uint8_t data_prep_id		BP(447, 574);
	uint8_t application_id		BP(575, 702);
	uint8_t copyright_file_id	BP(703, 739);
	uint8_t abstract_file_id	BP(740, 776);
	uint8_t bibliographic_file_id	BP(777, 813);
	uint8_t vol_creation_time	BP(814, 830);
	uint8_t vol_modification_time	BP(831, 847);
	uint8_t vol_expiration_time	BP(848, 864);
	uint8_t vol_effective_time	BP(865, 881);
	uint8_t file_structure_version	BP(882, 882);
	uint8_t reserved1		BP(883, 883);
	uint8_t app_use			BP(884, 1395);
	uint8_t reserved2		BP(1396, 2048);
};

struct ecma119_vol_desc_terminator
{
	uint8_t vol_desc_type		BP(1, 1);
	uint8_t std_identifier		BP(2, 6);
	uint8_t vol_desc_version	BP(7, 7);
	uint8_t reserved		BP(8, 2048);
};

struct ecma119_dir_record
{
	uint8_t len_dr			BP(1, 1);
	uint8_t len_xa			BP(2, 2);
	uint8_t block			BP(3, 10);
	uint8_t length			BP(11, 18);
	uint8_t recording_time		BP(19, 25);
	uint8_t flags			BP(26, 26);
	uint8_t file_unit_size		BP(27, 27);
	uint8_t interleave_gap_size	BP(28, 28);
	uint8_t vol_seq_number		BP(29, 32);
	uint8_t len_fi			BP(33, 33);
	uint8_t file_id			BP(34, 34); /* 34 to 33+len_fi */
	/* padding field (if len_fi is even) */
	/* system use (len_dr - len_su + 1 to len_dr) */
};

struct ecma119_path_table_record
{
	uint8_t len_di			BP(1, 1);
	uint8_t len_xa			BP(2, 2);
	uint8_t block			BP(3, 6);
	uint8_t parent			BP(7, 8);
	uint8_t dir_id			BP(9, 9); /* 9 to 8+len_di */
	/* padding field (if len_di is odd) */
};

/**
 * A utility function for writers that want to write their data all at once
 * rather than block-by-block. This creates a buffer of size \p size, passes
 * it to the given writer, then hands out block-sized chunks.
 */
void
ecma119_start_chunking(struct ecma119_write_target *t,
		       void (*)(struct ecma119_write_target*, uint8_t*),
		       off_t size,
		       uint8_t *buf);

#endif /* LIBISO_ECMA119_H */
