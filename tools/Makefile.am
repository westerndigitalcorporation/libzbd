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

polkitdir = $(datadir)/polkit-1/actions
polkit_action_in_FILES =

desktopdir = $(datadir)/applications
desktop_in_FILES =

pixmapdir = $(datadir)/pixmaps
pixmap_FILES =

bin_subst = sed -e 's,[@]bindir[@],$(bindir),g'

include gui/Makefile.am
include viewer/Makefile.am

polkit_action_FILES = $(polkit_action_in_FILES:.policy.in=.policy)
polkit_DATA = $(polkit_action_FILES)

desktop_FILES = $(desktop_in_FILES:.desktop.in=.desktop)
desktop_DATA = $(desktop_FILES)

pixmap_DATA = $(pixmap_FILES)

CLEANFILES = $(desktop_FILES) $(polkit_action_FILES)

EXTRA_DIST = \
        $(polkit_action_in_FILES) \
        $(desktop_in_FILES) \
        $(pixmap_FILES)

endif
