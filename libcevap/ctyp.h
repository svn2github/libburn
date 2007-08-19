
#ifndef Ctyp_includeD
#define Ctyp_includeD



struct CtyP {

 /* if 1 : .name contains comment text, all other elements are invalid */
 int is_comment;

 int is_pointer;  /* number of asterisks */
 int is_struct;
 int is_unsigned;
 int is_volatile;
 unsigned long array_size;

 int management;  /*
               -v    0= just a value
               -m    1= allocated memory which needs to be freed
               -c    2= mutual link with the next element 
               -c    3= mutual link with the prev element
               -l    4= list of -m , chained by -c pair named 'prev','next'    
                        supposed to be followed by a -v of the same type
                        which will mark the end of the list
                   */
 int with_getter;
 int with_setter;
 int bossless_list;
 int no_initializer;

 char *dtype;
 char *name;

 struct CtyP *prev;
 struct CtyP *next;
};


#endif /* Ctyp_includeD */

