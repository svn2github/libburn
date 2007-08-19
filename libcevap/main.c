
#include <stdio.h>
#include <stdlib.h>

#include "cevapi.h"

int main(int argc, char **argv)
{
 struct CevapI *cevap= NULL;
 int ret;

 /* full memory supervision */
 Smem_set_record_items(1);

 /* one short trip for testing */ 
 ret= Cevapi_new(&cevap,0);
 if(ret>0)
   Cevapi_destroy(&cevap,0);

 /* report any leaked memory */
 Smem_stderr(1|2);

 exit(ret<=0);
}
