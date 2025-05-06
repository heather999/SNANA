
AC_INIT([snana], [1.0], [heather999kelly@gmail.com])
AC_PREREQ([2.68])
AC_CONFIG_SRCDIR([src/sntools.c])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])

AC_CONFIG_MACRO_DIR([m4])

dnl setting up gfortran and gcc
AC_PROG_CC([gcc])
AC_PROG_FC([gfortran])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

dnl checking for GSL and CFITSIO
AC_CHECK_LIB([m],[cos])
AC_CHECK_LIB([gslcblas],[cblas_dgemm])
AC_CHECK_LIB([gsl],[gsl_blas_dgemm])
dnl AC_CHECK_LIB([cfitsio])
AC_CHECK_LIB([cfitsio], [ffopen], [CFITSIOLIB="$CFITSIOLIB -lcfitsio"], [], [$CFITSIOLIB $LIBS])
dnl AC_CHECK_LIB([cfitsio])


dnl set up C++ 17 flags
m4_include([m4/m4_ax_cxx_compile_stdcxx.m4])
AX_CXX_COMPILE_STDCXX(17)


dnl need to include config.sug and config.guess with the distribution
AC_CANONICAL_HOST


AC_PATH_PROG([GIT], [git])

if test -n "$GIT"; then
  GIT_VERSION=`$GIT --version | sed 's/git version //'`
  AC_MSG_NOTICE([Found Git version $GIT_VERSION])
else
  AC_MSG_ERROR([Git not found. Please install Git.])
fi

macos=no

case "${host_os}" in
darwin*)
macos=yes
;;
esac


AM_CONDITIONAL(OSX, test $macos="yes")

have64=no
havei686=no

case "${host_cpu}" in
     i?86) 
	AC_MSG_RESULT([ok (${host_cpu})])
	havei686=yes
        ;;
     x86_64) 
        AC_MSG_RESULT([ok (${host_cpu})])
        have64=yes 
        ;;
     powerpc64)
     # this only referrs to 64-bit Big Endian
        AC_MSG_RESULT([ok (${host_cpu})])
	have64=yes
        ;;
     powerpc64le)
     # this only referrs to 64-bit Little Endian
        AC_MSG_RESULT([ok (${host_cpu})])
	have64=yes
        ;;
     powerpc)
        # On Linux this means only a 32-bit capable CPU.
        AC_MSG_RESULT([ok (${host_cpu})])
        ;;
     aarch64*)
       AC_MSG_RESULT([ok (${host_cpu})])
       have64=yes
       ;;
     mips)
        AC_MSG_RESULT([ok (${host_cpu})])
        ;;
     mipsel)
        AC_MSG_RESULT([ok (${host_cpu})])
        ;;
     mipsisa32r2)
        AC_MSG_RESULT([ok (${host_cpu})])
        ;;
     mips64*)
        AC_MSG_RESULT([ok (${host_cpu})])
	have64=yes
        ;;
     mipsisa64*)
        AC_MSG_RESULT([ok (${host_cpu})])
	have64=yes
        ;;
     *) 
	AC_MSG_RESULT([no (${host_cpu})])
	AC_MSG_ERROR([Unsupported host architecture. Sorry])
	;;
esac


AM_CONDITIONAL(BIT64, test $have64=yes)
AM_CONDITIONAL(I686, test $havei686=yes)
