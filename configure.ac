AC_INIT([libburn], [0.6.9], [http://libburnia-project.org])
AC_PREREQ([2.50])
dnl AC_CONFIG_HEADER([config.h])	

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([subdir-objects])

dnl Notes by ts A71207 - A90714 :
dnl
dnl Regrettably the meaning of the various version types was misunderstood
dnl before version 0.4.1.
dnl
dnl In the past MAJOR.MINOR.MICRO versions led to the following SONAME numbers:
dnl    0.2.2 = 2 , 0.2.3 = 3 , 0.2.6 = 6
dnl    0.3.0 = 0 , 0.3.2 = 2 , 0.3.4 = 4 . 0.3.6 = 6 , 0.3.8 = 4
dnl    0.4.0 = 0 (also released as SONAME 4)
dnl
dnl Meanwhile the following schemes are maintained in parallel:
dnl
dnl BURN_MAJOR_VERSION , BURN_MINOR_VERSION , BURN_MICRO_VERSION 
dnl    are three small non-negative integers which describe the evolution
dnl    steps of the library.
dnl    Older applications are able to use younger libraries over
dnl    quite a long range of such steps. Some day, nevertheless,
dnl    compatibility might get terminated, after due notice.
dnl
dnl SONAME (libburn.so.4)
dnl    is a small positive integer which marks a family of compatible
dnl    evolution steps. Libraries with a particular SONAME allow a binary
dnl    with the same SONAME to start up. Any further compatibility check is to
dnl    be done by own runtime means. Especially  *_version() calls in the API
dnl    which return BURN_MAJOR_VERSION, BURN_MINOR_VERSION, BURN_MICRO_VERSION.
dnl    See below.
dnl
dnl CURRENT, AGE, REVISION
dnl    are three integers used by libtool. CURRENT is positive, the others
dnl    non-negative. The use at runtime is not known yet. But libtool computes
dnl    at build time  SONAME = CURRENT - AGE.
dnl    So this is a superspace of the SONAME version space. To avoid
dnl    ill SONAME, the value of  CURRENT must be larger than AGE.
dnl    See also http://www.gnu.org/software/libtool/manual.html#Interfaces
dnl
dnl On Linux the name of the dynamic library will be
dnl    libburn.so.$SONAME.$AGE.$REV
dnl In the terminology of this file:
dnl    CURRENT = LT_CURRENT
dnl    AGE     = LT_AGE
dnl    REVISION= LT_REVISION
dnl
dnl Beginning with libburn-0.4.1 a rectified counting was introduced as
dnl    CURRENT=10, REVISION=1, AGE=6
dnl This rectification declared that version to be binary compatible up
dnl from libburn-0.3.4.
dnl Real compatibility was given since libburn-0.3.2.
dnl Beware of libburn-0.2.6 which had SONAME=6 and is not binary compatible.
dnl Applications for libburn-0.2 to libburn-0.3.1 need recompilation but no
dnl source code changes.
dnl
dnl Neatly versioned stable releases meanwhile:
dnl    0.4.2 = libburn.so.4.7.0
dnl    0.4.4 = libburn.so.4.9.0
dnl    0.4.6 = libburn.so.4.11.0
dnl    0.4.8 = libburn.so.4.13.0
dnl    0.5.0 = libburn.so.4.15.0
dnl    0.5.2 = libburn.so.4.17.0
dnl    0.5.4 = libburn.so.4.19.0
dnl    0.5.6 = libburn.so.4.21.0
dnl    0.5.8 = libburn.so.4.23.0
dnl    0.6.0 = libburn.so.4.25.0
dnl    0.6.2 = libburn.so.4.27.0
dnl    0.6.4 = libburn.so.4.29.0
dnl    0.6.6 = libburn.so.4.31.0
dnl    0.6.8 = libburn.so.4.33.0
dnl
dnl So LT_CURRENT, LT_REVISION and LT_AGE get set directly here.
dnl SONAME of the emerging library is  LT_CURRENT - LT_AGE.
dnl The linker will do no finer checks. Especially no age range check for
dnl the application binary. If SONAME matches, then the couple starts.
dnl 
dnl Therefore at run time info is provided by libburn function burn_version().
dnl It returns the major, minor and micro revision of the library.
dnl Before using any API feature, a program should check for age.
dnl
dnl The variables BURN_*_VERSION are mere copies for informing libtool.
dnl The true values which get issued and should be compared are macros
dnl defined in libburn/libburn.h .
dnl
dnl Normally one can allow a program to run with a library which passed the
dnl linker SONAME test and which is not older than the library it was
dnl developed for. Library2 is younger than library1 if:
dnl   major2>major1 || (major2==major1 &&
dnl                     (minor2>minor1 || (minor2==minor1 && micro2 > micro1)))
dnl
dnl If BURN_*_VERSION changes, be sure to change AC_INIT above to match.
dnl
dnl As said: Only copies. Original in libburn/libburn.h : burn_header_version_*
BURN_MAJOR_VERSION=0
BURN_MINOR_VERSION=6
BURN_MICRO_VERSION=9
BURN_VERSION=$BURN_MAJOR_VERSION.$BURN_MINOR_VERSION.$BURN_MICRO_VERSION

AC_SUBST(BURN_MAJOR_VERSION)
AC_SUBST(BURN_MINOR_VERSION)
AC_SUBST(BURN_MICRO_VERSION)
AC_SUBST(BURN_VERSION)

dnl Libtool versioning
LT_RELEASE=$BURN_MAJOR_VERSION.$BURN_MINOR_VERSION.$BURN_MICRO_VERSION
dnl
dnl ts A90714
dnl ### This is the release version libburn-0.6.8 = libburn.so.4.33.0
dnl This is the development version after above release version
dnl LT_CURRENT++, LT_AGE++ has not yet happened.
dnl ### LT_CURRENT++, LT_AGE++ has happened meanwhile.
dnl
dnl SONAME = 37 - 33 = 4 . Linux library name = libburn.so.4.33.0
LT_CURRENT=37
LT_AGE=33
LT_REVISION=0
LT_CURRENT_MINUS_AGE=`expr $LT_CURRENT - $LT_AGE`

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)
AC_SUBST(LT_CURRENT_MINUS_AGE)

dnl ts A71207: This is done only not to break any old components
BURN_INTERFACE_AGE=$LT_REVISION
BURN_BINARY_AGE=`expr $LT_AGE + $BURN_INTERFACE_AGE`
AC_SUBST(BURN_INTERFACE_AGE)
AC_SUBST(BURN_BINARY_AGE)

AC_PREFIX_DEFAULT([/usr/local])
test "$prefix" = "NONE" && prefix=$ac_default_prefix

AM_MAINTAINER_MODE

AM_PROG_CC_C_O
AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN

dnl Large file support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
AC_CHECK_FUNC([fseeko])
if test ! $ac_cv_func_fseeko; then
   AC_ERROR([Libburn requires largefile support.])
fi

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
LIBTOOL="$LIBTOOL --silent"

AC_PROG_INSTALL

AC_CHECK_HEADERS()

THREAD_LIBS=-lpthread
AC_SUBST(THREAD_LIBS)

TARGET_SHIZZLE
AC_SUBST(ARCH)
AC_SUBST(LIBBURNIA_PKGCONFDIR)
AC_SUBST(LIBBURN_ARCH_LIBS)

dnl ts A90303
dnl Check the preconditions for using statvfs() in sg-dummy
dnl (sg-linux and sg-freebsd use statvfs() unconditionally)
STATVFS_DEF=-DLibburn_os_has_statvfS
AC_CHECK_HEADER(sys/statvfs.h, X=, STATVFS_DEF=)
AC_CHECK_FUNC([statvfs], X=, STATVFS_DEF=)
dnl If this would be done more specifically in Makefile.am
dnl via libburn_libburn_la_CFLAGS then undesired .o file names would emerge
CFLAGS="$CFLAGS $STATVFS_DEF"

dnl Add compiler-specific flags

dnl See if the user wants aggressive optimizations of the code
AC_ARG_ENABLE(debug,
[  --enable-debug          Disable aggressive optimizations [default=yes]],
              , enable_debug=yes)
if test x$enable_debug != xyes; then
    if test x$GCC = xyes; then
        CFLAGS="$CFLAGS -O3"
        CFLAGS="$CFLAGS -fexpensive-optimizations"
    fi
    CFLAGS="$CFLAGS -DNDEBUG"
else
    if test x$GCC = xyes; then
        CFLAGS="$CFLAGS -g -pedantic -Wall"
    fi
    CFLAGS="$CFLAGS -DDEBUG"
fi


dnl Determine target directory for libburn-*.pc
dnl Important: Must be performed _after_ TARGET_SHIZZLE
dnl
LIBBURNIA_SET_PKGCONFIG


AC_CONFIG_FILES([
	Makefile
	doc/doxygen.conf
	version.h
	libburn-1.pc
	])
AC_OUTPUT
