
/*
 cdrskin.c , Copyright 2006 Thomas Schmitt <scdbackup@gmx.net>
Provided under GPL. See future commitment below.

A cdrecord compatible command line interface for libburn.

This project is neither directed against original cdrecord nor does it exploit
any source code of said program. It rather tries to be an alternative method
to burn CD which is not based on the same code as cdrecord.
See also :  http://scdbackup.sourceforge.net/cdrskin_eng.html

Interested users of cdrecord are encouraged to contribute further option
implementations as they need them. Contributions will get published under GPL
but it is essential that the authors allow a future release under LGPL and/or
BSD license.

There is a script test/cdrecord_spy.sh which may be installed between
the cdrecord command and real cdrecord in order to learn about the options
used by your favorite cdrecord frontend. Edit said script and install it
according to the instructions given inside.

The implementation of an option would probably consist of
- necessary structure members for structs CdrpreskiN and/or CdrskiN
- code in Cdrpreskin_setup() and  Cdrskin_setup() which converts
  argv[i] into CdrpreskiN/CdrskiN members (or into direct actions)
- removal of option from ignore list "ignored_partial_options" resp.
  "ignored_full_options" in Cdrskin_setup()
- functions which implement the option's run time functionality
- eventually calls of those functions in Cdrskin_run()
- changes to be made within Cdrskin_burn() or Cdrskin_blank() or other
  existing methods
See option blank= for an example.

------------------------------------------------------------------------------
About compliance with *strong urge* of API towards burn_drive_scan_and_grab() 

For a more comprehensive example of the advised way to behave with libburn
see  test/libburner.c .
 
cdrskin was the initiator of the whitelist functionality within libburn.
Now it has problems to obviously comply with the new API best practice
presciptions literally. Therefore this explanation:

On start it restricts the library to a single drive if it already knows the
persistent address by option dev= . This is done with a combination of
burn_drive_add_whitelist() and burn_drive_scan(). Not compliant to the
literal strong urge but in fact exactly fulfilling the reason for that
urge in the API: any scanned drive might be opened exclusively after
burn_drive_scan(). It is kernel dependent wether this behavior is on, off
or switchable. The sysdamin will want it on - but only for one drive.

So with dev=... cdrskin complies to the spirit of the strong urge.
Without dev=... it has to leave out the whitelist in order to enable bus
scanning and implicit drive address 0. A tradition of 9 months shall not
be broken. So burns without dev= will stay possible - but harmless only
on single drive systems.

Burns without dev= resp. with dev=number are harmless on multi-drive systems.

This is because Cdrskin_grab_drive() either drops the unwanted drives or
it enforces a restart of the library with the desired drive's persistent
address. This restart then really uses the strongly urged function
burn_drive_scan_and_grab().
Thus, cdrskin complies with the new spirit of API by closing down libburn
or by dropping unused drives as soon as the persistent drive address is
known and the drive is to be used with a long running operation. To my
knowlege all long running operations in cdrskin need a grabbed drive.

This spaghetti approach seems necessary to keep small the impact of new API
urge on cdrskin's stability. cdrskin suffers from having donated the body
parts which have been transplanted to libburn in order to create
 burn_drive_scan_and_grab() . The desired sysadmin friendlyness was already
achieved by most cdrskin runs. The remaining problem situations should now
be defused by releasing any short time grabbed flocks of drives during the
restart of libburn.

------------------------------------------------------------------------------
This program is currently copyright Thomas Schmitt only.
The copyrights of several components of libburn.pykix.org are willfully tangled
at toplevel to form an irrevocable commitment to true open source spirit.
We have chosen the GPL for legal compatibility and clearly express that it
shall not hamper the use of our software by non-GPL applications which show
otherwise the due respect to the open source community.
See toplevel README and cdrskin/README for that commitment.

For a short time, this place showed a promise to release a BSD license on
mere request. I have to retract that promise now, and replace it by the
promise to make above commitment reality in a way that any BSD conformant
usage in due open source spirit will be made possible somehow and in the
particular special case. I will not raise public protest if you spawn yourself
a BSD license from an (outdated) cdrskin.c which still bears that old promise.
Note that this extended commitment is valid only for cdrskin.[ch],
cdrfifo.[ch] and cleanup.[ch], but not for libburn.pykix.org as a whole.

cdrskin is originally inspired by libburn-0.2/test/burniso.c :
(c) Derek Foreman <derek@signalmarketing.com> and Ben Jansens <xor@orodu.net>

------------------------------------------------------------------------------

Compilation within cdrskin-* :

  cd cdrskin
  cc -g -I.. -DCdrskin_build_timestamP='...' \
     -o cdrskin cdrskin.c cdrfifo.c cleanup.c \
     -L../libburn/.libs -lburn -lpthread

or

  cd ..
  cc -g -I. -DCdrskin_build_timestamP='...' \
     -o cdrskin/cdrskin cdrskin/cdrskin.c cdrskin/cdrfifo.c cdrskin/cleanup.c \
     libburn/async.o libburn/crc.o libburn/debug.o libburn/drive.o \
     libburn/file.o libburn/init.o libburn/lec.o \
     libburn/mmc.o libburn/options.o libburn/sbc.o libburn/sector.o \
     libburn/sg.o libburn/spc.o libburn/source.o libburn/structure.o \
     libburn/toc.o libburn/util.o libburn/write.o \
     libburn/libdax_audioxtr.o libburn/libdax_msgs.o \
     -lpthread

*/


/** The official program version */
#ifndef Cdrskin_prog_versioN
#define Cdrskin_prog_versioN "0.2.5"
#endif

/** The source code release timestamp */
#include "cdrskin_timestamp.h"
#ifndef Cdrskin_timestamP
#define Cdrskin_timestamP "-none-given-"
#endif

/** The binary build timestamp is to be set externally by the compiler */
#ifndef Cdrskin_build_timestamP
#define Cdrskin_build_timestamP "-none-given-"
#endif


#ifdef Cdrskin_libburn_versioN 
#undef Cdrskin_libburn_versioN 
#endif

/** use this to accomodate to the CVS version as of Feb 20, 2006
#define Cdrskin_libburn_cvs_A60220_tS 1
*/
#ifdef Cdrskin_libburn_cvs_A60220_tS

#define Cdrskin_libburn_versioN "0.2.tsA60220"
#define Cdrskin_libburn_no_burn_preset_device_opeN 1
#ifndef Cdrskin_oldfashioned_api_usE
#define Cdrskin_oldfashioned_api_usE 1
#endif

#endif /* Cdrskin_libburn_cvs_A60220_tS */


#ifdef Cdrskin_libburn_0_2_2
#define Cdrskin_libburn_versioN "0.2.2"
#define Cdrskin_libburn_from_pykix_svN 1
#endif

#ifdef Cdrskin_libburn_0_2_3
#define Cdrskin_libburn_versioN "0.2.3"
#define Cdrskin_libburn_from_pykix_svN 1
#define Cdrskin_libburn_has_is_enumerablE 1
#define Cdrskin_libburn_has_convert_fs_adR 1
#define Cdrskin_libburn_has_convert_scsi_adR 1
#define Cdrskin_libburn_has_burn_msgS 1
#define Cdrskin_libburn_has_burn_aborT 1
#define Cdrskin_libburn_has_cleanup_handleR 1
#define Cdrskin_libburn_has_audioxtR 1
#define Cdrskin_libburn_has_get_start_end_lbA 1
#define Cdrskin_libburn_has_burn_disc_unsuitablE 1
#define Cdrskin_libburn_has_read_atiP 1
#define Cdrskin_libburn_has_buffer_progresS 1
#define Cdrskin_libburn_has_pretend_fulL 1
#define Cdrskin_libburn_has_multI 1
#define Cdrskin_libburn_has_buffer_min_filL 1
#endif

#ifndef Cdrskin_libburn_versioN
#define Cdrskin_libburn_versioN "0.2.2"
#define Cdrskin_libburn_from_pykix_svN 1
#endif

#ifdef Cdrskin_libburn_from_pykix_svN

#define Cdrskin_libburn_does_ejecT 1
#define Cdrskin_libburn_has_drive_get_adR 1
#define Cdrskin_progress_track_does_worK 1
#define Cdrskin_is_erasable_on_load_does_worK 1
#define Cdrskin_grab_abort_does_worK 1
#define Cdrskin_allow_libburn_taO 1

#ifdef Cdrskin_new_api_tesT

/* put macros under test caveat here */
#define Cdrskin_allow_sao_for_appendablE 1

/* could be i repaired this with getting -atip minimum speed */
#ifdef Cdrskin_libburn_has_read_atiP
#define Cdrskin_atip_speed_is_oK 1
#endif

#endif

#ifdef Cdrskin_oldfashioned_api_usE

/* switch back to pre-0.2.2 libburn usage */;

#endif

#endif /* Cdrskin_libburn_from_pykix_svN */


/* These macros activate cdrskin workarounds for deficiencies resp.
   problematic features of libburn which hopefully will change in
   future. */

/** Work around the fact that neither /dev/sg0 (kernel 2.4 + ide-scsi) nor 
    /dev/hdc (kernel 2.6) get ejected by icculus.org/burn */
#ifndef Cdrskin_libburn_does_ejecT
#define Cdrskin_burn_drive_eject_brokeN 1
#endif

/** Work around the fact that after loading media speed report is wrong */
#ifndef Cdrskin_atip_speed_is_oK
#define Cdrskin_atip_speed_brokeN 1
#endif

/** Work around the fact that burn_drive_get_status() always reports to do
    track 0 in icculus.org/burn */
#ifndef Cdrskin_progress_track_does_worK
#define Cdrskin_progress_track_brokeN 1
#endif

/** Work around the fact that a drive interrupted at burn_drive_grab() never
    leaves status BURN_DRIVE_GRABBING in icculus.org/burn */
#ifndef Cdrskin_grab_abort_does_worK
#define Cdrskin_grab_abort_brokeN 1
#endif

/** Work around the fact that a freshly loaded tray with media reports
    arbitrary media erasability in icculuc.org/burn */
#ifndef Cdrskin_is_erasable_on_load_does_worK
#define Cdrskin_is_erasable_on_load_is_brokeN 1
#endif

/** http://libburn.pykix.org/ticket/41 reports of big trouble without 
    padding any track to a full sector
*/
#define Cdrskin_all_tracks_with_sector_paD 1


/** A macro which is able to eat up a function call like printf() */
#ifdef Cdrskin_extra_leaN
#define ClN(x) 
#else
#define ClN(x) x
#endif


/** Verbosity level for pacifying progress messages */
#define Cdrskin_verbose_progresS 1

/** Verbosity level for command recognition and execution logging */
#define Cdrskin_verbose_cmD 2

/** Verbosity level for reporting of debugging messages */
#define Cdrskin_verbose_debuG 3


#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

#include "../libburn/libburn.h"

#ifdef Cdrskin_libburn_has_audioxtR
#include "../libburn/libdax_audioxtr.h"
#endif

#ifdef Cdrskin_libburn_has_cleanup_handleR
#define Cleanup_set_handlers burn_set_signal_handling
#define Cleanup_app_handler_T burn_abort_handler_t
#else
#include "cleanup.h"
#endif


/** The size of a string buffer for pathnames and similar texts */
#define Cdrskin_strleN 4096

/** The maximum length +1 of a drive address */
#ifndef Cdrskin_oldfashioned_api_usE
#define Cdrskin_adrleN BURN_DRIVE_ADR_LEN
#else
#define Cdrskin_adrleN 80
#endif


/* --------------------------------------------------------------------- */

/* Imported from scdbackup-0.8.5/src/cd_backup_planer.c */

/** Macro for creation of arrays of objects (or single objects) */
#define TSOB_FELD(typ,anz) (typ *) malloc((anz)*sizeof(typ));


/** Convert a text so that eventual characters special to the shell are
    made literal. Note: this does not make a text terminal-safe !
    @param in_text The text to be converted
    @param out_text The buffer for the result. 
                    It should have size >= strlen(in_text)*5+2
    @param flag Unused yet
    @return For convenience out_text is returned
*/
char *Text_shellsafe(char *in_text, char *out_text, int flag)
{
 int l,i,w=0;

 /* enclose everything by hard quotes */
 l= strlen(in_text);
 out_text[w++]= '\'';
 for(i=0;i<l;i++){
   if(in_text[i]=='\''){
     /* escape hard quote within the text */
     out_text[w++]= '\'';
     out_text[w++]= '"';
     out_text[w++]= '\'';
     out_text[w++]= '"';
     out_text[w++]= '\'';
   } else {
     out_text[w++]= in_text[i];
   }
 }
 out_text[w++]= '\'';
 out_text[w++]= 0;
 return(out_text);
}


/** Convert a text into a number of type double and multiply it by unit code
    [kmgtpe] (2^10 to 2^60) or [s] (2048). (Also accepts capital letters.)
    @param text Input like "42", "2k", "3.14m" or "-1g"
    @param flag Bitfield for control purposes:
                bit0= return -1 rathern than 0 on failure
    @return The derived double value
*/
double Scanf_io_size(char *text, int flag)
/*
 bit0= default value -1 rather than 0
*/
{
 int c;
 double ret= 0.0;

 if(flag&1)
   ret= -1.0;
 if(text[0]==0)
   return(ret);
 sscanf(text,"%lf",&ret);
 c= text[strlen(text)-1];
 if(c=='k' || c=='K') ret*= 1024.0;
 if(c=='m' || c=='M') ret*= 1024.0*1024.0;
 if(c=='g' || c=='G') ret*= 1024.0*1024.0*1024.0;
 if(c=='t' || c=='T') ret*= 1024.0*1024.0*1024.0*1024.0;
 if(c=='p' || c=='P') ret*= 1024.0*1024.0*1024.0*1024.0*1024.0;
 if(c=='e' || c=='E') ret*= 1024.0*1024.0*1024.0*1024.0*1024.0*1024.0;
 if(c=='s' || c=='S') ret*= 2048.0;
 return(ret);
}


/** Return a double representing seconds and microseconds since 1 Jan 1970 */
double Sfile_microtime(int flag)
{
 struct timeval tv;
 struct timezone tz;
 gettimeofday(&tv,&tz);
 return((double) (tv.tv_sec+1.0e-6*tv.tv_usec));
}


#ifndef Cdrskin_extra_leaN

/** Read a line from fp and strip LF or CRLF */
char *Sfile_fgets(char *line, int maxl, FILE *fp)
{
int l;
char *ret;
 ret= fgets(line,maxl,fp);
 if(ret==NULL) return(NULL);
 l= strlen(line);
 if(l>0) if(line[l-1]=='\r') line[--l]= 0;
 if(l>0) if(line[l-1]=='\n') line[--l]= 0;
 if(l>0) if(line[l-1]=='\r') line[--l]= 0;
 return(ret);
}


/** Destroy a synthetic argument array */
int Sfile_destroy_argv(int *argc, char ***argv, int flag)
{
 int i;

 if(*argc>0 && *argv!=NULL){
   for(i=0;i<*argc;i++){
     if((*argv)[i]!=NULL) 
       free((*argv)[i]);
   }
   free((char *) *argv);
 }
 *argc= 0;
 *argv= NULL;
 return(1);
}


/** Read a synthetic argument array from a list of files.
    @param progname The content for argv[0]
    @param filenames The paths of the filex from where to read
    @param filenamecount The number of paths in filenames
    @param argc Returns the number of read arguments (+1 for progname)
    @param argv Returns the array of synthetic arguments
    @param argidx Returns source file indice of argv[] items
    @param arglno Returns source file line numbers of argv[] items
    @param flag Bitfield for control purposes:
                bit0= read progname as first argument from line
                bit1= just release argument array argv and return
                bit2= tolerate failure to open file
    @return 1=ok , 0=cannot open file , -1=cannot create memory objects
*/
int Sfile_multi_read_argv(char *progname, char **filenames, int filename_count,
                          int *argc, char ***argv, int **argidx, int **arglno,
                          int flag)
{
 int ret,i,pass,maxl=0,l,argcount=0,line_no;
 char buf[Cdrskin_strleN];
 FILE *fp= NULL;

 Sfile_destroy_argv(argc,argv,0);
 if(flag&2)
   return(1);
 if((*argidx)!=NULL)
   free((char *) *argidx);
 if((*arglno)!=NULL)
   free((char *) *arglno);
 *argidx= *arglno= NULL;

 for(pass=0;pass<2;pass++) {
   if(!(flag&1)){
     argcount= 1;
     if(pass==0)
       maxl= strlen(progname)+1;
     else {
      (*argv)[0]= (char *) malloc(strlen(progname)+1);
      if((*argv)[0]==NULL)
         {ret= -1; goto ex;}
       strcpy((*argv)[0],progname);
     }
   } else {
     argcount= 0;
     if(pass==0)
       maxl= 1;
   }
   for(i=0; i<filename_count;i++) {
     if(strlen(filenames[i])==0)
   continue;
     fp= fopen(filenames[i],"rb");
     if(fp==NULL) {
       if(flag&4)
   continue;
       {ret= 0; goto ex;}
     }

#ifdef Cdrskin_new_api_tesT
     if(pass>0)
       fprintf(stderr,"cdrskin: DEBUG : Reading arguments from file '%s'\n",
                      filenames[i]);
#endif

     line_no= 0;
     while(Sfile_fgets(buf,sizeof(buf)-1,fp)!=NULL) {
       line_no++;
       l= strlen(buf);
       if(l==0 || buf[0]=='#')
     continue;
       if(pass==0){
         if(l>maxl) 
           maxl= l;
       } else {
         if(argcount >= *argc)
     break;
         (*argv)[argcount]= (char *) malloc(l+1);
         if((*argv)[argcount]==NULL)
           {ret= -1; goto ex;}
         strcpy((*argv)[argcount],buf);
         (*argidx)[argcount]= i;
         (*arglno)[argcount]= line_no;
       }
       argcount++;
     }
     fclose(fp); fp= NULL;
   }
   if(pass==0){
     *argc= argcount;
     if(argcount>0) {
       *argv= (char **) malloc(argcount*sizeof(char *));
       *argidx= (int *) malloc(argcount*sizeof(int));
       *arglno= (int *) malloc(argcount*sizeof(int));
       if(*argv==NULL || *argidx==NULL || *arglno==NULL)
         {ret= -1; goto ex;}
     }
     for(i=0;i<*argc;i++) {
       (*argv)[i]= NULL;
       (*argidx)[i]= -1;
       (*arglno)[i]= -1;
     }
   }     
 }

 ret= 1;
ex:;
 if(fp!=NULL)
   fclose(fp);
 return(ret);
}


/** Combine environment variable HOME with given filename
    @param filename Address relative to $HOME
    @param fileadr Resulting combined address
    @param fa_size Size of array fileadr
    @param flag Unused yet
    @return 1=ok , 0=no HOME variable , -1=result address too long
*/
int Sfile_home_adr_s(char *filename, char *fileadr, int fa_size, int flag)
{
 char *home;

 strcpy(fileadr,filename);
 home= getenv("HOME");
 if(home==NULL)
   return(0);
 if(strlen(home)+strlen(filename)+1>=fa_size)
   return(-1);
 strcpy(fileadr,home);
 if(filename[0]!=0){
   strcat(fileadr,"/");
   strcat(fileadr,filename);
 }
 return(1);
}


#endif /* ! Cdrskin_extra_leaN */


/* --------------------------------------------------------------------- */

/** Address translation table for users/applications which do not look
   for the output of -scanbus but guess a Bus,Target,Lun on their own.
*/

/** The maximum number of entries in the address translation table */
#define Cdradrtrn_leN 256

/** The address prefix which will prevent translation */
#define Cdrskin_no_transl_prefiX "LITERAL_ADR:"


struct CdradrtrN {
 char *from_address[Cdradrtrn_leN];
 char *to_address[Cdradrtrn_leN];
 int fill_counter;
};


#ifndef Cdrskin_extra_leaN

/** Create a device address translator object */
int Cdradrtrn_new(struct CdradrtrN **trn, int flag)
{
 struct CdradrtrN *o;
 int i;

 (*trn)= o= TSOB_FELD(struct CdradrtrN,1);
 if(o==NULL)
   return(-1);
 for(i= 0;i<Cdradrtrn_leN;i++) {
   o->from_address[i]= NULL;
   o->to_address[i]= NULL;
 }
 o->fill_counter= 0;
 return(1);
}


/** Release from memory a device address translator object */
int Cdradrtrn_destroy(struct CdradrtrN **o, int flag)
{
 int i;
 struct CdradrtrN *trn;
 
 trn= *o;
 if(trn==NULL)
   return(0);
 for(i= 0;i<trn->fill_counter;i++) {
   if(trn->from_address[i]!=NULL)
     free(trn->from_address[i]);
   if(trn->to_address[i]!=NULL)
     free(trn->to_address[i]);
 }
 free((char *) trn);
 *o= NULL;
 return(1);
}


/** Add a translation pair to the table 
    @param trn The translator which shall learn
    @param from The user side address
    @param to The cdrskin side address
    @param flag Bitfield for control purposes:
                bit0= "from" contains from+to address, to[0] contains delimiter
*/
int Cdradrtrn_add(struct CdradrtrN *trn, char *from, char *to, int flag)
{
 char buf[2*Cdrskin_adrleN+1],*from_pt,*to_pt;
 int cnt;

 cnt= trn->fill_counter;
 if(cnt>=Cdradrtrn_leN)
   return(-1);
 if(flag&1) {
   if(strlen(from)>=sizeof(buf))
     return(0);
   strcpy(buf,from);
   to_pt= strchr(buf,to[0]);
   if(to_pt==NULL)
     return(0);
   *(to_pt)= 0;
   from_pt= buf;
   to_pt++;
 } else {
   from_pt= from;
   to_pt= to;
 }
 if(strlen(from)>=Cdrskin_adrleN || strlen(to)>=Cdrskin_adrleN)
   return(0);
 trn->from_address[cnt]= malloc(strlen(from_pt)+1);
 trn->to_address[cnt]= malloc(strlen(to_pt)+1);
 if(trn->from_address[cnt]==NULL ||
    trn->to_address[cnt]==NULL)
   return(-2);
 strcpy(trn->from_address[cnt],from_pt);
 strcpy(trn->to_address[cnt],to_pt);
 trn->fill_counter++;
 return(1);
}


/** Apply eventual device address translation
    @param trn The translator 
    @param from The address from which to translate
    @param driveno With backward translation only: The libburn drive number
    @param to The result of the translation
    @param flag Bitfield for control purposes:
                bit0= translate backward
    @return <=0 error, 1=no translation found, 2=translation found,
            3=collision avoided
*/
int Cdradrtrn_translate(struct CdradrtrN *trn, char *from, int driveno,
                        char to[Cdrskin_adrleN], int flag)
{
 int i,ret= 1;
 char *adr;

 to[0]= 0;
 adr= from;
 if(flag&1)
   goto backward;

 if(strncmp(adr,Cdrskin_no_transl_prefiX,
            strlen(Cdrskin_no_transl_prefiX))==0) {
   adr= adr+strlen(Cdrskin_no_transl_prefiX);
   ret= 2;
 } else {
   for(i=0;i<trn->fill_counter;i++)
     if(strcmp(adr,trn->from_address[i])==0)
   break;
   if(i<trn->fill_counter) {
     adr= trn->to_address[i];
     ret= 2;
   }
 }
 if(strlen(adr)>=Cdrskin_adrleN)
   return(-1);
 strcpy(to,adr);
 return(ret);

backward:;
 if(strlen(from)>=Cdrskin_adrleN)
   sprintf(to,"%s%d",Cdrskin_no_transl_prefiX,driveno);
 else
   strcpy(to,from);
 for(i=0;i<trn->fill_counter;i++)
   if(strcmp(from,trn->to_address[i])==0 &&
      strlen(trn->from_address[i])<Cdrskin_adrleN)
 break;
 if(i<trn->fill_counter) {
   ret= 2;
   strcpy(to,trn->from_address[i]);
 } else {
   for(i=0;i<trn->fill_counter;i++)
     if(strcmp(from,trn->from_address[i])==0)
   break;
   if(i<trn->fill_counter)
     if(strlen(from)+strlen(Cdrskin_no_transl_prefiX)<Cdrskin_adrleN) {
       ret= 3;
       sprintf(to,"%s%s",Cdrskin_no_transl_prefiX,from);
     }
 }
 return(ret);
}

#endif /* Cdrskin_extra_leaN */


/* --------------------------------------------------------------------- */


#ifndef Cdrskin_extra_leaN

/* Program is to be linked with cdrfifo.c */
#include "cdrfifo.h"

#else /* !  Cdrskin_extra_leaN */

/* Dummy */

struct CdrfifO {
 int dummy;
};

#endif /* Cdrskin_extra_leaN */


/* --------------------------------------------------------------------- */

/** cdrecord pads up to 600 kB in any case. 
    libburn yields blank result on tracks <~ 600 kB
    cdrecord demands 300 sectors = 705600 bytes for -audio */
static double Cdrtrack_minimum_sizE= 300;


/** This structure represents a track resp. a data source */
struct CdrtracK {

 struct CdrskiN *boss;
 int trackno;
  
 char source_path[Cdrskin_strleN];
 int source_fd;
 int is_from_stdin;
 double fixed_size;
 double padding;
 int set_by_padsize;
 int sector_pad_up; /* enforce single sector padding */
 int track_type;
 double sector_size;
 int track_type_by_default;
 int swap_audio_bytes;

 /* wether the data source is a container of defined size with possible tail */
 int extracting_container;

 /** Optional fifo between input fd and libburn. It uses a pipe(2) to transfer
     data to libburn. 
 */
 int fifo_enabled;

 /** The eventual own fifo object managed by this track object. */
 struct CdrfifO *fifo;

 /** fd[0] of the fifo pipe. This is from where libburn reads its data. */
 int fifo_outlet_fd;
 int fifo_size;
 int fifo_start_at;

 /** The possibly external fifo object which knows the real input fd and
     the fd[1] of the pipe. */
 struct CdrfifO *ff_fifo;
 /** The index number if fifo follow up fd item, -1= own fifo */
 int ff_idx;

 struct burn_track *libburn_track;

};

int Cdrtrack_destroy(struct CdrtracK **o, int flag);
int Cdrtrack_set_track_type(struct CdrtracK *o, int track_type, int flag);


/** Create a track resp. data source object.
    @param track Returns the address of the new object.
    @param boss The cdrskin control object (corresponds to session)
    @param trackno The index in the cdrskin tracklist array (is not constant)
    @param flag Bitfield for control purposes:
                bit0= set fifo_start_at to 0
                bit1= track is originally stdin
*/
int Cdrtrack_new(struct CdrtracK **track, struct CdrskiN *boss,
                 int trackno, int flag)
{
 struct CdrtracK *o;
 int ret,skin_track_type;
 int Cdrskin_get_source(struct CdrskiN *skin, char *source_path,
                        double *fixed_size, double *padding,
                        int *set_by_padsize, int *track_type,
                        int *track_type_by_default, int *swap_audio_bytes,
                        int flag);
 int Cdrskin_get_fifo_par(struct CdrskiN *skin, int *fifo_enabled,
                          int *fifo_size, int *fifo_start_at, int flag);

 (*track)= o= TSOB_FELD(struct CdrtracK,1);
 if(o==NULL)
   return(-1);
 o->boss= boss;
 o->trackno= trackno;
 o->source_path[0]= 0;
 o->source_fd= -1;
 o->is_from_stdin= !!(flag&2);
 o->fixed_size= 0.0;
 o->padding= 0.0;
 o->set_by_padsize= 0;
 o->sector_pad_up= Cdrskin_all_tracks_with_sector_paD;
 o->track_type= BURN_MODE1;
 o->sector_size= 2048.0;
 o->track_type_by_default= 1;
 o->swap_audio_bytes= 0;
 o->extracting_container= 0;
 o->fifo_enabled= 0;
 o->fifo= NULL;
 o->fifo_outlet_fd= -1;
 o->fifo_size= 0;
 o->fifo_start_at= -1;
 o->ff_fifo= NULL;
 o->ff_idx= -1;
 o->libburn_track= NULL;
 ret= Cdrskin_get_source(boss,o->source_path,&(o->fixed_size),&(o->padding),
                         &(o->set_by_padsize),&(skin_track_type),
                         &(o->track_type_by_default),&(o->swap_audio_bytes),
                         0);
 if(ret<=0)
   goto failed;
 Cdrtrack_set_track_type(o,skin_track_type,0);

#ifndef Cdrskin_extra_leaN
 ret= Cdrskin_get_fifo_par(boss, &(o->fifo_enabled),&(o->fifo_size),
                           &(o->fifo_start_at),0);
 if(ret<=0)
   goto failed;
#endif /* ! Cdrskin_extra_leaN */

 if(flag&1)
   o->fifo_start_at= 0;
 return(1);
failed:;
 Cdrtrack_destroy(track,0);
 return(-1);
}


/** Release from memory a track object previously created by Cdrtrack_new() */
int Cdrtrack_destroy(struct CdrtracK **o, int flag)
{
 struct CdrtracK *track;

 track= *o;
 if(track==NULL)
   return(0);

#ifndef Cdrskin_extra_leaN
 Cdrfifo_destroy(&(track->fifo),0);
#endif

 if(track->libburn_track!=NULL)
   burn_track_free(track->libburn_track);
 free((char *) track);
 *o= NULL;
 return(1);
}


int Cdrtrack_set_track_type(struct CdrtracK *o, int track_type, int flag)
{
 if(track_type==BURN_AUDIO) {
   o->track_type= BURN_AUDIO;
   o->sector_size= 2352.0;
 } else {
   o->track_type= BURN_MODE1;
   o->sector_size= 2048.0;
 }
 return(1);
}


int Cdrtrack_get_track_type(struct CdrtracK *o, int *track_type, 
                            int *sector_size, int flag)
{
 *track_type= o->track_type;
 *sector_size= o->sector_size;
 return(1);
}


/** 
    @param flag Bitfield for control purposes:
                bit0= size returns number of actually processed source bytes
                      rather than the predicted fixed_size (if available).
                      padding returns the difference from number of written
                      bytes.
*/
int Cdrtrack_get_size(struct CdrtracK *track, double *size, double *padding,
                      double *sector_size, int flag)
{

 *size= track->fixed_size;
 *padding= track->padding;
#ifdef Cdrskin_allow_libburn_taO
 if((flag&1) && track->libburn_track!=NULL) {
   off_t readcounter= 0,writecounter= 0;

   burn_track_get_counters(track->libburn_track,&readcounter,&writecounter);
   *size= readcounter;
   *padding= writecounter-readcounter;
/*
   fprintf(stderr,"cdrskin_debug: sizeof(off_t)=%d\n",
                  sizeof(off_t));
*/
 }

#endif
 *sector_size= track->sector_size;
 return(1);
}


int Cdrtrack_get_fifo(struct CdrtracK *track, struct CdrfifO **fifo, int flag)
{
 *fifo= track->fifo;
 return(1);
}


/** Try wether automatic audio extraction is appropriate and eventually open
    a file descriptor to the raw data.
    @return -3 identified as .wav but with cdrecord-inappropriate parameters
            -2 could not open track source, no use in retrying
            -1 severe error
             0 not appropriate to extract, burn plain file content
             1 to be extracted, *fd is a filedescriptor delivering raw data
*/
int Cdrtrack_extract_audio(struct CdrtracK *track, int *fd, off_t *xtr_size,
                           int flag)
{
 int l, ok= 0;
#ifdef Cdrskin_libburn_has_audioxtR
 struct libdax_audioxtr *xtr= NULL;
 char *fmt,*fmt_info;
 int num_channels,sample_rate,bits_per_sample,msb_first,ret;
#endif 

 *fd= -1;

 if(track->track_type!=BURN_AUDIO && !track->track_type_by_default)
   return(0);
 l= strlen(track->source_path);
 if(l>=4)
   if(strcmp(track->source_path+l-4,".wav")==0) 
     ok= 1;
 if(l>=3)
   if(strcmp(track->source_path+l-3,".au")==0)
     ok= 1;
 if(!ok)
   return(0);

 if(track->track_type_by_default) {
   Cdrtrack_set_track_type(track,BURN_AUDIO,0);
   track->track_type_by_default= 2;
   fprintf(stderr,"cdrskin: NOTE : Activated -audio for '%s'\n",
           track->source_path);
 }

#ifdef Cdrskin_libburn_has_audioxtR

 ret= libdax_audioxtr_new(&xtr,track->source_path,0);
 if(ret<=0)
   return(ret);
 libdax_audioxtr_get_id(xtr,&fmt,&fmt_info,
                     &num_channels,&sample_rate,&bits_per_sample,&msb_first,0);
 if((strcmp(fmt,".wav")!=0 && strcmp(fmt,".au")!=0) || 
    num_channels!=2 || sample_rate!=44100 || bits_per_sample!=16) {
   fprintf(stderr,"cdrskin: ( %s )\n",fmt_info);
   fprintf(stderr,"cdrskin: FATAL : Inappropriate audio coding in '%s'.\n",
                  track->source_path);
   {ret= -3; goto ex;}
 }
 libdax_audioxtr_get_size(xtr,xtr_size,0);
 ret= libdax_audioxtr_detach_fd(xtr,fd,0);
 if(ret<=0)
   {ret= -1*!!ret; goto ex;}
 track->swap_audio_bytes= !!msb_first;
 track->extracting_container= 1;
 fprintf(stderr,"cdrskin: NOTE : %.f %saudio bytes in '%s'\n",
                (double) *xtr_size, (msb_first ? "" : "(-swab) "),
                track->source_path);
 ret= 1;
ex:
 libdax_audioxtr_destroy(&xtr,0);
 return(ret);

#else /* Cdrskin_libburn_has_audioxtR */

 return(0);

#endif 
}


/** Deliver an open file descriptor corresponding to the source path of track.
    @return <=0 error, 1 success
*/
int Cdrtrack_open_source_path(struct CdrtracK *track, int *fd, int flag)
{
 int is_wav= 0, size_from_file= 0;
 off_t xtr_size= 0;
 struct stat stbuf;

 if(track->source_path[0]=='-' && track->source_path[1]==0)
   *fd= 0;
 else if(track->source_path[0]=='#' && 
         (track->source_path[1]>='0' && track->source_path[1]<='9'))
   *fd= atoi(track->source_path+1);
 else {
   *fd= -1;
   is_wav= Cdrtrack_extract_audio(track,fd,&xtr_size,0);
   if(is_wav==-1)
     return(-1);
   if(is_wav==-3)
     return(0);
   if(is_wav==0)
     *fd= open(track->source_path,O_RDONLY);
   if(*fd==-1) {
     fprintf(stderr,"cdrskin: failed to open source address '%s'\n",
             track->source_path);
     fprintf(stderr,"cdrskin: errno=%d , \"%s\"\n",errno,
                    errno==0?"-no error code available-":strerror(errno));
     return(0);
   }
   if(track->fixed_size<=0) {
     if(xtr_size>0) {
       track->fixed_size= xtr_size;
       size_from_file= 1;
     } else {
       if(fstat(*fd,&stbuf)!=-1) {
         if((stbuf.st_mode&S_IFMT)==S_IFREG) {
           track->fixed_size= stbuf.st_size;
           size_from_file= 1;
         } /* all other types are assumed of open ended size */
       }
     }
   }
 }

#ifdef Cdrskin_allow_libburn_taO

 if(track->fixed_size < Cdrtrack_minimum_sizE * track->sector_size
    && (track->fixed_size>0 || size_from_file)) {

#else

 if(track->fixed_size < Cdrtrack_minimum_sizE * track->sector_size) {

#endif

   if(track->track_type == BURN_AUDIO) {
     /* >>> cdrecord: We differ in automatic padding with audio:
       Audio tracks must be at least 705600 bytes and a multiple of 2352.
     */
     fprintf(stderr,
             "cdrskin: FATAL : Audio tracks must be at least %.f bytes\n",
             Cdrtrack_minimum_sizE*track->sector_size);
     return(0);
   } else {
     fprintf(stderr,
             "cdrskin: NOTE : Enforcing minimum track size of %.f bytes\n",
             Cdrtrack_minimum_sizE*track->sector_size);
     track->fixed_size= Cdrtrack_minimum_sizE*track->sector_size;
   }
 }
 track->source_fd= *fd;
 return(*fd>=0);
}


#ifndef Cdrskin_extra_leaN

/** Install a fifo object between data source and libburn.
    Its parameters are known to track.
    @param outlet_fd Returns the filedescriptor of the fifo outlet.
    @param previous_fifo Object address for chaining or follow-up attachment.
    @param flag Bitfield for control purposes:
                bit0= Debugging verbosity
                bit1= Do not create and attach a new fifo 
                      but attach new follow-up fd pair to previous_fifo
                bit2= Do not enforce fixed_size if not container extraction
    @return <=0 error, 1 success
*/
int Cdrtrack_attach_fifo(struct CdrtracK *track, int *outlet_fd, 
                         struct CdrfifO *previous_fifo, int flag)
{
 struct CdrfifO *ff;
 int source_fd,pipe_fds[2],ret;

 *outlet_fd= -1;
 if(track->fifo_size<=0)
   return(2);
 ret= Cdrtrack_open_source_path(track,&source_fd,0);
 if(ret<=0)
   return(ret);
 if(pipe(pipe_fds)==-1)
   return(0);

 Cdrfifo_destroy(&(track->fifo),0);
 if(flag&2) {
   ret= Cdrfifo_attach_follow_up_fds(previous_fifo,source_fd,pipe_fds[1],0);
   if(ret<=0)
     return(ret);
   track->ff_fifo= previous_fifo;
   track->ff_idx= ret-1;
 } else {

   /* >>> ??? obtain track sector size and use instead of 2048 ? */

   ret= Cdrfifo_new(&ff,source_fd,pipe_fds[1],2048,track->fifo_size,0);
   if(ret<=0)
     return(ret);
   if(previous_fifo!=NULL)
     Cdrfifo_attach_peer(previous_fifo,ff,0);
   track->fifo= track->ff_fifo= ff;
   track->ff_idx= -1;
 }
 track->fifo_outlet_fd= pipe_fds[0];

 if((track->extracting_container || !(flag&4)) && track->fixed_size>0)
   Cdrfifo_set_fd_in_limit(track->ff_fifo,track->fixed_size,track->ff_idx,0);

 if(flag&1)
   printf(
        "cdrskin_debug: track %d fifo replaced source_address '%s' by '#%d'\n",
        track->trackno+1,track->source_path,track->fifo_outlet_fd);
 sprintf(track->source_path,"#%d",track->fifo_outlet_fd);
 track->source_fd= track->fifo_outlet_fd;
 *outlet_fd= track->fifo_outlet_fd;
 return(1);
}


/** Read data into the fifo until either it is full or the data source is
    exhausted.
    @return <=0 error, 1 success
*/
int Cdrtrack_fill_fifo(struct CdrtracK *track, int flag)
{
 int ret,buffer_fill,buffer_space;

 if(track->fifo==NULL || track->fifo_start_at==0)
   return(2);
 if(track->fifo_start_at>0 && track->fifo_start_at<track->fifo_size)
   printf(
      "cdrskin: NOTE : Input buffer will be initially filled up to %d bytes\n",
      track->fifo_start_at);
 printf("Waiting for reader process to fill input buffer ... ");
 fflush(stdout);
 ret= Cdrfifo_fill(track->fifo,track->fifo_start_at,0);
 if(ret<=0)
   return(ret);

/** Ticket 55: check fifos for input, throw error on 0-bytes from stdin
    @return <=0 abort run, 1 go on with burning
*/
 if(track->is_from_stdin) {
   ret= Cdrfifo_get_buffer_state(track->fifo,&buffer_fill,&buffer_space,0);
   if(ret<0 || buffer_fill<=0) {
     fprintf(stderr,"\ncdrskin: FATAL : (First track) fifo did not read a single byte from stdin\n");
     return(0);
   }
 }
 return(1);
}

#endif /* ! Cdrskin_extra_leaN */


/** Create a corresponding libburn track object and add it to the libburn
    session. This may change the trackno index set by Cdrtrack_new().
*/
int Cdrtrack_add_to_session(struct CdrtracK *track, int trackno,
                            struct burn_session *session, int flag)
/*
 bit0= debugging verbosity
 bit1= apply padding hack (<<< should be unused for now)
*/
{
 struct burn_track *tr;
 struct burn_source *src= NULL;
 double padding,lib_padding;
 int ret,sector_pad_up;
 double fixed_size;
 int source_fd;

 track->trackno= trackno;
 tr= burn_track_create();
 track->libburn_track= tr;

 /* Note: track->track_type may get set in here */
 if(track->source_fd==-1) {
   ret= Cdrtrack_open_source_path(track,&source_fd,0);
   if(ret<=0)
     goto ex;
 }

 padding= 0.0;
 sector_pad_up= track->sector_pad_up;
 if(track->padding>0) {
   if(track->set_by_padsize || track->track_type!=BURN_AUDIO)
     padding= track->padding;
   else
     sector_pad_up= 1;
 }
 if(flag&2)
   lib_padding= 0.0;
 else
   lib_padding= padding;
 if(flag&1) {
   if(sector_pad_up) {
     ClN(fprintf(stderr,"cdrskin_debug: track %d telling burn_track_define_data() to pad up last sector\n",trackno+1));
   }
   if(lib_padding>0 || !sector_pad_up) {
     ClN(fprintf(stderr,
 "cdrskin_debug: track %d telling burn_track_define_data() to pad %.f bytes\n",
           trackno+1,lib_padding));
   }
 }
 burn_track_define_data(tr,0,(int) lib_padding,sector_pad_up,
                        track->track_type);
 burn_track_set_byte_swap(tr,
                   (track->track_type==BURN_AUDIO && track->swap_audio_bytes));
 fixed_size= track->fixed_size;
 if((flag&2) && track->padding>0) {
   if(flag&1)
     ClN(fprintf(stderr,"cdrskin_debug: padding hack : %.f + %.f = %.f\n",
                 track->fixed_size,track->padding,
                 track->fixed_size+track->padding));
   fixed_size+= track->padding;
 }
 src= burn_fd_source_new(track->source_fd,-1,(off_t) fixed_size);

 if(src==NULL) {
   fprintf(stderr,
           "cdrskin: FATAL : Could not create libburn data source object\n");
   {ret= 0; goto ex;}
 }
 if(burn_track_set_source(tr,src)!=BURN_SOURCE_OK) {
   fprintf(stderr,"cdrskin: FATAL : libburn rejects data source object\n");
   {ret= 0; goto ex;}
 }
 burn_session_add_track(session,tr,BURN_POS_END);
 ret= 1;
ex:
 if(src!=NULL)
   burn_source_free(src);
 return(ret);
}


/** Release libburn track information after a session is done */
int Cdrtrack_cleanup(struct CdrtracK *track, int flag)
{
 if(track->libburn_track==NULL)
   return(0);
 burn_track_free(track->libburn_track);
 track->libburn_track= NULL;
 return(1);
}


int Cdrtrack_ensure_padding(struct CdrtracK *track, int flag)
/*
flag:
 bit0= debugging verbosity
*/
{
 if(track->track_type!=BURN_AUDIO)
   return(2);
 if(flag&1)
   fprintf(stderr,"cdrskin_debug: enforcing -pad on last -audio track\n");
 track->sector_pad_up= 1;
 return(1);
}


#ifndef Cdrskin_extra_leaN

/** Try to read bytes from the track's fifo outlet and eventually discard
    them. Not to be called unless the track is completely written.
*/
int Cdrtrack_has_input_left(struct CdrtracK *track, int flag)
{
 struct timeval wt;
 fd_set rds,wts,exs;
 int ready,ret;
 char buf[2];

 if(track->fifo_outlet_fd<=0)
   return(0);
 FD_ZERO(&rds);
 FD_ZERO(&wts);
 FD_ZERO(&exs);
 FD_SET(track->fifo_outlet_fd,&rds); 
 wt.tv_sec= 0;
 wt.tv_usec= 0;
 ready= select(track->fifo_outlet_fd+1,&rds,&wts,&exs,&wt);
 if(ready<=0)
   return(0);
 ret= read(track->fifo_outlet_fd,buf,1);
 if(ret>0)
   return(1);
 return(0);
}

#endif /* ! Cdrskin_extra_leaN */
 

/* --------------------------------------------------------------------- */

/** The list of startup file names */
#define Cdrpreskin_rc_nuM 3

static char Cdrpreskin_sys_rc_nameS[Cdrpreskin_rc_nuM][80]= {
 "/etc/default/cdrskin",
 "/etc/opt/cdrskin/rc",
 "placeholder for $HOME/.cdrskinrc"
};


/** A structure which bundles several parameters for creation of the CdrskiN
    object. It finally becomes a managed subordinate of the CdrskiN object. 
*/
struct CdrpreskiN {

 /* to be transfered into skin */
 int verbosity;
 char queue_severity[81];
 char print_severity[81];

 /** Stores eventually given absolute device address before translation */
 char raw_device_adr[Cdrskin_adrleN];

 /** Stores an eventually given translated absolute device address between
    Cdrpreskin_setup() and Cdrskin_create() .
 */
 char device_adr[Cdrskin_adrleN];

 /** The eventual address translation table */
 struct CdradrtrN *adr_trn;

 /** Memorizes the abort handling mode from presetup to creation of 
     control object. Defined handling modes are:
     0= no abort handling
     1= try to cancel, release, exit (leave signal mode as set by caller)
     2= try to ignore all signals
     3= mode 1 in normal operation, mode 2 during abort handling
     4= mode 1 in normal operation, mode 0 during abort handling
    -1= install abort handling 1 only in Cdrskin_burn() after burning started
 */
 int abort_handler;

 /** Wether to allow getuid()!=geteuid() */
 int allow_setuid;

 /** Wether to allow user provided addresses like #4 */
 int allow_fd_source;

 /** Wether an option is given which needs a full bus scan */
 int no_whitelist;

 /** Wether the translated device address shall not follow softlinks, device
     clones and SCSI addresses */
 int no_convert_fs_adr;

 /** Wether Bus,Target,Lun addresses shall be converted literally as old
     Pseudo SCSI-Adresses. New default is to use (possibly system emulated)
     real SCSI addresses via burn_drive_convert_scsi_adr() and literally
     emulated and cdrecord-incompatible ATA: addresses. */
 int old_pseudo_scsi_adr;

 /** Wether bus scans shall exit!=0 if no drive was found */
 int scan_demands_drive;

 /** Wether to abort when a busy drive is encountered during bus scan */
 int abort_on_busy_drive;

 /** Wether to try to avoid collisions when opening drives */
 int drive_exclusive;

 /** Wether to try to wait for unwilling drives to become willing to open */
 int drive_blocking;

 /** Explicit write mode option is determined before skin processes
     any track arguments */
 char write_mode_name[80];

#ifndef Cdrskin_extra_leaN

 /** List of startupfiles */
 char rc_filenames[Cdrpreskin_rc_nuM][Cdrskin_strleN];
 int rc_filename_count;

 /** Non-argument options from startupfiles */
 int pre_argc;
 char **pre_argv;
 int *pre_argidx;
 int *pre_arglno;

#endif /* ! Cdrskin_extra_leaN */

};



/** Create a preliminary cdrskin program run control object. It will become
    part of the final control object.
    @param preskin Returns pointer to resulting
    @param flag Bitfield for control purposes: unused yet
    @return <=0 error, 1 success
*/
int Cdrpreskin_new(struct CdrpreskiN **preskin, int flag)
{
 struct CdrpreskiN *o;
 int i;

 (*preskin)= o= TSOB_FELD(struct CdrpreskiN,1);
 if(o==NULL)
   return(-1);

 o->verbosity= 0;
 strcpy(o->queue_severity,"NEVER");
 strcpy(o->print_severity,"SORRY");
 o->raw_device_adr[0]= 0;
 o->device_adr[0]= 0;
 o->adr_trn= NULL;
 o->abort_handler= 3;
 o->allow_setuid= 0;
 o->allow_fd_source= 0;
 o->no_whitelist= 0;
 o->no_convert_fs_adr= 0;
#ifdef Cdrskin_libburn_has_convert_scsi_adR
 o->old_pseudo_scsi_adr= 0;
#else
 o->old_pseudo_scsi_adr= 1;
#endif
 o->scan_demands_drive= 0;
 o->abort_on_busy_drive= 0;
 o->drive_exclusive= 1;
 o->drive_blocking= 0;
 strcpy(o->write_mode_name,"DEFAULT");

#ifndef Cdrskin_extra_leaN
 o->rc_filename_count= Cdrpreskin_rc_nuM;
 for(i=0;i<o->rc_filename_count-1;i++)
   strcpy(o->rc_filenames[i],Cdrpreskin_sys_rc_nameS[i]);
 o->rc_filenames[o->rc_filename_count-1][0]= 0;
 o->pre_argc= 0;
 o->pre_argv= NULL;
 o->pre_argidx= NULL;
 o->pre_arglno= NULL;
#endif /* ! Cdrskin_extra_leaN */

 return(1);
}


int Cdrpreskin_destroy(struct CdrpreskiN **preskin, int flag)
{
 struct CdrpreskiN *o;

 o= *preskin;
 if(o==NULL)
   return(0);

#ifndef Cdrskin_extra_leaN
 if((o->pre_arglno)!=NULL)
   free((char *) o->pre_arglno);
 if((o->pre_argidx)!=NULL)
   free((char *) o->pre_argidx);
 if(o->pre_argc>0 && o->pre_argv!=NULL)
   Sfile_destroy_argv(&(o->pre_argc),&(o->pre_argv),0);
 Cdradrtrn_destroy(&(o->adr_trn),0);
#endif /* ! Cdrskin_extra_leaN */

 free((char *) o);
 *preskin= NULL;
 return(1);
}


int Cdrpreskin_set_severities(struct CdrpreskiN *preskin, char *queue_severity,
                              char *print_severity, int flag)
{
/*
 if(preskin->verbosity>=Cdrskin_verbose_debuG)
   fprintf(stderr,
           "cdrskin: DEBUG : queue_severity='%s'  print_severity='%s'\n",
           queue_severity,print_severity);
*/
 
 if(queue_severity!=NULL)
   strcpy(preskin->queue_severity,queue_severity);
 if(print_severity!=NULL)
   strcpy(preskin->print_severity,print_severity);
#ifdef Cdrskin_libburn_has_burn_msgS
 burn_msgs_set_severities(preskin->queue_severity, preskin->print_severity,
                          "cdrskin: ");
#endif
 return(1);
}
 

int Cdrpreskin_initialize_lib(struct CdrpreskiN *preskin, int flag)
{
 int ret;

 ret= burn_initialize();
 if(ret==0) {
   fprintf(stderr,"cdrskin : FATAL : Initialization of libburn failed\n");
   return(0);
 }
 Cdrpreskin_set_severities(preskin,NULL,NULL,0);
 return(1);
}


/** Enable queuing of libburn messages or disable and print queue content.
    @param flag Bitfield for control purposes:
                bit0= enable queueing, else disable and print
*/
int Cdrpreskin_queue_msgs(struct CdrpreskiN *o, int flag)
{
#ifdef Cdrskin_libburn_has_burn_msgS
#ifndef Cdrskin_extra_leaN
#define Cdrskin_debug_libdax_msgS 1
#endif
/* <<< In cdrskin there is not much sense in queueing library messages.
        It is done here only for debugging */
#ifdef Cdrskin_debug_libdax_msgS

 int ret;
 static char queue_severity[81]= {"NEVER"}, print_severity[81]= {"SORRY"};
 static int queueing= 0;
 char msg[BURN_MSGS_MESSAGE_LEN],msg_severity[81],filler[81];
 int error_code,os_errno,first,i;

 if(flag&1) {
   if(!queueing) {
     strcpy(queue_severity,o->queue_severity);
     strcpy(print_severity,o->print_severity);
   }
   if(o->verbosity>=Cdrskin_verbose_debuG)
     Cdrpreskin_set_severities(o,"DEBUG","NEVER",0);
   else
     Cdrpreskin_set_severities(o,"SORRY","NEVER",0);
   queueing= 1;
   return(1);
 }

 if(queueing)
   Cdrpreskin_set_severities(o,queue_severity,print_severity,0);
 queueing= 0;

 for(first= 1; ; first= 0) {
   ret= burn_msgs_obtain("ALL",&error_code,msg,&os_errno,msg_severity);
   if(ret==0)
 break;
   if(ret<0) {
     fprintf(stderr,
           "cdrskin: NOTE : Please inform libburn-hackers@pykix.org about:\n");
     fprintf(stderr,
           "cdrskin:        burn_msgs_obtain() returns %d\n",ret);
 break;
   }
   if(first)
     fprintf(stderr,
"cdrskin: -------------------- Messages from Libburn ---------------------\n");
   for(i=0;msg_severity[i]!=0;i++)
     filler[i]= ' ';
   filler[i]= 0;
   fprintf(stderr,"cdrskin: %s : %s\n",msg_severity,msg);
   if(strcmp(msg_severity,"DEBUG")!=0 && os_errno!=0)
     fprintf(stderr,"cdrskin: %s   ( errno=%d  '%s')\n",
                    filler,os_errno,strerror(os_errno));
 }
 if(first==0)
   fprintf(stderr,
"cdrskin: ----------------------------------------------------------------\n");

#endif /* Cdrskin_debug_libdax_msgS */
#endif /* Cdrskin_libburn_has_burn_msgS */

 return(1);
}


/** Convert a cdrecord-style device address into a libburn device address or
    into a libburn drive number. It depends on the "scsibus" number of the
    cdrecord-style address which kind of libburn address emerges:
      bus=0 : drive number , bus=1 : /dev/sgN , bus=2 : /dev/hdX
    (This call intentionally has no CdrpreskiN argument)
    @param flag Bitfield for control purposes: 
                bit0= old_pseudo_scsi_adr
    @return 1 success, 0=no recognizable format, -1=severe error,
            -2 could not find scsi device, -3 address format error
*/
int Cdrpreskin__cdrecord_to_dev(char *adr, char device_adr[Cdrskin_adrleN],
                                int *driveno, int flag)
{
 int comma_seen= 0,digit_seen= 0,busno= 0,k,lun_no= -1;

 *driveno= -1;
 device_adr[0]= 0;
 if(strlen(adr)==0)
   return(0);

 /* read the trailing numeric string as device address code */
 /* accepts "1" , "0,1,0" , "ATA:0,1,0" , ... */
 for(k= strlen(adr)-1;k>=0;k--) {
   if(adr[k]==',' && !comma_seen) {
     sscanf(adr+k+1,"%d",&lun_no);
     comma_seen= 1;
     digit_seen= 0;
 continue;
   }
   if(adr[k]<'0' || adr[k]>'9')
 break;
   digit_seen= 1;
 }
 if(!digit_seen) {
   k= strlen(adr)-1;
   if(adr[k]==':' || (adr[k]>='A' && adr[k]<='Z')) { /* empty prefix ? */
     *driveno= 0;
     return(1);
   }
   return(0);
 }
 sscanf(adr+k+1,"%d",driveno);

 digit_seen= 0;
 if(k>0) if(adr[k]==',') {
   for(k--;k>=0;k--) {
     if(adr[k]<'0' || adr[k]>'9')
   break;
     digit_seen= 1;
   }
   if(digit_seen) {
     sscanf(adr+k+1,"%d",&busno);
     if(flag&1) {
       /* look for symbolic bus :  1=/dev/sgN  2=/dev/hdX */
       if(busno==1) {
         sprintf(device_adr,"/dev/sg%d",*driveno);
       } else if(busno==2) {
         sprintf(device_adr,"/dev/hd%c",'a'+(*driveno));
       } else if(busno!=0) {
         fprintf(stderr,
  "cdrskin: FATAL : dev=[Prefix:]Bus,Target,Lun expects Bus out of {0,1,2}\n");
         return(-3);
       }
     } else {
       if(busno<0) {
         fprintf(stderr,
     "cdrskin: FATAL : dev=[Prefix:]Bus,Target,Lun expects Bus number >= 0\n");
         return(-3);
       }
       if((strncmp(adr,"ATA",3)==0 && (adr[3]==0 || adr[3]==':')) ||
          (strncmp(adr,"ATAPI",5)==0 && (adr[5]==0 || adr[5]==':'))) {

         if(busno>12 || (*driveno)<0 || (*driveno)>1) {
           fprintf(stderr,
"cdrskin: FATAL : dev=ATA:Bus,Target,Lun expects Bus {0..12}, Target {0,1}\n");
           return(-3);
         }
         sprintf(device_adr,"/dev/hd%c",'a'+(2*busno)+(*driveno));

#ifdef Cdrskin_libburn_has_convert_scsi_adR
       } else {
         int ret;

         ret= burn_drive_convert_scsi_adr(busno,-1,-1,*driveno,lun_no,
                                          device_adr);
         if(ret==0) {
           fprintf(stderr,
   "cdrskin: FATAL : Cannot find /dev/sgN with Bus,Target,Lun = %d,%d,%d\n",
                   busno,*driveno,lun_no);
           fprintf(stderr,
   "cdrskin: HINT : This drive may be in use by another program currently\n");
           return(-2);
         } else if(ret<0)
           return(-1);
         return(1);

#endif /* Cdrskin_libburn_has_convert_scsi_adR */
       }
     }
   } 
 }
 return(1);
}


#ifndef Cdrskin_extra_leaN

/** Load content startup files into preskin cache */
int Cdrpreskin_read_rc(struct CdrpreskiN *o, char *progname, int flag)
{
 int ret,i;
 char *filenames_v[3];

 for(i=0;i<o->rc_filename_count;i++)
   filenames_v[i]= o->rc_filenames[i];
 Sfile_home_adr_s(".cdrskinrc",o->rc_filenames[o->rc_filename_count-1],
                  Cdrskin_strleN,0);
 ret= Sfile_multi_read_argv(progname,filenames_v,o->rc_filename_count,
                            &(o->pre_argc),&(o->pre_argv),
                            &(o->pre_argidx),&(o->pre_arglno),4);
 return(ret);
}

#endif /* ! Cdrskin_extra_leaN */


/** Interpret those arguments which do not need libburn or which influence the
    startup of libburn and/or the creation of the CdrskiN object. This is run
    before libburn gets initialized and before Cdrskin_new() is called.
    Options which need libburn or a CdrskiN object are processed in a different
    function named Cdrskin_setup().
    @param flag Bitfield for control purposes: 
                bit0= do not finalize setup
                bit1= do not read and interpret rc files
    @return <=0 error, 1 success , 2 end program run with exit value 0
*/
int Cdrpreskin_setup(struct CdrpreskiN *o, int argc, char **argv, int flag)
/*
return:
 <=0 error
   1 ok
   2 end program run (--help)
*/
{
 int i,ret;
 char *value_pt;

#ifndef Cdrskin_extra_leaN
 if(argc>1) {
   if(strcmp(argv[1],"--no_rc")==0 || strcmp(argv[1],"-version")==0 ||
      strcmp(argv[1],"--help")==0 || strcmp(argv[1],"-help")==0)
     flag|= 2;
 }
 if(!(flag&2)) {
   ret= Cdrpreskin_read_rc(o,argv[0],0);
   if(ret<0)
     return(-1);
   if(o->pre_argc>1) {
     ret= Cdrpreskin_setup(o,o->pre_argc,o->pre_argv,flag|1|2);
     if(ret<=0)
       return(ret);
     /* ??? abort on ret==2 ? */
   }
 }
#endif

 if(argc==1) {
   fprintf(stderr,"cdrskin: SORRY : no options given. Try option  --help\n");
   return(0);
 }
 for (i= 1;i<argc;i++) {

   if(strcmp(argv[i],"--abort_handler")==0) {
     o->abort_handler= 3;

   } else if(strcmp(argv[i],"--allow_setuid")==0) {
     o->allow_setuid= 1;

   } else if(strcmp(argv[i],"blank=help")==0 ||
             strcmp(argv[i],"-blank=help")==0) {

#ifndef Cdrskin_extra_leaN

     fprintf(stderr,"Blanking options:\n");
     fprintf(stderr,"\tall\t\tblank the entire disk\n");
     fprintf(stderr,"\tdisc\t\tblank the entire disk\n");
     fprintf(stderr,"\tdisk\t\tblank the entire disk\n");
     fprintf(stderr,
             "\tfast\t\tminimally blank the entire disk\n");
     fprintf(stderr,
          "\tminimal\t\tminimally blank the entire disk\n");

#else /* ! Cdrskin_extra_leaN */

     goto see_cdrskin_eng_html;

#endif /* ! Cdrskin_extra_leaN */

     if(argc==2)
       {ret= 2; goto final_checks;}

   } else if(strcmp(argv[i],"--bragg_with_audio")==0) {
     /* OBSOLETE 0.2.3 */;

   } else if(strcmp(argv[i],"--demand_a_drive")==0) {
     o->scan_demands_drive= 1;

   } else if(strcmp(argv[i],"--devices")==0) {
     printf("Note: If this hangs for a while then there is a drive with\n");
     printf("      unexpected problems (e.g. ill DMA).\n");
     printf("      One may exclude such a device file by removing r- and w-\n");
     printf("      permissions for all cdrskin users.\n");
     o->no_whitelist= 1;

   } else if(strncmp(argv[i],"dev_translation=",16)==0) {

#ifndef Cdrskin_extra_leaN

     if(o->adr_trn==NULL) {
       ret= Cdradrtrn_new(&(o->adr_trn),0);
       if(ret<=0)
         goto no_adr_trn_mem;
     }
     if(argv[i][16]==0) {
       fprintf(stderr,
         "cdrskin: FATAL : dev_translation= : missing separator character\n");
       return(0);
     }
     ret= Cdradrtrn_add(o->adr_trn,argv[i]+17,argv[i]+16,1);
     if(ret==-2) {
no_adr_trn_mem:;
       fprintf(stderr,
           "cdrskin: FATAL : address_translation= : cannot allocate memory\n");
     } else if(ret==-1)
       fprintf(stderr,
             "cdrskin: FATAL : address_translation= : table full (%d items)\n",
               Cdradrtrn_leN);
     else if(ret==0)
       fprintf(stderr,
  "cdrskin: FATAL : address_translation= : no address separator '%c' found\n",
               argv[i][17]);
     if(ret<=0)
       return(0);

#else /* ! Cdrskin_extra_leaN */

     fprintf(stderr,
       "cdrskin: FATAL : dev_translation= is not available in lean version\n");
     return(0);

#endif /* Cdrskin_extra_leaN */


   } else if(strncmp(argv[i],"-dev=",5)==0) {
     value_pt= argv[i]+5;
     goto set_dev;
   } else if(strncmp(argv[i],"dev=",4)==0) {
     value_pt= argv[i]+4;
set_dev:;
     if(strcmp(value_pt,"help")==0) {

#ifndef Cdrskin_extra_leaN

       printf("Supported SCSI transports for this platform:\n");
       fflush(stdout);
       fprintf(stderr,"\nTransport name:\t\tlibburn\n");
       fprintf(stderr,
         "Transport descr.:\tOpen-source library for writing optical discs\n");
       fprintf(stderr,"Transp. layer ind.:\t\n");
       fprintf(stderr,"Target specifier:\tbus,target,lun\n");
       fprintf(stderr,"Target example:\t\t1,2,0\n");
       fprintf(stderr,"SCSI Bus scanning:\tsupported\n");
       fprintf(stderr,
               "Open via UNIX device:\tsupported (see option --devices)\n");

#else /* ! Cdrskin_extra_leaN */

       goto see_cdrskin_eng_html;

#endif /* Cdrskin_extra_leaN */

       {ret= 2; goto final_checks;}
     }
     if(strlen(value_pt)>=sizeof(o->raw_device_adr))
       goto dev_too_long;
     strcpy(o->raw_device_adr,value_pt);

   } else if(strcmp(argv[i],"--drive_abort_on_busy")==0) {
     o->abort_on_busy_drive= 1;

   } else if(strcmp(argv[i],"--drive_blocking")==0) {
     o->drive_blocking= 1;

   } else if(strcmp(argv[i],"--drive_not_exclusive")==0) {
     o->drive_exclusive= 0;

   } else if(strcmp(argv[i],"--drive_scsi_exclusive")==0) {
     o->drive_exclusive= 2;

   } else if(strcmp(argv[i],"driveropts=help")==0 ||
             strcmp(argv[i],"-driveropts=help")==0) {

#ifndef Cdrskin_extra_leaN

     fprintf(stderr,"Driver options:\n");
     fprintf(stderr,"burnfree\tPrepare writer to use BURN-Free technology\n");
     fprintf(stderr,"noburnfree\tDisable using BURN-Free technology\n");

#else /* ! Cdrskin_extra_leaN */

     goto see_cdrskin_eng_html;

#endif /* Cdrskin_extra_leaN */

     if(argc==2 || (i==2 && argc==3 && strncmp(argv[1],"dev=",4)==0))
       {ret= 2; goto final_checks;}

   } else if(strcmp(argv[i],"--help")==0) {

#ifndef Cdrskin_extra_leaN

     printf("\n");
     printf("Usage: %s [options|source_addresses]\n", argv[0]);
     printf("Burns preformatted data to CD-R or CD-RW via libburn.\n");
     printf("For the cdrecord compatible options which control the work of\n");
     printf(
      "blanking and burning see output of option -help rather than --help.\n");
     printf("Non-cdrecord options:\n");
     printf(" --abort_handler    do not leave the drive in busy state\n");
     printf(" --allow_setuid     disable setuid blocker (very insecure !)\n");
     printf(
         " --any_track        allow source_addresses to match '^-.' or '='\n");
     printf(" --demand_a_drive   exit !=0 on bus scans with empty result\n");
     printf(" --devices          list accessible devices (tells /dev/...)\n");
     printf(
          " dev_translation=<sep><from><sep><to>   set input address alias\n");
     printf("                    e.g.: dev_translation=+ATA:1,0,0+/dev/sg1\n");
     printf(" --drive_abort_on_busy  abort process if busy drive is found\n");
     printf("                    (might be triggered by a busy hard disk)\n");
     printf(" --drive_blocking   try to wait for busy drive to become free\n");
     printf("                    (might be stalled by a busy hard disk)\n");
     printf(" --drive_not_exclusive  do not ask kernel to prevent opening\n");
     printf("                    busy drives. Effect is kernel dependend.\n");
     printf(
         " --drive_scsi_exclusive  try to exclusively reserve device files\n");
     printf("                    /dev/srN, /dev/scdM, /dev/stK of drive.\n");
#ifdef Cdrskin_burn_drive_eject_brokeN
     printf(
          " eject_device=<path>  set the device address for command eject\n");
#endif
     printf(" --fifo_disable     disable fifo despite any fs=...\n");
     printf(" --fifo_per_track   use a separate fifo for each track\n");
     printf(
      " fifo_start_at=<number> do not wait for full fifo but start burning\n");
     printf(
         "                    as soon as the given number of bytes is read\n");
     printf(
          " grab_drive_and_wait=<num>  grab drive, wait given number of\n");
     printf(
          "                    seconds, release drive, and do normal work\n");
     printf(
       " --ignore_signals   try to ignore any signals rather than to abort\n");
     printf(" --no_abort_handler  exit even if the drive is in busy state\n");
     printf(" --no_blank_appendable  refuse to blank appendable CD-RW\n");
     printf(" --no_convert_fs_adr  only literal translations of dev=\n");
     printf(
         " --no_rc            as first argument: do not read startup files\n");
     printf(" --old_pseudo_scsi_adr  use and report literal Bus,Target,Lun\n");
     printf("                    rather than real SCSI and pseudo ATA.\n");
     printf(
          " --single_track     accept only last argument as source_address\n");

#ifdef Cdrskin_allow_libburn_taO
     printf(
          " tao_to_sao_tsize=<num>  use num as fixed track size if in a\n");
     printf(
          "                    non-TAO mode track data are read from \"-\"\n");
     printf(
          "                    and no tsize= was specified.\n");
#else
     printf(
          " tao_to_sao_tsize=<num>  substitute -tao by -sao and eventually\n");
     printf("                    augment input from \"-\" by tsize=<num>\n");
     printf("                    (set tao_to_sao_tsize=0 to disable it)\n");
#endif

     printf(
        "Preconfigured arguments are read from the following startup files\n");
     printf(
          "if they exist and are readable. The sequence is as listed here:\n");
     printf("  /etc/default/cdrskin  /etc/opt/cdrskin/rc  $HOME/.cdrskinrc\n");
     printf("Each file line is a single argument. No whitespace.\n");
     printf(
         "By default any argument that does not match grep '^-.' or '=' is\n");
     printf(
         "used as track source. If it is \"-\" then stdin is used.\n");
     printf("cdrskin  : http://scdbackup.sourceforge.net/cdrskin_eng.html\n");
     printf("           mailto:scdbackup@gmx.net  (Thomas Schmitt)\n");
     printf("libburn  : http://libburn.pykix.org\n");
     printf("cdrecord : ftp://ftp.berlios.de/pub/cdrecord/\n");
     printf("My respect to the authors of cdrecord and libburn.\n");
     printf("scdbackup: http://scdbackup.sourceforge.net/main_eng.html\n");
     printf("\n");

#else /* ! Cdrskin_extra_leaN */

see_cdrskin_eng_html:;
     printf("This is a capability reduced lean version without help texts.\n");
     printf("See  http://scdbackup.sourceforge.net/cdrskin_eng.html\n");

#endif /* Cdrskin_extra_leaN */


     {ret= 2; goto final_checks;}
   } else if(strcmp(argv[i],"-help")==0) {

#ifndef Cdrskin_extra_leaN

     fprintf(stderr,"Usage: %s [options|source_addresses]\n",argv[0]);
     fprintf(stderr,"Note: This is not cdrecord. See cdrskin start message on stdout. See --help.\n");
     fprintf(stderr,"Options:\n");
     fprintf(stderr,"\t-version\tprint version information and exit\n");
     fprintf(stderr,
             "\tdev=target\tpseudo-SCSI target to use as CD-Recorder\n");
     fprintf(stderr,
         "\tgracetime=#\tset the grace time before starting to write to #.\n");
     fprintf(stderr,"\t-v\t\tincrement verbose level by one\n");
     fprintf(stderr,
             "\tdriveropts=opt\topt= one of {burnfree,noburnfree,help}\n");
     fprintf(stderr,
             "\t-checkdrive\tcheck if a driver for the drive is present\n");
     fprintf(stderr,"\t-scanbus\tscan the SCSI bus and exit\n");
     fprintf(stderr,"\tspeed=#\t\tset speed of drive\n");
     fprintf(stderr,"\tblank=type\tblank a CD-RW disc (see blank=help)\n");
     fprintf(stderr,
             "\tfs=#\t\tSet fifo size to # (0 to disable, default is 4 MB)\n");
     fprintf(stderr,
       "\t-eject\t\teject the disk after doing the work (might be ignored)\n");
     fprintf(stderr,"\t-dummy\t\tdo everything with laser turned off\n");
#ifdef Cdrskin_libburn_has_multI
     fprintf(stderr,
             "\t-msinfo\t\tretrieve multi-session info for mkisofs >= 1.10\n");
#endif
     fprintf(stderr,"\t-toc\t\tretrieve and print TOC/PMA data\n");
     fprintf(stderr,
             "\t-atip\t\tretrieve media state, print \"Is *erasable\"\n");
#ifdef Cdrskin_libburn_has_multI
     fprintf(stderr,
             "\t-multi\t\tgenerate a TOC that allows multi session\n");
#endif
     fprintf(stderr,
           "\t-force\t\tforce to continue on some errors to allow blanking\n");
#ifdef Cdrskin_allow_libburn_taO
     fprintf(stderr,"\t-tao\t\tWrite disk in TAO mode.\n");
#endif
     fprintf(stderr,"\t-dao\t\tWrite disk in SAO mode.\n");
     fprintf(stderr,"\t-sao\t\tWrite disk in SAO mode.\n");
     fprintf(stderr,"\t-raw96r\t\tWrite disk in RAW/RAW96R mode\n");
     fprintf(stderr,"\ttsize=#\t\tannounces exact size of source data\n");
     fprintf(stderr,"\tpadsize=#\tAmount of padding\n");
     fprintf(stderr,"\t-audio\t\tSubsequent tracks are CD-DA audio tracks\n");
     fprintf(stderr,
            "\t-data\t\tSubsequent tracks are CD-ROM data mode 1 (default)\n");
     fprintf(stderr,"\t-pad\t\tpadsize=30k\n");
     fprintf(stderr,
        "\t-nopad\t\tDo not pad (default, but applies only to data tracks)\n");
     fprintf(stderr,
       "\t-swab\t\tAudio data source is byte-swapped (little-endian/Intel)\n");
     fprintf(stderr,"\t-help\t\tprint this text to stderr and exit\n");
     fprintf(stderr,
             "Without option -data, .wav and .au files are extracted and burned as -audio.\n");
     fprintf(stderr,
    "By default any argument that does not match grep '^-.' or '=' is used\n");
     fprintf(stderr,
         "as track source address. Address \"-\" means stdin.\n");
     fprintf(stderr,
        "cdrskin will ensure that an announced tsize= is written even if\n");
     fprintf(stderr,
        "the source delivers fewer bytes. But 0 bytes from stdin with fifo\n");
     fprintf(stderr,
        "enabled will lead to abort and no burn attempt at all.\n");

#else /* ! Cdrskin_extra_leaN */

     fprintf(stderr,"Note: This is not cdrecord. See cdrskin start message on stdout.\n");
     fprintf(stderr,
          "(writer profile: -atip retrieve, blank=type, -eject after work)\n");
     goto see_cdrskin_eng_html;

#endif /* Cdrskin_extra_leaN */

     {ret= 2; goto final_checks;}

   } else if(strcmp(argv[i],"--ignore_signals")==0) {
     o->abort_handler= 2;

   } else if(strcmp(argv[i],"--no_abort_handler")==0) {
     o->abort_handler= 0;

   } else if(strcmp(argv[i],"--no_convert_fs_adr")==0) {
     o->no_convert_fs_adr= 1;

   } else if(strcmp(argv[i],"--old_pseudo_scsi_adr")==0) {
     o->old_pseudo_scsi_adr= 1;

   } else if(strcmp(argv[i],"--no_rc")==0) {
     if(i!=1)
       fprintf(stderr,
        "cdrskin: NOTE : option --no_rc would only work as first argument.\n");

   } else if(strcmp(argv[i],"-raw96r")==0) {
     strcpy(o->write_mode_name,"RAW/RAW96R");

   } else if(strcmp(argv[i],"-sao")==0 || strcmp(argv[i],"-dao")==0) {
     strcpy(o->write_mode_name,"SAO");

   } else if(strcmp(argv[i],"-scanbus")==0) {
     o->no_whitelist= 1;

   } else if(strcmp(argv[i],"-tao")==0) {
     strcpy(o->write_mode_name,"TAO");

   } else if(strcmp(argv[i],"-v")==0 || strcmp(argv[i],"-verbose")==0) {
     (o->verbosity)++;
     printf("cdrskin: verbosity level : %d\n",o->verbosity);
     if(o->verbosity>=Cdrskin_verbose_debuG)
       Cdrpreskin_set_severities(o,"NEVER","DEBUG",0);

   } else if(strcmp(argv[i],"-version")==0) {
     printf(
        "Cdrecord 2.01-Emulation Copyright (C) 2006, see libburn.pykix.org\n");
     printf("libburn version   :  %s\n",Cdrskin_libburn_versioN);

#ifndef Cdrskin_extra_leaN
     printf("cdrskin version   :  %s\n",Cdrskin_prog_versioN);
#else
     printf("cdrskin version   :  %s.lean (capability reduced lean version)\n",
            Cdrskin_prog_versioN);
#endif

     printf("Version timestamp :  %s\n",Cdrskin_timestamP);
     printf("Build timestamp   :  %s\n",Cdrskin_build_timestamP);
     {ret= 2; goto final_checks;}
   }

 }
 ret= 1;
final_checks:;
 if(flag&1)
   goto ex;

 if(o->allow_setuid==0 && getuid()!=geteuid()) {
   fprintf(stderr,
    "cdrskin: SORRY : uid and euid differ. Will abort for safety concerns.\n");
   fprintf(stderr,
    "cdrskin: HINT  : Consider to allow rw-access to the writer device and\n");
   fprintf(stderr,
    "cdrskin: HINT  : to run cdrskin under your normal user identity.\n");
   fprintf(stderr,
    "cdrskin: HINT  : Option  --allow_setuid  disables this safety check.\n");
   ret= 0; goto ex;
 }

 if(strlen(o->raw_device_adr)>0 && !o->no_whitelist) {
   int driveno,hret;
   char *adr,buf[Cdrskin_adrleN];

   adr= o->raw_device_adr;

#ifndef Cdrskin_extra_leaN
   if(o->adr_trn!=NULL) {
     hret= Cdradrtrn_translate(o->adr_trn,adr,-1,buf,0);
     if(hret<=0) {
       fprintf(stderr,
        "cdrskin: FATAL : address translation failed (address too long ?) \n");
       {ret= 0; goto ex;}
     }
     adr= buf;
   }
#endif /* ! Cdrskin_extra_leaN */

   if(adr[0]=='/') {
     if(strlen(adr)>=sizeof(o->device_adr)) {
dev_too_long:;
       fprintf(stderr,
               "cdrskin: FATAL : dev=... too long (max. %d characters)\n",
               sizeof(o->device_adr)-1);
       {ret= 0; goto ex;}
     }
     strcpy(o->device_adr,adr);
   } else {
     ret= Cdrpreskin__cdrecord_to_dev(adr,o->device_adr,&driveno,
                                      !!o->old_pseudo_scsi_adr);
     if(ret==-2 || ret==-3)
       {ret= 0; goto ex;}
     if(ret<0)
       goto ex;
     if(ret==0) {
       strcpy(o->device_adr,adr);
       ret= 1;
     }
   }

#ifdef Cdrskin_libburn_has_convert_fs_adR

   if(strlen(o->device_adr)>0 && !o->no_convert_fs_adr) {
     int lret;
     char link_adr[Cdrskin_strleN+1];

     strcpy(link_adr,o->device_adr);
     lret = burn_drive_convert_fs_adr(link_adr,o->device_adr);
     if(lret<0) {
       fprintf(stderr,
           "cdrskin: NOTE : Please inform libburn-hackers@pykix.org about:\n");
       fprintf(stderr,
	   "cdrskin:        burn_drive_convert_fs_adr() returned %d\n",lret);
     }
   }

#endif /* Cdrskin_libburn_has_convert_fs_adR */

 }

 /* A60927 : note to myself : no "ret= 1;" here. It breaks --help , -version */

ex:;

#ifndef Cdrskin_extra_leaN
 if(ret<=0 || !(flag&1))
   Cdradrtrn_destroy(&(o->adr_trn),0);
#endif

 return(ret);
}


/* --------------------------------------------------------------------- */



/** The maximum number of tracks */
#define Cdrskin_track_maX 99


/** Work around the fact that libburn leaves the track input fds open
    after the track is done. This can hide a few overflow bytes buffered 
    by the fifo-to-libburn pipe which would cause a broken-pipe error
    if libburn would close that outlet.
    This macro enables a coarse workaround which tries to read bytes from
    the track inlets after burning has ended. Probably not a good idea if
    libburn would close the inlet fds.
*/
#define Cdrskin_libburn_leaves_inlet_opeN 1


/** List of furter wishes towards libburn:
    - write mode which does not demand a track size in advance
    - obtain minimum drive speed (for cdrskin -atip)
    - obtain MMC profile of inserted media (for cdrskin -v -atip)
    - a possibility to implement cdrskin -multi
    - a possibilty to implement cdrskin -reset
*/


/** Limit to prevent int rollovers within libburn as long as not everything is
    changed to 64 bit off_t : 2 GB minus 800 MB for eventual computations. */
#define Cdrskin_tracksize_maX 1308622848


/* Some constants obtained by hearsay and experiments */

/** The payload speed factor for reporting progress: 1x = 150 kB/s */
static double Cdrskin_cd_speed_factoR= 150.0*1024.0;

/** The speed conversion factor consumer x-speed to libburn speed as used with
    burn_drive_set_speed() burn_drive_get_write_speed()
*/
static double Cdrskin_libburn_cd_speed_factoR= 176.0;

/** Add-on for burn_drive_set_speed() to accomodate to the slightley oversized
    speed ideas of my LG DVDRAM GSA-4082B. LITE-ON LTR-48125S tolerates it.
*/
static double Cdrskin_libburn_cd_speed_addoN= 50.0;


/** The program run control object. Defaults: see Cdrskin_new(). */
struct CdrskiN {

 /** Settings already interpreted by Cdrpreskin_setup */
 struct CdrpreskiN *preskin;

 /** Job: what to do, plus some parameters. */
 int verbosity;
 double x_speed;
 int gracetime;
 int dummy_mode;
 int force_is_set;
 int single_track;

 int do_devices;

 int do_scanbus;

 int do_checkdrive;

 int do_msinfo;
 int msinfo_fd;

 int do_atip;

 int do_blank;
 int blank_fast;
 int no_blank_appendable;

 int do_burn;
 int burnfree;
 /** The write mode (like SAO or RAW96/R). See libburn. 
     Controled by preskin->write_mode_name */
 enum burn_write_types write_type;
 int block_type;
 int multi;

 int do_eject;
 char eject_device[Cdrskin_strleN];


 /** The current data source and its eventual parameters. 
     source_path may be either "-" for stdin, "#N" for open filedescriptor N
     or the address of a readable file.
 */
 char source_path[Cdrskin_strleN];
 double fixed_size;
 double padding;
 int set_by_padsize;

 /** track_type may be set to BURN_MODE1, BURN_AUDIO, etc. */
 int track_type;
 int track_type_by_default; /* 0= explicit, 1=not set, 2=by file extension */
 int swap_audio_bytes;

 /** The list of tracks with their data sources and parameters */
 struct CdrtracK *tracklist[Cdrskin_track_maX];
 int track_counter;
 /** a guess about what track might be processing right now */
 int supposed_track_idx;

 int fifo_enabled;
 /** Optional fifo between input fd and libburn. It uses a pipe(2) to transfer
     data to libburn. This fifo may be actually the start of a chain of fifos
     which are to be processed simultaneously.
     The fifo object knows the real input fd and the fd[1] of the pipe.
     This is just a reference pointer. The fifos are managed by the tracks
     which either line up their fifos or share the fifo of the first track.
 */
 struct CdrfifO *fifo;
 /** fd[0] of the fifo pipe. This is from where libburn reads its data. */
 int fifo_outlet_fd;
 int fifo_size;
 int fifo_start_at;
 int fifo_per_track;


 /** User defined address translation */
 struct CdradrtrN *adr_trn;


 /** The drives known to libburn after scan */
 struct burn_drive_info *drives;
 unsigned int n_drives;
 /** The drive selected for operation by CdrskiN */
 int driveno;


 /** Progress state info: wether libburn is actually processing payload data */
 int is_writing;
 /** Previously detected drive state */
 enum burn_drive_status previous_drive_status;

 /** abort parameters */
 int abort_max_wait;

 /** Engagement info for eventual abort */
 int lib_is_initialized;
 pid_t control_pid; /* pid of the thread that calls libburn */
 int drive_is_grabbed;
 int drive_is_busy; /* Wether drive was told to do something cancel-worthy */
 struct burn_drive *grabbed_drive;

 /** Abort test facility */
 double abort_after_bytecount; 


 /** Some intermediate option info which is stored until setup finalization */
 double tao_to_sao_tsize;
 int stdin_source_used;

};

int Cdrskin_destroy(struct CdrskiN **o, int flag);
int Cdrskin_grab_drive(struct CdrskiN *skin, int flag);
int Cdrskin_release_drive(struct CdrskiN *skin, int flag);


/** Create a cdrskin program run control object.
    @param skin Returns pointer to resulting
    @param flag Bitfield for control purposes: 
                bit0= library is already initialized
    @return <=0 error, 1 success
*/
int Cdrskin_new(struct CdrskiN **skin, struct CdrpreskiN *preskin, int flag)
{
 struct CdrskiN *o;
 int ret,i;

 (*skin)= o= TSOB_FELD(struct CdrskiN,1);
 if(o==NULL)
   return(-1);
 o->preskin= preskin;
 o->verbosity= preskin->verbosity;
 o->x_speed= -1.0;
 o->gracetime= 0;
 o->dummy_mode= 0;
 o->force_is_set= 0;
 o->single_track= 0;
 o->do_devices= 0;
 o->do_scanbus= 0;
 o->do_checkdrive= 0;
 o->do_msinfo= 0;
 o->msinfo_fd= -1;
 o->do_atip= 0;
 o->do_blank= 0;
 o->blank_fast= 0;
 o->no_blank_appendable= 0;
 o->do_burn= 0;
 o->write_type= BURN_WRITE_SAO;
 o->block_type= BURN_BLOCK_SAO;
 o->multi= 0;
 o->burnfree= 0;
 o->do_eject= 0;
 o->eject_device[0]= 0;
 o->source_path[0]= 0;
 o->fixed_size= 0.0;
 o->padding= 0.0;
 o->set_by_padsize= 0;
 o->track_type= BURN_MODE1;
 o->swap_audio_bytes= 1;   /* cdrecord default is big-endian (msb_first) */
 o->track_type_by_default= 1;
 for(i=0;i<Cdrskin_track_maX;i++)
   o->tracklist[i]= NULL;
 o->track_counter= 0;
 o->supposed_track_idx= -1;
 o->fifo_enabled= 1;
 o->fifo= NULL;
 o->fifo_outlet_fd= -1;
 o->fifo_size= 4*1024*1024;
 o->fifo_start_at= -1;
 o->fifo_per_track= 0;
 o->adr_trn= NULL;
 o->drives= NULL;
 o->n_drives= 0;
 o->driveno= 0;
 o->is_writing= 0;
 o->previous_drive_status = BURN_DRIVE_IDLE;
 o->abort_max_wait= 74*60;
 o->lib_is_initialized= (flag&1);
 o->control_pid= getpid();
 o->drive_is_grabbed= 0;
 o->drive_is_busy= 0;
 o->grabbed_drive= NULL;
 o->abort_after_bytecount= -1.0;
 o->tao_to_sao_tsize= 0.0;
 o->stdin_source_used= 0;

#ifndef Cdrskin_extra_leaN
 ret= Cdradrtrn_new(&(o->adr_trn),0);
 if(ret<=0)
   goto failed;
#endif /* ! Cdrskin_extra_leaN */

 return(1);
failed:;
 Cdrskin_destroy(skin,0);
 return(-1);
}


/** Release from memory a cdrskin object */
int Cdrskin_destroy(struct CdrskiN **o, int flag)
{
 struct CdrskiN *skin;
 int i;

 skin= *o;
 if(skin==NULL)
   return(0);
 if(skin->drive_is_grabbed)
   Cdrskin_release_drive(skin,0);
 for(i=0;i<skin->track_counter;i++)
   Cdrtrack_destroy(&(skin->tracklist[i]),0);

#ifndef Cdrskin_extra_leaN
 Cdradrtrn_destroy(&(skin->adr_trn),0);
#endif /* ! Cdrskin_extra_leaN */

 Cdrpreskin_destroy(&(skin->preskin),0);
 if(skin->drives!=NULL)
   burn_drive_info_free(skin->drives);
 free((char *) skin);
 *o= NULL;
 return(1);
}


/** Set the eventual output fd for the result of Cdrskin_msinfo()
*/
int Cdrskin_set_msinfo_fd(struct CdrskiN *skin, int result_fd, int flag)
{
 skin->msinfo_fd= result_fd;
 return(1);
}


/** Return information about current track source */
int Cdrskin_get_source(struct CdrskiN *skin, char *source_path,
                       double *fixed_size, double *padding,
                       int *set_by_padsize, int *track_type,
                       int *track_type_by_default, int *swap_audio_bytes,
                       int flag)
{
 strcpy(source_path,skin->source_path);
 *fixed_size= skin->fixed_size;
 *padding= skin->padding;
 *set_by_padsize= skin->set_by_padsize;
 *track_type= skin->track_type;
 *track_type_by_default= skin->track_type_by_default;
 *swap_audio_bytes= skin->swap_audio_bytes;
 return(1);
}


#ifndef Cdrskin_extra_leaN

/** Return information about current fifo setting */
int Cdrskin_get_fifo_par(struct CdrskiN *skin, int *fifo_enabled,
                         int *fifo_size, int *fifo_start_at, int flag)
{
 *fifo_enabled= skin->fifo_enabled;
 *fifo_size= skin->fifo_size;
 *fifo_start_at= skin->fifo_start_at;
 return(1);
}


/** Create and install fifo objects between track data sources and libburn.
    The sources and parameters are known to skin.
    @return <=0 error, 1 success
*/
int Cdrskin_attach_fifo(struct CdrskiN *skin, int flag)
{
 struct CdrfifO *ff= NULL;
 int ret,i,hflag;

 skin->fifo= NULL;
 for(i=0;i<skin->track_counter;i++) {
   hflag= (skin->verbosity>=Cdrskin_verbose_debuG);
   if(i==skin->track_counter-1)
     hflag|= 4;
   if(skin->verbosity>=Cdrskin_verbose_cmD) {
     if(skin->fifo_per_track)
       printf("cdrskin: track %d establishing fifo of %d bytes\n",
              i+1,skin->fifo_size);
     else if(i==0)
       printf("cdrskin: establishing fifo of %d bytes\n",skin->fifo_size);
     else {
       if(skin->verbosity>=Cdrskin_verbose_debuG)
        ClN(fprintf(stderr,"cdrskin_debug: attaching track %d to fifo\n",i+1));
       hflag|= 2;
     }
   }
   ret= Cdrtrack_attach_fifo(skin->tracklist[i],&(skin->fifo_outlet_fd),ff,
                             hflag);
   if(ret<=0) {
     fprintf(stderr,"cdrskin: FATAL : failed to attach fifo.\n");
     return(0);
   }
   if(i==0 || skin->fifo_per_track)
     Cdrtrack_get_fifo(skin->tracklist[i],&ff,0);
   if(i==0)
     skin->fifo= ff;
 }
 return(1);
}


/** Read data into the track fifos until either #1 is full or its data source
    is exhausted.
    @return <=0 error, 1 success
*/
int Cdrskin_fill_fifo(struct CdrskiN *skin, int flag)
{
 int ret;

 ret= Cdrtrack_fill_fifo(skin->tracklist[0],0);
 if(ret<=0)
   return(ret);
 printf("input buffer ready.\n");
 fflush(stdout);
 return(1);
}

#endif /* ! Cdrskin_extra_leaN */


/** Inform libburn about the consumer x-speed factor of skin */
int Cdrskin_adjust_speed(struct CdrskiN *skin, int flag)
{
 int k_speed;

 if(skin->x_speed<0)
   k_speed= 0; /* libburn.h promises 0 to be max speed. */
 else if(skin->x_speed==0) /* cdrecord specifies 0 as minimum speed. */
   k_speed= Cdrskin_libburn_cd_speed_factoR+Cdrskin_libburn_cd_speed_addoN;
 else
   k_speed= skin->x_speed*Cdrskin_libburn_cd_speed_factoR +
            Cdrskin_libburn_cd_speed_addoN;

 if(skin->verbosity>=Cdrskin_verbose_debuG)
   ClN(fprintf(stderr,"cdrskin_debug: k_speed= %d\n",k_speed));

 burn_drive_set_speed(skin->drives[skin->driveno].drive,k_speed,k_speed);
 return(1);
}


/** Shutdown library and restart again on single drive which gets grabbed.
    Does only work with a valid skin->driveno or with an already set
    skin->preskin->device_adr . 
    @param flag Bitfield for control purposes:
                bit0= skin->driveno points to a valid drive. The library
                      will get reopened with that drive listed as only one
                      and already grabbed.
                bit1= do not load drive tray
    @return  1 = success , 
             0 = failure, drive is released, library initialized
            -1 = failure, library is finished (and could not get initialized)
*/    
int Cdrskin_reinit_lib_with_adr(struct CdrskiN *skin, int flag)
{
 int ret;

 if(skin->drive_is_grabbed)
   Cdrskin_release_drive(skin,0);
 if(flag&1)
   burn_drive_get_adr(&(skin->drives[skin->driveno]),
                      skin->preskin->device_adr);
 if(strlen(skin->preskin->device_adr)<=0) {
   fprintf(stderr,
           "cdrskin: FATAL : unable to determine persistent drive address\n");
   ret= 0; goto ex;
 }
 burn_drive_info_free(skin->drives);
 burn_finish();
 if(!burn_initialize()) {
   fflush(stdout);
   fprintf(stderr,"cdrskin : FATAL : Re-initialization of libburn failed\n");
   {ret= -1; goto ex;}
 }
 ret= Cdrskin_grab_drive(skin,1|(flag&2));/* uses burn_drive_scan_and_grab() */
 if(ret<=0)
   {ret=0; goto ex;}

 ret= 1;
ex:
 return(ret);
}


/** Obtain access to a libburn drive for writing or information retrieval.
    If libburn is not restricted to a single persistent address then the
    unused drives are dropped. This might be done by shutting down and
    restartiing libburn with the wanted drive only. Thus, after this call,
    libburn is supposed to have open only the reserved drive.
    All other drives should be free for other use.
    Warning: Do not store struct burn_drive pointer over this call.
             Any such pointer might be invalid afterwards.
    @param flag Bitfield for control purposes:
                bit0= bus is unscanned, device is known, 
                      use burn_drive_scan_and_grab()
                bit1= do not load drive tray
                bit2= do not issue error message on failure
    @return <=0 error, 1 success
*/
int Cdrskin_grab_drive(struct CdrskiN *skin, int flag)
{
 int ret,i;
 struct burn_drive *drive;
#ifdef Cdrskin_grab_abort_brokeN
 int restore_handler= 0;
#endif

 i= 0;/* as long as its use is conditional, so gcc -Wall does not complain */

 if(skin->drive_is_grabbed)
   Cdrskin_release_drive(skin,0);

 if(flag&1) {
   skin->driveno= 0;
   drive= NULL;
   skin->grabbed_drive= drive;
 } else {
   drive= skin->drives[skin->driveno].drive;
   skin->grabbed_drive= drive;
 }

#ifdef Cdrskin_grab_abort_brokeN

 /* There seems to be no way to get a drive out of status BURN_DRIVE_GRABBING 
    So try to block out signals if there is a signal handler installed */
 if(skin->preskin->abort_handler==1 || 
    skin->preskin->abort_handler==3 ||
    skin->preskin->abort_handler==4) {
   Cleanup_set_handlers(NULL,NULL,2);
   restore_handler= 1;
 }

#endif /* ! Cdrskin_grab_abort_brokeN */

#ifndef Cdrskin_oldfashioned_api_usE


 if(flag&1) {
   ret= burn_drive_scan_and_grab(&(skin->drives),skin->preskin->device_adr,
                                 !(flag&2));
   if(ret<=0) {
     if(!(flag&4))
       fprintf(stderr,"cdrskin: FATAL : unable to open drive '%s'\n",
               skin->preskin->device_adr);
     goto ex;
   }
   skin->driveno= 0;
   drive= skin->drives[skin->driveno].drive;
   skin->grabbed_drive= drive;
 } else {
   if(strlen(skin->preskin->device_adr)<=0) {

#define Cdrskin_drop_drives_by_forgeT 1
#ifdef Cdrskin_drop_drives_by_forgeT 

     if(skin->verbosity>=Cdrskin_verbose_debuG)   
       ClN(fprintf(stderr,
         "cdrskin_debug: Cdrskin_grab_drive() dropping unwanted drives (%d)\n",
          skin->n_drives-1));
     for(i=0;i<skin->n_drives;i++) {
       if(i==skin->driveno)
     continue;
       if(skin->verbosity>=Cdrskin_verbose_debuG)   
         ClN(fprintf(stderr,
           "cdrskin_debug: Cdrskin_grab_drive() dropped drive number %d\n",i));
       ret= burn_drive_info_forget(&(skin->drives[i]), 0);
       if(ret==1 || ret==2)
     continue;
       fprintf(stderr,
           "cdrskin: NOTE : Please inform libburn-hackers@pykix.org about:\n");
       fprintf(stderr,
           "cdrskin:        burn_drive_info_forget() returns %d\n",ret);
     }

#else

     ret= Cdrskin_reinit_lib_with_adr(skin,1|(flag&2));
     goto ex; /* this calls Cdrskin_grab() with persistent address or fails */

#endif /* ! Cdrskin_drop_drives_by_forgeT */

   }

#else

 {

#endif /* Cdrskin_oldfashioned_api_usE */

   ret= burn_drive_grab(drive,!(flag&2));
   if(ret==0) {
     if(!(flag&4))
       fprintf(stderr,"cdrskin: FATAL : unable to open drive %d\n",
               skin->driveno);
     goto ex;
   }

#ifdef Cdrskin_is_erasable_on_load_is_brokeN
   /* RIP-14.5 + LITE-ON 48125S produce a false status if tray was unloaded */
   /* Therefore the first grab was just for loading */
   skin->drive_is_grabbed= 1; /* message to eventual abort handler */
   burn_drive_release(drive,0);
   skin->drive_is_grabbed= 0;

   /* now grab the drive for real */
   ret= burn_drive_grab(drive,!(flag&2));
   if(ret==0) {
     if(!(flag&4))
       fprintf(stderr,"cdrskin: FATAL : unable to open drive %d\n",
               skin->driveno);
     goto ex;
   }
#endif /* ! Cdrskin_is_erasable_on_load_is_brokeN */

 }
 skin->drive_is_grabbed= 1;
 ret= 1;
ex:;

#ifdef Cdrskin_grab_abort_brokeN
 if(restore_handler) {
   int Cdrskin_abort_handler(struct CdrskiN *, int, int);
   Cleanup_set_handlers(skin,(Cleanup_app_handler_T) Cdrskin_abort_handler,4);
 }
#endif /* Cdrskin_grab_abort_brokeN */

 if(ret<=0) {
   skin->drive_is_grabbed= 0;
   skin->grabbed_drive= NULL;
 }
 return(ret);
}


/** Release grabbed libburn drive 
    @param flag Bitfield for control purposes:
                bit0= eject
*/
int Cdrskin_release_drive(struct CdrskiN *skin, int flag)
{
 if((!skin->drive_is_grabbed) || skin->grabbed_drive==NULL) {
   fprintf(stderr,"cdrskin: CAUGHT : release of non-grabbed drive.\n");
   return(0);
 }
 burn_drive_release(skin->grabbed_drive,(flag&1));
 skin->drive_is_grabbed= 0;
 skin->grabbed_drive= NULL;
 return(1);
}


/** Clean up resources in abort situations. To be called by Cleanup subsystem
    but hardly ever by the application. The program must exit afterwards.
*/
int Cdrskin_abort_handler(struct CdrskiN *skin, int signum, int flag)
{

#ifdef Cdrskin_libburn_has_burn_aborT

 int ret;

#else 

 int wait_grain= 100000,first_status= 1;
 double start_time,last_time,current_time;

#endif /* ! Cdrskin_libburn_has_burn_aborT */

 struct burn_progress p;
 enum burn_drive_status drive_status= BURN_DRIVE_GRABBING;

 if(getpid()!=skin->control_pid) {
   if(skin->verbosity>=Cdrskin_verbose_debuG)   
     ClN(fprintf(stderr,
          "\ncdrskin_debug: ABORT : [%d] Thread rejected: pid=%d, signum=%d\n",
          skin->control_pid,getpid(),signum));
   return(-2); /* do only process the control thread */
 }
 if(skin->preskin->abort_handler==3)
   Cleanup_set_handlers(NULL,NULL,2); /* ignore all signals */
 else if(skin->preskin->abort_handler==4)
   Cleanup_set_handlers(NULL,NULL,1); /* allow abort */
 fprintf(stderr,
     "\ncdrskin: ABORT : Handling started. Please do not press CTRL+C now.\n");
 if(skin->preskin->abort_handler==3)
   fprintf(stderr,"cdrskin: ABORT : Trying to ignore any further signals\n");

#ifndef Cdrskin_extra_leaN
 if(skin->fifo!=NULL)
   Cdrfifo_close_all(skin->fifo,0);
#endif

#ifdef Cdrskin_libburn_has_burn_aborT

 /* Only for user info */
 if(skin->grabbed_drive!=NULL)
   drive_status= burn_drive_get_status(skin->grabbed_drive,&p);
 if(drive_status!=BURN_DRIVE_IDLE) {
   fprintf(stderr,"cdrskin: ABORT : Abort processing depends on CD speed and buffer size\n");
   fprintf(stderr,"cdrskin: ABORT : Usually it is done with 4x speed after about a MINUTE\n");
   fprintf(stderr,"cdrskin: URGE  : But wait at least the normal burning time before any kill -9\n");
 }

 ret= burn_abort(skin->abort_max_wait, burn_abort_pacifier, "cdrskin: ");
 if(ret<=0) {
   fprintf(stderr,
         "\ncdrskin: ABORT : Cannot cancel burn session and release drive.\n");
   return(0);
 }
 fprintf(stderr,"\n");

#else /* Cdrskin_libburn_has_burn_aborT */

 if(skin->grabbed_drive!=NULL) {
   drive_status= burn_drive_get_status(skin->grabbed_drive,&p);
   if(drive_status!=BURN_DRIVE_IDLE && !skin->drive_is_grabbed)
     skin->drive_is_grabbed= 2;
   if(drive_status!=BURN_DRIVE_IDLE && !skin->drive_is_busy)
     skin->drive_is_busy= 2;
   if(skin->verbosity>=Cdrskin_verbose_debuG)
     ClN(fprintf(stderr,"cdrskin_debug: ABORT : Drive status: %d\n",
                 (int) drive_status));
 }
 if(skin->verbosity>=Cdrskin_verbose_debuG)
   ClN(fprintf(stderr,
        "cdrskin_debug: ABORT : drive_is_grabbed=%d , drive_is_busy=%d (%X)\n",
        skin->drive_is_grabbed,skin->drive_is_busy,
        (unsigned int) skin->grabbed_drive));

 if(skin->drive_is_grabbed) {
   if(skin->drive_is_busy && skin->grabbed_drive!=NULL) {
     if(drive_status==BURN_DRIVE_WRITING || drive_status==BURN_DRIVE_READING) {
       fprintf(stderr,"cdrskin: ABORT : Trying to cancel drive operation.\n");
       burn_drive_cancel(skin->grabbed_drive);
     } else if(drive_status==BURN_DRIVE_GRABBING) {

#ifndef Cdrskin_oldfashioned_api_usE
       int ret;

       fprintf(stderr,
            "cdrskin: ABORT : Trying to close drive in process of grabbing\n");

       /* >>> ??? rather inquire driveno from 
                  skin->grabbed_drive->global_index ? */;

       ret= burn_drive_info_forget(&(skin->drives[skin->driveno]),1);
       if(ret<=0)
         fprintf(stderr,
             "cdrskin: ABORT : Attempt to close drive failed (ret= %d)\n",ret);
       else {
         skin->drive_is_grabbed= 0;
         skin->grabbed_drive= NULL;
         goto try_to_finish_lib;
       }

#else
       /* >>> what to do in this state ? */;
#endif /* Cdrskin_oldfashioned_api_usE */

     } else if(drive_status!=BURN_DRIVE_IDLE) {
       fprintf(stderr,
               "cdrskin: ABORT : Will wait for current operation to end\n");
     }
     if(drive_status!=BURN_DRIVE_IDLE) {
       fprintf(stderr,"cdrskin: ABORT : Abort processing depends on CD speed and buffer size\n");
       fprintf(stderr,"cdrskin: ABORT : Usually it is done with 4x speed after about a MINUTE\n");
       fprintf(stderr,"cdrskin: URGE  : But wait at least the normal burning time before any kill -9\n");
     }
     last_time= start_time= Sfile_microtime(0);
     while(1) {
       drive_status= burn_drive_get_status(skin->grabbed_drive,&p);
       if(drive_status==BURN_DRIVE_IDLE)
     break;
       usleep(wait_grain);
       current_time= Sfile_microtime(0);
       if(current_time-last_time>=1.0) {
         if(first_status)
           fprintf(stderr,"\n");
         first_status= 0;
         fprintf(stderr,"\rcdrskin: ABORT : Status %d. Waiting for status %d since %d seconds (%d max)",
                (int) drive_status, (int) BURN_DRIVE_IDLE,
                (int) (current_time-start_time),skin->abort_max_wait);
         last_time= current_time;
       }
       if(current_time-start_time>=skin->abort_max_wait) {
         fprintf(stderr,
         "\ncdrskin: ABORT : Cannot cancel burn session and release drive.\n");
         return(0);
       }
     }
     fprintf(stderr,"\ncdrskin: ABORT : Status %d.\n",(int) drive_status);
   }
   fprintf(stderr,"cdrskin: ABORT : Trying to release drive.\n");
   Cdrskin_release_drive(skin,0);
 }

#ifndef Cdrskin_oldfashioned_api_usE
try_to_finish_lib:;
#endif

 if(skin->lib_is_initialized) {
   fprintf(stderr,"cdrskin: ABORT : Trying to finish libburn.\n");
   burn_finish();
 }

#endif /* ! Cdrskin_libburn_has_burn_aborT */

 fprintf(stderr,
    "cdrskin: ABORT : Drive is released and library is shut down now.\n");
 fprintf(stderr,
    "cdrskin: ABORT : Program done. Even if you do not see a shell prompt.\n");
 return(1);
}


/** Convert a libburn device address into a libburn drive number
    @return <=0 error, 1 success
*/
int Cdrskin_driveno_of_location(struct CdrskiN *skin, char *devicename,
                                int *driveno, int flag)
{
 int i,ret;
 char adr[Cdrskin_adrleN];

 for(i=0;i<skin->n_drives;i++) {

#ifdef Cdrskin_libburn_has_drive_get_adR
   ret= burn_drive_get_adr(&(skin->drives[i]), adr);
   if(ret<=0)
 continue;
#else 
   ret= 1; /* to please gcc -Wall */
   strcpy(adr,skin->drives[i].location);
#endif

   if(strcmp(adr,devicename)==0) {
     *driveno= i;
     return(1);
   }
 }
 return(0);
}


/** Convert a cdrskin address into a libburn drive number
    @return <=0 error, 1 success
*/
int Cdrskin_dev_to_driveno(struct CdrskiN *skin, char *in_adr, int *driveno,
                           int flag)
{
 int ret;
 char *adr,translated_adr[Cdrskin_adrleN],synthetic_adr[Cdrskin_adrleN];

 adr= in_adr;

#ifndef Cdrskin_extra_leaN
 /* user defined address translation */
 ret= Cdradrtrn_translate(skin->adr_trn,adr,-1,translated_adr,0);
 if(ret<=0) {
   fprintf(stderr,
        "cdrskin: FATAL : address translation failed (address too long ?) \n");
   return(0);
 }
 if(skin->verbosity>=Cdrskin_verbose_cmD && strcmp(adr,translated_adr)!=0)
   printf("cdrskin: dev_translation=... :  dev='%s' to dev='%s'\n",
          adr,translated_adr);
 adr= translated_adr;
#endif /* ! Cdrskin_extra_leaN */

 if(adr[0]=='/') {
   ret= Cdrskin_driveno_of_location(skin,adr,driveno,0);
   if(ret<=0) {
location_not_found:;
     fprintf(stderr,
         "cdrskin: FATAL : cannot find '%s' among accessible drive devices.\n",
            adr);
     fprintf(stderr,
      "cdrskin: HINT : use option  --devices  for a list of drive devices.\n");
     return(0);
   }
   return(1);
 }
 ret= Cdrpreskin__cdrecord_to_dev(adr,synthetic_adr,driveno,
                                  !!skin->preskin->old_pseudo_scsi_adr);
 if(ret<=0) {
wrong_devno:;
   if(skin->n_drives<=0) {
     fprintf(stderr,"cdrskin: FATAL : No accessible drives.\n");
   } else {
     fprintf(stderr,
         "cdrskin: FATAL : Address does not lead to an accessible drive: %s\n",
             in_adr);
     fprintf(stderr,
    "cdrskin: HINT : dev= expects /dev/xyz, Bus,Target,0 or a number [0,%d]\n",
           skin->n_drives-1);
   }
   return(0);
 }
 if(strlen(synthetic_adr)>0) {
   if(skin->verbosity>=Cdrskin_verbose_cmD)
     printf("cdrskin: converted address '%s' to '%s'\n",adr,synthetic_adr);
   ret= Cdrskin_driveno_of_location(skin,synthetic_adr,driveno,0);
   if(ret<=0) {
     fprintf(stderr,
             "cdrskin: failure while using address converted from '%s'\n",adr);
     adr= synthetic_adr;
     goto location_not_found;
   }
 }
 if((*driveno)>=skin->n_drives || (*driveno)<0) {
   fprintf(stderr,"cdrskin: obtained drive number  %d  from '%s'\n",
                  *driveno,adr);
   goto wrong_devno;
 }
 return(1);
}


/** Convert a libburn drive number into a cdrecord-style address which 
    represents a device address if possible and the drive number else.
    @param flag Bitfield for control purposes: 
                bit0= do not apply user defined address translation
    @return <0 error,
                pseudo transport groups:
                 0 volatile drive number, 
                 1 /dev/sgN, 2 /dev/hdX,
                 1000000+busno = non-pseudo SCSI bus
                 2000000+busno = pseudo-ATA|ATAPI SCSI bus (currently busno==2)
*/
int Cdrskin_driveno_to_btldev(struct CdrskiN *skin, int driveno,
                              char btldev[Cdrskin_adrleN], int flag)
{
 int k,ret,still_untranslated= 1,hret;
 char *loc= NULL,buf[Cdrskin_adrleN],adr[Cdrskin_adrleN];

 if(driveno<0 || driveno>skin->n_drives)
   goto fallback;

#ifdef Cdrskin_libburn_has_drive_get_adR
 ret= burn_drive_get_adr(&(skin->drives[driveno]), adr);
 if(ret<=0)
   goto fallback;
 loc= adr;
#else 
 adr[0]= 0; /* to please gcc -Wall */
 loc= skin->drives[driveno].location;
 if(loc==NULL)
   goto fallback;
#endif

#ifdef Cdrskin_libburn_has_convert_scsi_adR
 if(!skin->preskin->old_pseudo_scsi_adr) {
   int host_no= -1,channel_no= -1,target_no= -1,lun_no= -1, bus_no= -1;

   ret= burn_drive_obtain_scsi_adr(loc,&bus_no,&host_no,&channel_no,
                                   &target_no,&lun_no); 
   if(ret<=0) {
     if(strncmp(loc,"/dev/hd",7)==0)
       if(loc[7]>='a' && loc[7]<='z')
         if(loc[8]==0) {
           bus_no= (loc[7]-'a')/2;
           sprintf(btldev,"%d,%d,0",bus_no,(loc[7]-'a')%2);
           {ret= 2000000 + bus_no; goto adr_translation;}
         }
     goto fallback;
   } else {
     sprintf(btldev,"%d,%d,%d",bus_no,target_no,lun_no);
     ret= 1000000+bus_no;
     goto adr_translation;
   }
 }
#endif

 if(strncmp(loc,"/dev/sg",7)==0) {
   for(k= 7;loc[k]!=0;k++)
     if(loc[k]<'0' || loc[k]>'9')
   break;
   if(loc[k]==0 && k>7) {
     sprintf(btldev,"1,%s,0",loc+7);
     {ret= 1; goto adr_translation;}
   }
 }
 if(strncmp(loc,"/dev/hd",7)==0)
   if(loc[7]>='a' && loc[7]<='z')
     if(loc[8]==0) {
       sprintf(btldev,"2,%d,0",loc[7]-'a');
       {ret= 2; goto adr_translation;}
     }
fallback:;
 if(skin->preskin->old_pseudo_scsi_adr) {
   sprintf(btldev,"0,%d,0",driveno);
 } else {
   if(loc!=NULL)
     strcpy(btldev,loc);
   else
     sprintf(btldev,"%d",driveno);
 }
 ret= 0;

adr_translation:;
#ifndef Cdrskin_extra_leaN
 /* user defined address translation */
 if(!(flag&1)) {
   if(ret>0) {
     /* try wether a translation points to loc */
     hret= Cdradrtrn_translate(skin->adr_trn,loc,driveno,buf,1);
     if(hret==2) {
       still_untranslated= 0;
       strcpy(btldev,buf);
     }
   }
   if(still_untranslated) {
     Cdradrtrn_translate(skin->adr_trn,btldev,driveno,buf,1);
     strcpy(btldev,buf);
   }
 }
#endif /* ! Cdrskin_extra_leaN */

 return(ret);
}


/** Report media status s to the user */
int Cdrskin_report_disc_status(struct CdrskiN *skin, enum burn_disc_status s,
                               int flag)
{
 printf("cdrskin: status %d ",s);
 if(s==BURN_DISC_FULL) {
   printf("burn_disc_full \"There is a disc with data on it in the drive\"\n"); 
 } else if(s==BURN_DISC_BLANK) {
   printf("burn_disc_blank \"The drive holds a blank disc\"\n"); 
 } else if(s==BURN_DISC_APPENDABLE) {
   printf(
        "BURN_DISC_APPENDABLE \"There is an incomplete disc in the drive\"\n");
 } else if(s==BURN_DISC_EMPTY) {
   printf("BURN_DISC_EMPTY \"There is no disc at all in the drive\"\n");
 } else if(s==BURN_DISC_UNREADY) {
   printf("BURN_DISC_UNREADY \"The current status is not yet known\"\n");

#ifdef Cdrskin_libburn_has_burn_disc_unsuitablE

 } else if(s==BURN_DISC_UNGRABBED) {
   printf("BURN_DISC_UNGRABBED \"API usage error: drive not grabbed\"\n");
 } else if(s==BURN_DISC_UNSUITABLE) {
   printf("BURN_DISC_UNSUITABLE \"Media is not suitable\"\n");

#endif /* Cdrskin_libburn_has_burn_disc_unsuitablE */

 } else 
   printf("-unknown status code-\n");
 return(1);
}


/** Perform operations -scanbus or --devices 
    @param flag Bitfield for control purposes: 
                bit0= perform --devices rather than -scanbus
    @return <=0 error, 1 success
*/
int Cdrskin_scanbus(struct CdrskiN *skin, int flag)
{
 int ret,i,busno,first_on_bus,pseudo_transport_group= 0,skipped_devices= 0;
 int busmax= 16;
 char shellsafe[5*Cdrskin_strleN+2],perms[40],btldev[Cdrskin_adrleN];
 char adr[Cdrskin_adrleN],*raw_dev,*drives_shown= NULL;
 struct stat stbuf;

 drives_shown= malloc(skin->n_drives+1);
 if(drives_shown==NULL)
   {ret= -1; goto ex;}
 for(i=0;i<skin->n_drives;i++)
   drives_shown[i]= 0;
 if(flag&1) {
   printf("cdrskin: Overview of accessible drives (%d found) :\n",
          skin->n_drives);
   printf("-----------------------------------------------------------------------------\n");
   for(i=0;i<skin->n_drives;i++) {

#ifdef Cdrskin_libburn_has_drive_get_adR
     ret= burn_drive_get_adr(&(skin->drives[i]), adr);
     if(ret<=0) {
       /* >>> one should massively complain */;
   continue;
     }
#else
     strcpy(adr,skin->drives[i].location);
#endif

     if(stat(adr,&stbuf)==-1) {
       sprintf(perms,"errno=%d",errno);
     } else {
       strcpy(perms,"------");
       if(stbuf.st_mode&S_IRUSR) perms[0]= 'r';
       if(stbuf.st_mode&S_IWUSR) perms[1]= 'w';
       if(stbuf.st_mode&S_IRGRP) perms[2]= 'r';
       if(stbuf.st_mode&S_IWGRP) perms[3]= 'w';
       if(stbuf.st_mode&S_IROTH) perms[4]= 'r';
       if(stbuf.st_mode&S_IWOTH) perms[5]= 'w';
     }
     if(strlen(adr)>=Cdrskin_strleN)
       Text_shellsafe("failure:oversized string",shellsafe,0);
     else
       Text_shellsafe(adr,shellsafe,0);
     printf("%d  dev=%s  %s :  '%s'  '%s'\n",
            i,shellsafe,perms,skin->drives[i].vendor,skin->drives[i].product);
   }
   printf("-----------------------------------------------------------------------------\n");
 } else {
   if(!skin->preskin->old_pseudo_scsi_adr) {
     pseudo_transport_group= 1000000;
     raw_dev= skin->preskin->raw_device_adr;
     if(strncmp(raw_dev,"ATA",3)==0 && (raw_dev[3]==0 || raw_dev[3]==':'))
       pseudo_transport_group= 2000000;
     if(strncmp(raw_dev,"ATAPI",5)==0 && (raw_dev[5]==0 || raw_dev[5]==':'))
       pseudo_transport_group= 2000000;
     if(pseudo_transport_group==2000000) {
       fprintf(stderr,"scsidev: 'ATA'\ndevname: 'ATA'\n");
       fprintf(stderr,"scsibus: -2 target: -2 lun: -2\n");
     }
   }
   /* >>> fprintf(stderr,"Linux sg driver version: 3.1.25\n"); */
   printf("Using libburn version '%s'.\n", Cdrskin_libburn_versioN);
   if(pseudo_transport_group!=1000000)
   if(skin->preskin->old_pseudo_scsi_adr)
     printf("cdrskin: NOTE : The printed addresses are not cdrecord compatible !\n");

   for(busno= 0;busno<=busmax;busno++) {
     first_on_bus= 1;
     for(i=0;i<skin->n_drives;i++) {
       ret= Cdrskin_driveno_to_btldev(skin,i,btldev,1);
       if(busno==busmax && drives_shown[i]==0) {
         if(ret/1000000 != pseudo_transport_group) {
           skipped_devices++;
           if(skin->verbosity>=Cdrskin_verbose_debuG)
             ClN(fprintf(stderr,"cdrskin_debug: skipping drive '%s%s'\n",
                         ((ret/1000000)==2?"ATA:":""), btldev));
     continue;
         }
       } else if(ret != pseudo_transport_group + busno)
     continue;
       if(first_on_bus)
         printf("scsibus%d:\n",busno);
       first_on_bus= 0;
       printf("\t%s\t  %d) '%-8s' '%-16s' '%-4s' Removable CD-ROM\n",
              btldev,i,skin->drives[i].vendor,skin->drives[i].product,
              skin->drives[i].revision);
       drives_shown[i]= 1;
     }
   }
 }
 if(skipped_devices>0) {
   if(skipped_devices>1)
     printf("cdrskin: NOTE : There were %d drives not shown.\n",
            skipped_devices);
   else
     printf("cdrskin: NOTE : There was 1 drive not shown.\n");
   printf("cdrskin: HINT : To surely see all drives try option: --devices\n");
   if(pseudo_transport_group!=2000000)
     printf("cdrskin: HINT : or try options:               dev=ATA -scanbus\n");
 }
 ret= 1;
ex:;
 if(drives_shown!=NULL)
   free((char *) drives_shown);
 return(ret);
}


/** Perform -checkdrive .
    @param flag Bitfield for control purposes:
                bit0= do not print message about pseudo-checkdrive
    @return <=0 error, 1 success
*/
int Cdrskin_checkdrive(struct CdrskiN *skin, int flag)
{
 struct burn_drive_info *drive_info;
 int ret;
 char btldev[Cdrskin_adrleN];

 if(!(flag&1))
   printf("cdrskin: pseudo-checkdrive on drive %d\n",skin->driveno);
 if(skin->driveno>=skin->n_drives || skin->driveno<0) {
   fprintf(stderr,"cdrskin: FATAL : there is no drive #%d\n",skin->driveno);
   {ret= 0; goto ex;}  
 } 
 drive_info= &(skin->drives[skin->driveno]);
 ret= Cdrskin_driveno_to_btldev(skin,skin->driveno,btldev,0);
 if(ret>=0)
   fprintf(stderr,"scsidev: '%s'\n",btldev);
 printf("Device type    : %s\n","Removable CD-ROM");
 printf("Vendor_info    : '%s'\n",drive_info->vendor);
 printf("Identifikation : '%s'\n",drive_info->product);
 printf("Revision       : '%s'\n",drive_info->revision);
 printf("Driver flags   : %s\n","BURNFREE");
#ifdef Cdrskin_allow_libburn_taO

 /* <<< */ 
 if(skin->verbosity>=Cdrskin_verbose_debuG)
   ClN(fprintf(stderr,
 "cdrskin_debug: block_types: tao=%4.4X  sao=%4.4X  raw=%4.4X\n",
           drive_info->tao_block_types,drive_info->sao_block_types,
           drive_info->raw_block_types));

 printf("Supported modes:");
 if((drive_info->tao_block_types & (BURN_BLOCK_MODE1|BURN_BLOCK_RAW0))
    == (BURN_BLOCK_MODE1|BURN_BLOCK_RAW0))
   printf(" TAO");
 if(drive_info->sao_block_types & BURN_BLOCK_SAO)
   printf(" SAO");
 if(drive_info->raw_block_types & BURN_BLOCK_RAW96R)
   printf(" RAW/RAW96R");
 printf("\n");

#else
 printf("Supported modes: %s\n","SAO RAW/R96R");
#endif
 ret= 1;
ex:;
 return(ret);
}


int Cdrskin_obtain_nwa(struct CdrskiN *skin, int *nwa, int flag)
{
 int ret,lba;
 struct burn_drive *drive;
 struct burn_write_opts *o= NULL;

 /* Set write opts in order to provoke MODE SELECT. LG GSA-4082B needs it. */
 drive= skin->drives[skin->driveno].drive;
 o= burn_write_opts_new(drive);
 if(o!=NULL) {
   burn_write_opts_set_perform_opc(o, 0);
   burn_write_opts_set_write_type(o,skin->write_type,skin->block_type);
   burn_write_opts_set_underrun_proof(o,skin->burnfree);
 }
#ifdef Cdrskin_libburn_has_multI
 ret= burn_disc_track_lba_nwa(drive,o,0,&lba,nwa);
#else
 ret= 0;
 lba= 0;/* silence gcc warning */
#endif
 if(o!=NULL)
   burn_write_opts_free(o);
 return(ret);
}


/** Print lba of first track of last session and Next Writeable Address of
    the next unwritten session.
*/
int Cdrskin_msinfo(struct CdrskiN *skin, int flag)
{
 int num_sessions, session_no, ret, num_tracks;
 int nwa= -123456789, lba= -123456789, aux_lba;
 char msg[80];
 enum burn_disc_status s;
 struct burn_drive *drive;
 struct burn_disc *disc= NULL;
 struct burn_session **sessions;
 struct burn_track **tracks;
 struct burn_toc_entry toc_entry;

 ret= Cdrskin_grab_drive(skin,0);
 if(ret<=0)
   return(ret);
 drive= skin->drives[skin->driveno].drive;
 while(burn_drive_get_status(drive,NULL) != BURN_DRIVE_IDLE)
   usleep(100002);
 while ((s= burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
   usleep(100002);
 if(s!=BURN_DISC_APPENDABLE) {
   Cdrskin_report_disc_status(skin,s,0);
   fprintf(stderr,
 "cdrskin: FATAL : -msinfo can only operate on appendable (i.e. -multi) CD\n");
   {ret= 0; goto ex;}
 }
 disc= burn_drive_get_disc(drive);
 if(disc==NULL) {
   fprintf(stderr,"cdrskin: FATAL : Cannot obtain info about CD content\n");
   {ret= 0; goto ex;}
 }
 sessions= burn_disc_get_sessions(disc,&num_sessions);
 for(session_no= 0; session_no<num_sessions; session_no++) {
   tracks= burn_session_get_tracks(sessions[session_no],&num_tracks);
   if(tracks==NULL || num_tracks<=0)
 continue;
   burn_track_get_entry(tracks[0],&toc_entry);
   lba= burn_msf_to_lba(toc_entry.pmin,toc_entry.psec,toc_entry.pframe);
 }
 if(lba==-123456789) {
   fprintf(stderr,"cdrskin: FATAL : Cannot find any track on CD\n");
   {ret= 0; goto ex;}
 }

 ret= Cdrskin_obtain_nwa(skin,&nwa,flag);
 if(ret<=0) {
   fprintf(stderr,
           "cdrskin: NOTE : Guessing next writeable address from leadout\n");
   burn_session_get_leadout_entry(sessions[num_sessions-1],&toc_entry);
   aux_lba= burn_msf_to_lba(toc_entry.pmin,toc_entry.psec,toc_entry.pframe);
   if(num_sessions>0)
     nwa= aux_lba+6900;
   else
     nwa= aux_lba+11400;
 }
 if(skin->msinfo_fd>=0) {
   sprintf(msg,"%d,%d\n",lba,nwa);
   write(skin->msinfo_fd,msg,strlen(msg));
 } else
   printf("%d,%d\n",lba,nwa);
 ret= 1;
ex:;

 /* must calm down my NEC ND-4570A afterwards */
 if(skin->verbosity>=Cdrskin_verbose_debuG)
   ClN(fprintf(stderr,"cdrskin_debug: doing extra release-grab cycle\n"));
 Cdrskin_release_drive(skin,0);
 Cdrskin_grab_drive(skin,0);

 Cdrskin_release_drive(skin,0);
 return(ret);
}


/** Perform -toc under control of Cdrskin_atip().
    @param flag Bitfield for control purposes:
                bit0= do not list sessions separately (do it cdrecord style)
    @return <=0 error, 1 success
*/
int Cdrskin_toc(struct CdrskiN *skin, int flag)
{
 int num_sessions= 0,num_tracks= 0,lba= 0,track_count= 0,total_tracks= 0;
 int session_no, track_no;
 struct burn_drive *drive;
 struct burn_disc *disc= NULL;
 struct burn_session **sessions;
 struct burn_track **tracks;
 struct burn_toc_entry toc_entry;

 drive= skin->drives[skin->driveno].drive;

 disc= burn_drive_get_disc(drive);
 if(disc==NULL)
   goto cannot_read;
 sessions= burn_disc_get_sessions(disc,&num_sessions);
 if(flag&1) {
   for(session_no= 0; session_no<num_sessions; session_no++) {
     tracks= burn_session_get_tracks(sessions[session_no],&num_tracks);
     total_tracks+= num_tracks;
   }
   printf("first: 1 last %d\n",total_tracks);
 }
 for(session_no= 0; session_no<num_sessions; session_no++) {
   tracks= burn_session_get_tracks(sessions[session_no],&num_tracks);
   if(tracks==NULL)
 continue;
   if(!(flag&1))
     printf("first: %d last %d\n",track_count+1,track_count+num_tracks);
   for(track_no= 0; track_no<num_tracks; track_no++) {
     track_count++;
     burn_track_get_entry(tracks[track_no], &toc_entry);
     lba= burn_msf_to_lba(toc_entry.pmin,toc_entry.psec,toc_entry.pframe);
     if(track_no==0 && burn_session_get_hidefirst(sessions[session_no]))
       printf("cdrskin: NOTE : first track is marked as \"hidden\".\n");
     printf("track:  %2d lba: %9d (%9d) %2.2u:%2.2u:%2.2u",track_count,
            lba,4*lba,toc_entry.pmin,toc_entry.psec,toc_entry.pframe);
     printf(" adr: %d control: %d",toc_entry.adr,toc_entry.control);

     /* >>> From where does cdrecord take "mode" ? */

     /* This is not the "mode" as printed by cdrecord :
       printf(" mode: %d\n",burn_track_get_mode(tracks[track_no]));
     */
     /* own guess: cdrecord says "1" on data and "0" on audio : */
     printf(" mode: %d\n",((toc_entry.control&7)<4?0:1));

   }
   if((flag&1) && session_no<num_sessions-1)
 continue;
   burn_session_get_leadout_entry(sessions[session_no],&toc_entry);
   lba= burn_msf_to_lba(toc_entry.pmin,toc_entry.psec,toc_entry.pframe);
   printf("track:lout lba: %9d (%9d) %2.2u:%2.2u:%2.2u",
          lba,4*lba,toc_entry.pmin,toc_entry.psec,toc_entry.pframe);
   printf(" adr: %d control: %d",toc_entry.adr,toc_entry.control);
   printf(" mode: -1\n");
 }
 if(disc!=NULL)
   burn_disc_free(disc);
 return(1);
cannot_read:;
 fprintf(stderr,"cdrecord_emulation: Cannot read TOC header\n");
 fprintf(stderr,"cdrecord_emulation: Cannot read TOC/PMA\n");
 return(0);
}


/** Perform -atip .
    @param flag Bitfield for control purposes:
                bit0= perform -toc
    @return <=0 error, 1 success
*/
int Cdrskin_atip(struct CdrskiN *skin, int flag)
{
 int ret,is_not_really_erasable= 0;
 double x_speed_max, x_speed_min= -1.0;
 enum burn_disc_status s;
 struct burn_drive *drive;

 printf("cdrskin: pseudo-atip on drive %d\n",skin->driveno);
 ret= Cdrskin_checkdrive(skin,1);
 if(ret<=0)
   return(ret);
 ret= Cdrskin_grab_drive(skin,0);
 if(ret<=0)
   return(ret);
 drive= skin->drives[skin->driveno].drive;
 while(burn_drive_get_status(drive,NULL) != BURN_DRIVE_IDLE)
   usleep(100002);
 while ((s= burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
   usleep(100002);
 Cdrskin_report_disc_status(skin,s,0);
 if(s==BURN_DISC_APPENDABLE && skin->no_blank_appendable) {
   is_not_really_erasable= 1;
 } else if(s==BURN_DISC_EMPTY) {
   if(skin->verbosity>=Cdrskin_verbose_progresS)
     printf("Current: none\n");
   ret= 0; goto ex;
 }


#ifdef Cdrskin_atip_speed_brokeN

 /* <<< terrible stunt to get correct media speed info */
 if(skin->verbosity>=Cdrskin_verbose_debuG)   
   ClN(fprintf(stderr,
          "cdrskin_debug: redoing startup for speed inquiry stabilization\n"));


#ifndef Cdrskin_oldfashioned_api_usE

 if(strlen(skin->preskin->device_adr)<=0)
   burn_drive_get_adr(&(skin->drives[skin->driveno]),
                      skin->preskin->device_adr);

 Cdrskin_release_drive(skin,0);
 burn_finish();
 if(!burn_initialize()) {
   fflush(stdout);
   fprintf(stderr,"cdrskin : FATAL : Re-initialization of libburn failed\n");
   {ret= 0; goto ex;}
 } 
 ret= Cdrskin_grab_drive(skin,1); /* uses burn_drive_scan_and_grab() */
 if(ret<=0)
   return(ret);
 drive= skin->drives[skin->driveno].drive;

#else /* ! Cdrskin_oldfashioned_api_usE */

 Cdrskin_release_drive(skin,0);
 burn_finish();
 if(!burn_initialize()) {
   fflush(stdout);
   fprintf(stderr,"cdrskin : FATAL : Re-initialization of libburn failed\n");
   {ret= 0; goto ex;}
 } 
 if(strlen(skin->preskin->device_adr)>0)
   burn_drive_add_whitelist(skin->preskin->device_adr);
 while(!burn_drive_scan(&(skin->drives),&(skin->n_drives)))
   usleep(1002);
 ret= Cdrskin_grab_drive(skin,0);
 if(ret<=0)
   return(ret);
 drive= skin->drives[skin->driveno].drive;

#endif /* Cdrskin_oldfashioned_api_usE */

#endif /* Cdrskin_atip_speed_brokeN */

#ifdef Cdrskin_libburn_has_read_atiP
 ret= burn_disc_read_atip(drive);
 if(ret>0) {
   ret= burn_drive_get_min_write_speed(drive);
   x_speed_min= ((double) ret)/Cdrskin_libburn_cd_speed_factoR;
 }
#endif

#ifdef Cdrskin_libburn_has_burn_disc_unsuitablE
 if(burn_disc_get_status(drive) == BURN_DISC_UNSUITABLE) {
   printf("Current: UNSUITABLE MEDIA\n");
   {ret= 0; goto ex;}
 }
#endif

 ret= burn_drive_get_write_speed(drive);
 x_speed_max= ((double) ret)/Cdrskin_libburn_cd_speed_factoR;
 if(x_speed_min<0)
   x_speed_min= x_speed_max;
 printf("cdrskin: burn_drive_get_write_speed = %d  (%.1fx)\n",ret,x_speed_max);
 if(skin->verbosity>=Cdrskin_verbose_progresS) {
   if(burn_disc_erasable(drive))
     printf("Current: CD-RW\n");
   else
     printf("Current: CD-R\n");
 }    
 printf("ATIP info from disk:\n");
 if(burn_disc_erasable(drive)) {
   if(is_not_really_erasable)
     printf("  Is erasable (but not while in this incomplete state)\n");
   else
     printf("  Is erasable\n");
 } else {
   printf("  Is not erasable\n");
 }

#ifdef Cdrskin_libburn_has_get_start_end_lbA
 { int start_lba,end_lba,min,sec,fr;
   ret= burn_drive_get_start_end_lba(drive,&start_lba,&end_lba,0);
   if(ret>0) {
     burn_lba_to_msf(start_lba,&min,&sec,&fr);
     printf("  ATIP start of lead in:  %d (%-2.2d:%-2.2d/%-2.2d)\n",
            start_lba,min,sec,fr);
     burn_lba_to_msf(end_lba,&min,&sec,&fr);
     printf("  ATIP start of lead out: %d (%-2.2d:%-2.2d/%-2.2d)\n",
            end_lba,min,sec,fr);
   }
 }
#endif /* Cdrskin_libburn_has_get_start_end_lbA */

 printf("  1T speed low:  %.f 1T speed high: %.f\n",x_speed_min,x_speed_max);
 ret= 1;
 if(flag&1)
   Cdrskin_toc(skin,1);/*cdrecord seems to ignore -toc errors if -atip is ok */
ex:;
 Cdrskin_release_drive(skin,0);
 return(ret);
}


#ifndef Cdrskin_extra_leaN

/** Emulate the gracetime= behavior of cdrecord 
    @param flag Bitfield for control purposes:
                bit0= do not print message about pseudo-checkdrive
*/
int Cdrskin_wait_before_action(struct CdrskiN *skin, int flag)
/* flag: bit0= BLANK rather than write mode */
{
 int i;

 if(skin->verbosity>=Cdrskin_verbose_progresS) {
   char speed_text[80];
   if(skin->x_speed<0)
     strcpy(speed_text,"MAX");
   else if(skin->x_speed==0)
     strcpy(speed_text,"MIN");
   else
     sprintf(speed_text,"%.f",skin->x_speed);
   printf(
  "Starting to write CD/DVD at speed %s in %s %s mode for %s session.\n",
          speed_text,(skin->dummy_mode?"dummy":"real"),
          (flag&1?"BLANK":skin->preskin->write_mode_name),
          (skin->multi?"multi":"single"));
   printf("Last chance to quit, starting real write in %3d seconds.",
          skin->gracetime);
   fflush(stdout);
 }
 for(i= skin->gracetime-1;i>=0;i--) {
   usleep(1000000);
   if(skin->verbosity>=Cdrskin_verbose_progresS) {
     printf("\b\b\b\b\b\b\b\b\b\b\b\b\b %3d seconds.",i);
     fflush(stdout);
   }
 }
 if(skin->verbosity>=Cdrskin_verbose_progresS)
   {printf(" Operation starts.\n");fflush(stdout);} 
 return(1);
}

#endif /* Cdrskin_extra_leaN */


/** Perform blank=[all|fast]
    @return <=0 error, 1 success
*/
int Cdrskin_blank(struct CdrskiN *skin, int flag)
{
 enum burn_disc_status s;
 struct burn_progress p;
 struct burn_drive *drive;
 int ret,loop_counter= 0,hint_force= 0;
 double start_time;

 start_time= Sfile_microtime(0); /* will be refreshed later */
 ret= Cdrskin_grab_drive(skin,0);
 if(ret<=0)
   return(ret);
 drive= skin->drives[skin->driveno].drive;

 while(burn_drive_get_status(drive,NULL) != BURN_DRIVE_IDLE)
   usleep(100002);
 while ((s = burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
   usleep(100002);
 if(skin->verbosity>=Cdrskin_verbose_progresS)
   Cdrskin_report_disc_status(skin,s,0);

#ifdef Cdrskin_libburn_has_pretend_fulL
 if(s==BURN_DISC_UNSUITABLE) {
   if(skin->force_is_set) {
     fprintf(stderr,
             "cdrskin: NOTE : -force blank=... : Treating unsuitable media as burn_disc_full\n");
     ret= burn_disc_pretend_full(drive);
     s= burn_disc_get_status(drive);
   } else
     hint_force= 1;
 }
#endif /* Cdrskin_libburn_has_pretend_fulL */

 if(s!=BURN_DISC_FULL && 
    (s!=BURN_DISC_APPENDABLE || skin->no_blank_appendable)) {
   Cdrskin_release_drive(skin,0);
   if(s==BURN_DISC_BLANK) {
     fprintf(stderr,
       "cdrskin: NOTE : blank=... : media was already blank (and still is)\n");
     return(2);
   } else if(s==BURN_DISC_APPENDABLE) {
     fprintf(stderr,
             "cdrskin: FATAL : blank=... : media is still appendable\n");
   } else {
     fprintf(stderr,
             "cdrskin: FATAL : blank=... : no blankworthy disc found\n");
     if(hint_force)
       fprintf(stderr,
    "cdrskin: HINT : If you are certain to have a CD-RW, try option -force\n");
   }
   return(0);
 }
 if(!burn_disc_erasable(drive)) {
   fprintf(stderr,
           "cdrskin: FATAL : blank=... : media is not erasable\n");
   return(0);
 }
 if(skin->dummy_mode) {
   fprintf(stderr,
           "cdrskin: would have begun to blank disc if not in -dummy mode\n");
   goto blanking_done;
 }
 fprintf(stderr,"cdrskin: beginning to blank disc\n");
 Cdrskin_adjust_speed(skin,0);

#ifndef Cdrskin_extra_leaN
 Cdrskin_wait_before_action(skin,1);
#endif /* ! Cdrskin_extra_leaN */

 skin->drive_is_busy= 1;
 burn_disc_erase(drive,skin->blank_fast);

 loop_counter= 0;
 start_time= Sfile_microtime(0);
 while(burn_drive_get_status(drive, &p) != BURN_DRIVE_IDLE) {
   if(loop_counter>0)
     if(skin->verbosity>=Cdrskin_verbose_progresS) {
       int percent= 50;

       if(p.sectors>0) /* i want a display of 1 to 99 percent */
         percent= 1.0+((double) p.sector+1.0)/((double) p.sectors)*98.0;
       fprintf(stderr,
          "\rcdrskin: blanking ( done %2d%% , %lu seconds elapsed )          ",
          percent,(unsigned long) (Sfile_microtime(0)-start_time));
     }
   sleep(1);
   loop_counter++;
 }
blanking_done:;
 skin->drive_is_busy= 0;
 if(skin->verbosity>=Cdrskin_verbose_progresS) {
   fprintf(stderr,
    "\rcdrskin: blanking done                                             \n");
   printf("Blanking time:   %.3fs\n",Sfile_microtime(0)-start_time);
   fflush(stdout);
 }
 Cdrskin_release_drive(skin,0);
 return(1);
}


/** Report burn progress. This is done partially in cdrecord style.
    Actual reporting happens only if write progress hit the next MB or if in
    non-write-progress states a second has elapsed since the last report. 
    After an actual report a new statistics interval begins.
    @param drive_status As obtained from burn_drive_get_status()
    @param p Progress information from burn_drive_get_status()
    @param start_time Timestamp of burn start in seconds
    @param last_time Timestamp of report interval start in seconds
    @param total_count Returns the total number of bytes written so far
    @param total_count Returns the number of bytes written during interval
    @param flag Bitfield for control purposes:
                bit0= report in growisofs style rather than cdrecord style
    @return <=0 error, 1 seems to be writing payload, 2 doing something else 
*/
int Cdrskin_burn_pacifier(struct CdrskiN *skin,
                          enum burn_drive_status drive_status,
                          struct burn_progress *p,
                          double start_time, double *last_time,
                          double *total_count, double *last_count,
                          int *min_buffer_fill, int flag)
/*
 bit0= growisofs style
*/
{
 double bytes_to_write,written_bytes= 0.0,written_total_bytes= 0.0,buffer_size;
 double fixed_size,padding,sector_size,speed_factor;
 double measured_total_speed,measured_speed;
 double elapsed_time,elapsed_total_time,current_time;
 double estim_time,estim_minutes,estim_seconds,percent;
 int ret,fifo_percent,fill,space,advance_interval=0,new_mb,old_mb,time_to_tell;
 int fs,bs,old_track_idx,buffer_fill;
 char fifo_text[80],mb_text[40];
 char *debug_mark= ""; /* use this to prepend a marker text for experiments */

 /* for debugging */
 static double last_fifo_in= 0.0,last_fifo_out= 0.0,curr_fifo_in,curr_fifo_out;

 current_time= Sfile_microtime(0);
 elapsed_total_time= current_time-start_time;
 elapsed_time= current_time-*last_time;
 time_to_tell= (elapsed_time>=1.0)&&(elapsed_total_time>=1.0);

 if(drive_status==BURN_DRIVE_WRITING) {
   ;
 } else if(drive_status==BURN_DRIVE_WRITING_LEADIN

#ifdef Cdrskin_allow_libburn_taO
           || drive_status==BURN_DRIVE_WRITING_PREGAP
#endif

           ) {
   if(time_to_tell || skin->is_writing) {
     if(skin->verbosity>=Cdrskin_verbose_progresS) {
       if(skin->is_writing)
         fprintf(stderr,"\n");
       fprintf(stderr,
           "\rcdrskin: working pre-track (burning since %.f seconds)         ",
           elapsed_total_time);
     }
     skin->is_writing= 0;
     advance_interval= 1;
   }
   {ret= 2; goto ex;}
 } else if(drive_status==BURN_DRIVE_WRITING_LEADOUT

#ifdef Cdrskin_allow_libburn_taO
           || drive_status==BURN_DRIVE_CLOSING_TRACK
           || drive_status==BURN_DRIVE_CLOSING_SESSION
#endif

          ) {

#ifdef Cdrskin_allow_libburn_taO
   if(drive_status==BURN_DRIVE_CLOSING_SESSION &&
      skin->previous_drive_status!=drive_status)
     {printf("\nFixating...\n"); fflush(stdout);}
#endif

   if(time_to_tell || skin->is_writing) {
     if(skin->verbosity>=Cdrskin_verbose_progresS) {
       if(skin->is_writing)
         fprintf(stderr,"\n");
       fprintf(stderr,
           "\rcdrskin: working post-track (burning since %.f seconds)        ",
           elapsed_total_time);
     }
     skin->is_writing= 0;
     advance_interval= 1;
   }
   {ret= 2; goto ex;}
 } else
   goto thank_you_for_patience;

 old_track_idx= skin->supposed_track_idx;
#ifdef Cdrskin_progress_track_brokeN
 /* with libburn.0.2 there is always reported 0 as p->track */
 if(written_bytes<0) { /* track hop ? */
   if(skin->supposed_track_idx+1<skin->track_counter)
     skin->supposed_track_idx++;
 }
 /* >>> ask eventual fifo about writing fd */;
 if(p->track>0)
   skin->supposed_track_idx= p->track;
#else /* Cdrskin_progress_track_brokeN */
 skin->supposed_track_idx= p->track;
#endif /* ! Cdrskin_progress_track_brokeN */

 if(old_track_idx>=0 && old_track_idx<skin->supposed_track_idx) {
   Cdrtrack_get_size(skin->tracklist[old_track_idx],&fixed_size,&padding,
                     &sector_size,1);
   if(skin->verbosity>=Cdrskin_verbose_progresS)
     printf("\n");
   printf("%sTrack %-2.2d: Total bytes read/written: %.f/%.f (%.f sectors).\n",
          debug_mark,old_track_idx+1,fixed_size,fixed_size+padding,
          (fixed_size+padding)/sector_size);
 }

 sector_size= 2048.0;
 if(skin->supposed_track_idx>=0 && 
    skin->supposed_track_idx<skin->track_counter)
   Cdrtrack_get_size(skin->tracklist[skin->supposed_track_idx],&fixed_size,
                     &padding,&sector_size,0);

 bytes_to_write= ((double) p->sectors)*sector_size;
 written_total_bytes= ((double) p->sector)*sector_size;
 written_bytes= written_total_bytes-*last_count;

 if(written_total_bytes<1024*1024) {
thank_you_for_patience:;
   if(time_to_tell || (skin->is_writing && elapsed_total_time>=1.0)) {
     if(skin->verbosity>=Cdrskin_verbose_progresS) {
       if(skin->is_writing)
         fprintf(stderr,"\n");
       fprintf(stderr,
 "\rcdrskin: thank you for being patient since %.f seconds                   ",
           elapsed_total_time);
     }
     advance_interval= 1;
   }
   skin->is_writing= 0;
   {ret= 2; goto ex;}
 }
 new_mb= written_total_bytes/(1024*1024);
 old_mb= (*last_count)/(1024*1024);
 if(new_mb==old_mb && !(written_total_bytes>=skin->fixed_size &&
                        skin->fixed_size>0 && time_to_tell))
   {ret= 1; goto ex;}


#ifndef Cdrskin_extra_leaN

 percent= 0.0;
 if(bytes_to_write>0)
   percent= written_total_bytes/bytes_to_write*100.0;
 measured_total_speed= 0.0;
 measured_speed= 0.0;
 estim_time= -1.0;
 estim_minutes= -1.0;
 estim_seconds= -1.0;
 if(elapsed_total_time>0.0) {
   measured_total_speed= written_total_bytes/elapsed_total_time;
   estim_time= (bytes_to_write-written_bytes)/measured_total_speed;
   if(estim_time>0.0 && estim_time<86400.0) {
     estim_minutes= ((int) estim_time)/60;
     estim_seconds= estim_time-estim_minutes*60.0;
     if(estim_seconds<0.0)
       estim_seconds= 0.0;
   }
 }
 if(elapsed_time>0.0)
   measured_speed= written_bytes/elapsed_time;
 else if(written_bytes>0.0)
   measured_speed= 99.91*Cdrskin_cd_speed_factoR;
 if(measured_speed<=0.0 && written_total_bytes>=skin->fixed_size && 
    skin->fixed_size>0) {
   if(!skin->is_writing)
     goto thank_you_for_patience;
   skin->is_writing= 0;
   measured_speed= measured_total_speed;
 } else 
   skin->is_writing= 1;
 if(skin->supposed_track_idx<0)
   skin->supposed_track_idx= 0;
 if(*last_count<=0.0)
   printf("%-78.78s\r","");
 if(skin->verbosity>=Cdrskin_verbose_progresS) {
   if(flag&1) {
     printf("%.f/%.f (%2.1f%%) @%1.1f, remaining %.f:%2.2d\n",
            written_total_bytes,bytes_to_write,percent,
            measured_speed/Cdrskin_cd_speed_factoR,
            estim_minutes,(int) estim_seconds);
   } else {
     fill= 0;
     fifo_percent= 50;
     fifo_text[0]= 0;
     curr_fifo_in= last_fifo_in;
     curr_fifo_out= last_fifo_out;
     if(skin->fifo!=NULL) {
       ret= Cdrfifo_get_buffer_state(skin->fifo,&fill,&space,0);
       buffer_size= fill+space;
       if(ret==2 || ret==0) {
         fifo_percent= 100;
       } else if(ret>0 && buffer_size>0.0) {
         /* obtain minimum fill of pacifier interval */
         Cdrfifo_next_interval(skin->fifo,&fill,0);
         fifo_percent= 100.0*((double) fill)/buffer_size;
         if(fifo_percent<100 && fill>0)
           fifo_percent++;
       }
       if(skin->verbosity>=Cdrskin_verbose_debuG) {
         Cdrfifo_get_counters(skin->fifo,&curr_fifo_in,&curr_fifo_out,0);
         Cdrfifo_get_sizes(skin->fifo,&bs,&fs,0);
       }
     }
     if(skin->fifo_size>0) {
       sprintf(fifo_text,"(fifo %3d%%) ",fifo_percent);
       if(skin->verbosity>=Cdrskin_verbose_debuG) {
         fprintf(stderr,
                 "\ncdrskin_debug: fifo >= %9d / %d :  %8.f in, %8.f out\n",
                 fill,space+fill,
                 curr_fifo_in-last_fifo_in,curr_fifo_out-last_fifo_out);
         last_fifo_in= curr_fifo_in;
         last_fifo_out= curr_fifo_out;
       }
     }
     if(skin->supposed_track_idx >= 0 && 
        skin->supposed_track_idx < skin->track_counter) {
       /* fixed_size,padding are fetched above via Cdrtrack_get_size() */;
     } else if(skin->fixed_size!=0) {
       fixed_size= skin->fixed_size;
       padding= skin->padding;
     }
     if(fixed_size) {
       sprintf(mb_text,"%4d of %4d",
               (int) (written_total_bytes/1024.0/1024.0),
               (int) ((fixed_size+padding)/1024.0/1024.0));
     } else
       sprintf(mb_text,"%4d",(int) (written_total_bytes/1024.0/1024.0));
     speed_factor= Cdrskin_cd_speed_factoR*sector_size/2048;

     buffer_fill= 50;
#ifdef Cdrskin_libburn_has_buffer_progresS
     if(p->buffer_capacity>0)
       buffer_fill= (double) (p->buffer_capacity - p->buffer_available)*100.0
                    / (double) p->buffer_capacity;
     
#endif /* Cdrskin_libburn_has_buffer_progresS */
     if(buffer_fill<*min_buffer_fill)
       *min_buffer_fill= buffer_fill;

     printf("\r%sTrack %-2.2d: %s MB written %s[buf %3d%%]  %4.1fx.",
            debug_mark,skin->supposed_track_idx+1,mb_text,fifo_text,
            buffer_fill,measured_speed/speed_factor);
     fflush(stdout);
   }
   if(skin->is_writing==0) {
     printf("\n");
     goto thank_you_for_patience;
   }
 }

#else /* ! Cdrskin_extra_leaN */

 if(skin->supposed_track_idx<0)
   skin->supposed_track_idx= 0;
 if(written_bytes<=0.0 && written_total_bytes>=skin->fixed_size && 
    skin->fixed_size>0) {
   if(!skin->is_writing)
     goto thank_you_for_patience;
   skin->is_writing= 0;
 } else {
   if(!skin->is_writing)
     printf("\n");
   skin->is_writing= 1;
 }
 printf("\rTrack %-2.2d: %3d MB written ",
        skin->supposed_track_idx+1,(int) (written_total_bytes/1024.0/1024.0));
 fflush(stdout);
 if(skin->is_writing==0)
   printf("\n");

#endif /* Cdrskin_extra_leaN */


 advance_interval= 1;
 ret= 1;
ex:; 
 if(advance_interval) {
   if(written_total_bytes>0)
     *last_count= written_total_bytes;
   else
     *last_count= 0.0;
   if(*last_count>*total_count)
     *total_count= *last_count;
   *last_time= current_time;
 }
 skin->previous_drive_status= drive_status;
 return(ret);
}


/** Determines the effective write mode and checks wether the drive promises
    to support it.
    @param s state of target media, obtained from burn_disc_get_status(), 
             submit BURN_DISC_BLANK if no real state is available
*/
int Cdrskin_activate_write_mode(struct CdrskiN *skin, enum burn_disc_status s,
                                int flag)
{
 int ok, was_still_default= 0, block_type_demand,track_type,sector_size, i;
 struct burn_drive_info *drive_info;

 if(strcmp(skin->preskin->write_mode_name,"DEFAULT")==0) {
   was_still_default= 1;

#ifdef Cdrskin_allow_libburn_taO
   if(s  == BURN_DISC_APPENDABLE) {
     strcpy(skin->preskin->write_mode_name,"TAO");

     was_still_default= 2; /*<<< prevents trying of SAO if drive dislikes TAO*/

   } else
#endif

     strcpy(skin->preskin->write_mode_name,"SAO");
 }
 if(strcmp(skin->preskin->write_mode_name,"RAW/RAW96R")==0) {
   skin->write_type= BURN_WRITE_RAW;
   skin->block_type= BURN_BLOCK_RAW96R;

#ifdef Cdrskin_allow_libburn_taO
 } else if(strcmp(skin->preskin->write_mode_name,"TAO")==0) {
   strcpy(skin->preskin->write_mode_name,"TAO");
   skin->write_type= BURN_WRITE_TAO;
   skin->block_type= BURN_BLOCK_MODE1;
#endif /* Cdrskin_allow_libburn_taO */

 } else {
   strcpy(skin->preskin->write_mode_name,"SAO");
   skin->write_type= BURN_WRITE_SAO;
   skin->block_type= BURN_BLOCK_SAO;
 }

 /* check wether desired type combination is available with drive */
 if(skin->driveno<0 || skin->driveno>skin->n_drives) {
   if(skin->verbosity>=Cdrskin_verbose_debuG)
     ClN(printf("cdrskin_debug: WARNING : No drive selected with Cdrskin_activate_write_mode\n"));
   goto it_is_done;
 }

 /* <<< this should become a libburn API function.The knowledge about TAO audio
        track block type is quite inappropriate here. It refers to a habit of
        spc_select_write_params() (and MMC-1 table 61). But the knowledge about
        the tracklist is rather cdrskin realm. (ponder ...)
 */
check_with_drive:;
 drive_info= skin->drives+skin->driveno;
 ok= 0;
 if(skin->write_type==BURN_WRITE_RAW)
   ok= !!(drive_info->raw_block_types & BURN_BLOCK_RAW96R);
 else if(skin->write_type==BURN_WRITE_SAO)
   ok= !!(drive_info->sao_block_types & BURN_BLOCK_SAO);
 else if(skin->write_type==BURN_WRITE_TAO) {
   block_type_demand= 0;
   for(i=0;i<skin->track_counter;i++) {
     Cdrtrack_get_track_type(skin->tracklist[i],&track_type,&sector_size,0);
     if(track_type==BURN_AUDIO)
       block_type_demand|= BURN_BLOCK_RAW0;
     else
       block_type_demand|= BURN_BLOCK_MODE1;
   }
   ok= ((drive_info->tao_block_types & block_type_demand)==block_type_demand);
 }

 if(!ok) {
   fprintf(stderr,
           "cdrskin: %s : Drive indicated refusal for write mode %s.\n",
           (skin->force_is_set || was_still_default==1?"WARNING":"FATAL"),
           skin->preskin->write_mode_name);
   if(! skin->force_is_set) {
     if(was_still_default==1) {
       if(skin->write_type==BURN_WRITE_RAW ||
          skin->write_type==BURN_WRITE_SAO) {
         skin->write_type= BURN_WRITE_TAO;
         skin->block_type= BURN_BLOCK_MODE1;
         strcpy(skin->preskin->write_mode_name,"TAO");
       } else {
         skin->write_type= BURN_WRITE_SAO;
         skin->block_type= BURN_BLOCK_SAO;
         strcpy(skin->preskin->write_mode_name,"SAO");
       }
       was_still_default= 2; /* do not try more than once */
       goto check_with_drive;
     }
     fprintf(stderr,"cdrskin: HINT : If you are certain that the drive will do, try option -force\n");
     return(0);
   }
 }
it_is_done:;
 if(skin->verbosity>=Cdrskin_verbose_cmD)
   printf("cdrskin: write type : %s\n", skin->preskin->write_mode_name);
 return(1);
}


/** Burn data via libburn according to the parameters set in skin.
    @return <=0 error, 1 success
*/
int Cdrskin_burn(struct CdrskiN *skin, int flag)
{
 struct burn_disc *disc;
 struct burn_session *session;
 struct burn_write_opts *o;
 enum burn_disc_status s;
 enum burn_drive_status drive_status;
 struct burn_progress p;
 struct burn_drive *drive;
 int ret,loop_counter= 0,max_track= -1,i,hflag,nwa,num;
 int fifo_disabled= 0,fifo_percent,total_min_fill,mb,min_buffer_fill= 101;
 double put_counter,get_counter,empty_counter,full_counter;
 double start_time,last_time;
 double total_count= 0.0,last_count= 0.0,size,padding,sector_size= 2048.0;
 double sectors;

 printf("cdrskin: beginning to burn disk\n");

 disc= burn_disc_create();
 session= burn_session_create();
 ret= burn_disc_add_session(disc,session,BURN_POS_END);
 if(ret==0) {
   fprintf(stderr,"cdrskin: FATAL : cannot add session to disc object.\n");
   return(0);
 }

 skin->fixed_size= 0.0;
 for(i=0;i<skin->track_counter;i++) {
   hflag= (skin->verbosity>=Cdrskin_verbose_debuG);
   if(i==skin->track_counter-1)
     Cdrtrack_ensure_padding(skin->tracklist[i],hflag&1);
   ret= Cdrtrack_add_to_session(skin->tracklist[i],i,session,hflag);
   if(ret<=0) {
     fprintf(stderr,"cdrskin: FATAL : cannot add track %d to session.\n",i+1);
     return(0);
   }
   Cdrtrack_get_size(skin->tracklist[i],&size,&padding,&sector_size,0);
   if(size>0)
     skin->fixed_size+= size+padding;
 }

 ret= Cdrskin_grab_drive(skin,0);
 if(ret<=0)
   return(ret);
 drive= skin->drives[skin->driveno].drive;

 while(burn_drive_get_status(drive, NULL) != BURN_DRIVE_IDLE)
   usleep(100002); /* >>> ??? add a timeout ? */

 while((s= burn_disc_get_status(drive)) == BURN_DISC_UNREADY)
   usleep(100002); /* >>> ??? add a timeout ? */

 if(skin->verbosity>=Cdrskin_verbose_progresS)
   Cdrskin_report_disc_status(skin,s,0);

 ret= Cdrskin_activate_write_mode(skin,s,0);
 if(ret<=0) {
   fprintf(stderr,
           "cdrskin: FATAL : Cannot activate the desired write mode\n");
   ret= 0; goto ex;
 } 

#ifdef Cdrskin_libburn_has_multI
 if (s == BURN_DISC_APPENDABLE) {

#ifdef Cdrskin_allow_sao_for_appendablE
   ;
#else
   if(skin->write_type!=BURN_WRITE_TAO) {
     Cdrskin_release_drive(skin,0);
     fprintf(stderr,"cdrskin: FATAL : For now only write mode -tao can be used with appendable disks\n");
     return(0);
   }
#endif /* ! Cdrskin_allow_sao_for_appendablE */

 } else if (s != BURN_DISC_BLANK) {
#else
 if (s != BURN_DISC_BLANK) {
#endif
   Cdrskin_release_drive(skin,0);
   fprintf(stderr,"cdrskin: FATAL : no writeable media detected.\n");
   return(0);
 }

 
#ifndef Cdrskin_extra_leaN

 if(skin->verbosity>=Cdrskin_verbose_progresS) {
   for(i=0;i<skin->track_counter;i++) {
     Cdrtrack_get_size(skin->tracklist[i],&size,&padding,&sector_size,0);
     if(size<=0) {
       printf("Track %-2.2d: data  unknown length",i+1);
     } else {
       mb= size/1024.0/1024.0;
       printf("Track %-2.2d: data %5d MB        ",i+1,mb);
     }
     if(padding>0)
       printf(" padsize:  %.f KB\n",padding/1024.0);
     else
       printf("\n");
   }
   if(skin->fixed_size<=0) {
     printf("Total size:       0 MB (00:00.00) = 0 sectors\n");
     printf("Lout start:       0 MB (00:02/00) = 0 sectors\n");
   } else {
     /* >>> This is quite a fake. Need to learn about 12:35.25 and "Lout" 
            ??? Is there a way to obtain the toc in advance (print_cue()) ? */
     double seconds;
     int min,sec,frac;

     mb= skin->fixed_size/1024.0/1024.0;
     seconds= skin->fixed_size/150.0/1024.0+2.0;
     min= seconds/60.0;
     sec= seconds-min*60;
     frac= (seconds-min*60-sec)*100;
     if(frac>99)
       frac= 99;
     sectors= (int) (skin->fixed_size/sector_size);
     if(sectors*sector_size != skin->fixed_size)
       sectors++;
     printf("Total size:    %5d MB (%-2.2d:%-2.2d.%-2.2d) = %d sectors\n",
             mb,min,sec,frac,(int) sectors);
     seconds+= 2;
     min= seconds/60.0;
     sec= seconds-min*60;
     frac= (seconds-min*60-sec)*100;
     if(frac>99)
       frac= 99;
     printf("Lout start:    %5d MB (%-2.2d:%-2.2d/%-2.2d) = %d sectors\n",
            mb,min,sec,frac,(int) sectors);
   }
 }

 Cdrskin_wait_before_action(skin,0);
 ret= Cdrskin_fill_fifo(skin,0);
 if(ret<=0) {
   fprintf(stderr,"cdrskin: FATAL : filling of fifo failed\n");
   goto ex;
 }

#endif /* ! Cdrskin_extra_leaN */


 o= burn_write_opts_new(drive);
 burn_write_opts_set_perform_opc(o, 0);

#ifdef Cdrskin_libburn_has_multI
 if(skin->multi)
   fprintf(stderr,
           "cdrskin: NOTE : Option -multi set. Media will be appendable.\n");
 burn_write_opts_set_multi(o,skin->multi);
#endif

 burn_write_opts_set_write_type(o,skin->write_type,skin->block_type);
 if(skin->dummy_mode) {
   fprintf(stderr,
           "cdrskin: NOTE : -dummy mode will prevent actual writing\n");
   burn_write_opts_set_simulate(o, 1);
 }
 burn_write_opts_set_underrun_proof(o,skin->burnfree);

 Cdrskin_adjust_speed(skin,0);
 if(skin->verbosity>=Cdrskin_verbose_progresS) {
   ret= Cdrskin_obtain_nwa(skin, &nwa,0);
   if(ret>0)
     printf("Starting new track at sector: %d\n",nwa);
 }
 skin->drive_is_busy= 1;
 burn_disc_write(o, disc);
 if(skin->preskin->abort_handler==-1)
   Cleanup_set_handlers(skin,(Cleanup_app_handler_T) Cdrskin_abort_handler,4);
 last_time= start_time= Sfile_microtime(0);

 burn_write_opts_free(o);

 while (burn_drive_get_status(drive, NULL) == BURN_DRIVE_SPAWNING) {

   /* >>> how do i learn about success or failure ? */

   ;
 }
 loop_counter= 0;
 while (1) {
    drive_status= burn_drive_get_status(drive, &p);
    if(drive_status==BURN_DRIVE_IDLE)
 break;

   /* >>> how do i learn about success or failure ? */

   if(loop_counter>0)
     Cdrskin_burn_pacifier(skin,drive_status,&p,start_time,&last_time,
                           &total_count,&last_count,&min_buffer_fill,0);


   /* <<< debugging : artificial abort without a previous signal */;
   if(skin->abort_after_bytecount>=0.0 && 
      total_count>=skin->abort_after_bytecount) {
      /* whatever signal handling is installed: this thread is the boss now */
      fprintf(stderr,
       "cdrskin: DEVELOPMENT : synthetic abort by abort_after_bytecount=%.f\n",
              skin->abort_after_bytecount);
      skin->control_pid= getpid();
      ret= Cdrskin_abort_handler(skin,0,0);
      fprintf(stderr,"cdrskin: done (aborted)\n");
      exit(1);
   }


   if(max_track<skin->supposed_track_idx)
      max_track= skin->supposed_track_idx;

#ifndef Cdrskin_extra_leaN
   if(skin->fifo==NULL || fifo_disabled) {
     usleep(20000);
   } else {
     ret= Cdrfifo_try_to_work(skin->fifo,20000,NULL,NULL,0);
     if(ret<0) {
       int abh;

       abh= skin->preskin->abort_handler;
       if(abh!=2) 
         fprintf(stderr,
              "\ncdrskin: FATAL : fifo encountered error during burn loop.\n");
       if(abh==0) {
         ret= -1; goto ex;
       } else if(abh==1 || abh==3 || abh==4 || abh==-1) {
         Cdrskin_abort_handler(skin,0,0);
         fprintf(stderr,"cdrskin: done (aborted)\n");
         exit(10);
       } else {
          if(skin->verbosity>=Cdrskin_verbose_debuG)
            fprintf(stderr,
                    "\ncdrskin_debug: Cdrfifo_try_to_work() returns %d\n",ret);
       }
     }
     if(ret==2) { /* <0 = error , 2 = work is done */
       if(skin->verbosity>=Cdrskin_verbose_debuG)
         fprintf(stderr,"\ncdrskin_debug: fifo ended work with ret=%d\n",ret);
       fifo_disabled= 1;
     }
   }
#else /* ! Cdrskin_extra_leaN */
   usleep(20000);
#endif /* Cdrskin_extra_leaN */

   loop_counter++;
 }
 skin->drive_is_busy= 0;
 if(skin->verbosity>=Cdrskin_verbose_progresS)
   printf("\n");
 if(max_track<0) {
   printf("Track 01: Total bytes read/written: %.f/%.f (%.f sectors).\n",
          total_count,total_count,total_count/sector_size);
 } else {
   Cdrtrack_get_size(skin->tracklist[max_track],&size,&padding,&sector_size,1);
   printf(
         "Track %-2.2d: Total bytes read/written: %.f/%.f (%.f sectors).\n",
         max_track+1,size,size+padding,(size+padding)/sector_size);
 }
 if(skin->verbosity>=Cdrskin_verbose_progresS)
   printf("Writing  time:  %.3fs\n",Sfile_microtime(0)-start_time);


#ifndef Cdrskin_extra_leaN

 if(skin->fifo!=NULL && skin->fifo_size>0) {
   int dummy,final_fill;
   Cdrfifo_get_buffer_state(skin->fifo,&final_fill,&dummy,0);
   if(final_fill>0) {
fifo_full_at_end:;
     fprintf(stderr,
       "cdrskin: FATAL : Fifo still contains data after burning has ended.\n");
     fprintf(stderr,
       "cdrskin: FATAL : %.d bytes left.\n",final_fill);
     fprintf(stderr,
       "cdrskin: FATAL : This indicates an overflow of the last track.\n");
     fprintf(stderr,
     "cdrskin: NOTE : The media might appear ok but is probably truncated.\n");
     ret= -1; goto ex;
   }

#ifdef Cdrskin_libburn_leaves_inlet_opeN
   for(i= 0;i<skin->track_counter;i++) {
     ret= Cdrtrack_has_input_left(skin->tracklist[i],0);
     if(ret>0) {
       fprintf(stderr,
  "cdrskin: FATAL : fifo outlet of track #%d is still buffering some bytes.\n",
               i+1);
       goto fifo_full_at_end;
     }
   }
#endif /* Cdrskin_libburn_leaves_inlet_opeN */

 }

 if(skin->verbosity>=Cdrskin_verbose_progresS) {
   if(skin->fifo!=NULL && skin->fifo_size>0) {
     int dummy;

     Cdrfifo_get_min_fill(skin->fifo,&total_min_fill,&dummy,0);
     fifo_percent= 100.0*((double) total_min_fill)/(double) skin->fifo_size;
     if(fifo_percent==0 && total_min_fill>0)
       fifo_percent= 1;
     Cdrfifo_get_cdr_counters(skin->fifo,&put_counter,&get_counter,
                              &empty_counter,&full_counter,0);
     fflush(stdout);
     fprintf(stderr,"Cdrskin: fifo had %.f puts and %.f gets.\n",
             put_counter,get_counter);
     fprintf(stderr,
  "Cdrskin: fifo was %.f times empty and %.f times full, min fill was %d%%.\n",
             empty_counter,full_counter,fifo_percent);
   }
   drive_status= burn_drive_get_status(drive, &p);

#ifdef Cdrskin_libburn_has_buffer_min_filL
   /* cdrskin recorded its own coarse min_buffer_fill.
      libburn's is finer - if enough bytes were processed so it is available.*/
   if(p.buffer_min_fill<=p.buffer_capacity && p.buffer_capacity>0) {
     num= 100.0 * ((double) p.buffer_min_fill)/(double) p.buffer_capacity;
     if(num<min_buffer_fill)
       min_buffer_fill= num; 
   }
#endif /* Cdrskin_libburn_has_buffer_min_filL */

   if(min_buffer_fill>100)
     min_buffer_fill= 50;
   printf("Min drive buffer fill was %d%%\n", min_buffer_fill);
 }

#endif /* ! Cdrskin_extra_leaN */


 if(skin->verbosity>=Cdrskin_verbose_progresS) 
   printf("cdrskin: burning done\n");
 ret= 1;
ex:;
 skin->drive_is_busy= 0;
 if(skin->verbosity>=Cdrskin_verbose_debuG)
   ClN(printf("cdrskin_debug: do_eject= %d\n",skin->do_eject));
 Cdrskin_release_drive(skin,0);
 for(i= 0;i<skin->track_counter;i++)
   Cdrtrack_cleanup(skin->tracklist[i],0);
 burn_session_free(session);
 burn_disc_free(disc);
 return(ret);
}


/** Work around the failure of libburn to eject the tray.
    This employs a system(2) call and is therefore an absolute no-no for any
    pseudo user identities.
    @return <=0 error, 1 success
*/
int Cdrskin_eject(struct CdrskiN *skin, int flag)
{

#ifndef Cdrskin_burn_drive_eject_brokeN

#ifndef Cdrskin_oldfashioned_api_usE
 int i,ret,max_try= 5;

 if(!skin->do_eject)
   return(1);

 /* A60923 :
    Still not in libburn-0.2.2 : prevent SIGSEV on non-existent drive */
 if(skin->n_drives<=skin->driveno || skin->driveno < 0)
   return(2);

 /* <<< A61012 : retry loop might now be obsolete 
                (a matching bug in burn_disc_write_sync() was removed ) */
 for(i= 0;i<max_try;i++) {
   ret= Cdrskin_grab_drive(skin,2|((i<max_try-1)<<2));
   if(ret>0 || i>=max_try-1)
 break;
   if(skin->verbosity>=Cdrskin_verbose_progresS)
     fprintf(stderr,
          "cdrskin: NOTE : Attempt #%d of %d failed to grab drive for eject\n",
          i+1,max_try);
   usleep(1000000);
 }
 if(ret>0) {
    ret= Cdrskin_release_drive(skin,1);
    if(ret<=0)
      goto sorry_failed_to_eject;
 } else {
sorry_failed_to_eject:;
   fprintf(stderr,"cdrskin: SORRY : Failed to finally eject tray.\n");
   return(0);
 }
 return(1);

#else 

 if(!skin->do_eject)
   return(1);
 if(Cdrskin_grab_drive(skin,2)>0) {
    Cdrskin_release_drive(skin,1);
 } else {
   fprintf(stderr,"cdrskin: SORRY : Failed to finally eject tray.\n");
   return(0);
 }
 return(1);

#endif

#else /* Cdrskin_burn_drive_eject_brokeN */

 int ret;
 char adr[Cdrskin_adrleN];
 char cmd[5*Cdrskin_strleN+16],shellsafe[5*Cdrskin_strleN+2];
 
 if(!skin->do_eject)
   return(1);
 if(skin->verbosity>=Cdrskin_verbose_progresS) 
   printf("cdrskin: trying to eject media\n");
 if(getuid()!=geteuid()) {
   fprintf(stderr,
     "cdrskin: SORRY : uid and euid differ. Will not start external eject.\n");
   fprintf(stderr,
     "cdrskin: HINT : Consider to allow rw-access to the writer device and\n");
   fprintf(stderr,
     "cdrskin: HINT : to run cdrskin under your normal user identity.\n");
   return(0);
 }

#ifdef Cdrskin_libburn_has_drive_get_adR
 ret= burn_drive_get_adr(&(skin->drives[skin->driveno]), adr);
 if(ret<=0)
   adr[0]= 0;
#else
 strcpy(adr,skin->drives[skin->driveno].location);
#endif

 if(strlen(skin->eject_device)>0)
   sprintf(cmd,"eject %s",Text_shellsafe(skin->eject_device,shellsafe,0));
 else if(strcmp(adr,"/dev/sg0")==0)
   sprintf(cmd,"eject /dev/sr0");
 else
   sprintf(cmd,"eject %s",Text_shellsafe(adr,shellsafe,0));
 ret= system(cmd);
 if(ret==0)
   return(1);
 return(0);

#endif /* Cdrskin_burn_drive_eject_brokeN */

}


/** Interpret all arguments of the program after libburn has been initialized
    and drives have been scanned. This call reports to stderr any valid 
    cdrecord options which are not implemented yet.
    @param flag Bitfield for control purposes: 
                bit0= do not finalize setup
                bit1= do not interpret (again) skin->preskin->pre_argv
    @return <=0 error, 1 success
*/
int Cdrskin_setup(struct CdrskiN *skin, int argc, char **argv, int flag)
{
 int i,k,ret,source_has_size=0;
 double value,grab_and_wait_value= -1.0;
 char *cpt,*value_pt,adr[Cdrskin_adrleN];
 struct stat stbuf;

 /* cdrecord 2.01 options which are not scheduled for implementation, yet */
 static char ignored_partial_options[][41]= {
   "timeout=", "debug=", "kdebug=", "kd=", "driver=", "ts=",
   "pregap=", "defpregap=", "mcn=", "isrc=", "index=", "textfile=",
   "pktsize=", "cuefile=",
   ""
 };
 static char ignored_full_options[][41]= {
   "-d", "-Verbose", "-V", "-silent", "-s", "-setdropts", "-prcap", "-inq",
   "-reset", "-abort", "-overburn", "-ignsize", "-useinfo", "-format", "-load",
   "-lock", "-fix", "-nofix", "-waiti",
   "-immed", "-raw", "-raw96p", "-raw16",
   "-clone", "-text", "-mode2", "-xa", "-xa1", "-xa2", "-xamix",
   "-cdi", "-isosize", "-preemp", "-nopreemp", "-copy", "-nocopy",
   "-scms", "-shorttrack", "-noshorttrack", "-packet", "-noclose",
   ""
 };

 /* are we pretending to be cdrecord ? */
 cpt= strrchr(argv[0],'/');
 if(cpt==NULL)
   cpt= argv[0];
 else
   cpt++;
 if(strcmp(cpt,"cdrecord")==0 && !(flag&1)) {
   fprintf(stderr,"\n");
   fprintf(stderr,
     "Note: This is not cdrecord by Joerg Schilling. Do not bother him.\n");
   fprintf(stderr,
     "      See cdrskin start message on stdout. See --help. See -version.\n");
   fprintf(stderr,"\n");
   /* allow automatic -tao to -sao redirection */
   skin->tao_to_sao_tsize=650*1024*1024;
 }

#ifndef Cdrskin_extra_leaN
 if(!(flag&2)) {
   if(skin->preskin->pre_argc>1) {
     ret= Cdrskin_setup(skin,skin->preskin->pre_argc,skin->preskin->pre_argv,
                        flag|1|2);
     if(ret<=0)
       return(ret);
   }
 }
#endif

 for (i= 1;i<argc;i++) {

   /* is this a known option which is not planned to be implemented ? */
   /* such an option will not be accepted as data source */
   for(k=0;ignored_partial_options[k][0]!=0;k++) {
     if(argv[i][0]=='-')
       if(strncmp(argv[i]+1,ignored_partial_options[k],
                            strlen(ignored_partial_options[k]))==0)
         goto no_volunteer;
     if(strncmp(argv[i],ignored_partial_options[k],
                        strlen(ignored_partial_options[k]))==0)
       goto no_volunteer;
   }
   for(k=0;ignored_full_options[k][0]!=0;k++) 
     if(strcmp(argv[i],ignored_full_options[k])==0) 
       goto no_volunteer;
   if(0) {
no_volunteer:;
     fprintf(stderr,"cdrskin: NOTE : ignoring unimplemented option : '%s'\n",
                    argv[i]);
     fprintf(stderr,
       "cdrskin: NOTE : option is waiting for a volunteer to implement it.\n");
 continue;
   }

   if(strncmp(argv[i],"abort_after_bytecount=",22)==0) {
     skin->abort_after_bytecount= Scanf_io_size(argv[i]+22,0);
     fprintf(stderr,
             "cdrskin: NOTE : will perform synthetic abort after %.f bytes\n",
             skin->abort_after_bytecount);

   } else if(strcmp(argv[i],"--abort_handler")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strncmp(argv[i],"-abort_max_wait=",16)==0) {
     value_pt= argv[i]+16;
     goto set_abort_max_wait;
   } else if(strncmp(argv[i],"abort_max_wait=",15)==0) {
     value_pt= argv[i]+15;
set_abort_max_wait:;
     value= Scanf_io_size(value_pt,0);
     if(value<0 || value>86400) {
       fprintf(stderr,
             "cdrskin: NOTE : ignored out-of-range value: abort_max_wait=%s\n",
             value_pt);
     } else {
       skin->abort_max_wait= value;
       if(skin->verbosity>=Cdrskin_verbose_cmD)
         printf(
            "cdrskin: maximum waiting time with abort handling : %d seconds\n",
            skin->abort_max_wait);
     }

   } else if(strcmp(argv[i],"--allow_setuid")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"--any_track")==0) {
     skin->single_track= -1;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf(
    "cdrskin: --any_track : will accept any unknown option as track source\n");

   } else if(strcmp(argv[i],"-atip")==0) {
     if(skin->do_atip<1)
       skin->do_atip= 1;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: will put out some -atip style lines\n");

   } else if(strcmp(argv[i],"-audio")==0) {
     skin->track_type= BURN_AUDIO;
     skin->track_type_by_default= 0;

   } else if(strncmp(argv[i],"-blank=",7)==0) {
     cpt= argv[i]+7;
     goto set_blank;
   } else if(strncmp(argv[i],"blank=",6)==0) {
     cpt= argv[i]+6;
set_blank:;
     if(strcmp(cpt,"all")==0 || strcmp(cpt,"disc")==0 
        || strcmp(cpt,"disk")==0) {
       skin->do_blank= 1;
       skin->blank_fast= 0;
     } else if(strcmp(cpt,"fast")==0 || strcmp(cpt,"minimal")==0) { 
       skin->do_blank= 1;
       skin->blank_fast= 1;
     } else if(strcmp(cpt,"help")==0) { 
       /* is handled in Cdrpreskin_setup() */;
     } else { 
       fprintf(stderr,"cdrskin: FATAL : blank option '%s' not supported yet\n",
                      cpt);
       return(0);
     }
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: blank mode : blank=%s\n",
            (skin->blank_fast?"fast":"all"));

   } else if(strcmp(argv[i],"--bragg_with_audio")==0) {
     /* OBSOLETE 0.2.3 : was handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"-checkdrive")==0) {
     skin->do_checkdrive= 1;

   } else if(strcmp(argv[i],"-data")==0) {

     /* >>> !!! All Subsequent Tracks Option */

     skin->track_type= BURN_MODE1;
     skin->track_type_by_default= 0;

   } else if(strcmp(argv[i],"--demand_a_drive")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"--devices")==0) {
     skin->do_devices= 1;


#ifndef Cdrskin_extra_leaN

   } else if(strncmp(argv[i],"dev_translation=",16)==0) {

     if(argv[i][16]==0) {
       fprintf(stderr,
         "cdrskin: FATAL : dev_translation= : missing separator character\n");
       return(0);
     }
     ret= Cdradrtrn_add(skin->adr_trn,argv[i]+17,argv[i]+16,1);
     if(ret==-2)
       fprintf(stderr,
           "cdrskin: FATAL : address_translation= : cannot allocate memory\n");
     else if(ret==-1)
       fprintf(stderr,
             "cdrskin: FATAL : address_translation= : table full (%d items)\n",
               Cdradrtrn_leN);
     else if(ret==0)
       fprintf(stderr,
   "cdrskin: FATAL : address_translation= : no address separator '%c' found\n",
               argv[i][17]);
     if(ret<=0)
       return(0);

#endif /* Cdrskin_extra_leaN */


   } else if(strncmp(argv[i],"-dev=",5)==0) {
     /* is handled in Cdrpreskin_setup() */;
   } else if(strncmp(argv[i],"dev=",4)==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"--drive_abort_on_busy")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"--drive_blocking")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"--drive_not_exclusive")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strncmp(argv[i],"-driveropts=",12)==0) {
     value_pt= argv[i]+12;
     goto set_driveropts;
   } else if(strncmp(argv[i],"driveropts=",11)==0) {
     value_pt= argv[i]+11;
set_driveropts:;
     if(strcmp(value_pt,"burnfree")==0 || strcmp(value_pt,"burnproof")==0) {
       skin->burnfree= 1;
       if(skin->verbosity>=Cdrskin_verbose_cmD)
         printf("cdrskin: burnfree : on\n");
     } else if(strcmp(argv[i]+11,"noburnfree")==0 ||
               strcmp(argv[i]+11,"noburnproof")==0 ) {
       skin->burnfree= 0;
       if(skin->verbosity>=Cdrskin_verbose_cmD)
         printf("cdrskin: burnfree : off\n");
     } else if(strcmp(argv[i]+11,"help")==0) {
       /* handled in Cdrpreskin_setup() */;
     } else 
       goto ignore_unknown;

   } else if(strcmp(argv[i],"-dummy")==0) {
     skin->dummy_mode= 1;

   } else if(strcmp(argv[i],"-eject")==0) {
     skin->do_eject= 1;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: eject after work : on\n");

   } else if(strncmp(argv[i],"eject_device=",13)==0) {
     if(strlen(argv[i]+13)>=sizeof(skin->eject_device)) {
       fprintf(stderr,
          "cdrskin: FATAL : eject_device=... too long. (max: %d, given: %d)\n",
          sizeof(skin->eject_device)-1,strlen(argv[i]+13));
       return(0);
     }
     strcpy(skin->eject_device,argv[i]+13);
     if(skin->verbosity>=Cdrskin_verbose_cmD)
#ifdef Cdrskin_burn_drive_eject_brokeN
       printf("cdrskin: eject_device : %s\n",skin->eject_device);
#else
       printf("cdrskin: ignoring obsolete  eject_device=%s\n",
              skin->eject_device);
#endif


#ifndef Cdrskin_extra_leaN

   } else if(strcmp(argv[i],"--fifo_disable")==0) {
     skin->fifo_enabled= 0;
     skin->fifo_size= 0;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: option fs=... disabled\n");

   } else if(strcmp(argv[i],"--fifo_start_empty")==0) { /* obsoleted */
     skin->fifo_start_at= 0;

   } else if(strncmp(argv[i],"fifo_start_at=",14)==0) {
     value= Scanf_io_size(argv[i]+14,0);
     if(value>1024.0*1024.0*1024.0)
       value= 1024.0*1024.0*1024.0;
     else if(value<0)
       value= 0;
     skin->fifo_start_at= value;

   } else if(strcmp(argv[i],"--fifo_per_track")==0) {
     skin->fifo_per_track= 1;

   } else if(strcmp(argv[i],"-force")==0) {
     skin->force_is_set= 1;

   } else if(strncmp(argv[i],"-fs=",4)==0) {
     value_pt= argv[i]+4;
     goto fs_equals;
   } else if(strncmp(argv[i],"fs=",3)==0) {
     value_pt= argv[i]+3;
fs_equals:;
     if(skin->fifo_enabled) {
       value= Scanf_io_size(value_pt,0);
       if(value<0.0 || value>1024.0*1024.0*1024.0) {
         fprintf(stderr,
                 "cdrskin: FATAL : fs=N expects a size between 0 and 1g\n");
         return(0);
       }
       skin->fifo_size= value;
       if(skin->verbosity>=Cdrskin_verbose_cmD)
         printf("cdrskin: fifo size : %d\n",skin->fifo_size);
     }

   } else if(strncmp(argv[i],"grab_drive_and_wait=",20)==0) {
     value_pt= argv[i]+20;
     grab_and_wait_value= Scanf_io_size(value_pt,0);

   } else if(strncmp(argv[i],"-gracetime=",11)==0) {
     value_pt= argv[i]+11;
     goto gracetime_equals;
   } else if(strncmp(argv[i],"gracetime=",10)==0) {
     value_pt= argv[i]+10;
gracetime_equals:;
     sscanf(value_pt,"%d",&(skin->gracetime));

#else /* ! Cdrskin_extra_leaN */

   } else if(
      strcmp(argv[i],"--fifo_disable")==0 ||
      strcmp(argv[i],"--fifo_start_empty")==0 ||
      strncmp(argv[i],"fifo_start_at=",14)==0 ||
      strcmp(argv[i],"--fifo_per_track")==0 ||
      strncmp(argv[i],"-fs=",4)==0 ||
      strncmp(argv[i],"fs=",3)==0 ||
      strncmp(argv[i],"-gracetime=",11)==0 ||
      strncmp(argv[i],"gracetime=",10)==0) {
     fprintf(stderr,
             "cdrskin: NOTE : lean version ignores option: '%s'\n",
             argv[i]);

#endif /*  Cdrskin_extra_leaN */

     
   } else if(strcmp(argv[i],"--help")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"-help")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"--ignore_signals")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"-multi")==0) {
#ifdef Cdrskin_libburn_has_multI
     skin->multi= 1;
#else
     fprintf(stderr,"cdrskin: SORRY : Option -multi is not available yet.\n");
#endif

   } else if(strcmp(argv[i],"-msinfo")==0) {
#ifdef Cdrskin_libburn_has_multI
     skin->do_msinfo= 1;
#else
     fprintf(stderr,"cdrskin: SORRY : Option -msinfo is not available yet.\n");
     return(0);
#endif

   } else if(strcmp(argv[i],"--no_abort_handler")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"--no_blank_appendable")==0) {
     skin->no_blank_appendable= 1;

   } else if(strcmp(argv[i],"--no_convert_fs_adr")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"--no_rc")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"-nopad")==0) {
     skin->padding= 0.0;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: padding : off\n");

   } else if(strcmp(argv[i],"--old_pseudo_scsi_adr")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"-pad")==0) {
     skin->padding= 15*2048;
     skin->set_by_padsize= 0;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: padding : %.f\n",skin->padding);

   } else if(strncmp(argv[i],"-padsize=",9)==0) {
     value_pt= argv[i]+9;
     goto set_padsize;
   } else if(strncmp(argv[i],"padsize=",8)==0) {
     value_pt= argv[i]+8;
set_padsize:;
     skin->padding= Scanf_io_size(argv[i]+8,0);
     skin->set_by_padsize= 1;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: padding : %.f\n",skin->padding);

   } else if(strcmp(argv[i],"-raw96r")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"-sao")==0 || strcmp(argv[i],"-dao")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if(strcmp(argv[i],"-scanbus")==0) {
     skin->do_scanbus= 1;

   } else if(strcmp(argv[i],"--single_track")==0) {
     skin->single_track= 1;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf(
 "cdrskin: --single_track : will only accept last argument as track source\n");

   } else if(strncmp(argv[i],"-speed=",7)==0) {
     value_pt= argv[i]+7;
     goto set_speed;
   } else if(strncmp(argv[i],"speed=",6)==0) {
     value_pt= argv[i]+6;
set_speed:;
     sscanf(value_pt,"%lf",&(skin->x_speed));
     if(skin->x_speed<1.0 && skin->x_speed!=0.0 && skin->x_speed!=-1) {
       fprintf(stderr,"cdrskin: FATAL : speed= must be -1, 0 or at least 1\n");
       return(0);
     }

     /* >>> cdrecord speed=0 -> minimum speed , libburn -> maximum speed */;

     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: speed : %f\n",skin->x_speed);

   } else if(strcmp(argv[i],"-swab")==0) {
     skin->swap_audio_bytes= 0;

   } else if(strcmp(argv[i],"-tao")==0) {
     /* is partly handled in Cdrpreskin_setup() */;

#ifndef Cdrskin_allow_libburn_taO

     if(skin->tao_to_sao_tsize<=0.0) {
       fprintf(stderr,"cdrskin: FATAL : libburn does not support -tao yet.\n");
       fprintf(stderr,"cdrskin: HINT : Try option  tao_to_sao_tsize=650m\n");
       return(0);
     }
     printf("cdrskin: NOTE : substituting mode -tao by mode -sao\n");
     strcpy(skin->preskin->write_mode_name,"SAO");

#endif /* ! Cdrskin_allow_libburn_taO */

   } else if(strncmp(argv[i],"tao_to_sao_tsize=",17)==0) {
     skin->tao_to_sao_tsize= Scanf_io_size(argv[i]+17,0);
     if(skin->tao_to_sao_tsize>Cdrskin_tracksize_maX)
       goto track_too_large;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
#ifdef Cdrskin_allow_libburn_taO
       printf("cdrskin: size default for non-tao write modes: %.f\n",
#else 
       printf("cdrskin: replace -tao by -sao with fixed size : %.f\n",
#endif
              skin->tao_to_sao_tsize);

   } else if(strcmp(argv[i],"-toc")==0) {
     skin->do_atip= 2;
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: will put out some -atip style lines plus -toc\n");

   } else if(strncmp(argv[i],"-tsize=",7)==0) {
     value_pt= argv[i]+7;
     goto set_tsize;
   } else if(strncmp(argv[i],"tsize=",6)==0) {
     value_pt= argv[i]+6;
set_tsize:;
     skin->fixed_size= Scanf_io_size(value_pt,0);
     if(skin->fixed_size>Cdrskin_tracksize_maX) {
track_too_large:;
       fprintf(stderr,"cdrskin: FATAL : track size too large\n");
       return(0);
     }
     if(skin->verbosity>=Cdrskin_verbose_cmD)
       printf("cdrskin: fixed track size : %.f\n",skin->fixed_size);

   } else if(strcmp(argv[i],"-v")==0 || strcmp(argv[i],"-verbose")==0) {
     /* is handled in Cdrpreskin_setup() */;

   } else if( i==argc-1 ||
             (skin->single_track==0 && strchr(argv[i],'=')==NULL 
               && !(argv[i][0]=='-' && argv[i][1]!=0) ) ||
             (skin->single_track==-1)) {
     if(strlen(argv[i])>=sizeof(skin->source_path)) {
       fprintf(stderr,
            "cdrskin: FATAL : source_address too long. (max: %d, given: %d)\n",
               sizeof(skin->source_path)-1,strlen(argv[i]));
       return(0);
     }
     source_has_size= 0;
     strcpy(skin->source_path,argv[i]);
     if(strcmp(skin->source_path,"-")==0) {
       if(skin->stdin_source_used) {
         fprintf(stderr,
    "cdrskin: FATAL : \"-\" (stdin) can be used as track source only once.\n");
         return(0);
       }
       skin->stdin_source_used= 1;
     } else if(argv[i][0]=='#' && (argv[i][1]>='0' && argv[i][1]<='9')) {
       if(skin->preskin->allow_fd_source==0) { 
         fprintf(stderr,
              "cdrskin: SORRY : '%s' is a reserved source path with cdrskin\n",
              argv[i]);
         fprintf(stderr,
      "cdrskin: SORRY : which would use an open file descriptor as source.\n");
         fprintf(stderr,
            "cdrskin: SORRY : Its usage is dangerous and disabled for now.\n");
         return(0);
       }
     } else {
       if(stat(skin->source_path,&stbuf)!=-1) {
         if((stbuf.st_mode&S_IFMT)==S_IFREG)
           source_has_size= 1;
         else if((stbuf.st_mode&S_IFMT)==S_IFDIR) {
           fprintf(stderr,
                   "cdrskin: FATAL : source address is a directory: '%s'\n",
                   skin->source_path);
           return(0);
         }
       }
     }
     if(! source_has_size) {
       if(skin->fixed_size<=0.0) {
         if(strcmp(skin->preskin->write_mode_name,"TAO")==0) {
           /* with TAO it is ok to have an undefined track length */;

#ifdef Cdrskin_allow_libburn_taO
         } else if(strcmp(skin->preskin->write_mode_name,"DEFAULT")==0) {
           strcpy(skin->preskin->write_mode_name,"TAO");
#endif

         } else if(skin->tao_to_sao_tsize>0.0) {
           skin->fixed_size= skin->tao_to_sao_tsize;
           printf(
        "cdrskin: NOTE : augmenting non-tao write mode by tao_to_sao_tsize\n");
           printf("cdrskin: NOTE : fixed size : %.f\n",skin->fixed_size);
         } else {
           fprintf(stderr,
#ifdef Cdrskin_allow_libburn_taO
           "cdrskin: FATAL : Track source '%s' needs -tao or tsize= or tao_to_sao_tsize=\n",
#else
           "cdrskin: FATAL : Track source '%s' needs a fixed tsize= or tao_to_sao_tsize=\n",
#endif
                   skin->source_path);
           return(0);
         }
       }
     }
     if(skin->track_counter>=Cdrskin_track_maX) {
       fprintf(stderr,"cdrskin: FATAL : too many tracks given. (max %d)\n",
               Cdrskin_track_maX);
       return(0);
     }
     ret= Cdrtrack_new(&(skin->tracklist[skin->track_counter]),skin,
                       skin->track_counter,
                       (strcmp(skin->source_path,"-")==0)<<1);
     if(ret<=0) {
       fprintf(stderr,
               "cdrskin: FATAL : creation of track control object failed.\n");
       return(ret);
     }
     skin->track_counter++;
     if(skin->verbosity>=Cdrskin_verbose_cmD) {
       if(strcmp(skin->source_path,"-")==0)
         printf("cdrskin: track %d data source : '-'  (i.e. standard input)\n",
                skin->track_counter);
       else
         printf("cdrskin: track %d data source : '%s'\n",
                skin->track_counter,skin->source_path);
     }
     /* reset track options */
     if(skin->set_by_padsize)
       skin->padding= 0; /* cdrecord-ProDVD-2.01b31 resets to 30k
                            the man page says padsize= is reset to 0
                            Joerg Schilling will change in 2.01.01 to 0 */
     skin->fixed_size= 0;
   } else {
ignore_unknown:;
     fprintf(stderr,"cdrskin: NOTE : ignoring unknown option : '%s'\n",
                    argv[i]);
   }
 }

 if(flag&1) /* no finalizing yet */
   return(1);

 if(skin->verbosity>=Cdrskin_verbose_cmD) {
   if(skin->preskin->abort_handler==1)
     printf("cdrskin: installed abort handler.\n");
   else if(skin->preskin->abort_handler==2) 
     printf("cdrskin: will try to ignore any signals.\n");
   else if(skin->preskin->abort_handler==3)
     printf("cdrskin: installed hard abort handler.\n");
   else if(skin->preskin->abort_handler==4)
     printf("cdrskin: installed soft abort handler.\n");
   else if(skin->preskin->abort_handler==-1)
     printf("cdrskin: will install abort handler in eventual burn loop.\n");
 }

 if(strlen(skin->preskin->raw_device_adr)>0 ||
    strlen(skin->preskin->device_adr)>0) {
   if(strlen(skin->preskin->device_adr)>0)
     cpt= skin->preskin->device_adr;
   else
     cpt= skin->preskin->raw_device_adr;
   if(strcmp(cpt,"ATA")!=0 && strcmp(cpt,"ATAPI")!=0 && strcmp(cpt,"SCSI")!=0){
     ret= Cdrskin_dev_to_driveno(skin,cpt,&(skin->driveno),0);
     if(ret<=0)
       return(ret);
     if(skin->verbosity>=Cdrskin_verbose_cmD) {

#ifdef Cdrskin_libburn_has_drive_get_adR
       ret= burn_drive_get_adr(&(skin->drives[skin->driveno]), adr);
       if(ret<=0)
         adr[0]= 0;
#else
       strcpy(adr,skin->drives[skin->driveno].location);
#endif

       printf("cdrskin: active drive number : %d  '%s'\n",
              skin->driveno,adr);
     }
   }
 }
 if(grab_and_wait_value>0) {
   Cdrskin_grab_drive(skin,0);
   for(k= 0; k<grab_and_wait_value; k++) {
     fprintf(stderr,
        "\rcdrskin: holding drive grabbed since %d seconds                 ",
        k);
     usleep(1000000);
   }
   fprintf(stderr,
        "\rcdrskin: held drive grabbed for %d seconds                      \n",
        k);
   Cdrskin_release_drive(skin,0);
 }
     
 if(skin->track_counter>0) {
   skin->do_burn= 1;

#ifndef Cdrskin_extra_leaN
   ret= Cdrskin_attach_fifo(skin,0);
   if(ret<=0)
     return(ret);
#endif /* ! Cdrskin_extra_leaN */

 }
 return(1);
}


/** Initialize libburn, create a CdrskiN program run control object,
    set eventual device whitelist, and obtain the list of available drives.
    @param o Returns the CdrskiN object created
    @param lib_initialized Returns wether libburn was initialized here
    @param exit_value Returns after error the proposal for an exit value
    @param flag Unused yet
    @return <=0 error, 1 success
*/
int Cdrskin_create(struct CdrskiN **o, struct CdrpreskiN **preskin,
                   int *exit_value, int flag)
{
 int ret;
 struct CdrskiN *skin;

 *o= NULL;
 *exit_value= 0;

#ifndef Cdrskin_libburn_no_burn_preset_device_opeN
 burn_preset_device_open((*preskin)->drive_exclusive,
                         (*preskin)->drive_blocking,
                         (*preskin)->abort_on_busy_drive);
#endif

 if(strlen((*preskin)->device_adr)>0) {       /* disable scan for all others */
   printf("cdrskin: NOTE : greying out all drives besides given dev='%s'\n",
          (*preskin)->device_adr);
   burn_drive_add_whitelist((*preskin)->device_adr);
 }

 ret= Cdrskin_new(&skin,*preskin,1);
 if(ret<=0) {
   fprintf(stderr,"cdrskin: FATAL : creation of control object failed\n");
   {*exit_value= 2; goto ex;}
 }
 *preskin= NULL; /* the preskin object now is under management of skin */
 *o= skin;
 if(skin->preskin->abort_handler==1 || skin->preskin->abort_handler==3 || 
    skin->preskin->abort_handler==4)
   Cleanup_set_handlers(skin,(Cleanup_app_handler_T) Cdrskin_abort_handler,4);
 else if(skin->preskin->abort_handler==2)
   Cleanup_set_handlers(skin,(Cleanup_app_handler_T) Cdrskin_abort_handler,2|8);

 printf("cdrskin: scanning for devices ...\n");
 fflush(stdout);

 /* In cdrskin there is not much sense in queueing library messages.
    It is done here only for debugging */
 Cdrpreskin_queue_msgs(skin->preskin,1);

 while (!burn_drive_scan(&(skin->drives), &(skin->n_drives))) {
   usleep(20000);
   /* >>> ??? set a timeout ? */
 }

 /* This prints the eventual queued messages */
 Cdrpreskin_queue_msgs(skin->preskin,0);

 printf("cdrskin: ... scanning for devices done\n");
 fflush(stdout);
ex:;
 return((*exit_value)==0);
}


/** Perform the activities which were ordered by setup
    @param skin Knows what to do
    @param exit_value Returns the proposal for an exit value
    @param flag Unused yet
    @return <=0 error, 1 success
*/
int Cdrskin_run(struct CdrskiN *skin, int *exit_value, int flag)
{
 int ret;

 *exit_value= 0;
 if(skin->do_devices) {
   if(skin->n_drives<=0 && skin->preskin->scan_demands_drive)
     {*exit_value= 4; goto no_drive;}
   ret= Cdrskin_scanbus(skin,1);
   if(ret<=0) {
     fprintf(stderr,"cdrskin: FATAL : --devices failed.\n");
     {*exit_value= 4; goto ex;}
   }
 }
 if(skin->do_scanbus) {
   if(skin->n_drives<=0 && skin->preskin->scan_demands_drive)
     {*exit_value= 5; goto no_drive;}
   ret= Cdrskin_scanbus(skin,0);
   if(ret<=0)
     fprintf(stderr,"cdrskin: FATAL : -scanbus failed.\n");
   {*exit_value= 5*(ret<=0); goto ex;}
 }
 if(skin->do_checkdrive) {
   ret= Cdrskin_checkdrive(skin,0);
   {*exit_value= 6*(ret<=0); goto ex;}
 }
 if(skin->do_msinfo) {
   if(skin->n_drives<=0)
     {*exit_value= 12; goto no_drive;}
   ret= Cdrskin_msinfo(skin,0);
   if(ret<=0)
     {*exit_value= 12; goto ex;}
 }
 if(skin->do_atip) {
   if(skin->n_drives<=0)
     {*exit_value= 7; goto no_drive;}
   ret= Cdrskin_atip(skin,(skin->do_atip>1));
   if(ret<=0)
     {*exit_value= 7; goto ex;}
 }
 if(skin->do_blank) {
   if(skin->n_drives<=0)
     {*exit_value= 8; goto no_drive;}
   ret= Cdrskin_blank(skin,0);
   if(ret<=0)
     {*exit_value= 8; goto ex;}
 }
 if(skin->do_burn) {
   if(skin->n_drives<=0)
     {*exit_value= 10; goto no_drive;}
   ret= Cdrskin_burn(skin,0);
   if(ret<=0)
     {*exit_value= 10; goto ex;}
 }
ex:;
 return((*exit_value)==0);
no_drive:;
 fprintf(stderr,"cdrskin: FATAL : This run would need an accessible drive\n");
 goto ex;
}


int main(int argc, char **argv)
{
 int ret,exit_value= 0,lib_initialized= 0,i,result_fd= -1;
 struct CdrpreskiN *preskin= NULL;
 struct CdrskiN *skin= NULL;
 char *lean_id= "";
#ifdef Cdrskin_extra_leaN
 lean_id= ".lean";
#endif

 /* For -msinfo: Redirect normal stdout to stderr */
 for(i=1; i<argc; i++)
   if(strcmp(argv[i],"-msinfo")==0)
 break;
 if(i<argc) {
   result_fd= dup(1);
   close(1);
   dup2(2,1);
 }
 
 printf("cdrskin %s%s : limited cdrecord compatibility wrapper for libburn\n",
        Cdrskin_prog_versioN,lean_id);
 fflush(stdout);

 ret= Cdrpreskin_new(&preskin,0);
 if(ret<=0) {
   fprintf(stderr,"cdrskin: FATAL : Creation of control object failed\n");
   {exit_value= 2; goto ex;}
 }

 /* <<< A60925: i would prefer to do this later, after it is clear that no
       -version or -help cause idle end. But address conversion and its debug
       messaging need libburn running */
 ret= Cdrpreskin_initialize_lib(preskin,0);
 if(ret<=0) {
   fprintf(stderr,"cdrskin: FATAL : Initializiation of burn library failed\n");
   {exit_value= 2; goto ex;}
 }
 lib_initialized= 1;

 ret= Cdrpreskin_setup(preskin,argc,argv,0);
 if(ret<=0)
   {exit_value= 11; goto ex;}
 if(ret==2)
   {exit_value= 0; goto ex;}
 ret= Cdrskin_create(&skin,&preskin,&exit_value,0);
 if(ret<=0)
   {exit_value= 2; goto ex;}
 if(skin->n_drives<=0) {
   fprintf(stderr,"cdrskin: NOTE : No usable drive detected.\n");
   if(getuid()!=0) {
     fprintf(stderr,
      "cdrskin: HINT : Run this program as superuser with option --devices\n");
     fprintf(stderr,
      "cdrskin: HINT : Allow rw-access to the dev='...' file of the burner.\n");
     fprintf(stderr,
      "cdrskin: HINT : Busy drives are invisible. (Busy = open O_EXCL)\n");
   }
 }
 Cdrskin_set_msinfo_fd(skin,result_fd,0);

 ret= Cdrskin_setup(skin,argc,argv,0);
 if(ret<=0)
   {exit_value= 3; goto ex;}
 if(skin->verbosity>=Cdrskin_verbose_cmD)
   printf("cdrskin: called as :  %s\n",argv[0]);

 if(skin->verbosity>=Cdrskin_verbose_debuG) {
#ifdef Cdrskin_oldfashioned_api_usE
   ClN(fprintf(stderr,"cdrskin_debug: Compiled with option -oldfashioned\n"));
#endif
#ifdef Cdrskin_new_api_tesT
   ClN(fprintf(stderr,"cdrskin_debug: Compiled with option -experimental\n"));
#endif
 }

 Cdrskin_run(skin,&exit_value,0);

ex:;
 if(skin!=NULL) {
   Cleanup_set_handlers(NULL,NULL,1);
   Cdrskin_eject(skin,0);
   Cdrskin_destroy(&skin,0);
 }
 Cdrpreskin_destroy(&preskin,0);
 if(lib_initialized)
   burn_finish();
 exit(exit_value);
}
