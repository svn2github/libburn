
/*
   cc -g -DCevap_lowercasE -c main.c  
*/
#include <stdio.h>
#include <stdlib.h>

#include "cevapi.h"

#include "smem.h"

int main(int argc, char **argv)
{
#ifdef Cevap_lowercasE
 struct cevapi *cevap= NULL;
#else
 struct CevapI *cevap= NULL;
#endif
 int ret;

 /* full memory supervision */
 Smem_set_record_items(1);

 /* one short trip for testing */ 
#ifdef Cevap_lowercasE
 ret= cevapi_new(&cevap,0);
 if(ret>0)
   cevapi_destroy(&cevap,0);
#else /* Cevap_lowercasE */
 ret= Cevapi_new(&cevap,0);
 if(ret>0)
   Cevapi_destroy(&cevap,0);
#endif /* ! Cevap_lowercasE */

 /* report any leaked memory */
 Smem_stderr(1|2);

 exit(ret<=0);
}
