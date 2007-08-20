
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>


#include "smem.h"

char *Sfile_fgets();
int Sregex_string();
int Sregex_trimline();


#include "ctyp.h"

#include "cgen.h"



/* ----------------------------- CgeN ------------------------- */

int Cgen_new(cgen,flag)
struct CgeN **cgen;
int flag;
{
 int ret;
 struct CgeN *c;

 *cgen= c= TSOB_FELD(struct CgeN,1);
 if(c==NULL) {
   fprintf(stderr,"+++ Cannot create cgen object : %s\n",strerror(errno));
   return(-1);
 }
 c->classname= NULL;
 c->structname= NULL;
 c->functname= NULL;
 c->is_managed_list= 0;
 c->is_bossless_list= 0;
 c->gen_for_stic= 1;
 c->make_ansi= 0;
 c->make_lowercase= 0;
 c->global_include_file[0]= 0;
 c->global_include_fp= NULL;
 c->elements= NULL;
 c->last_element= NULL;
 c->may_overwrite= 0;
 c->fp= NULL;
 c->filename[0]= 0;
 c->ptt_fp= NULL;
 c->ptt_filename[0]= 0;
 c->msg[0]= 0;
 return(1);
}


int Cgen_destroy(cgen,flag)
struct CgeN **cgen;
int flag;
{
 struct CgeN *c;
 struct CtyP *ct,*next_ct;
 
 c= *cgen;
 if(c==NULL)
   return(0);

 if(c->fp!=NULL)
   fclose(c->fp);
 if(c->ptt_fp!=NULL)
   fclose(c->ptt_fp);
 Sregex_string(&(c->classname),NULL,0);
 Sregex_string(&(c->structname),NULL,0);
 Sregex_string(&(c->functname),NULL,0);
 for(ct= c->elements; ct!=NULL; ct= next_ct) {
   next_ct= ct->next;
   Ctyp_destroy(&ct,0);
 }

 free((char *) c);
 *cgen= NULL; 
 return(1);
}


int Cgen_make_names(cgen,flag)
struct CgeN *cgen;
int flag;
{
 int l;

 if(Sregex_string(&(cgen->structname),cgen->classname,0)<=0)
   return(-1);
 if(Sregex_string(&(cgen->functname),cgen->classname,0)<=0)
   return(-1);
 if(!cgen->make_lowercase) { 
   cgen->structname[0]= toupper(cgen->structname[0]);
   l= strlen(cgen->structname);
   cgen->structname[l-1]= toupper(cgen->structname[l-1]);
   cgen->functname[0]= toupper(cgen->functname[0]);
 }
 return(1);
}


int Cgen_read_fp(cgen,fp,flag)
struct CgeN *cgen;
FILE *fp;
int flag;
/*
 bit0= return 0 if eof at classname
*/
{
 char line[4096],*cpt,*bpt;
 int ret;

 line[0]= 0;
 while(1) {
   printf("[-list] classname ?\n");
   if(Sfile_fgets(line,sizeof(line)-1,fp)==NULL) {
     if(!(flag&1))
       return(2);
no_name:;
     sprintf(cgen->msg,"No classname given.");
     return(0);
   }
   printf("%s\n",line);
   if(strcmp(line,"@@@")==0)
     return(2);

   if(line[0]==0 || line[0]=='#') {

     /* >>> record class comments */;

   } else
 break;
 }
 cpt= line;
 while(cpt[0]=='-') {
    /* look for management specifiers:
       -l*   listable by prev-next chain
    */
   if(cpt[1]=='l' || cpt[1]=='L') {
     cgen->is_managed_list= 1;
   } else if(cpt[1]=='b' || cpt[1]=='B') {
     cgen->is_bossless_list= 1;
   }
   while(*cpt!=0 && !isspace(*cpt)) cpt++;
   while(*cpt!=0 && isspace(*cpt)) cpt++;
   if(*cpt==0)
     goto no_name; 
 }
 if(Sregex_string(&(cgen->classname),cpt,0)<=0)
   return(-1);
 ret= Cgen_make_names(cgen,0);
 if(ret<=0)
   return(ret);

 while(1) {
   ret= Ctyp_read_fp(&(cgen->last_element),fp,cgen->msg,
                     !!cgen->make_lowercase);
   if(ret<=0)
     return(ret);
   if(ret==2)
 break;
   if(cgen->elements==NULL)
     cgen->elements= cgen->last_element;
 }
 if(cgen->is_managed_list) {
   sprintf(line,"-c struct %s *prev",cgen->structname);
   ret= Ctyp_new_from_line(&(cgen->last_element),cgen->last_element,
                           line,cgen->msg,0);
   if(ret<=0)
     return(ret);
   if(cgen->elements==NULL)
     cgen->elements= cgen->last_element;
   sprintf(line,"-c struct %s *next",cgen->structname);
   ret= Ctyp_new_from_line(&(cgen->last_element),cgen->last_element,
                           line,cgen->msg,0);
   if(ret<=0)
     return(ret);
 }
 return(1);
}


int Cgen_open_wfile(cgen,flag)
struct CgeN *cgen;
int flag;
/*
 bit0-3: modes
  0= open cgen->fp
  1= open cgen->ptt_fp
  2= open cgen->global_include_fp
*/
{
 struct stat stbuf;
 int ret, mode;
 char *name, fmode[4];
 FILE *fp;

 mode= flag&15;
 strcpy(fmode,"w");
 if(mode==0) {
   name= cgen->filename;
   fp= cgen->fp; 
   cgen->fp= NULL;
 } else if(mode==1) {
   name= cgen->ptt_filename;
   fp= cgen->ptt_fp; 
   cgen->ptt_fp= NULL;
 } else if(mode==2) {
   strcpy(fmode,"a");
   name= cgen->global_include_file;
   fp= cgen->global_include_fp; 
   cgen->global_include_fp= NULL;
 } else {
   fprintf(stderr,"+++ Cgen_open_wfile : program error : unknown mode %d\n",
           mode);
   ret= -1; goto ex;
 }
 if(fmode[0]=='w' && stat(name,&stbuf)!=-1 && !cgen->may_overwrite) {
   sprintf(cgen->msg,"File '%s' already existing.",name);
   ret= 0; goto ex;
 }
 if(fp!=NULL)
   {fclose(fp); fp= NULL;}
 fp= fopen(name,fmode);
 if(fp==NULL) {
   sprintf(cgen->msg,"Cannot open file '%s' in %s-mode. %s",
                     name,fmode,strerror(errno));
   ret= 0; goto ex;
 }
 ret= 1;
ex:;
 if(mode==0)
   cgen->fp= fp;
 else if(mode==1)
   cgen->ptt_fp= fp;
 else if(mode==2)
   cgen->global_include_fp= fp;
 return(ret);
}


int Cgen_write_datestr(cgen,flag)
struct CgeN *cgen;
int flag;
/*
 bit0= operate on ptt (= ANSI prototype) file rather than on internal header
*/
{
 time_t t0;
 char timetext[81];
 FILE *fp;

 if(flag&1)
   fp= cgen->ptt_fp;
 else
   fp= cgen->fp;
 t0= time(0);
 strftime(timetext,sizeof(timetext),"%a, %d %b %Y %H:%M:%S GMT",
                                    gmtime(&t0));
 fprintf(fp,"/* ( derived from stub generated by CgeN on  %s ) */\n",
         timetext);
 return(1);
}


int Cgen_write_h(cgen,flag)
struct CgeN *cgen;
int flag;
{
 int ret,i,pointer_level;
 FILE *fp= NULL;
 struct CtyP *ct;
 char pvt[16],macro_name[4096],*cpt;

 if(cgen->make_ansi) {
   sprintf(cgen->filename,"%s_private.h",cgen->classname);
   strcpy(pvt,"_private");
 } else {
   sprintf(cgen->filename,"%s.h",cgen->classname);
   strcpy(pvt,"");
 }

 ret= Cgen_open_wfile(cgen,0);
 if(ret<=0)
   goto ex;
 sprintf(macro_name,"%s%s_includeD",cgen->functname,pvt);
 macro_name[0]= toupper(macro_name[0]);
 fp= cgen->fp;

 /* >>> print class comments */;

 fprintf(fp,"\n");
 fprintf(fp,"#ifndef %s\n",macro_name);
 fprintf(fp,"#define %s\n",macro_name);
 fprintf(fp,"\n");
 if(strlen(cgen->global_include_file)!=0) {
   fprintf(fp,"#include \"%s\"\n",cgen->global_include_file);
   fprintf(fp,"\n\n");
 }
 if(cgen->make_ansi)
   fprintf(fp,"/* For function prototypes see file %s.h */\n",cgen->classname);
 fprintf(fp,"\n\n");
 fprintf(fp,"struct %s {\n",cgen->structname);
 fprintf(fp,"\n");
 ct= cgen->elements;
 for(ct= cgen->elements;ct!=NULL;ct= ct->next) {

   if(ct->is_comment) {
     if(ct->name[0]==0) {
       fprintf(fp,"\n");
 continue;
     }
     fprintf(fp," /* ");
     for(cpt= ct->name; *cpt!=0; cpt++) {
       fprintf(fp,"%c",*cpt);
       if(cpt[0]=='*' && cpt[1]=='/')
         fprintf(fp," ");
     }
     fprintf(fp," */\n");
 continue;
   }

   if(ct->is_volatile)
     fprintf(fp," volatile");
   if(Ctyp_is_struct(ct,0))
     fprintf(fp," struct");
   else if(ct->is_unsigned)
     fprintf(fp," unsigned");
   fprintf(fp," %s ",ct->dtype);
   pointer_level= Ctyp_get_pointer_level(ct,0);
   for(i=0;i<pointer_level;i++)
     fprintf(fp,"*");
   fprintf(fp,"%s;\n",ct->name);
 }
 fprintf(fp,"\n");
 fprintf(fp,"};\n");
 fprintf(fp,"\n");
 fprintf(fp,"\n");
 fprintf(fp,"#endif /* %s */\n",macro_name);
 fprintf(fp,"\n");
 Cgen_write_datestr(cgen,0);

 /* Eventually write start of ANSI prototype include file */
 if(!cgen->make_ansi)
   goto after_ansi_h;
 sprintf(cgen->ptt_filename,"%s.h",cgen->classname);
 ret= Cgen_open_wfile(cgen,1);
 if(ret<=0)
   goto ex;
 sprintf(macro_name,"%s_includeD",cgen->functname);
 macro_name[0]= toupper(macro_name[0]);
 fp= cgen->ptt_fp;

 /* >>> print class comments */;

 fprintf(fp,"\n");
 fprintf(fp,"#ifndef %s\n",macro_name);
 fprintf(fp,"#define %s\n",macro_name);
 fprintf(fp,"\n\n");
 if(strlen(cgen->global_include_file)!=0) {
   fprintf(fp,"#include \"%s\"\n",cgen->global_include_file);
 } else {
   fprintf(fp,"struct %s;\n",cgen->structname);
 }
 fprintf(fp,"\n\n");
 fprintf(fp,"/* For inner details see file %s_private.h */\n",cgen->classname);
 fprintf(fp,"\n\n");

after_ansi_h:;
 if(strlen(cgen->global_include_file)==0)
   goto after_global_include;
 ret= Cgen_open_wfile(cgen,2);
 if(ret<=0)
   goto ex;
 fprintf(cgen->global_include_fp,"struct %s;\n",cgen->structname);

after_global_include:;
 ret= 1;
ex:;
 if(cgen->fp!=NULL) 
   {fclose(cgen->fp); cgen->fp= NULL;}
 /* ( note: cgen->ptt_fp stays open ) */
 if(cgen->global_include_fp!=NULL) 
   {fclose(cgen->global_include_fp); cgen->global_include_fp= NULL;}
 return(ret);
}


int Cgen_write_to_ptt(cgen,ptt,flag)
struct CgeN *cgen;
char *ptt;
int flag;
{
 if(cgen->ptt_fp==NULL)
   return(-1);
 fprintf(cgen->ptt_fp,"%s;\n",ptt);
 return(1);
}


int Cgen_finish_public_h(cgen,flag)
struct CgeN *cgen;
int flag;
{
 char macro_name[4096];

 if(cgen->ptt_fp==NULL)
   return(-1);
 fprintf(cgen->ptt_fp,"\n");
 fprintf(cgen->ptt_fp,"\n");
 sprintf(macro_name,"%s_includeD",cgen->functname);
 macro_name[0]= toupper(macro_name[0]);
 fprintf(cgen->ptt_fp,"#endif /* %s */\n",macro_name);
 fprintf(cgen->ptt_fp,"\n");
 Cgen_write_datestr(cgen,1);
 if(cgen->ptt_fp!=NULL) 
   {fclose(cgen->ptt_fp); cgen->ptt_fp= NULL;}
 return(1);
}


int Cgen_write_c_head(cgen,flag)
struct CgeN *cgen;
int flag;
{
 int ret,is_pointer,is_struct,array_size;
 FILE *fp= NULL;
 struct CtyP *ct,*hct;
 char *dtype= NULL,*name= NULL;

 fp= cgen->fp;
 fprintf(fp,"\n");
 fprintf(fp,"/*\n");
 fprintf(fp,"  cc -g -o %s.c\n",cgen->classname);
 fprintf(fp,"*/\n");
 Cgen_write_datestr(cgen,0);
 fprintf(fp,"\n");
 fprintf(fp,"#include <sys/types.h>\n");
 fprintf(fp,"#include <stdlib.h>\n");
 fprintf(fp,"#include <stdio.h>\n");
 fprintf(fp,"#include <string.h>\n");
 fprintf(fp,"#include <errno.h>\n");
 fprintf(fp,"\n");
 fprintf(fp,"#include \"%s.h\"\n",cgen->classname);
 if(cgen->make_ansi) {
   fprintf(fp,"#include \"%s_private.h\"\n",cgen->classname);
 }
 fprintf(fp,"\n");
 for(ct= cgen->elements; ct!=NULL; ct= ct->next) {
   if(ct->is_comment)
 continue;
   Ctyp_get_dtype(ct,&dtype,0);
   Ctyp_get_type_mod(ct,&is_pointer,&is_struct,&array_size,0);
/*
   fprintf(stderr,"DEBUG:  %s  %s\n",(is_struct?"struct ":""),dtype);
*/
   /* already included ? */
   if(strcmp(dtype,cgen->structname)==0)
 continue;
   for(hct= cgen->elements; hct!=NULL && hct!=ct; hct= hct->next) {
     if(hct->is_comment)
   continue;
     if(hct->dtype!=NULL)
       if(strcmp(hct->dtype,dtype)==0)
   break;
   }
   if(hct!=ct && hct!=NULL)
 continue;

   if(is_struct && (isupper(dtype[0]) && isupper(dtype[strlen(dtype)-1]))) {
     dtype[0]= tolower(dtype[0]);
     dtype[strlen(dtype)-1]= tolower(dtype[strlen(dtype)-1]);
     fprintf(fp,"#include \"%s.h\"\n",dtype);
   }
 }
 fprintf(fp,"\n");
 if(cgen->gen_for_stic==1) {
   fprintf(fp,"#include \"../s_tools/smem.h\"\n");
   fprintf(fp,"#include \"../s_tools/sfile.h\"\n");
   fprintf(fp,"#include \"../s_tools/sregex.h\"\n");
   fprintf(fp,"\n");
 } else if(cgen->gen_for_stic==2) {
   fprintf(fp,"#include \"smem.h\"\n");
   fprintf(fp,"\n");
 }
 fprintf(fp,"\n");
 fprintf(fp,"/* --------------------------  %s  ----------------------- */\n",
            cgen->structname);
 fprintf(fp,"\n");

 if(dtype!=NULL)
   Sregex_string(&dtype,NULL,0);
 if(name!=NULL)
   Sregex_string(&name,NULL,0);
 return(1);
}


int Cgen_write_c_new(cgen,flag)
struct CgeN *cgen;
int flag;
{
 int ret,pointer_level,management,boss_parm= 0;
 unsigned long array_size;
 FILE *fp= NULL;
 struct CtyP *ct;
 char ptt[4096];

 fp= cgen->fp;

 if(!cgen->is_bossless_list) {
   if(cgen->elements!=NULL)
     if(strcmp(cgen->elements->name,"boss")==0 && cgen->elements->is_struct &&
        cgen->elements->is_pointer==1 && cgen->elements->no_initializer==0)
       boss_parm= 1;
   if(cgen->is_managed_list && boss_parm==0)
     fprintf(stderr,
       "+++ Warning: -l %s without -v struct ... *boss as first attribute\n",
       cgen->classname);
 }
 fprintf(fp,"\n");
 if(cgen->make_ansi) {
   sprintf(ptt,"int %s_new(struct %s **objpt, ",
           cgen->functname,cgen->structname);
   if(boss_parm)
     sprintf(ptt+strlen(ptt),"struct %s *boss, ",cgen->elements->dtype);
   sprintf(ptt+strlen(ptt),"int flag)");
   fprintf(fp,"%s\n",ptt);
   ret= Cgen_write_to_ptt(cgen, ptt, 0);
   if(ret<=0)
     return(ret);
 } else {
   fprintf(fp,"int %s_new(objpt,\n",cgen->functname);
   if(boss_parm)
     fprintf(fp,"boss,");
   fprintf(fp,"flag)\n");
   fprintf(fp,"struct %s **objpt;\n",cgen->structname);
   if(boss_parm)
     fprintf(fp,"struct %s *boss;",cgen->elements->dtype);
   fprintf(fp,"int flag;\n");
 }
 fprintf(fp,"{\n");
 fprintf(fp," struct %s *o;\n",cgen->structname);
 fprintf(fp,"\n");
 if(cgen->gen_for_stic)
   fprintf(fp," *objpt= o= TSOB_FELD(struct %s,1);\n",cgen->structname);
 else
   fprintf(fp," *objpt= o= (struct %s *) malloc(sizeof(struct %s));\n",
              cgen->structname, cgen->structname);
 fprintf(fp," if(o==NULL)\n");
 fprintf(fp,"   return(-1);\n");
 fprintf(fp,"\n");
 for(ct= cgen->elements; ct!=NULL; ct= ct->next) {
   if(ct->is_comment || ct->no_initializer)
 continue;
   array_size= Ctyp_get_array_size(ct,0);
   pointer_level= Ctyp_get_pointer_level(ct,0);
   if(ct==cgen->elements && boss_parm) {
     fprintf(fp," o->boss= boss;\n");
   } else if(array_size>0) {
     if(strcmp(ct->dtype,"char")==0) {
       fprintf(fp," o->%s[0]= 0;\n;",ct->name);
     } else if(pointer_level>0) {
       fprintf(fp," { int i;");
       fprintf(fp,"   for(i=0;i<array_size;i++)\n;");
       fprintf(fp,"     o->%s[i]= NULL;\n",ct->name);
       fprintf(fp," }");
     } else {
       fprintf(fp," { int i;");
       fprintf(fp,"   for(i=0;i<array_size;i++)\n;");
       fprintf(fp,"     o->%s[i]= 0;\n",ct->name);
       fprintf(fp," }");
     }  
   } else if(pointer_level>0) {
     fprintf(fp," o->%s= NULL;\n",ct->name);
   } else
     fprintf(fp," o->%s= 0;\n",ct->name);
 }
 fprintf(fp,"\n");
 fprintf(fp," return(1);\n");
 fprintf(fp,"/*\n");
 fprintf(fp,"failed:;\n");
 fprintf(fp," %s_destroy(objpt,0);\n",cgen->functname);
 fprintf(fp," return(-1);\n");
 fprintf(fp,"*/\n");
 fprintf(fp,"}\n");
 fprintf(fp,"\n");
 for(ct= cgen->elements; ct!=NULL; ct= ct->next) {
   if(ct->is_comment)
 continue;
   management= Ctyp_get_management(ct,0);
   if(management==4) {
     if(ct->next==NULL) {
no_last_pt:;
       sprintf(cgen->msg,
               "Lonely -l found. A -v of same type must follow.\nName is : %s",
               ct->name);
       return(0);
     }
     if(strcmp(ct->next->dtype,ct->dtype)!=0 
               || ct->next->is_pointer!=ct->is_pointer)
       goto no_last_pt;
     ct->next->with_getter= ct->next->with_setter= 0;
     ret= Cgen_write_c_new_type(cgen,ct,ct->next,0);
     if(ret<=0)
       return(ret);
   }
 }
 return(1);
}


int Cgen_write_c_new_type(cgen,ct_first,ct_last,flag)
struct CgeN *cgen;
struct CtyP *ct_first,*ct_last;
int flag;
{
 int ret,l,management,pointer_level,i;
 FILE *fp= NULL;
 char funct[4096],classname[4096],*npt,ptt[4096];

 strcpy(funct,ct_first->dtype);
 strcpy(classname,funct);
 l= strlen(funct);
 if(l>0) {
   if(cgen->make_lowercase)
     funct[0]= tolower(funct[0]);
   else
     funct[0]= toupper(funct[0]);
   funct[l-1]= tolower(funct[l-1]);
   classname[0]= tolower(funct[0]);
   classname[l-1]= funct[l-1];
 }
 fp= cgen->fp;
 fprintf(fp,"\n");
 if(cgen->make_ansi) {
   sprintf(ptt, "int %s_new_%s(struct %s *o, int flag)",
           cgen->functname,ct_first->name,cgen->structname);
   fprintf(fp,"%s\n",ptt);
   if(ct_first->with_setter) {
     ret= Cgen_write_to_ptt(cgen, ptt, 0);
     if(ret<=0)
       return(ret);
   }
 } else {
   fprintf(fp,"int %s_new_%s(o,flag)\n",cgen->functname,ct_first->name);
   fprintf(fp,"struct %s *o;\n",cgen->structname);
   fprintf(fp,"int flag;\n");
 }
 fprintf(fp,"{\n");
 fprintf(fp," int ret;\n");
 fprintf(fp," struct %s *c= NULL;\n",ct_first->dtype);
 fprintf(fp,"\n");
 if(ct_first->bossless_list)
   fprintf(fp," ret= %s_new(&c,0);\n",funct);
 else
   fprintf(fp," ret= %s_new(&c,o,0);\n",funct);
 fprintf(fp," if(ret<=0)\n");
 fprintf(fp,"   return(ret);\n");
 fprintf(fp," %s_link(c,o->%s,0);\n",funct,ct_last->name);
 fprintf(fp," o->%s= c;\n",ct_last->name);
 fprintf(fp," if(o->%s==NULL)\n",ct_first->name);
 fprintf(fp,"   o->%s= c;\n",ct_first->name);
 fprintf(fp," return(1);\n");
 fprintf(fp,"}\n");
 fprintf(fp,"\n");
 ret= 1;
ex:;
 return(ret);
}


int Cgen_write_c_destroy(cgen,flag)
struct CgeN *cgen;
int flag;
{
 int ret,l,management,pointer_level,i;
 FILE *fp= NULL;
 struct CtyP *ct,*next;
 char funct[4096],*npt,ptt[4096];

 fp= cgen->fp;
 fprintf(fp,"\n");
 if(cgen->make_ansi) {
   sprintf(ptt, "int %s_destroy(struct %s **objpt, int flag)",
           cgen->functname,cgen->structname);
   fprintf(fp,"%s\n",ptt);
   ret= Cgen_write_to_ptt(cgen, ptt, 0);
   if(ret<=0)
     return(ret);
 } else {
   fprintf(fp,"int %s_destroy(objpt,flag)\n",cgen->functname);
   fprintf(fp,"struct %s **objpt;\n",cgen->structname);
   fprintf(fp,"int flag;\n");
 }
 fprintf(fp,"{\n");
 fprintf(fp," struct %s *o;\n",cgen->structname);
 fprintf(fp,"\n");
 fprintf(fp," o= *objpt;\n");
 fprintf(fp," if(o==NULL)\n");
 fprintf(fp,"   return(0);\n");
 fprintf(fp,"\n");
 for(ct= cgen->elements; ct!=NULL; ct= ct->next) {
   if(ct->is_comment)
 continue;
   management= Ctyp_get_management(ct,0);
   if(management==1 || management==4) {
     strcpy(funct,ct->dtype);
     l= strlen(funct);
     if(l>0) {
       if(cgen->make_lowercase)
         funct[0]= tolower(funct[0]);
       else
         funct[0]= toupper(funct[0]);
       funct[l-1]= tolower(funct[l-1]);
     }
     if(strcmp(ct->dtype,"char")==0) {
       if(cgen->gen_for_stic==1)
         fprintf(fp," Sregex_string(");
       else if(cgen->gen_for_stic==2)
         fprintf(fp," Smem_freE((char *) ");
       else
         fprintf(fp," free(");
     } else if(strcmp(ct->dtype,"LstrinG")==0 || management==4)
       fprintf(fp," %s_destroy_all(",funct);
     else
       fprintf(fp," %s_destroy(",funct);
     
     pointer_level= Ctyp_get_pointer_level(ct,0)-2;
     for(i=0; i>pointer_level; i--)
       fprintf(fp,"&");
     for(i=0; i<pointer_level; i++)
       fprintf(fp,"*");
     fprintf(fp,"(o->%s)",ct->name);
     if(strcmp(ct->dtype,"char")==0) {
       if(cgen->gen_for_stic==1)
         fprintf(fp,",NULL,0);\n");
       else
         fprintf(fp,");\n");
     } else
       fprintf(fp,",0);\n");
   } else if(management==2) {
     next= ct->next;
     if(next==NULL) {
broken_chain:;
       sprintf(cgen->msg,
               "Lonely -c found. They have to appear in pairs.\nName is : %s",
               ct->name);
       ret= 0; goto ex;
     }
     if(next->management!=3)
       goto broken_chain;
     fprintf(fp," if(o->%s!=NULL)\n",ct->name);
     fprintf(fp,"   o->%s->%s= o->%s;\n",ct->name,next->name,next->name);
     fprintf(fp," if(o->%s!=NULL)\n",next->name);
     fprintf(fp,"   o->%s->%s= o->%s;\n",next->name,ct->name,ct->name);
     ct= next;
   }
 }
 fprintf(fp,"\n");
 if(cgen->gen_for_stic)
   fprintf(fp," Smem_freE((char *) o);\n");
 else
   fprintf(fp," free((char *) o);\n");
 fprintf(fp," *objpt= NULL;\n");
 fprintf(fp," return(1);\n");
 fprintf(fp,"}\n");
 fprintf(fp,"\n");
 if(cgen->is_managed_list){
   ret= Cgen_write_c_destroy_all(cgen,0);
   if(ret<=0)
     goto ex;
 }
 ret= 1;
ex:;
 return(ret);
}


int Cgen_write_c_destroy_all(cgen,flag)
struct CgeN *cgen;
int flag;
{
 int ret,l,management,pointer_level,i;
 FILE *fp= NULL;
 struct CtyP *ct;
 char ptt[4096];

 fp= cgen->fp;
 fprintf(fp,"\n");
 if(cgen->make_ansi) {
   sprintf(ptt, "int %s_destroy_all(struct %s **objpt, int flag)",
           cgen->functname, cgen->structname);
   fprintf(fp,"%s\n",ptt);
   ret= Cgen_write_to_ptt(cgen, ptt, 0);
   if(ret<=0)
     return(ret);
 } else {
   fprintf(fp,"int %s_destroy_all(objpt,flag)\n",cgen->functname);
   fprintf(fp,"struct %s **objpt;\n",cgen->structname);
   fprintf(fp,"int flag;\n");
 }
 fprintf(fp,"{\n");
 fprintf(fp," struct %s *o,*n;\n",cgen->structname);
 fprintf(fp,"\n");
 fprintf(fp," o= *objpt;\n");
 fprintf(fp," if(o==NULL)\n");
 fprintf(fp,"   return(0);\n");
 fprintf(fp," for(;o->prev!=NULL;o= o->prev);\n");
 fprintf(fp," for(;o!=NULL;o= n) {\n");
 fprintf(fp,"   n= o->next;\n");
 fprintf(fp,"   %s_destroy(&o,0);\n",cgen->functname);
 fprintf(fp," }\n");
 fprintf(fp," *objpt= NULL;\n");
 fprintf(fp," return(1);\n");
 fprintf(fp,"}\n");
 fprintf(fp,"\n");
 ret= 1;
ex:;
 return(ret);
}


int Cgen_write_c_access(cgen,flag)
struct CgeN *cgen;
int flag;
{
 int ret,l,mgt,pointer_level,i;
 FILE *fp= NULL;
 struct CtyP *ct;
 char funct[4096],*npt,ptt[4096];

 fp= cgen->fp;
 for(ct= cgen->elements; ct!=NULL; ct= ct->next) {
   if(ct->is_comment)
 continue;
   pointer_level= Ctyp_get_pointer_level(ct,0);
   if(Ctyp_get_with_getter(ct,0)<=0)
     goto after_getter;
   fprintf(fp,"\n");
   if(cgen->make_ansi) {
     sprintf(ptt, "int %s_get_%s(struct %s *o, ",
             cgen->functname,ct->name,cgen->structname);
     if(Ctyp_is_struct(ct,0))
       strcat(ptt,"struct ");
     strcat(ptt,ct->dtype);
     strcat(ptt," ");
     for(i=0; i<pointer_level+1; i++)
       strcat(ptt,"*");
     if(Ctyp_get_array_size(ct,0)>0)
       strcat(ptt,"*");
     strcat(ptt,"pt");
     if(ct->management==4)
       strcat(ptt,", int idx");
     strcat(ptt,", int flag)");
     fprintf(fp,"%s\n",ptt);
     ret= Cgen_write_to_ptt(cgen, ptt, 0);
     if(ret<=0)
       return(ret);
   } else {
     fprintf(fp,"int %s_get_%s(o,pt",cgen->functname,ct->name);
     if(ct->management==4)
       fprintf(fp,",idx");
     fprintf(fp,",flag)\n");
     fprintf(fp,"struct %s *o;\n",cgen->structname);
     if(Ctyp_is_struct(ct,0))
       fprintf(fp,"struct ");
     fprintf(fp,"%s ",ct->dtype);
     for(i=0; i<pointer_level+1; i++)
       fprintf(fp,"*");
     if(Ctyp_get_array_size(ct,0)>0)
       fprintf(fp,"*");
     fprintf(fp,"pt;\n");
     if(ct->management==4)
       fprintf(fp,"int idx;\n");
     fprintf(fp,"int flag;\n");
   }
   fprintf(fp,"/* Note: idx==-1 fetches the last item of the list */\n");
   fprintf(fp,"{\n");
   if(ct->management==4) {
     strcpy(funct,ct->dtype);
     l= strlen(funct);
     if(cgen->make_lowercase)
       funct[0]= tolower(funct[0]);
     if(l>1)
       funct[l-1]= tolower(funct[l-1]);
     fprintf(fp," if(idx==-1) {\n");
     fprintf(fp,"   *pt= o->%s;\n",ct->next->name);
     fprintf(fp,"   return(*pt!=NULL);\n");
     fprintf(fp," }\n");
     fprintf(fp," return(%s_by_idx(o->%s,(flag&1?1:idx),pt,flag&1));\n",
             funct,ct->name);
   } else {
     fprintf(fp," *pt= o->%s;\n",ct->name);
     fprintf(fp," return(1);\n");
   }
   fprintf(fp,"}\n");
   fprintf(fp,"\n");
after_getter:;

   if(Ctyp_get_with_setter(ct,0)<=0)
     goto after_setter;

   /* <<< provisory : develop a setter for arrays */
   if(Ctyp_get_array_size(ct,0)>0)
     goto after_setter;

   mgt= Ctyp_get_management(ct,0);
   if(mgt==0 || 
     (mgt==1 && pointer_level==1)) {
      /* -value or -managed pointers */
      /* was: -value or -managed char * */
      /* (mgt==1 && strcmp(ct->dtype,"char")==0 && pointer_level==1)) { */
  
     fprintf(fp,"\n");
     if(cgen->make_ansi) {
       sprintf(ptt, "int %s_set_%s(struct %s *o, ",
               cgen->functname,ct->name,cgen->structname);
       if(Ctyp_is_struct(ct,0))
         strcat(ptt,"struct ");
       strcat(ptt,ct->dtype);
       strcat(ptt," ");
       for(i=0; i<pointer_level; i++)
         strcat(ptt,"*");
       strcat(ptt,"value, int flag)");
       fprintf(fp,"%s\n",ptt);
       ret= Cgen_write_to_ptt(cgen, ptt, 0);
       if(ret<=0)
         return(ret);
     } else {
       fprintf(fp,"int %s_set_%s(o,value,flag)\n",cgen->functname,ct->name);
       fprintf(fp,"struct %s *o;\n",cgen->structname);
       if(Ctyp_is_struct(ct,0))
         fprintf(fp,"struct ");
       fprintf(fp,"%s ",ct->dtype);
       for(i=0; i<pointer_level; i++)
         fprintf(fp,"*");
       fprintf(fp,"value;\n");
       fprintf(fp,"int flag;\n");
     }
     fprintf(fp,"{\n");
     if(mgt==1 && strcmp(ct->dtype,"char")==0) {
       if(cgen->gen_for_stic==1) {
         fprintf(fp," if(Sregex_string(&(o->%s),value,0)<=0)\n",ct->name);
         fprintf(fp,"   return(-1);\n");
       } else if(cgen->gen_for_stic==2) {
         fprintf(fp," if(Smem_clone_string(&(o->%s),value)<=0)\n",ct->name);
         fprintf(fp,"   return(-1);\n");
       } else {
         fprintf(fp," char *cpt;\n");
         fprintf(fp,"\n");
         fprintf(fp," cpt= malloc(strlen(value)+1);\n");
         fprintf(fp," if(cpt==NULL)\n");
         fprintf(fp,"   return(-1);\n");
         fprintf(fp," o->%s= cpt;\n",ct->name);
         fprintf(fp," \n");
       }
     } else {
       fprintf(fp," o->%s= value;\n",ct->name);
     }
     fprintf(fp," return(1);\n");
     fprintf(fp,"}\n");
     fprintf(fp,"\n");
   }  
    
after_setter:;
 }

 if(cgen->is_managed_list) {
   fprintf(fp,"\n");
   if(cgen->make_ansi) {
     sprintf(ptt,"int %s_link(struct %s *o, struct %s *link, int flag)",
             cgen->functname,cgen->structname,cgen->structname);
     fprintf(fp,"%s\n",ptt);
/*   if(cgen->readonly) */
     {
       ret= Cgen_write_to_ptt(cgen, ptt, 0);
       if(ret<=0)
         return(ret);
     }
   } else {
     fprintf(fp,"int %s_link(o,link,flag)\n",cgen->functname);
     fprintf(fp,"struct %s *o;\n",cgen->structname);
     fprintf(fp,"struct %s *link;\n",cgen->structname);
     fprintf(fp,"int flag;\n");
   }
   fprintf(fp,"/*\n");
   fprintf(fp,"  bit0= insert as link->prev rather than as link->next\n");
   fprintf(fp,"*/\n");
   fprintf(fp,"{\n");
   fprintf(fp," if(o->prev!=NULL)\n");
   fprintf(fp,"   o->prev->next= o->next;\n");
   fprintf(fp," if(o->next!=NULL)\n");
   fprintf(fp,"   o->next->prev= o->prev;\n");
   fprintf(fp," o->prev= o->next= NULL;\n");
   fprintf(fp," if(link==NULL)\n");
   fprintf(fp,"   return(1);\n");
   fprintf(fp," if(flag&1) {\n");
   fprintf(fp,"   o->next= link;\n");
   fprintf(fp,"   o->prev= link->prev;\n");
   fprintf(fp,"   if(o->prev!=NULL)\n");
   fprintf(fp,"     o->prev->next= o;\n");
   fprintf(fp,"   link->prev= o;\n");
   fprintf(fp," } else {\n");
   fprintf(fp,"   o->prev= link;\n");
   fprintf(fp,"   o->next= link->next;\n");
   fprintf(fp,"   if(o->next!=NULL)\n");
   fprintf(fp,"     o->next->prev= o;\n");
   fprintf(fp,"   link->next= o;\n");
   fprintf(fp," }\n");
   fprintf(fp," return(1);\n");
   fprintf(fp,"}\n");
   fprintf(fp,"\n");

   fprintf(fp,"\n");
   if(cgen->make_ansi) {
     sprintf(ptt,"int %s_count(struct %s *o, int flag)",
             cgen->functname,cgen->structname);
     fprintf(fp,"%s\n",ptt);
     ret= Cgen_write_to_ptt(cgen, ptt, 0);
     if(ret<=0)
       return(ret);
   } else {
     fprintf(fp,"int %s_count(o,flag)\n",cgen->functname);
     fprintf(fp,"struct %s *o;\n",cgen->structname);
     fprintf(fp,"int flag;\n");
   }
   fprintf(fp,"/* flag: bit1= count from start of list */\n");
   fprintf(fp,"{\n");
   fprintf(fp," int counter= 0;\n");
   fprintf(fp,"\n");
   fprintf(fp," if(flag&2)\n");
   fprintf(fp,"   for(;o->prev!=NULL;o= o->prev);\n");
   fprintf(fp," for(;o!=NULL;o= o->next)\n");
   fprintf(fp,"   counter++;\n");
   fprintf(fp," return(counter);\n");
   fprintf(fp,"}\n");
   fprintf(fp,"\n");

   fprintf(fp,"\n");
   if(cgen->make_ansi) {
     sprintf(ptt,
             "int %s_by_idx(struct %s *o, int idx, struct %s **pt, int flag)",
             cgen->functname,cgen->structname,cgen->structname);
     fprintf(fp,"%s\n",ptt);
     ret= Cgen_write_to_ptt(cgen, ptt, 0);
     if(ret<=0)
       return(ret);
   } else {
     fprintf(fp,"int %s_count(o,idx,pt,flag)\n",cgen->functname);
     fprintf(fp,"struct %s *o;\n",cgen->structname);
     fprintf(fp,"int idx;\n");
     fprintf(fp,"struct %s **pt;\n",cgen->structname);
     fprintf(fp,"int flag;\n");
   }
   fprintf(fp,
          "/* flag: bit0= fetch first (idx<0) or last (idx>0) item in list\n");
   fprintf(fp,
          "         bit1= address from start of list */\n");
   fprintf(fp,"{\n");
   fprintf(fp," int i,abs_idx;\n");
   fprintf(fp,"struct %s *npt;\n",cgen->structname);
   fprintf(fp,"\n");
   fprintf(fp," if(flag&2)\n");
   fprintf(fp,"   for(;o->prev!=NULL;o= o->prev);\n");
   fprintf(fp," abs_idx= (idx>0?idx:-idx);\n");
   fprintf(fp," *pt= o;\n");
   fprintf(fp," for(i= 0;(i<abs_idx || (flag&1)) && *pt!=NULL;i++) {\n");
   fprintf(fp,"   if(idx>0)\n");
   fprintf(fp,"     npt= o->next;\n");
   fprintf(fp,"   else\n");
   fprintf(fp,"     npt= o->prev;\n");
   fprintf(fp,"   if(npt==NULL && (flag&1))\n");
   fprintf(fp," break;\n");
   fprintf(fp,"   *pt= npt;\n");
   fprintf(fp," }\n");
   fprintf(fp," return(*pt!=NULL);\n");
   fprintf(fp," ;\n");
   fprintf(fp,"}\n");
   fprintf(fp,"\n");
 }

 return(1);
}


int Cgen_write_c_method_include(cgen,flag)
struct CgeN *cgen;
int flag;
{
 FILE *fp= NULL;
 char filename[4096],line[4096];
 struct stat stbuf;
 time_t t0;

 sprintf(filename,"%s.c.methods",cgen->classname);
 if(stat(filename,&stbuf)!=-1)
   goto write_include;
 fp= fopen(filename,"w");
 if(fp==NULL) {
   sprintf(cgen->msg,"Cannot open file '%s' in %s-mode. %s",
                     filename,"w",strerror(errno));
   return(0);
 }
 fprintf(fp,"\n");
 fprintf(fp,"/* File                                %s  */\n",filename);
 fprintf(fp,"/* Manually provided C code for class  %s    */\n",
         cgen->classname);
 fprintf(fp,"/* This file gets copied to the end of %s.c  */\n",
         cgen->classname);
 fprintf(fp,"\n");
 fclose(fp); fp= NULL;

write_include:;
 fp= fopen(filename,"r");
 if(fp==NULL) {
   sprintf(cgen->msg,"Cannot open file '%s' in %s-mode. %s",
                     filename,"r",strerror(errno));
   return(0);
 }
 fprintf(cgen->fp,"\n");
 fprintf(cgen->fp,
"/* -------------- end of automatically regenerated code -------------- */\n");
 fprintf(cgen->fp,"\n");
 while(1) {
   if(Sfile_fgets(line,sizeof(line)-1,fp)==NULL)
 break;
   fprintf(cgen->fp,"%s\n",line);
 }
 fclose(fp); fp= NULL;
 return(1);
}

int Cgen_write_c(cgen,flag)
struct CgeN *cgen;
int flag;
/*
 bit0= also write access functions *_set_* *_get_* [*_link_*]
*/
{
 int ret;

 sprintf(cgen->filename,"%s.c",cgen->classname);
 ret= Cgen_open_wfile(cgen,0);
 if(ret<=0)
   goto ex;
 ret= Cgen_write_c_head(cgen,0);
 if(ret<=0)
   goto ex;
 ret= Cgen_write_c_new(cgen,0);
 if(ret<=0)
   goto ex;
 ret= Cgen_write_c_destroy(cgen,0);
 if(ret<=0)
   goto ex;
 if(flag&1) {
   ret= Cgen_write_c_access(cgen,0);
   if(ret<=0)
     goto ex;
 }
 ret= Cgen_write_c_method_include(cgen,0);
 if(ret<=0)
   goto ex;

 if(cgen->make_ansi) { /* public .h file collected ANSI prototypes */
   ret= Cgen_finish_public_h(cgen,0);
   if(ret<=0)
     goto ex;
 }
 
 ret= 1;
ex:;
 if(cgen->fp!=NULL)
   {fclose(cgen->fp); cgen->fp= NULL;}
 return(ret);
}


int Cgen__write_global_include(global_include_file,flag)
char *global_include_file;
int flag;
/*
 bit0= write footer rather than header
 bit1= allow overwriting of existing file
*/
{
 FILE *fp= NULL;
 int ret;
 char fmode[4],timetext[81],macro_name[4096],*cpt;
 time_t t0;
 struct stat stbuf;

 strcpy(macro_name,global_include_file);
 for(cpt= macro_name; *cpt!=0; cpt++) {
   if(*cpt>='A' && *cpt<='Z')
     *cpt= tolower(*cpt);
   else if((*cpt>='a' && *cpt<='z') || (*cpt>='0' && *cpt<='9') || *cpt=='_')
     ;
   else
     *cpt= '_';
 }
 macro_name[0]= toupper(macro_name[0]);
 strcat(macro_name,"_includeD");

 strcpy(fmode,"w");
 if(flag&1) {
   strcpy(fmode,"a");
 } else {
   if(stat(global_include_file,&stbuf)!=-1 && !(flag&2)) {
     fprintf(stderr,"+++ File '%s' already existing.",global_include_file);
     ret= 0; goto ex;
   }
 }
 fp= fopen(global_include_file,fmode);
 if(fp==NULL) {
   fprintf(stderr,"+++ Cannot open file '%s' in %s-mode. %s",
                  global_include_file,fmode,strerror(errno));
   ret= 0; goto ex;
 }
 if(flag&1) {
   fprintf(fp,"\n");
   fprintf(fp,"#endif /* %s */\n\n",macro_name);
   t0= time(0);
   strftime(timetext,sizeof(timetext),"%a, %d %b %Y %H:%M:%S GMT",
                                    gmtime(&t0));
   fprintf(fp,"/* ( derived from stub generated by CgeN on  %s ) */\n",
           timetext);

 } else {
   fprintf(fp,"\n");
   fprintf(fp,"#ifndef %s\n",macro_name);
   fprintf(fp,"#define %s\n",macro_name);
   fprintf(fp,"\n");
 }

ex:;
 if(fp!=NULL)
   fclose(fp);
 return(ret);
}


/* ---------------- Sfile and Sregex Emancipation copies ---------------- */


char *Sfile_fgets(line,maxl,fp)
char *line;
int maxl;
FILE *fp;
{
int l;
char *ret;
 ret= fgets(line,maxl,fp);
 if(ret==NULL)
   return(NULL);
 l= strlen(line);
 if(l>0) if(line[l-1]=='\r') line[--l]= 0;
 if(l>0) if(line[l-1]=='\n') line[--l]= 0;
 if(l>0) if(line[l-1]=='\r') line[--l]= 0;
 return(ret);
}


int Sregex_string_cut(handle,text,len,flag)
char **handle;
char *text;
int len;
int flag;
/*
 bit0= append (text!=NULL)
 bit1= prepend (text!=NULL)
*/
{
 int l=0;
 char *old_handle;

 if((flag&(1|2))&&*handle!=NULL)
   l+= strlen(*handle);
 old_handle= *handle;
 if(text!=NULL) {
   l+= len;
   *handle= TSOB_FELD(char,l+1);
   if(*handle==NULL) {
     *handle= old_handle;
     return(0);
   }
   if((flag&2) && old_handle!=NULL) {
     strncpy(*handle,text,len);
     strcpy((*handle)+len,old_handle);
   } else {
     if((flag&1) && old_handle!=NULL)
       strcpy(*handle,old_handle);
     else
       (*handle)[0]= 0;
     if(len>0)
       strncat(*handle,text,len);
   }
 } else {
   *handle= NULL;
 }
 if(old_handle!=NULL)
   Smem_freE(old_handle);
 return(1);
}


int Sregex_string(handle,text,flag)
char **handle;
char *text;
int flag;
/*
 bit0= append (text!=NULL)
 bit1= prepend (text!=NULL)
*/
{
 int ret,l=0;

 if(text!=NULL)
   l= strlen(text);
 
/* #define Sregex_looking_for_contenT 1 */
#ifdef Sregex_looking_for_contenT
 /* a debugging point if a certain text content has to be caught */
 if(text!=NULL)
   if(strcmp(text,"clear")==0)
     ret= 0;
#endif

 ret= Sregex_string_cut(handle,text,l,flag&(1|2));
 return(ret);
}


int Sregex_trimline(line,flag)
/*
  removes line endings as well as leading and trailing blanks
*/
char *line;
int flag;
/*
 bit0= do not remove line end (protects trailing blanks if line end is present)
 bit1= do not remove leading blanks
 bit2= do not remove trailing blanks
 bit3= remove surrounding quotation marks (after removing line end)
*/
{
 char *cpt,*wpt;
 int l;

 if(!(flag&1)){
   l= strlen(line);
   if(l>0) if(line[l-1]=='\r') line[--l]= 0;
   if(l>0) if(line[l-1]=='\n') line[--l]= 0;
   if(l>0) if(line[l-1]=='\r') line[--l]= 0;
 }
 if(flag&3){
   l= strlen(line);
   if(l>1) if(line[0]==34 && line[l-1]==34) {
     wpt= line;
     cpt= wpt+1;
     while(*cpt!=0) 
       *(wpt++)= *(cpt++);
     line[l-2]= 0;
   }
 }
 if(!(flag&2)){
   wpt= cpt= line;
   while(*(cpt)!=0) {
     if(!isspace(*cpt))
   break;
     cpt++;
   }
   while(*(cpt)!=0) 
     *(wpt++)= *(cpt++);
   *wpt= 0;  
 }
 if(!(flag&4)){
   l= strlen(line);
   if(l<=0)
     return(1);
   cpt= line+l;
   while(cpt-->=line){
     if(!isspace(*cpt))
   break;
     *(cpt)= 0;
   }
 }
 return(1);
}


/* -------------------------------------------------------------- */


main(argc,argv)
int argc;
char **argv;
{
 struct CgeN *cgen= NULL;
 int ret, msg_printed= 0,first=1,gen_for_stic= 1, make_ansi= 0, i;
 int make_lowercase= 0, may_overwrite= 0;
 char global_include_file[4096];

 global_include_file[0]= 0;

 for(i= 1; i<argc; i++) {
   if(strcmp(argv[i],"-no_stic")==0)
     gen_for_stic= 0;
   else if(strcmp(argv[i],"-smem_local")==0)
     gen_for_stic= 2;
   else if(strcmp(argv[i],"-ansi")==0)
     make_ansi= 1;
   else if(strcmp(argv[i],"-global_include")==0) {
     if(i+1>=argc)
       strcpy(global_include_file,"global_include.h");
     else {
       i++;
       strcpy(global_include_file,argv[i]);
     }
   } else if(strcmp(argv[i],"-lowercase")==0) {
     make_lowercase= 1;
   } else if(strcmp(argv[i],"-overwrite")==0) {
     may_overwrite= 1;
   } else {
     fprintf(stderr,"+++ %s: Unrecognized option: %s\n",argv[0],argv[i]);
     {ret= 0; goto ex;}
   }
 }

 if(strlen(global_include_file)>0) {
   /* begin */
   ret= Cgen__write_global_include(global_include_file,(!!may_overwrite)<<1);
   if(ret<=0)
     goto ex;
 }
 while(!feof(stdin)) {
   ret= Cgen_new(&cgen,0);
   if(ret<=0)
     goto ex;

   /* <<< can be done neater */
   cgen->gen_for_stic= gen_for_stic;
   cgen->make_ansi= make_ansi;
   strcpy(cgen->global_include_file,global_include_file);
   cgen->make_lowercase= make_lowercase;
   cgen->may_overwrite= may_overwrite;

   ret= Cgen_read_fp(cgen,stdin,first);
   if(ret<=0)
     goto ex;
   if(ret==2)
 break;
   first= 0;
   ret= Cgen_write_h(cgen,0);
   if(ret<=0)
     goto ex;
   ret= Cgen_write_c(cgen,1);
   if(ret<=0)
     goto ex;
 }
 if(strlen(global_include_file)>0) {
   /* finalize */
   ret= Cgen__write_global_include(global_include_file,1);
   if(ret<=0)
     goto ex;
 }
 ret= 1;
ex:
 if(cgen!=NULL)
   if(cgen->msg[0]!=0) {
     fprintf(stderr,"+++ %s\n",cgen->msg);
     msg_printed= 1;
   }
 if(ret<=0 &&!msg_printed) {
   if(errno>0)
     fprintf(stderr,"+++ Error : %s\n",strerror(errno));
   else if(ret==-1)
     fprintf(stderr,
             "+++ Program run failed (probably due to lack of memory)\n");
   else
     fprintf(stderr,"+++ Program run failed\n");
 }
 Cgen_destroy(&cgen,0);
 exit(1-ret);
}
