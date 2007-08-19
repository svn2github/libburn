
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define Smem_included_by_smem_C
#include "smem.h"



/* ------------------------------ SmemiteM ----------------------------- */


int Smemitem_new(item,data,size,next,hash_start,flag)
struct SmemiteM **item;
char *data;
size_t size;
struct SmemiteM *next;
struct SmemiteM **hash_start;
int flag;
{
 struct SmemiteM *t;

 *item= t= (struct SmemiteM *) malloc(sizeof(struct SmemiteM));
 if(t==NULL)
   return(-1);
 t->data= data;
 t->size= size;
 t->prev= NULL;
 t->next= next;

#ifdef Smem_with_hasH
 t->hash_next= NULL;
 t->hash_prev= NULL;
#endif /* Smem_with_hasH */

 if(next!=NULL) {
   if(next->prev!=NULL) {
     t->prev= next->prev;
     next->prev->next= t;
   }
   next->prev= t;
 }

#ifdef Smem_with_hasH
 if(hash_start!=NULL) {
   t->hash_next= *hash_start;
   if(t->hash_next!=NULL) {
     t->hash_next->hash_prev= t;
   }
   *hash_start= t;
 }
#endif /* Smem_with_hasH */
 
 return(1);
}


int Smemitem_destroy(in_item,hash_start,flag)
struct SmemiteM **in_item;
struct SmemiteM **hash_start;
int flag;
{
 struct SmemiteM *item;

 item= *in_item;
 if(item==NULL)
   return(0);
 if(item==Smem_start_iteM)
   Smem_start_iteM= item->next;
 if(item->prev!=NULL)
   item->prev->next= item->next;
 if(item->next!=NULL)
   item->next->prev= item->prev;

#ifdef Smem_with_hasH
 if(hash_start!=NULL) {
   if(item==*hash_start)
     *hash_start= item->hash_next;
   if(item->hash_prev!=NULL)
     item->hash_prev->hash_next= item->hash_next;
   if(item->hash_next!=NULL)
     item->hash_next->hash_prev= item->hash_prev;
 }
#endif /* Smem_with_hasH */
 
 free((char *) item);
 *in_item= NULL;
 return(1);
}


int Smemitem_report(item,line,flag)
struct SmemiteM *item;
char line[1024];
int flag;
{
 char *cpt;
 int i,upto;

 sprintf(line,"%4lu bytes at %8.8lx ",(unsigned long) item->size,
                                      (unsigned long) item->data);
 cpt= line+strlen(line); 
 if(item->size<=256)
   upto= item->size;
 else
   upto= 256;
 if(item->data!=NULL) {
   strcpy(cpt,"= \"");
   cpt+= 3;
   for(i=0;i<upto;i++){
     if(item->data[i]<32 || item->data[i]>=127 || item->data[i]=='\\') {
       sprintf(cpt,"\\%2.2X",(unsigned char) item->data[i]);
       cpt+= 3;
     } else {
       *(cpt++)= item->data[i];
     }
   }
   if(i<item->size) {
     sprintf(cpt,"\" [truncated]");
   } else {
     *(cpt++)= '"';
     *cpt= 0;
   }  
 }
 return(1);
}


int Smemitem_stderr(item,flag)
struct SmemiteM *item;
int flag;
{
 char line[1024];
 Smemitem_report(item,line,0);
 fprintf(stderr,"%s\n",line);
 return(1);
}



/* -------------------------------- Smem ------------------------------ */


int Smem_protest(line,flag)
char *line;
int flag;
{
 fprintf(stderr,"%s\n",line);
 return(1); 
}


int Smem_hashindex(ptr,flag)
char *ptr;
int flag;
{
 unsigned long idx;

 idx= (unsigned long) ptr;
 return((idx>>Smem_hashshifT)%(Smem_hashsizE));
}


/* find a certain memory item */
struct SmemiteM *Smem_find_item(ptr,flag)
char *ptr;
int flag;
{
 int misscount= 0,idx;
 struct SmemiteM *current;

#ifdef Smem_with_hasH

 idx= Smem_hashindex(ptr,0);
 for(current= Smem_hasH[idx];current!=NULL;current= current->hash_next) {
   if(current->data==ptr)
     return(current);
   misscount++;
 }

#else /* Smem_with_hasH */

 for(current= Smem_start_iteM;current!=NULL;current= current->next) {
   if(current->data==ptr)
     return(current);
   misscount++;
 }

#endif /* ! Smem_with_hasH */

 return(NULL);
}


int Smem_search_and_delete(ptr,flag)
char *ptr;
int flag;
/*
 bit0= revoke registration : decrement counters
*/
{
 int idx;
 struct SmemiteM *current;

 current= Smem_find_item(ptr,0);
 if(current==NULL)
   return(0);
 Smem_record_counT--;
 Smem_record_byteS-= current->size;
 idx= Smem_hashindex(ptr,0);
 Smemitem_destroy(&current,&(Smem_hasH[idx]),0);
 Smem_hash_counteR[idx]-= 1.0;
 if(flag&1) {
   Smem_malloc_counT--;
   Smem_pending_counT--;
 }
 return(1);   
}


char *Smem_malloc(size)
size_t size;
{
 int idx;
 char *cpt;

 if(size==0) {
   Smem_protest("###########  smem.c : malloc(0) caught",0);
   return(NULL);
 }

 /* if(size==1032)
   cpt= NULL; / * set breakpoint here to find requests of certain size */

 cpt= (char *) malloc(size);
 if(cpt==NULL) {
   char text[161];
   sprintf(text,"###########  smem.c : malloc( %lu ) returned NULL",
                (unsigned long) size);
   Smem_protest(text,0);
   return(NULL);
 }
 /* if(cpt==0x080a1e20)
   cpt= NULL; / * set breakpoint here to find origin of certain address  */

 Smem_malloc_counT++;
 Smem_pending_counT++;
 if(Smem_record_itemS) {
   idx= Smem_hashindex(cpt,0);
   Smem_hash_counteR[idx]+= 1.0;
   if(Smemitem_new(&Smem_start_iteM,cpt,size,Smem_start_iteM,
                                             &(Smem_hasH[idx]),0)<=0) {
     Smem_protest(
           "###########  smem.c : malloc( sizeof(SmemiteM) ) returned NULL",0);
     return(NULL);
   }
   Smem_record_counT++;
   Smem_record_byteS+= size;
 }
 return(cpt);
}


int Smem_free(ptr)
char *ptr;
{
 if(ptr==NULL) {
   Smem_protest("########### smem.c : free() of NULL pointer caught",0);
   return(0);
 }
 if(Smem_record_itemS) {
   if(Smem_search_and_delete(ptr,0)<=0) {
     Smem_protest("########### smem.c : free() of unrecorded pointer caught",0);
     return(0);
   }
 }
 Smem_free_counT++;
 Smem_pending_counT--;
 free(ptr);
 return(1);
}


int Smem_report(line,flag)
char line[1024];
int flag;
{
 sprintf(line,"malloc= %.f , free= %.f , pending= %.f",
              Smem_malloc_counT,Smem_free_counT,Smem_pending_counT);
 if(Smem_record_itemS) {
   sprintf(line+strlen(line)," , bytes=%.f , records= %.f",
                             Smem_record_byteS,Smem_record_counT);
 }
 return(1);
}


int Smem_stderr(flag)
int flag;
/*
 bit0= report 50 youngest pending items too
 bit1= do not report if nothing is pending
*/
{
 struct SmemiteM *current;
 char line[1024];
 int i= 0;

 if(flag&2)
   if(Smem_pending_counT==0.0 
      && Smem_record_counT==0.0 
      && Smem_record_byteS==0.0)
     return(2);
 Smem_report(line,0);
 fprintf(stderr,"%s\n",line);
 if(flag&1) {
   for(current= Smem_start_iteM;current!=NULL;current= current->next) {
     Smemitem_stderr(current,0);
     if(++i>=50)
   break;
   }
   if(current!=NULL)
     if(current->next!=NULL)
       fprintf(stderr,"[list truncated]\n");
 }
 return(1);
}


int Smem_set_record_items(value)
int value;
{
 int i;

 if(!Smem_hash_initializeD) {
   for(i=0;i<Smem_hashsizE;i++) {
     Smem_hasH[i]= NULL;
     Smem_hash_counteR[i]= 0.0;
   }
   Smem_hash_initializeD= 1;
 }
 Smem_record_itemS= value;
 return(1);
}


int Smem_is_recorded(ptr,flag)
char *ptr;
int flag;
/*
 bit0= complain if return(0)
*/
{
 if(Smem_record_itemS==0)
   return(2);
 if(Smem_find_item(ptr,0)!=NULL)
   return(1);
 if(flag&1)
   Smem_protest("########### smem.c : free() of unrecorded pointer caught",0);
 return(0);
}


/* A simple C string cloner */
int Smem_clone_string(ptr,text)
char **ptr;
char *text;
{
 *ptr= Smem_malloC(strlen(text)+1);
 if(*ptr==NULL)
   return(-1);
 strcpy(*ptr,text);
 return(1);
}


/* ----------------- for usage via debugger commands --------------------- */


/* find a certain memory item */
struct SmemiteM *Smem_find_data(ptr)
char *ptr;
{
  return(Smem_find_item(ptr,0));
}


/* browsing the list */
struct SmemiteM *Smem_fetch_item(step,flag)
int step;
int flag;
/*
 bit0= reset cursor (and therefore address absolutely)
*/
{
 static struct SmemiteM *current= NULL;

 if((flag&1)||current==NULL)
   current= Smem_start_iteM;
 if(step>0) {
   for(;current!=NULL;current= current->next) {
     if(step==0)
       return(current);
     step--;
   }
 } else if(step<0) {
   for(;current!=NULL;current= current->prev) {
     if(step==0)
       return(current);
     step++;
   }
 } else {
   return(current);
 }
 return(NULL);
}


int Smem_print_hash_counter() {
 int i;

 for(i=0;i<Smem_hashsizE;i++)
   printf("%4d :  %10.f\n",i,Smem_hash_counteR[i]);
 return(1);
}


/* delete all recorded memory items */
int Smem_delete_all_items()
{
 int ret;

 while(Smem_start_iteM!=NULL) {
   ret= Smem_free(Smem_start_iteM->data);
   if(ret<=0)
     return(0);
 }
 return(1);
}


