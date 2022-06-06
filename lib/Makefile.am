# SPDX-License-Identifier: LGPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

AM_CFLAGS = \
        $(CFLAGS) \
        -Wall -Wextra -Wno-unused-parameter \
        -I$(top_srcdir)/include

pkgconfdir = $(libdir)/pkgconfig
pkgconf_DATA = libzbd.pc
pkginclude_HEADERS = ../include/libzbd/zbd.h

lib_LTLIBRARIES = libzbd.la

EXTRA_DIST = exports

CFILES = \
	zbd.c \
	zbd_utils.c

HFILES = \
	zbd.h

libzbd_la_DEPENDENCIES = exports
libzbd_la_SOURCES = $(CFILES) $(HFILES)
libzbd_la_CFLAGS = $(AM_CFLAGS) -fPIC
libzbd_la_LDFLAGS = \
        -lpthread \
	-Wl,--version-script,exports \
	-version-number @LIBZBD_VERSION_LT@


