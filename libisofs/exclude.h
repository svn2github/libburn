#ifndef ISO_EXCLUDE_H
#define ISO_EXCLUDE_H

/**
 * Add a path to ignore when adding a directory recursively.
 *
 * \param path The path, on the local filesystem, of the file.
 */
int
iso_exclude_lookup(const char *path);

#endif /* ISO_EXCLUDE */
