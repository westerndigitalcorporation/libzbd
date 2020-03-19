# SPDX-License-Identifier: LGPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = . $(subdirs)
EXTRA_DIST = \
	autogen.sh \
	exports

AM_CPPFLAGS = \
	-O2 \
	-Wall -Wextra -Wno-unused-parameter \
	-I$(top_srcdir)/include

pkgconfdir = $(libdir)/pkgconfig

pkgconf_DATA = libzbd.pc
pkginclude_HEADERS =
noinst_HEADERS =
bin_PROGRAMS =
lib_LTLIBRARIES =
noinst_PROGRAMS =

libzbd_ldadd = $(LDADD) libzbd.la

pkginclude_HEADERS += include/libzbd/zbd.h

include lib/Makemodule.am

include tools/zbd/Makemodule.am
if BUILD_GUI
include tools/gzbd/Makemodule.am
include tools/gzbd-viewer/Makemodule.am
endif

