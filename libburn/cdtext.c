
/* Copyright (c) 2011 Thomas Schmitt <scdbackup@gmx.net>
   Provided under GPL version 2 or later.
*/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "libburn.h"
#include "init.h"

#include "libdax_msgs.h"
extern struct libdax_msgs *libdax_messenger;


/* ---------------- Reader of Sony Input Sheet Version 0.7T ------------- */


static char *v07t_printify(char *msg)
{
	char *cpt;

	for (cpt = msg; *cpt != 0; cpt++)
		if (*cpt < 32 || *cpt > 126)
			*cpt = '#';
	return msg;
}


/* @param flag bit0= allow two byte codes 0xNNNN or 0xNN 0xNN
*/
static int v07t_hexcode(char *payload, int flag)
{
	unsigned int x;
	int lo, hi, l;
	char buf[10], *cpt;

	l = strlen(payload);
	if (strncmp(payload, "0x", 2) != 0)
		return -1;
	if ((l == 6 || l == 9) && (flag & 1))
		goto double_byte;
	if (strlen(payload) != 4)
		return -1;
	if (!(isxdigit(payload[2]) && isxdigit(payload[3])))
		return -1;
	sscanf(payload + 2, "%x", &x);
	return x;

double_byte:;
	strcpy(buf, payload);
	buf[4] = 0;
	hi = v07t_hexcode(buf, 0);
	if (strlen(payload) == 6) {
		buf[4] = payload[4];
		buf[2] = '0';
		buf[3] = 'x';
		cpt = buf + 2;
	} else {
		if(payload[4] != 32 && payload[4] != 9)
			return(-1);
		cpt = buf + 5;
	}
	lo = v07t_hexcode(cpt, 0);
	if (lo < 0 || hi < 0)
		return -1;
	return ((hi << 8) | lo);
}


static int v07t_cdtext_char_code(char *payload, int flag)
{
	int ret;
	char *msg = NULL;

	ret = v07t_hexcode(payload, 0);
	if (ret >= 0)
		return ret;
	if (strstr(payload, "8859") != NULL)
		return 0x00;
	else if(strstr(payload, "ASCII") != NULL)
		return 0x01;
	else if(strstr(payload, "JIS") != NULL)
		return 0x80;

	BURN_ALLOC_MEM(msg, char, 160);
	sprintf(msg, "Unknown v07t Text Code '%.80s'", payload);
	libdax_msgs_submit(libdax_messenger, -1, 0x00020191,
				LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
				v07t_printify(msg), 0, 0);
	ret = -1;
ex:;
	BURN_FREE_MEM(msg);
	return ret;
}


static int v07t_cdtext_lang_code(char *payload, int flag)
{
	int i, ret;
	static char *languages[128] = {
		BURN_CDTEXT_LANGUAGES_0X00,
		BURN_CDTEXT_FILLER,
		BURN_CDTEXT_LANGUAGES_0X45
	};
	char *msg = NULL;

	ret = v07t_hexcode(payload, 0);
	if (ret >= 0)
		return ret;
	if (payload[0] != 0)
		for(i = 0; i < 128; i++)
			if(strcmp(languages[i], payload) == 0)
				return i;

	BURN_ALLOC_MEM(msg, char, 160);
	sprintf(msg, "Unknown v07t Language Code '%.80s'", payload);
	libdax_msgs_submit(libdax_messenger, -1, 0x00020191,
				LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
				v07t_printify(msg), 0, 0);
	ret = -1;
ex:;
	BURN_FREE_MEM(msg);
	return ret;
}


static int v07t_cdtext_genre_code(char *payload, int flag)
{
	int i, ret;
	static char *genres[BURN_CDTEXT_NUM_GENRES] = {
		BURN_CDTEXT_GENRE_LIST
	};
	char *msg = NULL;
	
	ret = v07t_hexcode(payload, 1);
	if(ret >= 0)
		return ret;
	for (i= 0; i < BURN_CDTEXT_NUM_GENRES; i++)
		if (strcmp(genres[i], payload) == 0)
			return i;

	BURN_ALLOC_MEM(msg, char, 160);
	sprintf(msg, "Unknown v07t Genre Code '%.80s'", payload);
	libdax_msgs_submit(libdax_messenger, -1, 0x00020191,
				LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
				v07t_printify(msg), 0, 0);
	ret = -1;
ex:;
	BURN_FREE_MEM(msg);
	return ret;
}


static int v07t_cdtext_len_db(char *payload, int *char_code,
				int *length, int *double_byte, int flag)
{
	if (*char_code < 0)
		*char_code = 0x00;
	*double_byte = (*char_code == 0x80);
	*length = strlen(payload) + 1 + *double_byte;
	return 1;
}


static int v07t_cdtext_to_session(struct burn_session *session, int block,
				char *payload, int *char_code, int pack_type,
				char *pack_type_name, int flag)
{
	int length, double_byte, ret;

	ret = v07t_cdtext_len_db(payload, char_code, &length, &double_byte, 0);
	if (ret <= 0)
		return ret;
	ret = burn_session_set_cdtext(session, block, pack_type,
			pack_type_name, (unsigned char *) payload, length,
			double_byte);
	return ret;
}


static int v07t_cdtext_to_track(struct burn_track *track, int block,
			char *payload, int *char_code, int pack_type,
			char *pack_type_name, int flag)
{
	int length, double_byte, ret;

	ret = v07t_cdtext_len_db(payload, char_code, &length, &double_byte, 0);
	if (ret <= 0)
		return ret;
	ret = burn_track_set_cdtext(track, block, pack_type, pack_type_name,
			(unsigned char *) payload, length, double_byte);
	return ret;
}


/** Read a line from fp and strip LF or CRLF */
static char *sfile_fgets(char *line, int maxl, FILE *fp)
{
	int l;
	char *ret;

	ret = fgets(line, maxl, fp);
	if (ret == NULL)
		return NULL;
	l = strlen(line);
	if (l > 0)
		if (line[l - 1] == '\r')
			line[--l] = 0;
	if (l > 0)
		if (line[l - 1] == '\n')
			line[--l] = 0;
	if(l > 0)
		if(line[l - 1] == '\r')
			line[--l] = 0;
	return ret;
}


int burn_session_input_sheet_v07t(struct burn_session *session,
					char *path, int block, int flag)
{
	int ret = 0, num_tracks, char_codes[8], copyrights[8], languages[8], i;
	int genre_code = -1, track_offset = 1, length, pack_type, tno;
	int session_attr_seen[16], track_attr_seen[16];
	int int0x00 = 0x00, int0x01 = 0x01;
	struct stat stbuf;
	FILE *fp = NULL;
	char *line = NULL, *eq_pos, *payload, *genre_text, track_txt[3];
	char *msg = NULL;
	struct burn_track **tracks;

	BURN_ALLOC_MEM(msg, char, 4096);
	BURN_ALLOC_MEM(line, char, 4096);
	BURN_ALLOC_MEM(genre_text, char, 160);

	for (i = 0; i < 8; i++)
		char_codes[i] = copyrights[i] = languages[i]= -1;
	for (i = 0; i < 16; i++)
		session_attr_seen[i] = track_attr_seen[i] = 0;
	genre_text[0] = 0;

	tracks = burn_session_get_tracks(session, &num_tracks);
	if (stat(path, &stbuf) == -1) {
cannot_open:;
		sprintf(msg, "Cannot open CD-TEXT input sheet v07t '%.4000s'",
			path);
		libdax_msgs_submit(libdax_messenger, -1, 0x00020193,
				LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
				v07t_printify(msg), errno, 0);
		ret = 0; goto ex;
	}
	if (!S_ISREG(stbuf.st_mode)) {
		sprintf(msg,
		  "File is not of usable type: CD-TEXT input sheet v07t '%s'",
			path);
		libdax_msgs_submit(libdax_messenger, -1, 0x00020193,
				LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
				v07t_printify(msg), 0, 0);
		ret = 0; goto ex;
	}

	fp = fopen(path, "rb");
	if (fp == NULL)
		goto cannot_open;

	while (1) {
		if (sfile_fgets(line, 4095, fp) == NULL) {
			if (!ferror(fp))
	break;
			sprintf(msg,
	"Cannot read all bytes from  CD-TEXT input sheet v07t '%.4000s'",
				path);
			libdax_msgs_submit(libdax_messenger, -1, 0x00020193,
				LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
				v07t_printify(msg), 0, 0);
			ret = 0; goto ex;
		}
		if (strlen(line) == 0)
	continue;
		eq_pos = strchr(line, '=');
		if (eq_pos == NULL) {
			sprintf(msg,
		"CD-TEXT v07t input sheet line without '=' : '%.4000s'",
				line);
			libdax_msgs_submit(libdax_messenger, -1, 0x00020194,
				LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
				v07t_printify(msg), 0, 0);
			ret = 0; goto ex;
		}
		for (payload = eq_pos + 1; *payload == 32 || *payload == 9;
		     payload++);
		*eq_pos = 0;
		for (eq_pos--;
		     (*eq_pos == 32 || *eq_pos == 9) && eq_pos > line;
		     eq_pos--)
			*eq_pos= 0;

		if (payload[0] == 0)
	continue;

		if (strcmp(line, "Text Code") == 0) {
			ret = v07t_cdtext_char_code(payload, 0);
			if (ret < 0)
				goto ex;
			if (char_codes[block] >= 0 &&
			    char_codes[block] != ret) {
				libdax_msgs_submit(libdax_messenger, -1,
					0x00020192, LIBDAX_MSGS_SEV_FAILURE,
					LIBDAX_MSGS_PRIO_HIGH,
					"Unexpected v07t Text Code change",
					0, 0);
				ret = 0; goto ex;
			}
			char_codes[block] = ret;

		} else if (strcmp(line, "Language Code") == 0) {
			ret = v07t_cdtext_lang_code(payload, 0);
			if(ret < 0)
				goto ex;
			languages[block] = ret;

		} else if (strcmp(line, "0x80") == 0 ||
				strcmp(line, "Album Title") == 0) {
			ret = v07t_cdtext_to_session(session, block, payload,
					char_codes + block, 0, "TITLE", 0);
			if (ret <= 0)
				goto ex;
			session_attr_seen[0x0] = 1;

		} else if (strcmp(line, "0x81") == 0 ||
				strcmp(line, "Artist Name") == 0) {
			ret = v07t_cdtext_to_session(session, block, payload,
					char_codes + block, 0, "PERFORMER", 0);
			if (ret <= 0)
				goto ex;
			session_attr_seen[0x1] = 1;

		} else if (strcmp(line, "0x82") == 0 ||
				strcmp(line, "Songwriter") == 0) {
			ret = v07t_cdtext_to_session(session, block, payload,
					char_codes + block, 0, "SONGWRITER",
					0);
			if (ret <= 0)
				goto ex;
			session_attr_seen[0x2] = 1;

		} else if (strcmp(line, "0x83") == 0 ||
				strcmp(line, "Composer") == 0) {
			ret = v07t_cdtext_to_session(session, block, payload,
					char_codes + block, 0, "COMPOSER", 0);
			if (ret <= 0)
				goto ex;
			session_attr_seen[0x3] = 1;

		} else if (strcmp(line, "0x84") == 0 ||
				 strcmp(line, "Arranger") == 0) {
			ret = v07t_cdtext_to_session(session, block, payload,
					char_codes + block, 0, "ARRANGER", 0);
			if (ret <= 0)
				goto ex;
			session_attr_seen[0x4] = 1;

		} else if (strcmp(line, "0x85") == 0 ||
				strcmp(line, "Album Message") == 0) {
			ret = v07t_cdtext_to_session(session, block, payload,
					char_codes + block, 0, "MESSAGE", 0);
			if (ret <= 0)
				goto ex;
			session_attr_seen[0x5] = 1;

		} else if (strcmp(line, "0x86") == 0 ||
				      strcmp(line, "Catalog Number") == 0) {
			ret = v07t_cdtext_to_session(session, block, payload,
					&int0x01, 0, "DISCID", 0);
			if(ret <= 0)
				goto ex;

		} else if (strcmp(line, "Genre Code") == 0) {
			genre_code = v07t_cdtext_genre_code(payload, 0);
			if (genre_code < 0) {
				ret = 0; goto ex;
			}

		} else if (strcmp(line, "Genre Information") == 0) {
			strncpy(genre_text, payload, 159);
			genre_text[159] = 0;

		} else if (strcmp(line, "0x8d") == 0 ||
				strcmp(line, "Closed Information") == 0) {
			ret = v07t_cdtext_to_session(session, block, payload,
					&int0x00, 0, "CLOSED", 0);
			if (ret <= 0)
				goto ex;

		} else if(strcmp(line, "0x8e") == 0 ||
				strcmp(line, "UPC / EAN") == 0) {
			ret = v07t_cdtext_to_session(session, block, payload,
					&int0x01, 0, "UPC_ISRC", 0);
			if (ret <= 0)
				goto ex;
			session_attr_seen[0xe] = 1;

		} else if (strncmp(line, "Disc Information ", 17) == 0) {

			/* >>> ??? is this good for anything ? */;

		} else if (strcmp(line, "Input Sheet Version") == 0) {
			if (strcmp(payload, "0.7T") != 0) {
				sprintf(msg,
		"Wrong Input Sheet Version '%.4000s'. Expected '0.7T'.",
					payload);
				libdax_msgs_submit(libdax_messenger, -1,
					0x00020194, LIBDAX_MSGS_SEV_FAILURE,
					LIBDAX_MSGS_PRIO_HIGH,
					v07t_printify(msg), 0, 0);
				ret = 0; goto ex;
			}

		} else if (strcmp(line, "Remarks") == 0) {
			;

		} else if (strcmp(line, "Text Data Copy Protection") == 0) {
			ret = v07t_hexcode(payload, 0);
			if (ret >= 0)
				copyrights[block] = ret;
			else if (strcmp(payload, "ON") == 0)
				copyrights[block] = 0x03;
			else if (strcmp(payload, "OFF") == 0)
				copyrights[block] = 0x00;
			else {
				sprintf(msg,
			   "Unknown v07t Text Data Copy Protection '%.4000s'",
				payload);
				libdax_msgs_submit(libdax_messenger, -1,
					0x00020191, LIBDAX_MSGS_SEV_FAILURE,
					LIBDAX_MSGS_PRIO_HIGH,
					v07t_printify(msg), 0, 0);

				ret = 0; goto ex;
			}

		} else if (strcmp(line, "First Track Number") == 0) {
			ret = -1;
			sscanf(payload, "%d", &ret);
			if (ret <= 0 || ret > 99) {
bad_tno:;
				sprintf(msg,
	 		"Inappropriate v07t First Track Number '%.4000s'",
					payload);
				libdax_msgs_submit(libdax_messenger, -1,
					0x00020194, LIBDAX_MSGS_SEV_FAILURE,
					LIBDAX_MSGS_PRIO_HIGH,
					v07t_printify(msg), 0, 0);
				ret = 0; goto ex;
			} else {
				track_offset = ret;
				if (ret != 1) {
					sprintf(msg,
				"First Track Number '%s' will be mapped to 1",
						payload);
					libdax_msgs_submit(libdax_messenger,-1,
					  0x00020195, LIBDAX_MSGS_SEV_WARNING,
					  LIBDAX_MSGS_PRIO_HIGH,
					  v07t_printify(msg), 0, 0);
				}
			}

		} else if (strcmp(line, "Last Track Number") == 0) {
			ret = -1;
			sscanf(payload, "%d", &ret);
			if (ret < 0) {
				goto bad_tno;
			} else {

				/* >>> ??? Is it good for anything ? */;

			}

		} else if (strncmp(line, "Track ", 6) == 0) {
			tno = -1;
			sscanf(line + 6, "%d", &tno);
			if (tno < 0 || tno - track_offset < 0 ||
				tno - track_offset >= num_tracks) {
				track_txt[0] = line[6];
				track_txt[1] = line[7];
				track_txt[2] = 0;
bad_track_no:;
				if (track_offset != 1)
					sprintf(msg,
		"Inappropriate v07t Track number '%.3900s' (mapped to %2.2d)",
					   track_txt, tno - track_offset + 1);
				else
				  sprintf(msg,
				   "Inappropriate v07t Track number '%.3900s'",
					track_txt);
				sprintf(msg + strlen(msg),
				    "  (acceptable range: %2.2d to %2.2d)",
				    track_offset,
				    num_tracks + track_offset - 1);
				libdax_msgs_submit(libdax_messenger, -1,
					  0x00020194, LIBDAX_MSGS_SEV_FAILURE,
					  LIBDAX_MSGS_PRIO_HIGH,
					  v07t_printify(msg), 0, 0);
				ret = 0; goto ex;
			}
			tno -= track_offset;

			if (strcmp(line, "0x80") == 0 ||
			    strcmp(line + 9, "Title") == 0)
				pack_type = 0x80;
			else if (strcmp(line + 9, "0x81") == 0 ||
					strcmp(line + 9, "Artist") == 0)
				pack_type = 0x81;
			else if (strcmp(line + 9, "0x82") == 0 ||
					strcmp(line + 9, "Songwriter") == 0)
				pack_type = 0x82;
			else if (strcmp(line + 9, "0x83") == 0 ||
					strcmp(line + 9, "Composer") == 0)
				pack_type = 0x83;
			else if (strcmp(line + 9, "0x84") == 0 ||
					strcmp(line + 9, "Arranger") == 0)
				pack_type = 0x84;
			else if (strcmp(line + 9, "0x85") == 0 ||
					strcmp(line + 9, "Message") == 0)
				pack_type = 0x85;
			else if (strcmp(line + 9, "0x8e") == 0 ||
					strcmp(line + 9, "ISRC") == 0)
				pack_type = 0x8e;
			else {
				sprintf(msg,
				   "Unknown v07t Track purpose specifier '%s'",
				       line + 9);
				libdax_msgs_submit(libdax_messenger, -1,
					  0x00020191, LIBDAX_MSGS_SEV_FAILURE,
					  LIBDAX_MSGS_PRIO_HIGH,
					  v07t_printify(msg), 0, 0);
				ret = 0; goto ex;
			}
			ret = v07t_cdtext_to_track(tracks[tno], block, payload,
						&int0x00, pack_type, "", 0);
			if (ret <= 0)
				goto ex;
			track_attr_seen[pack_type - 0x80] = 1;

		} else if (strncmp(line, "ISRC ", 5) == 0) {
			/* Track variation of UPC EAN = 0x8e */
			tno = -1;
			sscanf(line + 5, "%d", &tno);
			if (tno < 0 || tno - track_offset < 0 ||
				tno - track_offset >= num_tracks) {
				track_txt[0] = line[5];
				track_txt[1] = line[6];
				track_txt[2] = 0;
				goto bad_track_no;
			}
			tno -= track_offset;
			ret = v07t_cdtext_to_track(tracks[tno], block, payload,
						&int0x00, 0x8e, "", 0);
			if (ret <= 0)
				goto ex;
			track_attr_seen[0xe] = 1;

		} else {
			sprintf(msg,
				"Unknown v07t purpose specifier '%.4000s'",
				line);
			libdax_msgs_submit(libdax_messenger, -1, 0x00020191,
				LIBDAX_MSGS_SEV_FAILURE, LIBDAX_MSGS_PRIO_HIGH,
				v07t_printify(msg), 0, 0);
			ret = 0; goto ex;
		}
	}

	for (i= 0x80; i <= 0x8e; i++) {
		if (i > 0x85 && i != 0x8e)
	continue;
		if (session_attr_seen[i - 0x80] || !track_attr_seen[i - 0x80])
	continue;
		ret = v07t_cdtext_to_session(session, block, "",
					char_codes + block, i, NULL, 0);
		if (ret <= 0)
			goto ex;
	}
	if (genre_code >= 0 && genre_text[0]) {
		line[0] = (genre_code >> 8) & 0xff;
		line[1] = genre_code & 0xff;
		strcpy(line + 2, genre_text);
		length = 2 + strlen(line + 2) + 1;
		ret = burn_session_set_cdtext(session, block, 0, "GENRE",
					(unsigned char *) line, length, 0);
		if (ret <= 0)
			goto ex;
	}
	ret = burn_session_set_cdtext_par(session, char_codes, copyrights,
								languages, 0);
	if (ret <= 0)
		goto ex;

	ret = 1;
ex:;
	if(fp != NULL)
		fclose(fp);
	BURN_FREE_MEM(genre_text);
	BURN_FREE_MEM(line);
	BURN_FREE_MEM(msg);
	return ret;
}

