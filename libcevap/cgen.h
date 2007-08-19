
#ifndef Cgen_includeD
#define Cgen_includeD Yes



struct CgeN {

 char *classname;
 char *structname;
 char *functname;

 int is_managed_list;
 int is_bossless_list;
 int gen_for_stic; /* 0=no smem,srgex,sfile , 1=all three, 2=smem only */
 int make_ansi;
 int make_lowercase;
 char global_include_file[4096];
 FILE *global_include_fp;

 struct CtyP *elements;
 struct CtyP *last_element;

 int may_overwrite;
 FILE *fp;
 char filename[4096];
 FILE *ptt_fp;
 char ptt_filename[4096];

 char msg[8192];
};


#endif /* Cgen_includeD */

