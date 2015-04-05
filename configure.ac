#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(flint, 0.1, flint@fb.com)
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([foreign nostdinc])

AC_CONFIG_MACRO_DIR([m4])

AC_PROG_INSTALL
AM_PROG_LIBTOOL

AC_LANG([C++])

# Checks for programs.
AC_PROG_CXX
AC_CXX_COMPILE_STDCXX_0X

AC_CHECK_PROGS([DC],[gdc dmd ldc])
if test -z "$DC" ; then
    AC_MSG_ERROR([\
*** No D Compiler found.  Please install one of gdc, ldc, or dmd])
fi

# Be sure to add any -std option to CXXFLAGS before we invoke any
# AC_COMPILE_IFELSE() or similar macros. Any such macros that are invoked
# before we update CXXFLAGS will not be run with the same options that we use
# during the real build.
STD=""
if test "x$ac_cv_cxx_compile_cxx0x_cxx" = xyes; then
   STD="-std=c++0x"
fi
if test "x$ac_cv_cxx_compile_cxx0x_gxx" = xyes; then
   STD="-std=gnu++0x"
fi

CXXFLAGS="$STD $CXXFLAGS"

# Checks for libraries.
AC_CHECK_LIB([folly],[getenv],[],[AC_MSG_ERROR(
             [Please install the folly library])])
AC_CHECK_LIB([gflags],[getenv],[],[AC_MSG_ERROR(
             [Please install google-gflags library])])

# Checks for header files.
AC_HEADER_STDC
# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_C_VOLATILE

# check for boost libs
AX_BOOST_BASE([1.20.0], [], [AC_MSG_ERROR(
              [Please install boost >= 1.20.0 (filesystem)])])
AX_BOOST_FILESYSTEM

# Output
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
