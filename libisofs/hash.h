#ifndef ISO_HASH_H
#define ISO_HASH_H

struct iso_hash_node {
	struct iso_hash_node *next;
	char *path;
};

#define HASH_NODES	128

/**
 * Searches in the hash table if the path exists.
 *
 * \param table The hash table.
 * \param path The path of the file to look for.
 *
 * \return 1 if the path exists in the hash table, 0 otherwise.
 */
int iso_hash_lookup(struct iso_hash_node **table, const char *path);

/**
 * Insert a new path in the hash table.
 *
 * \param table The hash table.
 * \param path The path of a file to add to the hash table.
 *
 * \return 1 if the file wasn't already in the hash table, 0 otherwise.
 */
int iso_hash_insert(struct iso_hash_node **table, const char *path);

/**
 * Remove a path from the hash table.
 *
 * \param table The hash table.
 * \param path The path of a file to remove from the hash table.
 *
 * \return 1 if the file was found and removed, 0 otherwise.
 */
int iso_hash_remove(struct iso_hash_node **table, const char *path);

/**
 * Empty the hash table.
 */
void iso_hash_empty(struct iso_hash_node **table);

#endif /* ISO_HASH_H */
