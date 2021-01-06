# SPDX-License-Identifier: LGPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

AC_INIT([libzbd], [1.1.0],
	[damien.lemoal@wdc.com, niklas.cassel@wdc.com],
	[libzbd], [https://github.com/westerndigitalcorporation/libzbd])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([include/config.h])
AC_PREFIX_DEFAULT(/usr)

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AM_SILENT_RULES([yes])

AX_CHECK_ENABLE_DEBUG([no], [_DBG_])
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_GNU_SOURCE

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_pattern_allow([AM_PROG_AR])
LT_INIT

ACX_PTHREAD
CFLAGS="$CFLAGS $EXTRA_CFLAGS $PTHREAD_CFLAGS"
if test "x$enable_debug" == "xno"; then
        CFLAGS="-O2 $CFLAGS"
fi
LIBS="$PTHREAD_LIBS $LIBS"
CC="$PTHREAD_CC"

# Checks for rpm package builds
AC_PATH_PROG([RPMBUILD], [rpmbuild], [notfound])
AC_PATH_PROG([RPM], [rpm], [notfound])

AC_ARG_ENABLE([rpm],
	[AS_HELP_STRING([--enable-rpm],
		[Configure for building RPM package (default=no, requires rpm and rpmbuild)])],
	[AS_IF([test "x$enable_rpm" = xyes],
		[AS_IF([test "x$RPMBUILD" = xnotfound],
			[AC_MSG_ERROR([rpmbuild is required for --enable-rpm])],
		[test "x$RPM" = xnotfound],
			[AC_MSG_ERROR([rpm is required for --enable-rpm])])])],
	[enable_rpm=no])
AM_CONDITIONAL(BUILDING_RPM, test "x$enable_rpm" = xyes)

# Checks for deb package builds
AC_PATH_PROG([DEBUILD], [dpkg-buildpackage], [notfound])

AC_ARG_ENABLE([deb],
	[AS_HELP_STRING([--enable-deb],
		[Configure for building Debian package (default=no, requires dpkg-buildpackage)])],
	[AS_IF([test "x$enable_deb" = xyes],
		[AS_IF([test "x$DEBUILD" = xnotfound],
			[AC_MSG_ERROR([dpkg-buildpackage is required for --enable-deb])],
		[test "x$enable_rpm" = xyes],
			[AC_MSG_ERROR([--enable-rpm and --enable-deb are mutually exclusive])])])],
	[enable_deb=no])
AM_CONDITIONAL([BUILDING_DEB], [test "x$enable_deb" = xyes])

# Checks for header files
AC_CHECK_HEADER(libgen.h, [],
		[AC_MSG_ERROR([Couldn't find libgen.h])],
		[[#include <libgen.h>]])
AC_CHECK_HEADER(linux/blkzoned.h, [],
		[AC_MSG_ERROR([Couldn't find linux/blkzoned.h. Kernel too old ?])],
		[[#include <linux/blkzoned.h>]])

AC_CHECK_MEMBER([struct blk_zone.capacity],
		[AC_DEFINE(HAVE_BLK_ZONE_REP_V2, [1], [report zones includes zone capacity])],
		[], [[#include <linux/blkzoned.h>]])

# Conditionals

# Build GUI tools only if GTK3 is installed and can be detected with pkg-config.
AC_ARG_ENABLE([gui],
        AS_HELP_STRING([--disable-gui],
		       [Disable build of GUI tools (gzbd and gzviewer) [default=no]]))
AS_IF([test "x$enable_gui" != "xno"],
      [m4_ifdef([PKG_CHECK_MODULES],
		[PKG_CHECK_MODULES([GTK], [gtk+-3.0], [HAVE_GTK3=1], [HAVE_GTK3=0])],
		[HAVE_GTK3=0])
       AM_CONDITIONAL([BUILD_GUI], [test "$HAVE_GTK3" -eq 1])],
      [AM_CONDITIONAL([BUILD_GUI], false)])

AC_CONFIG_FILES([
	libzbd.pc
	Makefile
])

AC_OUTPUT

