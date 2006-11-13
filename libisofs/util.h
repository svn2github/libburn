/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set noet ts=8 sts=8 sw=8 : */

/**
 * Utility functions for the Libisofs library.
 */

#ifndef LIBISO_UTIL_H
#define LIBISO_UTIL_H

#include <stdint.h>
#include <time.h>
#include <wchar.h>

#ifndef MAX
#	define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#	define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

extern inline int div_up(int n, int div)
{
	return (n + div - 1) / div;
}

extern inline int round_up(int n, int mul)
{
	return div_up(n, mul) * mul;
}

wchar_t *towcs(const char *);
char *str2ascii(const char*);
uint16_t *str2ucs(const char*);

/**
 * Create a level 1 directory identifier.
 */
char *iso_1_dirid(const char *src);

/**
 * Create a level 2 directory identifier.
 */
char *iso_2_dirid(const char *src);

/**
 * Create a level 1 file identifier that consists of a name, extension and
 * version number.  The resulting string will have a file name of maximum
 * length 8, followed by a separator (.), an optional extension of maximum
 * length 3, followed by a separator (;) and a version number (digit 1).
 * @return NULL if the original name and extension both are of length 0.
 */
char *iso_1_fileid(const char *src);

/**
 * Create a level 2 file identifier that consists of a name, extension and
 * version number. The combined file name and extension length will not exceed
 * 30, the name and extension will be separated (.), and the extension will be
 * followed by a separator (;) and a version number (digit 1).
 * @return NULL if the original name and extension both are of length 0.
 */
char *iso_2_fileid(const char *src);

/**
 * Create a Joliet file or directory identifier that consists of a name,
 * extension and version number. The combined name and extension length will
 * not exceed 128 bytes, the name and extension will be separated (.),
 * and the extension will be followed by a separator (;) and a version number
 * (digit 1). All characters consist of 2 bytes and the resulting string is
 * NULL-terminated by a 2-byte NULL. Requires the locale to be set correctly.
 *
 * @param size will be set to the size (in bytes) of the identifier.
 * @return NULL if the original name and extension both are of length 0 or the conversion from the current codeset to UCS-2BE is not available.
 */
uint16_t *iso_j_id(const char *src);

/**
 * FIXME: what are the requirements for these next two? Is this for RR?
 *
 * Create a POSIX portable file name that consists of a name and extension.
 * The resulting file name will not exceed 250 characters.
 * @return NULL if the original name and extension both are of length 0.
 */
char *iso_p_fileid(const char *src);

/**
 * Create a POSIX portable directory name.
 * The resulting directory name will not exceed 250 characters.
 * @return NULL if the original name is of length 0.
 */
char *iso_p_dirid(const char *src);

void iso_lsb(uint8_t *buf, uint32_t num, int bytes);
void iso_msb(uint8_t *buf, uint32_t num, int bytes);
void iso_bb(uint8_t *buf, uint32_t num, int bytes);

uint32_t iso_read_lsb(const uint8_t *buf, int bytes);
uint32_t iso_read_msb(const uint8_t *buf, int bytes);
uint32_t iso_read_bb(const uint8_t *buf, int bytes);

/** Records the date/time into a 7 byte buffer (9.1.5) */
void iso_datetime_7(uint8_t *buf, time_t t);

/** Records the date/time into a 17 byte buffer (8.4.26.1) */
void iso_datetime_17(uint8_t *buf, time_t t);

time_t iso_datetime_read_7(const uint8_t *buf);
time_t iso_datetime_read_17(const uint8_t *buf);

/**
 * Like strlen, but for Joliet strings.
 */
size_t ucslen(const uint16_t *str);

/**
 * Like strcmp, but for Joliet strings.
 */
int ucscmp(const uint16_t *s1, const uint16_t *s2);

#endif /* LIBISO_UTIL_H */
