
/*
  cc -g -o ctyp.c
*/

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "smem.h"
extern char *Sfile_fgets();
extern int Sregex_string();
extern int Sregex_trimline();

#include "ctyp.h"


/* --------------------------  CtyP  ----------------------- */


int Ctyp_new(objpt,link,flag)
struct CtyP **objpt;
struct CtyP *link;
int flag;
{
 struct CtyP *o;
 int ret;

 *objpt= o= TSOB_FELD(struct CtyP,1);
 if(o==NULL)
   return(-1);

 o->is_comment= 0;
 o->is_pointer= 0;
 o->is_struct= 0;
 o->is_unsigned= 0;
 o->is_volatile= 0;
 o->array_size= 0;
 o->management= 0;
 o->with_getter= 1;
 o->with_setter= 1;
 o->bossless_list= 0;
 o->no_initializer= 0;
 o->dtype= NULL;
 o->name= NULL;
 o->prev= NULL;
 o->next= NULL;

 if(link!=NULL)
   link->next= o;
 o->prev= link;

 return(1);
failed:;
 Ctyp_destroy(objpt,0);
 return(-1);
}


int Ctyp_destroy(objpt,flag)
struct CtyP **objpt;
int flag;
{
 struct CtyP *o;

 o= *objpt;
 if(o==NULL)
   return(0);

 if(o->prev!=NULL)
   o->prev->next= o->next;
 if(o->next!=NULL)
   o->next->prev= o->prev;
 Sregex_string(&(o->dtype),NULL,0);
 Sregex_string(&(o->name),NULL,0);

 free((char *) o);
 *objpt= NULL;
 return(1);
}


int Ctyp_get_pointer_level(ct,flag)
struct CtyP *ct;
int flag;
{
 return(ct->is_pointer);
}


int Ctyp_is_struct(ct,flag)
struct CtyP *ct;
int flag;
{
 return(ct->is_struct);
}


int Ctyp_get_array_size(ct,flag)
struct CtyP *ct;
int flag;
{
 return(ct->array_size);
}


int Ctyp_get_management(ct,flag)
struct CtyP *ct;
int flag;
{
 return(ct->management);
}


int Ctyp_get_with_getter(ct,flag)
struct CtyP *ct;
int flag;
{
 return(ct->with_getter);
}


int Ctyp_get_with_setter(ct,flag)
struct CtyP *ct;
int flag;
{
 return(ct->with_setter);
}


int Ctyp_get_dtype(ct,text,flag)
struct CtyP *ct;
char **text; /* must point to NULL of freeable memory */
int flag;
/*
 bit0=eventually prepend "struct "
*/
{
 if((flag&1) && ct->is_struct) {
   if(Sregex_string(text,"struct ",0)<=0)
     return(-1);
 } else {
   if(Sregex_string(text,"",0)<=0)
     return(-1);
 }
 if(Sregex_string(text,ct->dtype,1)<=0)
   return(-1);
 return(1);
}


int Ctyp_get_name(ct,text,flag)
struct CtyP *ct;
char **text; /* must point to NULL of freeable memory */
int flag;
{
 if(Sregex_string(text,ct->name,0)<=0)
   return(-1);
 return(1);
}


int Ctyp_get_type_mod(ct,is_spointer,is_struct,array_size,flag)
struct CtyP *ct;
int *is_spointer,*is_struct,*array_size;
int flag;
{
 *is_spointer= ct->is_pointer;
 *is_struct= ct->is_struct;
 *array_size= ct->array_size;
}

int Ctyp_new_from_line(ct,link,line,msg,flag)
struct CtyP **ct;
struct CtyP *link;
char *line;
char *msg;
int flag;
/*
 bit0= make  struct ClassnamE  to  struct classname 
*/
{
 struct CtyP *o;
 char *cpt,*bpt;
 int ret,l;
 char orig_line[4096];

 ret= Ctyp_new(ct,*ct,0);
 if(ret<=0) {
   sprintf(msg,"Failed to create CtyP object (due to lack of memory ?)");
   goto ex;
 }
 o= *ct;

 strcpy(orig_line,line); 
 cpt= line;
 while(*cpt!=0 && isspace(*cpt)) cpt++;
 if(cpt[0]=='#') {
   cpt++;
   if(cpt[1]==' ')
     cpt++;
   l= strlen(cpt);
   if(cpt[0]==' ')
     cpt++;
   if(l>1)
     if(cpt[l-1]==' ')
       cpt[l-1]= 0;
   if(Sregex_string(&(o->name),cpt,0)<=0)
     {ret= -1; goto ex;}
   o->is_comment= 1;
   {ret= 1; goto ex;}
 } else if(cpt[0]==0) {
   if(Sregex_string(&(o->name),cpt,0)<=0)
     {ret= -1; goto ex;}
   o->is_comment= 1;
   {ret= 1; goto ex;}
 } else if(cpt[0]=='/' && cpt[1]=='*') {
   sprintf(msg,
          "C-style multi line comments (/* ... */) not supported yet. Use #.");
   goto ex;

   /* >>> */

 }
 cpt= line;
 while(cpt[0]=='-') {
   /* look for management specifiers:
      -v*   just a value
      -m*   allocated memory which needs to be freed
      -c*   mutual link (like prev+next)
      -l*   list of -m chained by mutual links prev and next

      -r*   read-only : no setter function
      -p*   private   : neither setter nor getter function
      -b*   bossless_list : Class_new(o,flag), not Class_new(o,boss,flag)
      -i*   no_initializer : do not initialize element in <Class>_new()
      #...  line is a comment
   */
   if(cpt[1]=='v' || cpt[1]=='V') {
     o->management= 0;
   } else if(cpt[1]=='m' || cpt[1]=='M') {
     o->management= 1;
   } else if(cpt[1]=='c' || cpt[1]=='C') {
     o->management= 2;
     if(o->prev!=NULL)
       if(o->prev->management==2)
         o->management= 3;
   } else if(cpt[1]=='l' || cpt[1]=='L') {
     o->management= 4;
   } else if(cpt[1]=='r' || cpt[1]=='R') {
     o->with_setter= 0;
   } else if(cpt[1]=='p' || cpt[1]=='P') {
     o->with_setter= 0;
     o->with_getter= 0;
   } else if(cpt[1]=='b' || cpt[1]=='B') {
     o->bossless_list= 1;
   } else if(cpt[1]=='i' || cpt[1]=='I') {
     o->no_initializer= 1;
   }
   while(*cpt!=0 && !isspace(*cpt)) cpt++;
   while(*cpt!=0 && isspace(*cpt)) cpt++;
   if(*cpt==0)
     goto no_name;
 }

 if(strncmp(cpt,"struct ",7)==0) {
   o->is_struct= 1;
   cpt+= 7;
 } else if(strncmp(cpt,"unsigned ",9)==0) {
   o->is_unsigned= 1;
   cpt+= 9;
 } else if(strncmp(cpt,"volatile ",9)==0) {
   o->is_volatile= 1;
   cpt+= 9;
   if(strncmp(cpt,"unsigned ",9)==0) {
     o->is_unsigned= 1;
     cpt+= 9;
   }
 }
 if(*cpt==0)
   goto no_name;
 while(*cpt!=0 && isspace(*cpt)) cpt++;
 bpt= cpt;
 while(*bpt!=0 && !isspace(*bpt)) bpt++;
 if(*bpt==0) 
   goto no_name;
 if(*bpt==0) {
no_name:;
   sprintf(msg,"No name found after type description : %s",orig_line);
   ret= 0; goto ex;
 }
 *bpt= 0;
 if(Sregex_string(&(o->dtype),cpt,0)<=0)
   {ret= -1; goto ex;}
 if((flag&1) && o->is_struct && strlen(o->dtype)>=3)
   if(isupper(o->dtype[0]) && islower(o->dtype[1]) && 
      isupper(o->dtype[strlen(o->dtype)-1])) {
     o->dtype[0]= tolower(o->dtype[0]);
     o->dtype[strlen(o->dtype)-1]= tolower(o->dtype[strlen(o->dtype)-1]);
   }
 cpt= bpt+1;
 while(*cpt!=0 && isspace(*cpt)) cpt++;
 if(*cpt==0)
   goto no_name;
 for(;*cpt=='*';cpt++) 
   o->is_pointer++;
 if(*cpt==0)
   goto no_name;
 bpt= strchr(cpt,'[');
 if(bpt!=NULL) {
   if(strchr(bpt,']')!=NULL)
     *strchr(bpt,']')= 0;
   sscanf(bpt,"%lu",&(o->array_size));
   *bpt= 0;
 }
 if(Sregex_string(&(o->name),cpt,0)<=0)
   {ret= -1; goto ex;}
 if(o->management==1) {
   if((!(o->is_pointer>=1 && o->is_pointer<=2)) ||
       ((!o->is_struct) && strcmp(o->dtype,"char")!=0 &&
        (strcmp(o->dtype,"unsigned char")!=0))) {
     sprintf(msg,"-m can only be applied to pointers of struct or char : %s",
             orig_line);
     ret= 0; goto ex;
   }
 }
 ret= 1;
ex:;
 return(ret);
}


int Ctyp_read_fp(ct,fp,msg,flag)
struct CtyP **ct;
FILE *fp;
char msg[]; /* at least [4096+256] */
int flag;
/*
 bit0= make  struct ClassnamE  to  struct classname 
*/
{
 int ret;
 char line[4096];
 struct CtyP *o;

 line[0]= 0;
 printf(
     "[-value|-managed|-chain|-list] class element ? (e.g.: -l struct XyZ)\n");
 if(Sfile_fgets(line,sizeof(line)-1,fp)==NULL)
   {ret= 2; goto ex;}
 printf("%s\n",line);
 Sregex_trimline(line,0);
 if(strcmp(line,"@")==0)
   {ret= 2; goto ex;}
 ret= Ctyp_new_from_line(ct,*ct,line,msg,flag&1);
 if(ret<=0)
   goto ex; 
 ret= 1;
ex:;
 return(ret);
}

