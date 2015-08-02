
/*
  ( cd cdrskin ; cc -g -Wall -o unite_html_b_line unite_html_b_line.c )
*/
/*
   Specialized converter for the output of man -H,
   which unites lines where the line end is between <b> and </b>.

   Copyright 2015 Thomas Schmitt, <scdbackup@gmx.net>

   Provided under GPL version 2 or later.
*/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>


int unite_lines(char *buffer, int *b_open, int *b_state, int flag)
{
 char *cpt;
 int last_was_nl= 0;

 for(cpt= buffer; *cpt != 0; cpt++) {
   if(*b_open) {
     if(*b_state == 0 && *cpt == '<') {
       *b_state= 1;
     } else if(*b_state == 1) {
       if(*cpt == '/')
         *b_state= 2;
       else
         *b_state= 0;
     } else if(*b_state == 2) {
       if(*cpt == 'b' || *cpt == 'B')
         *b_state= 3;
       else
         *b_state= 0;
     } else if(*b_state == 3) {
       if(*cpt == '>')
         *b_open= 0;
       *b_state= 0;
     }
   } else {
     if(*b_state == 0 && *cpt == '<') {
       *b_state= 1;
     } else if(*b_state == 1) {
       if(*cpt == 'b' || *cpt == 'B')
         *b_state= 2;
       else
         *b_state= 0;
     } else if(*b_state == 2) {
       if(*cpt == '>')
         *b_open= 1;
       *b_state= 0;
     }
   }
   last_was_nl= (*cpt == '\n');
 }
 if(*b_open && last_was_nl) {
   /* replace newline */
   *(cpt - 1)= ' ';
 }
 return(1);
}


int main(int argc, char **argv)
{
 FILE *fpin, *fpout;
 char buffer[4096], *respt;
 int ret, b_open= 0, b_state= 0;

 if(argc != 3) {
   fprintf(stderr, "usage: %s input_path output_path\n", argv[0]);
   return(1); 
 }
 if(strcmp(argv[1], "-") == 0) {
   fpin= stdin;
 } else {
   fpin= fopen(argv[1], "rb");
   if(fpin == 0) {
     fprintf(stderr, "Error with input file '%s' : %s\n",
                      argv[1], strerror(errno));
     return(2);
   }
 }
 if(strcmp(argv[2], "-") == 0) {
   fpout= stdout;
 } else {
   fpout= fopen(argv[2], "wb");
   if(fpout == 0) {
     fprintf(stderr, "Error with output file '%s' : %s\n",
                      argv[2], strerror(errno));
     return(3);
   }
 }
 while(1) {
   respt= fgets(buffer, sizeof(buffer), fpin);
   if(respt == NULL)
 break;
   ret= unite_lines(buffer, &b_open, &b_state, 0);
   if(ret <= 0)
 break;
   ret= fputs(buffer, fpout);
   if(ret < 0) {
     fprintf(stderr, "Error writing to output file '%s' : %s\n",
                      argv[2], strerror(errno));
     return(4);
   }
 }
 if(fpin != stdin)
   fclose(fpin);
 if(fpout != stdout)
   fclose(stdout);
 return(0);
}

