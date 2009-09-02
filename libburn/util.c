#include <string.h>

/* ts A61008 */
/* #include <a ssert.h> */

#include <stdlib.h>
#include <stdio.h>

/* ts A80914 : This is unneeded. Version info comes from libburn.h.
#include "v ersion.h"
*/

#include "util.h"
#include "libburn.h"

char *burn_strdup(char *s)
{
	char *ret;
	int l;

	/* ts A61008 */
	/* a ssert(s); */
	if (s == NULL)
		return NULL;

	l = strlen(s) + 1;
	ret = malloc(l);
	memcpy(ret, s, l);

	return ret;
}

char *burn_strndup(char *s, int n)
{
	char *ret;
	int l;

	/* ts A61008 */
	/* a ssert(s); */
	/* a ssert(n > 0); */
	if (s == NULL || n <= 0)
		return NULL;

	l = strlen(s);
	ret = malloc(l < n ? l : n);

	memcpy(ret, s, l < n - 1 ? l : n - 1);
	ret[n - 1] = '\0';

	return ret;
}

void burn_version(int *major, int *minor, int *micro)
{
/* ts A80408 : switched from configure.ac versioning to libburn.h versioning */
	*major = burn_header_version_major;
	*minor = burn_header_version_minor;
	*micro = burn_header_version_micro;
}


struct mid_record {
	char *manufacturer;
	int m_li;
	int s_li;
	int f_li;
	int m_lo;
	int s_lo;
	int f_lo;
	char *other_brands;
};
typedef struct mid_record mid_record_t;


/* ts A90902 */
/** API
   @param flag  Bitfield for control purposes,
                bit0= append "(aka %s)",other_brands to reply
 */
char *burn_guess_cd_manufacturer(int m_li, int s_li, int f_li,
                                 int m_lo, int s_lo, int f_lo, int flag)
{
	static mid_record_t mid_list[]= {
	{"SKC",                                 96, 40,  0,  0, 0, 0, ""},
	{"Ritek Corp"                         , 96, 43, 30,  0, 0, 0, ""},
	{"TDK / Ritek"                        , 97, 10,  0,  0, 0, 0, "TRAXDATA"},
	{"TDK Corporation"                    , 97, 15,  0,  0, 0, 0, ""},
	{"Ritek Corp"                         , 97, 15, 10,  0, 0, 0, "7-plus, Aopen, PONY, Power Source, TDK, TRAXDATA, HiCO, PHILIPS, Primdisc, Victor.JVC, OPTI STORAGE, Samsung"},
	{"Mitsubishi Chemical Corporation"    , 97, 15, 20,  0, 0, 0, ""},
	{"Nan-Ya Plastics Corporation"        , 97, 15, 30,  0, 0, 0, "Hatron, MMore, Acer, LITEON"},
	{"Delphi"                             , 97, 15, 50,  0, 0, 0, ""},
	{"Shenzhen SG&SAST"                   , 97, 16, 20,  0, 0, 0, ""},
	{"Moser Baer India Limited"           , 97, 17,  0,  0, 0, 0, "EMTEC, Intenso, YAKUMO, PLATINUM, Silver Circle"},
	{"SKY media Manufacturing SA"         , 97, 17, 10,  0, 0, 0, ""},
	{"Wing"                               , 97, 18, 10,  0, 0, 0, ""},
	{"DDT"                                , 97, 18, 20,  0, 0, 0, ""},
	{"Daxon Technology Inc. / Acer"       , 97, 22, 60,  0, 0, 0, "Maxmax, Diamond Data, BenQ, gold, SONY"},
	{"Taiyo Yuden Company Limited"        , 97, 24,  0,  0, 0, 0, "Maxell, FUJIFILM, SONY"},
	{"Sony Corporation"                   , 97, 24, 10,  0, 0, 0, "LeadData, Imation"},
	{"Computer Support Italcard s.r.l"    , 97, 24, 20,  0, 0, 0, ""},
	{"Unitech Japan Inc."                 , 97, 24, 30,  0, 0, 0, ""},
	{"MPO, France"                        , 97, 25,  0,  0, 0, 0, ""},
	{"Hitachi Maxell Ltd."                , 97, 25, 20,  0, 0, 0, ""},
	{"Infodisc Technology Co,Ltd."        , 97, 25, 30,  0, 0, 0, "MEMOREX, SPEEDA, Lead data"},
	{"Xcitec"                             , 97, 25, 60,  0, 0, 0, ""},
	{"Fornet International Pte Ltd"       , 97, 26,  0,  0, 0, 0, "COMPUSA, Cdhouse"},
	{"Postech Corporation"                , 97, 26, 10,  0, 0, 0, "Mr.Platinum"},
	{"SKC Co Ltd."                        , 97, 26, 20,  0, 0, 0, "Infinite"},
	{"Fuji Photo Film Co,Ltd."            , 97, 26, 40,  0, 0, 0, ""},
	{"Lead Data Inc."                     , 97, 26, 50,  0, 0, 0, "SONY, Gigastorage, MIRAGE"},
	{"CMC Magnetics Corporation"          , 97, 26, 60,  0, 0, 0, "Daxon, Verbatim, Memorex, Bi-Winner, PLEXTOR, YAMAHA, Melody, Office DEPOT, Philips, eMARK, imation, HyperMedia, Samsung, Shintaro, Techworks"},
	{"Ricoh Company Limited"              , 97, 27,  0,  0, 0, 0, "Sony, Digital Storage, Csita"},
	{"Plasmon Data Systems Ltd"           , 97, 27, 10,  0, 0, 0, "Ritek, TDK, EMTEC, ALPHAPET, MANIA"},
	{"Princo Corporation"                 , 97, 27, 20,  0, 0, 0, ""},
	{"Pioneer"                            , 97, 27, 30,  0, 0, 0, ""},
	{"Eastman Kodak Company"              , 97, 27, 40,  0, 0, 0, ""},
	{"Mitsui Chemicals Inc."              , 97, 27, 50,  0, 0, 0, "MAM-A, TDK"},
	{"Ricoh Company Limited"              , 97, 27, 60,  0, 0, 0, "Ritek"},
	{"Gigastorage Corporation"            , 97, 28, 10,  0, 0, 0, "MaxMax, Nan-Ya"},
	{"Multi Media Masters&Machinary SA"   , 97, 28, 20,  0, 0, 0, "King, Mmirex"},
	{"Ritek Corp"                         , 97, 31,  0,  0, 0, 0, "TDK"},
	{"Grand Advance Technology Sdn. Bhd." , 97, 31, 30,  0, 0, 0, ""},
	{"TDK Corporation"                    , 97, 32, 00,  0, 0, 0, ""},
	{"Prodisc Technology Inc."            , 97, 32, 10,  0, 0, 0, "Smartbuy, Mitsubishi, Digmaster, LG, Media Market"},
	{"Mitsubishi Chemical Corporation"    , 97, 34, 20,  0, 0, 0, "YAMAHA, Verbatim"},
	{"", 0, 0, 0, 0, 0, 0, ""}
	};

	int i, f_li_0;
	char buf[1024];
	char *result = NULL;

	f_li_0 = f_li - (f_li % 10);
	for (i = 0; mid_list[i].manufacturer[0]; i++) {
		if (m_li == mid_list[i].m_li &&
		    s_li == mid_list[i].s_li &&
		    (f_li_0 == mid_list[i].f_li || f_li == mid_list[i].f_li))
	break;
	}
	if (mid_list[i].manufacturer[0] == 0) {
		sprintf(buf, "Unknown CD manufacturer. Please report code '%2.2dm%2.2ds%2.2df-%2.2dm%2.2ds%2.2df', human readable brand, size, and speed to scdbackup@gmx.net.", m_li, s_li, f_li, m_lo, s_lo, f_lo);
		result = strdup(buf);
		return result;
	}

	/* Compose, allocate and copy result */
	if ((flag & 1) && mid_list[i].other_brands[0]) {
		sprintf(buf, "%s  (aka %s)",
			mid_list[i].manufacturer, mid_list[i].other_brands);
		result = strdup(buf);
	} else
		result = strdup(mid_list[i].manufacturer);
	return result;
}

