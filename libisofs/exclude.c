#include "hash.h"
#include "exclude.h"

static struct iso_hash_node *table[HASH_NODES]={0,};
static int num=0;

void
iso_exclude_add_path(const char *path)
{
	if (!path)
		return;

	num += iso_hash_insert(table, path);
}

void
iso_exclude_remove_path(const char *path)
{
	if (!num || !path)
		return;

	num -= iso_hash_remove(table, path);
}

void
iso_exclude_empty(void)
{
	if (!num)
		return;

	iso_hash_empty(table);
	num=0;
}

int
iso_exclude_lookup(const char *path)
{
	if (!num || !path)
		return 0;

	return iso_hash_lookup(table, path);
}
