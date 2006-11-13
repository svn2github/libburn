/* -*- indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; -*- */
/* vim: set noet ts=8 sts=8 sw=8 : */

/**
 * Utility functions for the Libisofs library.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <wchar.h>
#include <iconv.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <locale.h>

#include "util.h"

/* avoids warning and names in iso, joliet and rockridge can't be > 255 bytes
 * anyway. There are at most 31 characters in iso level 1, 255 for rockridge,
 * 64 characters (* 2 since UCS) for joliet. */
#define NAME_BUFFER_SIZE 255

int div_up(int n, int div)
{
	return (n + div - 1) / div;
}

int round_up(int n, int mul)
{
	return div_up(n, mul) * mul;
}

/* this function must always return a name
 * since the caller never checks if a NULL
 * is returned. It also avoids some warnings. */
char *str2ascii(const char *src_arg)
{
	wchar_t wsrc_[NAME_BUFFER_SIZE];
	char *src = (char*)wsrc_;
	char *ret_;
	char *ret;
	mbstate_t state;
	iconv_t conv;
	size_t numchars;
	size_t outbytes;
	size_t inbytes;
	size_t n;

	if (!src_arg)
		return NULL;

	/* convert the string to a wide character string. Note: outbytes
	 * is in fact the number of characters in the string and doesn't
	 * include the last NULL character. */
	memset(&state, 0, sizeof(state));
	numchars = mbsrtowcs(wsrc_, &src_arg, NAME_BUFFER_SIZE-1, &state);
	if (numchars < 0)
		return NULL;

	inbytes = numchars * sizeof(wchar_t);

	ret_ = malloc(numchars+1);
	outbytes = numchars;
	ret = ret_;

	/* initialize iconv */
	conv = iconv_open("ASCII", "WCHAR_T");
	if (conv == (iconv_t)-1)
		return NULL;

	n = iconv(conv, &src, &inbytes, &ret, &outbytes);
	while(n == -1) {
		/* The destination buffer is too small. Stops here. */
		if(errno == E2BIG)
			break;

		/* An incomplete multi bytes sequence was found. We 
		 * can't do anything here. That's quite unlikely. */
		if(errno == EINVAL)
			break;

		/* The last possible error is an invalid multi bytes
		 * sequence. Just replace the character with a "_". 
		 * Probably the character doesn't exist in ascii like
		 * "é, è, à, ç, ..." in French. */
		*ret++ = '_';
		outbytes--;

		if(!outbytes)
			break;

		/* There was an error with one character but some other remain
		 * to be converted. That's probably a multibyte character.
		 * See above comment. */
		src += sizeof(wchar_t);
		inbytes -= sizeof(wchar_t);

		if(!inbytes)
			break;

		n = iconv(conv, &src, &inbytes, &ret, &outbytes);
	}

	iconv_close(conv);

	*ret='\0';
	return ret_;
}

/* FIXME: C&P */
uint16_t *str2ucs(const char *src_arg)
{
	wchar_t wsrc_[NAME_BUFFER_SIZE];
	char *src = (char*)wsrc_;
	char *ret_;
	char *ret;
	mbstate_t state;
	iconv_t conv;
	size_t outbytes;
	size_t numchars;
	size_t inbytes;
	size_t n;

	if (!src_arg)
		return calloc(2, 1); /* empty UCS string */

	/* convert the string to a wide character string. Note: outbytes
	 * is in fact the number of characters in the string and doesn't
	 * include the last NULL character. */
	memset(&state, 0, sizeof(state));
	numchars = mbsrtowcs(wsrc_, &src_arg, NAME_BUFFER_SIZE-1, &state);
	if (numchars < 0)
		return calloc(2, 1); /* empty UCS string */

	inbytes = numchars * sizeof(wchar_t);

	outbytes = numchars * sizeof(uint16_t);
	ret_ = malloc ((numchars+1) * sizeof(uint16_t));
	ret = ret_;

	/* initialize iconv */
	conv = iconv_open("UCS-2BE", "WCHAR_T");
	if (conv == (iconv_t)-1)
		return calloc(2, 1); /* empty UCS string */

	n = iconv(conv, &src, &inbytes, &ret, &outbytes);
	while(n == -1) {
		/* The destination buffer is too small. Stops here. */
		if(errno == E2BIG)
			break;

		/* An incomplete multi bytes sequence was found. We 
		 * can't do anything here. That's quite unlikely. */
		if(errno == EINVAL)
			break;

		/* The last possible error is an invalid multi bytes
		 * sequence. Just replace the character with a "_". 
		 * Probably the character doesn't exist in ascii like
		 * "é, è, à, ç, ..." in French. */
		*((uint16_t*) ret) = '_';
		ret += sizeof(uint16_t);
		outbytes -= sizeof(uint16_t);

		if(!outbytes)
			break;

		/* There was an error with one character but some other remain
		 * to be converted. That's probably a multibyte character.
		 * See above comment. */
		src += sizeof(wchar_t);
		inbytes -= sizeof(wchar_t);

		if(!inbytes)
			break;

		n = iconv(conv, &src, &inbytes, &ret, &outbytes);
	}

	iconv_close(conv);

	/* close the ucs string */
	*((uint16_t*) ret) = 0;

	return (uint16_t*)ret_;
}


static int valid_d_char(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c == '_');
}

static int valid_a_char(char c)
{
	return (c >= ' ' && c <= '"') || (c >= '%' && c <= '?')
				      || (c >= 'A' && c <= 'Z')
				      || (c == '_');
}

static int valid_j_char(uint16_t c)
{
	return !(c < (uint16_t)' ' || c == (uint16_t)'*' || c == (uint16_t)'/'
				  || c == (uint16_t)':' || c == (uint16_t)';'
				  || c == (uint16_t)'?' || c == (uint16_t)'\\');
}

/* FIXME: where are these documented? */
static int valid_p_char(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')
				      || (c >= 'a' && c <= 'z')
				      || (c == '.') || (c == '_') || (c == '-');
}

static char *iso_dirid(const char *src, int size)
{
	char *ret = str2ascii(src);
	size_t len, i;

	if (!ret)
		return NULL;

	len = strlen(ret);
	if (len > size) {
		ret[size] = '\0';
		len = size;
	}
	for (i = 0; i < len; i++) {
		char c = toupper(ret[i]);
		ret[i] = valid_d_char(c) ? c : '_';
	}

	return ret;
}

char *iso_1_dirid(const char *src)
{
	return iso_dirid(src, 8);
}

char *iso_2_dirid(const char *src)
{
	return iso_dirid(src, 31);
}

char *iso_1_fileid(const char *src_arg)
{
	char *src = str2ascii(src_arg);
	char *dest;
	char *dot;			/* Position of the last dot in the
					   filename, will be used to calculate 
					   lname and lext. */
	int lname, lext, pos, i;

	if (!src)
		return NULL;

	dest = malloc(15);		/* 15 = 8 (name) + 1 (.) + 3 (ext) + 2 
					   (;1) + 1 (\0) */
	dot = strrchr(src, '.');

	lext = dot ? strlen(dot + 1) : 0;
	lname = strlen(src) - lext - (dot ? 1 : 0);

	/* If we can't build a filename, return NULL. */
	if (lname == 0 && lext == 0) {
		free(src);
		free(dest);
		return NULL;
	}

	pos = 0;
	/* Convert up to 8 characters of the filename. */
	for (i = 0; i < lname && i < 8; i++) {
		char c = toupper(src[i]);

		dest[pos++] = valid_d_char(c) ? c : '_';
	}
	/* This dot is mandatory, even if there is no extension. */
	dest[pos++] = '.';
	/* Convert up to 3 characters of the extension, if any. */
	for (i = 0; i < lext && i < 3; i++) {
		char c = toupper(src[lname + 1 + i]);

		dest[pos++] = valid_d_char(c) ? c : '_';
	}
	/* File versions are mandatory, even if they aren't used. */
	dest[pos++] = ';';
	dest[pos++] = '1';
	dest[pos] = '\0';
	dest = (char *)realloc(dest, pos + 1);

	free(src);
	return dest;
}

char *iso_2_fileid(const char *src_arg)
{
	char *src = str2ascii(src_arg);
	char *dest;
	char *dot;
	int lname, lext, lnname, lnext, pos, i;

	if (!src)
		return NULL;

	dest = malloc(34);		/* 34 = 30 (name + ext) + 1 (.) + 2
					   (;1) + 1 (\0) */
	dot = strrchr(src, '.');

	/* Since the maximum length can be divided freely over the name and
	   extension, we need to calculate their new lengths (lnname and
	   lnext). If the original filename is too long, we start by trimming
	   the extension, but keep a minimum extension length of 3. */
	if (dot == NULL || dot == src || *(dot + 1) == '\0') {
		lname = strlen(src);
		lnname = (lname > 30) ? 30 : lname;
		lext = lnext = 0;
	} else {
		lext = strlen(dot + 1);
		lname = strlen(src) - lext - 1;
		lnext = (strlen(src) > 31 && lext > 3)
			? (lname < 27 ? 30 - lname : 3) : lext;
		lnname = (strlen(src) > 31) ? 30 - lnext : lname;
	}

	if (lnname == 0 && lnext == 0) {
		free(src);
		free(dest);
		return NULL;
	}

	pos = 0;
	/* Convert up to lnname characters of the filename. */
	for (i = 0; i < lnname; i++) {
		char c = toupper(src[i]);

		dest[pos++] = valid_d_char(c) ? c : '_';
	}
	dest[pos++] = '.';
	/* Convert up to lnext characters of the extension, if any. */
	for (i = 0; i < lnext; i++) {
		char c = toupper(src[lname + 1 + i]);

		dest[pos++] = valid_d_char(c) ? c : '_';
	}
	dest[pos++] = ';';
	dest[pos++] = '1';
	dest[pos] = '\0';
	dest = (char *)realloc(dest, pos + 1);

	free(src);
	return dest;
}

char *
iso_p_fileid(const char *src)
{
	char *ret = str2ascii(src);
	size_t i, len;

	if (!ret)
		return NULL;
	len = strlen(ret);
	for (i = 0; i < len; i++) {
		if (!valid_p_char(ret[i])) {
			ret[i] = (uint16_t)'_';
		}
	}
	return ret;
}

uint16_t *
iso_j_id(const char *src_arg)
{
	uint16_t *j_str = str2ucs(src_arg);
	size_t len = ucslen(j_str);
	size_t n;

	if (len > 128) {
		j_str[128] = '\0';
		len = 128;
	}

	for (n = 0; n < len; n++)
		if (!valid_j_char(j_str[n]))
			j_str[n] = '_';
	return j_str;
}

void iso_lsb(uint8_t *buf, uint32_t num, int bytes)
{
	int i;

	assert(bytes <= 4);

	for (i = 0; i < bytes; ++i)
		buf[i] = (num >> (8 * i)) & 0xff;
}

void iso_msb(uint8_t *buf, uint32_t num, int bytes)
{
	int i;

	assert(bytes <= 4);

	for (i = 0; i < bytes; ++i)
		buf[bytes - 1 - i] = (num >> (8 * i)) & 0xff;
}

void iso_bb(uint8_t *buf, uint32_t num, int bytes)
{
	iso_lsb(buf, num, bytes);
	iso_msb(buf+bytes, num, bytes);
}


void iso_datetime_7(unsigned char *buf, time_t t)
{
	static int tzsetup = 0;
	int tzoffset;
	struct tm tm;

	if (!tzsetup) {
		tzset();
		tzsetup = 1;
	}

	localtime_r(&t, &tm);

	buf[0] = tm.tm_year;
	buf[1] = tm.tm_mon + 1;
	buf[2] = tm.tm_mday;
	buf[3] = tm.tm_hour;
	buf[4] = tm.tm_min;
	buf[5] = tm.tm_sec;
#ifdef HAVE_TM_GMTOFF
	tzoffset = -tm.tm_gmtoff / 60 / 15;
#else
	tzoffset = -timezone / 60 / 15;
#endif
	if (tzoffset < -48)
		tzoffset += 101;
	buf[6] = tzoffset;
}

time_t iso_datetime_read_7(const uint8_t *buf)
{
	struct tm tm;

	tm.tm_year = buf[0];
	tm.tm_mon = buf[1] + 1;
	tm.tm_mday = buf[2];
	tm.tm_hour = buf[3];
	tm.tm_min = buf[4];
	tm.tm_sec = buf[5];

	return mktime(&tm) - buf[6] * 60 * 60;
}

void iso_datetime_17(unsigned char *buf, time_t t)
{
	static int tzsetup = 0;
	static int tzoffset;
	struct tm tm;

	if (t == (time_t) - 1) {
		/* unspecified time */
		memset(buf, '0', 16);
		buf[16] = 0;
	} else {
		if (!tzsetup) {
			tzset();
			tzsetup = 1;
		}

		localtime_r(&t, &tm);

		sprintf((char*)&buf[0], "%04d", tm.tm_year + 1900);
		sprintf((char*)&buf[4], "%02d", tm.tm_mon + 1);
		sprintf((char*)&buf[6], "%02d", tm.tm_mday);
		sprintf((char*)&buf[8], "%02d", tm.tm_hour);
		sprintf((char*)&buf[10], "%02d", tm.tm_min);
		sprintf((char*)&buf[12], "%02d", MIN(59, tm.tm_sec));
		memcpy(&buf[14], "00", 2);
#ifdef HAVE_TM_GMTOFF
		tzoffset = -tm.tm_gmtoff / 60 / 15;
#else
		tzoffset = -timezone / 60 / 15;
#endif
		if (tzoffset < -48)
			tzoffset += 101;
		buf[16] = tzoffset;
	}
}

time_t iso_datetime_read_17(const uint8_t *buf)
{
	struct tm tm;

	sscanf((char*)&buf[0], "%4d", &tm.tm_year);
	sscanf((char*)&buf[4], "%2d", &tm.tm_mon);
	sscanf((char*)&buf[6], "%2d", &tm.tm_mday);
	sscanf((char*)&buf[8], "%2d", &tm.tm_hour);
	sscanf((char*)&buf[10], "%2d", &tm.tm_min);
	sscanf((char*)&buf[12], "%2d", &tm.tm_sec);
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;

	return mktime(&tm) - buf[16] * 60 * 60;
}

size_t ucslen(const uint16_t *str)
{
	int i;

	for (i=0; str[i]; i++)
		;
	return i;
}

/**
 * Although each character is 2 bytes, we actually compare byte-by-byte
 * (thats what the spec says).
 */
int ucscmp(const uint16_t *s1, const uint16_t *s2)
{
	const char *s = (const char*)s1;
	const char *t = (const char*)s2;
	size_t len1 = ucslen(s1);
	size_t len2 = ucslen(s2);
	size_t i, len = MIN(len1, len2) * 2;

	for (i=0; i < len; i++) {
		if (s[i] < t[i]) {
			return -1;
		} else if (s[i] > t[i]) {
			return 1;
		}
	}

	if (len1 < len2)
		return -1;
	else if (len1 > len2)
		return 1;
	return 0;
}

uint32_t iso_read_lsb(const uint8_t *buf, int bytes)
{
	int i;
	uint32_t ret = 0;

	for (i=0; i<bytes; i++) {
		ret += ((uint32_t) buf[i]) << (i*8);
	}
	return ret;
}

uint32_t iso_read_msb(const uint8_t *buf, int bytes)
{
	int i;
	uint32_t ret = 0;

	for (i=0; i<bytes; i++) {
		ret += ((uint32_t) buf[bytes-i-1]) << (i*8);
	}
	return ret;
}

uint32_t iso_read_bb(const uint8_t *buf, int bytes)
{
	uint32_t v1 = iso_read_lsb(buf, bytes);
	uint32_t v2 = iso_read_msb(buf+bytes, bytes);

	assert(v1 == v2);
	return v1;
}
