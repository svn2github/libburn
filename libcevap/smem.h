
#ifndef Smem_includeD
#define Smem_includeD


/* compile time adjustable parameters : */

/* if not defined, flat malloc() and free() is used */
#define Smem_own_functionS
#ifdef Smem_no_own_functionS
#undef Smem_own_functionS
#endif /* Smem_no_own_functionS */

/* if not defined, the record items will be smaller by 8 byte
   but deletion of items may be much slower */
#define Smem_with_hasH



struct SmemiteM {

 char *data;
 size_t size;

 struct SmemiteM *prev,*next;

 struct SmemiteM *hash_prev,*hash_next;

};




#ifdef Smem_own_functionS

char *Smem_malloc();
#define TSOB_FELD(typ,anz) (typ *) Smem_malloc((anz)*sizeof(typ));
#define Smem_malloC Smem_malloc
#define Smem_freE Smem_free

#else /* Smem_own_functionS */

#define TSOB_FELD(typ,anz) (typ *) malloc((anz)*sizeof(typ));
#define Smem_malloC malloc
#define Smem_freE free

#endif /* ! Smem_own_functionS */




#define Smem_hashsizE 251
#define Smem_hashshifT 8

#ifdef Smem_included_by_smem_C

double Smem_malloc_counT= 0.0;
double Smem_free_counT= 0.0;
double Smem_pending_counT= 0.0;
struct SmemiteM *Smem_start_iteM= NULL;
struct SmemiteM *Smem_hasH[Smem_hashsizE];
double Smem_hash_counteR[Smem_hashsizE];

/* these both init values are essential, since setting Smem_record_itemS=1
   by use of Smem_set_record_items() initializes the hash array
   (i do not really trust the compiler producers to have read K&R) */
int Smem_hash_initializeD= 0;
int Smem_record_itemS= 0;

double Smem_record_counT= 0.0;
double Smem_record_byteS= 0.0;

#else /* Smem_included_by_smem_C */

extern double Smem_malloc_counT;
extern double Smem_free_counT;
extern double Smem_pending_counT;
extern struct SmemiteM *Smem_start_iteM;
extern struct SmemiteM *Smem_hasH[Smem_hashsizE];
extern double Smem_hash_counteR[Smem_hashsizE];
extern int Smem_hash_initializeD;
extern int Smem_record_itemS;
extern double Smem_record_counT;
extern double Smem_record_byteS;

#endif /* ! Smem_included_by_smem_C */



#endif /* ! Smem_includeD */


/*

                                   smem

 Functions to replace  malloc()  and  free() in order to get more control
 over memory leaks or spurious errors caused by faulty usage of malloc()
 and free(). 


 Sourcecode provisions:
 
 Use only the following macros for memory management:
    TSOB_FELD(type,count)  creates an array of items of given type 
    Smem_malloC()          analogue of malloc()
    Smem_freE()            analogue of free()
 One may #define malloc Smem_malloC resp. #define free Smem_freE 
 but better would be to review (and often to streamline) the sourcecode
 in respect to those two functions. 


 Speed versus control:

 In production versions, where maximum speed is required, one may undefine
 the macro  Smem_own_functionS  in  smem.h . 
 This causes the above macros to directly invoke malloc() and free() without
 any speed reduction (and without any additional use).
 Undefinitio can be done globaly by modifying smem.h or locally by defining
  Smem_no_own_functionS  before including smem.h .

 If Smem_own_functionS remains defined, then the functions
   Smem_malloc() 
   Smem_free()
 are used rather than malloc() and free().
 They count the number of calls to maintain a rough overview of memory usage.
 Smem_malloc() additionally checks for 0 size and Smem_free() checks for
 NULL pointers, which they both report to stderr. Eventually one should set
 a breakpoint in function Smem_protest() to learn about the origin of such
 messages.
 A status line may be obtained by  Smem_report()  or printed by Smem_stderr().

 As long as the variable Smem_record_itemS is set to 0, there is not very much
 overhead compared with malloc() and free().
 If the variable is set to 1 by Smem_set_record_items() then all malloc()
 results are kept in a list where they will be deleted by their corresponding
 Smem_free() calls. If a pointer is to be freed, which is not recorded in the
 list then an error message will be printed to stderr. The memory will not
 be freed !
 This mode not only may be very slow, it also consumes at least 16 byte per
 piece of data which was obtained by malloc as long as it has not been freed.
 Due to the current nature of the list, large numbers of memory items are freed
 much faster in the reverse order of their creation. If there is a list of
 100000 strings to delete, it is very rewarding to free the youngest ones first.
 A shortcut via hashing is available but consumes 24 bytes rather than 16.
 (see above  Smem_with_hasH  )

 The function Smem_is_recorded() can be used to check wether a pointer is
 valid according to the list. It returns :
   0 = is not in list , 1 = is in list , 2 = recording is off 

 If one decides to start recording malloc() results in the midst of a program
 run, one has to be aware of false protests of Smem_free() if a memory piece
 has been allocated before recording started. This will also cause those pieces
 to be memory leaks because Smem_free() refuses to delete them. (Freeing memory
 that was not obtained by malloc or was already freed previously can result in
 deferred SIGSEGV or similar trouble, depending on OS and library.)
 Also in that case one should stop recording before ending the program, to
 avoid a lot of false complaints about longliving memory objects.    

*/
