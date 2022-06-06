# SPDX-License-Identifier: LGPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

AC_INIT([libzbd], [2.0.3],
	[damien.lemoal@wdc.com, niklas.cassel@wdc.com],
	[libzbd], [https://github.com/westerndigitalcorporation/libzbd])

AC_CONFIG_AUX_DIR([build-aux])
AC_CANONICAL_TARGET
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([include/config.h])
AC_PREFIX_DEFAULT(/usr)
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AM_SILENT_RULES([yes])

# Change default CFLAGS from "-g -O2" to "-O2" for regular builds.
AC_ARG_ENABLE(debug,
    [  --enable-debug          Compile with "-g" option],
    [DBGCFLAGS="-g"],
    [DBGCFLAGS="-O2"])
CFLAGS="$CFLAGS $DBGCFLAGS"

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
m4_pattern_allow([AM_PROG_AR])
LT_INIT

# Package version details: <major>.<minor>.<release>
PACKAGE_VERSION_MAJOR=$(echo $PACKAGE_VERSION | awk -F. '{print $1}')
PACKAGE_VERSION_MINOR=$(echo $PACKAGE_VERSION | awk -F. '{print $2}')
PACKAGE_VERSION_RELEASE=$(echo $PACKAGE_VERSION | awk -F. '{print $3}')

# libtool friendly library version format
LIBZBD_VERSION_LT=$PACKAGE_VERSION_MAJOR:$PACKAGE_VERSION_MINOR:$PACKAGE_VERSION_RELEASE
AC_SUBST([LIBZBD_VERSION_LT])

# Checks for rpm package builds
AC_PATH_PROG([RPMBUILD], [rpmbuild], [notfound])
AC_PATH_PROG([RPM], [rpm], [notfound])
AM_CONDITIONAL([BUILDING_RPM],
		[test "x$RPMBUILD" != xnotfound && test "x$RPM" != xnotfound])

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
		       [Disable build of GUI tools (gzbd and gzbd-viewer) [default=no]]))
AS_IF([test "x$enable_gui" != "xno"],
      [m4_ifdef([PKG_CHECK_MODULES],
		[PKG_CHECK_MODULES([GTK], [gtk+-3.0], [HAVE_GTK3=1], [HAVE_GTK3=0])],
		[HAVE_GTK3=0])
       AM_CONDITIONAL([BUILD_GUI], [test "$HAVE_GTK3" -eq 1])],
      [AM_CONDITIONAL([BUILD_GUI], false)])

AC_CONFIG_FILES([
	lib/libzbd.pc
	lib/Makefile
	tools/Makefile
	Makefile
])

AC_OUTPUT
