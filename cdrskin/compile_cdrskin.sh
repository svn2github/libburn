#!/bin/sh

# compile_cdrskin.sh  
# Copyright 2005 - 2006 Thomas Schmitt, scdbackup@gmx.net, GPL
# to be executed within  ./libburn-*  resp ./cdrskin-*

debug_opts=
def_opts=
largefile_opts="-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1"
libvers="-DCdrskin_libburn_0_3_1"
cleanup_src_or_obj="libburn/cleanup.o"
libdax_msgs_o="libburn/libdax_msgs.o"
libdax_audioxtr_o="libburn/libdax_audioxtr.o"
do_strip=0
static_opts=
warn_opts="-Wall"
fifo_source="cdrskin/cdrfifo.c"
compile_cdrskin=1
compile_cdrfifo=0
compile_dewav=0

for i in "$@"
do
  if test "$i" = "-compile_cdrfifo"
  then
    compile_cdrfifo=1
  elif test "$i" = "-compile_dewav"
  then
    compile_dewav=1
  elif test "$i" = "-cvs_A60220"
  then
    libvers="-DCdrskin_libburn_cvs_A60220_tS"
    libdax_audioxtr_o=
    libdax_msgs_o="libburn/message.o"
    cleanup_src_or_obj="-DCleanup_has_no_libburn_os_H cdrskin/cleanup.c"
  elif test "$i" = "-libburn_0_3_0"
  then
    libvers="-DCdrskin_libburn_0_3_0"
    libdax_audioxtr_o="libburn/libdax_audioxtr.o"
    libdax_msgs_o="libburn/libdax_msgs.o"
    cleanup_src_or_obj="libburn/cleanup.o"
  elif test "$i" = "-libburn_svn"
  then
    libvers="-DCdrskin_libburn_0_3_1"
    libdax_audioxtr_o="libburn/libdax_audioxtr.o"
    libdax_msgs_o="libburn/libdax_msgs.o"
    cleanup_src_or_obj="libburn/cleanup.o"
  elif test "$i" = "-newapi" -o "$i" = "-experimental"
  then
    def_opts="$def_opts -DCdrskin_new_api_tesT"
  elif test "$i" = "-oldfashioned"
  then
    def_opts="$def_opts -DCdrskin_oldfashioned_api_usE"
    cleanup_src_or_obj="-DCleanup_has_no_libburn_os_H cdrskin/cleanup.c"
  elif test "$i" = "-no_largefile"
  then
    largefile_opts=	
  elif test "$i" = "-do_not_compile_cdrskin"
  then
    compile_cdrskin=0
  elif test "$i" = "-do_diet"
  then
    fifo_source=
    def_opts="$def_opts -DCdrskin_extra_leaN"
    warn_opts=
  elif test "$i" = "-do_strip"
  then
    do_strip=1
  elif test "$i" = "-g"
  then
    debug_opts="$debug_opts -g"
  elif test "$i" = "-O2"
  then
    debug_opts="$debug_opts -O2"
  elif test "$i" = "-help" -o "$i" = "--help" -o "$i" = "-h"
  then
    echo "cdrskin/compile_cdrskin.sh : to be executed within top level directory"
    echo "Options:"
    echo "  -compile_cdrfifo  compile program cdrskin/cdrfifo."
    echo "  -compile_dewav    compile program test/dewav without libburn."
    echo "  -cvs_A60220       set macro to match libburn-CVS of 20 Feb 2006."
    echo "  -libburn_0_3_0    set macro to match libburn-0.3.0."
    echo "  -libburn_svn      set macro to match current libburn-SVN."
    echo "  -no_largefile     do not use 64 bit off_t (must match libburn)."
    echo "  -do_not_compile_cdrskin  omit compilation of cdrskin/cdrskin."
    echo "  -experimental     use newly introduced libburn features."
    echo "  -oldfashioned     use pre-0.2.2 libburn features only."
    echo "  -do_diet          produce capability reduced lean version."
    echo "  -do_strip         apply program strip to compiled programs."
    echo "  -g                compile with cc option -g."
    echo "  -O2               compile with cc option -O2."
    echo "  -static           compile with cc option -static."
    exit 0
  elif test "$i" = "-static"
  then
    static_opts="-static"
  fi
done


timestamp="$(date -u '+%Y.%m.%d.%H%M%S')"
echo "Version timestamp :  $(sed -e 's/#define Cdrskin_timestamP "//' -e 's/"$//' cdrskin/cdrskin_timestamp.h)"
echo "Build timestamp   :  $timestamp"

if test "$compile_cdrskin"
then
  echo "compiling program cdrskin/cdrskin.c $static_opts $debug_opts $libvers $def_opts $cleanup_src_or_obj"
  cc -I. \
    $warn_opts \
    $static_opts \
    $debug_opts \
    $libvers \
    $largefile_opts \
    $def_opts \
    \
    -DCdrskin_build_timestamP='"'"$timestamp"'"' \
    \
    -o cdrskin/cdrskin \
    \
    cdrskin/cdrskin.c \
    $fifo_source \
    \
    $cleanup_src_or_obj \
    \
    libburn/async.o \
    libburn/debug.o \
    libburn/drive.o \
    libburn/file.o \
    libburn/init.o \
    libburn/options.o \
    libburn/source.o \
    libburn/structure.o \
    \
    libburn/sg.o \
    libburn/write.o \
    $libdax_audioxtr_o \
    $libdax_msgs_o \
    \
    libburn/mmc.o \
    libburn/sbc.o \
    libburn/spc.o \
    libburn/util.o \
    \
    libburn/sector.o \
    libburn/toc.o \
    \
    libburn/crc.o \
    libburn/lec.o \
    \
    -lpthread

    ret=$?
    if test "$ret" = 0
    then
      dummy=dummy
    else
      echo >&2
      echo "+++ FATAL : Compilation of cdrskin failed" >&2
      echo >&2
      exit 1
    fi
fi

if test "$compile_cdrfifo" = 1
then
  echo "compiling program cdrskin/cdrfifo.c $static_opts $debug_opts"
  cc $static_opts $debug_opts \
     -DCdrfifo_standalonE \
     -o cdrskin/cdrfifo \
     cdrskin/cdrfifo.c

    ret=$?
    if test "$ret" = 0
    then
      dummy=dummy
    else
      echo >&2
      echo "+++ FATAL : Compilation of cdrfifo failed" >&2
      echo >&2
      exit 2
    fi
fi

if test "$compile_dewav" = 1
then
  echo "compiling program test/dewav.c -DDewav_without_libburN $static_opts $debug_opts"
  cc $static_opts $debug_opts \
     -DDewav_without_libburN \
     -o test/dewav \
     test/dewav.c \
     libburn/libdax_audioxtr.o \
     libburn/libdax_msgs.o \
     \
    -lpthread

    ret=$?
    if test "$ret" = 0
    then
      dummy=dummy
    else
      echo >&2
      echo "+++ FATAL : Compilation of test/dewav failed" >&2
      echo >&2
      exit 2
    fi
fi     

if test "$do_strip" = 1
then
  echo "stripping result cdrskin/cdrskin"
  strip cdrskin/cdrskin
  if test "$compile_cdrfifo" = 1
  then
    echo "stripping result cdrskin/cdrfifo"
    strip cdrskin/cdrfifo
  fi
fi

echo 'done.'
