# SPDX-License-Identifier: GPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

AM_LDFLAGS = -lm

bin_PROGRAMS += gzbd-viewer
gzbd_viewer_SOURCES = \
	viewer/gzbd_viewer.c \
	viewer/gzbd_viewer_if.c \
	viewer/gzbd_viewer.h

gzbd_viewer_CFLAGS = $(CFLAGS) $(GTK_CFLAGS)
gzbd_viewer_LDADD = $(libzbd_ldadd) $(GTK_LIBS)

dist_man8_MANS += viewer/gzbd-viewer.8
