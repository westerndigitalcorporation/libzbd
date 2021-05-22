# SPDX-License-Identifier: LGPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2021 Western Digital Corporation or its affiliates.

SUBDIRS = . $(subdirs)

AM_CFLAGS = \
	$(CFLAGS) \
	-Wall -Wextra -Wno-unused-parameter \
	-I$(top_srcdir)/include

libzbd_ldadd = $(top_builddir)/lib/libzbd.la

bin_PROGRAMS =
dist_man8_MANS =

include cli/Makefile.am

if BUILD_GUI
include gui/Makefile.am
include viewer/Makefile.am
endif
