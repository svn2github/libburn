AC_INIT([libburn], [0.4.1], [http://libburnia-project.org])
AC_PREREQ([2.50])
dnl AC_CONFIG_HEADER([config.h])	

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([subdir-objects])

dnl Note by ts A71207:
dnl
dnl BURN_MICRO_VERSION is _not_ the third component Z of libburn-X.Y.Z
dnl but an eternal counter which leads to CURRENT. This misunderstanding
dnl caused a messed up sequence of CURRENTs.
dnl
dnl CURRENT and AGE describe the binary compatibility interval of a
dnl dynamic library.
dnl See also http://www.gnu.org/software/libtool/manual.html#Interfaces
dnl
dnl The name of the library will be libburn.so.$CURRENT-$AGE.$AGE.$REV
dnl In the terminology of this file:
dnl   CURRENT = LT_CURRENT
dnl   REV     = LT_REVISION
dnle  AGE     = LT_AGE
dnl
dnl In the past the following CURRENTs of libburn have been released.
dnl All with AGE=0.
dnl  0.2.2 = 2 , 0.2.3 = 3 , 0.2.6 = 6
dnl  0.3.0 = 0 , 0.3.2 = 2 , 0.3.4 = 4 . 0.3.6 = 6 , 0.3.8 = 4
dnl  0.4.0 = 0
dnl 
dnl Beginning with libburn-0.4.1 a rectified counting was introduced as
dnl   CURRENT=10, REV=1, AGE=6
dnl This rectification declared the current version
dnl to be binary compatible down to libburn-0.3.4.
dnl Real backward compatibility is given down to libburn-0.3.2.
dnl Beware of libburn-0.2.6 which had CURRENT=6.
dnl
dnl CURRENT=10, REV=0, AGE=6 would be appropriate for libburn-0.4.0
dnl but was not issued, regreattably.
dnl
dnl If libburn-0.4.0 gets patched, its CURRENT will become 10, AGE 6, and
dnl REV will be set to one higher than the highest REV of CURRENT 10.
dnl If libburn-0.4.1 is still on CURRENT 10 then its REV gets set one higher
dnl than that.
dnl As soon as 0.4.1 becomes upwardly incompatible, it shall become 11,0,7.
dnl From then on it counts REV independently of libburn-0.4.0.
dnl Whether 0.4.1 alters independent REV at all, is not decided yet.
dnl It may well stay 0 until a release version joins CURRENT 11.
dnl
dnl Under the preconditions
dnl   CURRENT= $BURN_MICRO_VERSION - $BURN_INTERFACE_AGE
dnl   REV=     $BURN_INTERFACE_AGE
dnl   AGE=     $BURN_BINARY_AGE
dnl the following old instructions were reasonable. Their drawback
dnl is that they cause large jumps in CURRENT, when BURN_INTERFACE_AGE gets
dnl reset to 0 from a high value. So this is outdated now:
dnl ---------------------------------------------------------------------------
dnl .Making releases:
dnl .  BURN_MICRO_VERSION += 1;
dnl .  BURN_INTERFACE_AGE += 1;
dnl .  BURN_BINARY_AGE += 1;
dnl .if any functions have been added, set BURN_INTERFACE_AGE to 0.
dnl .if backwards compatibility has been broken,
dnl .set BURN_BINARY_AGE and BURN_INTERFACE_AGE to 0.
dnl .
dnl .if MAJOR or MINOR version changes, be sure to change AC_INIT above to match
dnl ---------------------------------------------------------------------------
dnl
dnl Instead, LT_CURRENT, LT_REVISION and LT_AGE get set directly now.
dnl If BURN_VERSION changes, be sure to change AC_INIT above to match.
dnl
BURN_MAJOR_VERSION=0
BURN_MINOR_VERSION=4
BURN_DEV_VERSION=1
BURN_VERSION=$BURN_MAJOR_VERSION.$BURN_MINOR_VERSION.$BURN_DEV_VERSION

AC_SUBST(BURN_MAJOR_VERSION)
AC_SUBST(BURN_MINOR_VERSION)
AC_SUBST(BURN_DEV_VERSION)
AC_SUBST(BURN_VERSION)

dnl Libtool versioning
# LT_RELEASE=$BURN_MAJOR_VERSION.$BURN_MINOR_VERSION
LT_RELEASE=$BURN_MAJOR_VERSION.$BURN_MINOR_VERSION.$BURN_DEV_VERSION
LT_CURRENT=10
LT_REVISION=1
LT_AGE=6
LT_CURRENT_MINUS_AGE=`expr $LT_CURRENT - $LT_AGE`

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)
AC_SUBST(LT_CURRENT_MINUS_AGE)

# ts A71207: This is done only not to break any old components
BURN_INTERFACE_AGE=$LT_REVISION
BURN_BINARY_AGE=`expr $LT_AGE + $BURN_INTERFACE_AGE`
BURN_MICRO_VERSION=`expr $LT_CURRENT + $BURN_INTERFACE_AGE`
AC_SUBST(BURN_MICRO_VERSION)
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
AC_SUBST(LIBBURN_ARCH_LIBS)

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

AC_CONFIG_FILES([
	Makefile
	doc/doxygen.conf
	version.h
	libburn-5.pc
	])
AC_OUTPUT
