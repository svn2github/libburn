/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set ts=8 sts=8 sw=8 noet : */

#define _GNU_SOURCE

#include "libisofs.h"
#include "libburn/libburn.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <err.h>

#define SECSIZE 2048

const char * const optstring = "JRL:h";
extern char *optarg;
extern int optind;

void usage()
{
	printf("test [OPTIONS] DIRECTORY OUTPUT\n");
}

void help()
{
	printf(
"Options:\n"
"  -J       Add Joliet support\n"
"  -R       Add Rock Ridge support\n"
"  -L <num> Set the ISO level (1 or 2)\n"
"  -h       Print this message\n"
);
}

int main(int argc, char **argv)
{
	struct iso_volset *volset;
	struct iso_volume *volume;
	struct iso_tree_node *root;
	struct burn_source *src;
	unsigned char buf[2048];
	FILE *fd;
	int c;
	int level=1, flags=0;

	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch(c) {
		case 'h':
			usage();
			help();
			exit(0);
			break;
		case 'J':
			flags |= ECMA119_JOLIET;
			break;
		case 'R':
			flags |= ECMA119_ROCKRIDGE;
			break;
		case 'L':
			level = atoi(optarg);
			break;
		case '?':
			usage();
			exit(1);
			break;
		}
	}

	if (argc < 2) {
		printf ("must pass directory to build iso from\n");
		usage();
		return 1;
	}
	if (argc < 3) {
		printf ("must supply output file\n");
		usage();
		return 1;
	}
	fd = fopen(argv[optind+1], "w");
	if (!fd) {
		err(1, "error opening output file");
	}

	root = iso_tree_radd_dir(NULL, argv[optind]);
	if (!root) {
		err(1, "error opening input directory");
	}
	volume = iso_volume_new_with_root( "VOLID", "PUBID", "PREPID", root );
	volset = iso_volset_new( volume, "VOLSETID" );

	src = iso_source_new_ecma119(volset, 0, level, flags);

	while (src->read(src, buf, 2048) == 2048) {
		fwrite(buf, 1, 2048, fd);
	}
	fclose(fd);

	return 0;
}
