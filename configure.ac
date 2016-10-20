# configure.ac - Configure script for miredo
# based on earlier configure.ac from tcpreen by the same author

# Process this file with GNU Autoconf to produce a configure script

# ***********************************************************************
# *  Copyright © 2004-2013 Rémi Denis-Courmont.                         *
# *  This program is free software; you can redistribute and/or modify  *
# *  it under the terms of the GNU General Public License as published  *
# *  by the Free Software Foundation; version 2 of the license, or (at  *
# *  your option) any later version.                                    *
# *                                                                     *
# *  This program is distributed in the hope that it will be useful,    *
# *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
# *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
# *  See the GNU General Public License for more details.               *
# *                                                                     *
# *  You should have received a copy of the GNU General Public License  *
# *  along with this program; if not, you can get it from:              *
# *  http://www.gnu.org/copyleft/gpl.html                               *
# ***********************************************************************

AC_COPYRIGHT([Copyright (C) 2004-2014 Remi Denis-Courmont])
AC_INIT(miredo, 1.3.0, miredo-devel_no_bulk_mail@remlab.net)
AC_PREREQ(2.59c)
INVOCATION="$0 $ac_configure_args"

AS_MESSAGE(checking system...)
AC_CONFIG_SRCDIR(configure.ac)
AC_CONFIG_AUX_DIR(admin)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_LIBOBJ_DIR(compat)
AC_CONFIG_HEADERS(config.h)

AC_DEFINE_UNQUOTED(PACKAGE_CONFIGURE_INVOCATION, "$INVOCATION",
                [Define to the command line used to invoke the configure script.])
RDC_BUILD_HOSTNAME
AC_CANONICAL_HOST


# Checks for programs.
AS_MESSAGE([checking required programs...])

AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS
AC_DEFINE(_APPLE_C_SOURCE, 1, [Define to fix pthread_cancel() on Mac OS X.])

AM_PROG_AR

# Check pkg-config is available
PKG_PROG_PKG_CONFIG([0.20])

# Work-around for libtool bug (from Darren Salt)
LT_PREREQ([2.2.0])
LT_INIT
AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])


# Checks for libraries.
AS_MESSAGE([checking required libraries...])
#AM_GNU_GETTEXT_VERSION([0.19.2])
#AM_GNU_GETTEXT([external])

LIBRT=""
AC_CHECK_LIB(rt, clock_gettime, [LIBRT="-lrt"])
AC_SUBST(LIBRT)
RDC_FUNC_SOCKET
AC_SEARCH_LIBS(inet_ntop, [nsl])
AC_CHECK_LIB(resolv, res_init)

# Oooh, evil platform-rather-than-feature tests
case "${host_os}" in
	darwin*)
		AC_DEFINE(HAVE_SYSTEMCONFIGURATION_FRAMEWORK, 1,
		          [Define to 1 if Apple's SystemConfiguration framework is available.])
		CFLAGS="${CFLAGS} -framework SystemConfiguration"
		AC_DEFINE(HAVE_COREFOUNDATION_FRAMEWORK, 1,
		          [Define to 1 if Apple's CoreFoundation framework is available.])
		CFLAGS="${CFLAGS} -framework CoreFoundation"
		;;
esac

AC_MSG_CHECKING([which hook scripts to use])
case "${host_os}" in
	*linux*)
		hook_suffix=iproute
		;;
	*)
		hook_suffix=bsd
		;;
esac
AC_MSG_RESULT([${hook_suffix}])
AC_SUBST(hook_suffix)


# POSIX threads
AC_ARG_VAR(PTHREAD_CFLAGS, [C compiler flags for POSIX threads])
AS_IF([test "${PTHREAD_CFLAGS}"], [
	CFLAGS="${CFLAGS} ${PTHREAD_CFLAGS}"
])
AC_ARG_VAR(PTHREAD_LDFLAGS, [Linker flags for POSIX threads])
AC_ARG_VAR(PTHREAD_LIBS, [Same as PTHREAD_LFLAGS])
AS_IF([test "${PTHREAD_LDFAGS}${PTHREAD_LIBS}"], [
	# Neat! the packaging system tells us how to use pthread
	LIBS="${PTHREAD_LDFLAGS} ${PTHREAD_LIBS} ${LIBS}"
], [
	AC_CHECK_LIB(pthread, pthread_create)
])

AC_DEFINE_UNQUOTED(PACKAGE_BUILD, "$build",
                   [Define to the canonical build-system name])
AC_DEFINE_UNQUOTED(PACKAGE_HOST, "$host",
                   [Define to the canonical host-system name])


# Checks for header files.
AS_MESSAGE([checking header files...])
AC_HEADER_ASSERT
AC_CHECK_HEADERS([libintl.h net/if_tun.h net/tun/if_tun.h])
AC_CHECK_HEADERS([net/if_var.h],,,
[#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
])


# Checks for typedefs, structures, and compiler characteristics.
AS_MESSAGE([checking target characteristics...])
RDC_PROG_CC_WFLAGS([all extra undef pointer-arith bad-function-cast cast-align write-strings aggregate-return strict-prototypes old-style-definition missing-prototypes missing-noreturn packed nested-externs redundant-decls volatile-register-var])
#shadow fails because of ntohl()
#cast-qual fails because of const/iovec, libcap, Judy
# padded unreachable-code missing-format-attribute inline

AC_C_BIGENDIAN
RDC_STRUCT_SOCKADDR_LEN
AC_CHECK_TYPE([clockid_t],, [
  AC_DEFINE([clockid_t], [int], [Define to int if clockid_t is not supported.])],
[#include <time.h>
])


# Checks for library functions.
AS_MESSAGE([checking library functions...])
RDC_REPLACE_FUNC_GETOPT_LONG
LIBS_save="$LIBS"
LIBS="$LIBRT $LIBS"
AC_CHECK_FUNCS([devname_r kldload])
AC_REPLACE_FUNCS([clearenv strlcpy clock_gettime clock_nanosleep fdatasync])
LIBS="$LIBS_save"

# Checks for optionnal features
AS_MESSAGE([checking optional features...])

# POSIX capabilities
PKG_CHECK_MODULES([LIBCAP],[libcap],[have_libcap=yes],[have_libcap=no])
if test "x$have_libcap" = "xyes"; then
  AC_DEFINE([HAVE_LIBCAP],[1],[Define to 1 if you have libcap.])
  save_CFLAGS=$CFLAGS
  save_CPPFLAGS=$CPPFLAGS
  save_LIBS=$LIBS
  CFLAGS="$CFLAGS $LIBCAP_CFLAGS"
  CPPFLAGS="$CPPFLAGS $LIBCAP_CFLAGS"
  LIBS="$LIBS $LIBCAP_LIBS"
  AC_CHECK_FUNCS([cap_set_proc])
  AC_CHECK_HEADERS([sys/capability.h])
  CFLAGS=$save_CFLAGS
  CPPFLAGS=$save_CPPFLAGS
  LIBS=$save_LIBS
else
  AC_MSG_WARN([$LIBCAP_PKG_ERRORS])
fi

# Judy
PKG_CHECK_MODULES([LIBJUDY],[Judy],[have_judy=yes],[have_judy=no])
if test "x$have_judy" = "xyes"; then
  AC_DEFINE([HAVE_LIBJUDY], [1], [Define to 1 if you have Judy.])
  save_CFLAGS=$CFLAGS
  save_CPPFLAGS=$CPPFLAGS
  save_LIBS=$LIBS
  CFLAGS="$CFLAGS $LIBJUDY_CFLAGS"
  CPPFLAGS="$CPPFLAGS $LIBJUDY_CFLAGS"
  LIBS="$LIBS $LIBJUDY_LIBS"
  AC_CHECK_FUNCS([JudyHSIns])
  AC_CHECK_HEADERS([Judy.h])
  CFLAGS=$save_CFLAGS
  CPPFLAGS=$save_CPPFLAGS
  LIBS=$save_LIBS
else
  if test "x$with_Judy" = "xyes"; then
    AC_MSG_ERROR([Judy dynamic arrays library missing.])
  else
    AC_CHECK_FUNCS([tdestroy],,
      [AC_MSG_ERROR([Required tdestroy() or Judy dynamic arrays.])]
    )
  fi
fi

# Test coverage build
AC_MSG_CHECKING([whether to build for test coverage])
AC_ARG_ENABLE(coverage,
	[AS_HELP_STRING(--enable-coverage,
		[build for test coverage (default disabled)])],,
	[enable_coverage="no"])
AS_IF([test "${enable_coverage}" != "no"], [
	CFLAGS="${CFLAGS} -g -O0 -fprofile-arcs -ftest-coverage"
	LDFLAGS="${LDFLAGS} -lgcov"
	AC_MSG_RESULT(yes)
], [
	AC_MSG_RESULT(no)
])


# Teredo client
AC_MSG_CHECKING([whether to include Teredo client support])
AC_ARG_ENABLE(teredo-client,
	[AS_HELP_STRING(--disable-teredo-client,
		[do not compile Teredo client (default enabled)])],,
	[enable_teredo_client="yes"])
AM_CONDITIONAL(TEREDO_CLIENT, [test "${enable_teredo_client}" != "no"])
AS_IF([test "${enable_teredo_client}" != "no"], [
	AC_DEFINE(MIREDO_TEREDO_CLIENT, 1,
		[Define to 1 if the Teredo client support must be compiled.])
])
AC_MSG_RESULT([${enable_teredo_client}])


# Configuration files installation
AC_ARG_ENABLE(examplesdir,
	[AS_HELP_STRING(--enable-examplesdir,
		[install sample config files (default ${docdir}/examples)])],,
	[enable_examplesdir='${docdir}/examples'])
AS_IF([test "x${enable_examplesdir}" != "xno"],
	[examplesdir="${enable_examplesdir}"])
AC_SUBST(examplesdir)

AC_ARG_ENABLE(sample-conf,
	[AS_HELP_STRING(--disable-sample-conf,
		[do not install sample config files (default enabled)])],,
	[enable_sample_conf="yes"])
AM_CONDITIONAL(CONF_SAMPLE, [test "${enable_sample_conf}" != "no"])


# Unprivileged user
AC_MSG_CHECKING([user to run as])
AC_ARG_ENABLE(miredo-user,
	[AS_HELP_STRING(--enable-miredo-user,
		[run as a specific user (default nobody)])], [
	AS_IF([test "${enable_miredo_user}" = "yes"],
		[enable_miredo_user="miredo"])
	AS_IF([test "${enable_miredo_user}" = "no"],
		[enable_miredo_user="root"])
], [
	enable_miredo_user="nobody"
])
AC_MSG_RESULT([${enable_miredo_user}])
AS_IF([test "${enable_miredo_user}" != "root"], [
	AC_DEFINE_UNQUOTED(MIREDO_DEFAULT_USERNAME, "${enable_miredo_user}",
		[Define to the default system username to be used.])
], [
	AC_MSG_WARN([Running as root is DANGEROUS!])
])


# Defines for <config.h>
AH_BOTTOM([#include "compat/fixups.h"])


# END
AS_MESSAGE(writing results...)
#AC_CONFIG_FILES([Makefile po/Makefile.in Doxyfile])
AC_CONFIG_FILES([Makefile Doxyfile])
AC_OUTPUT
