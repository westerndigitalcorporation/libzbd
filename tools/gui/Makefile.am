# SPDX-License-Identifier: GPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

bin_PROGRAMS += gzbd
gzbd_SOURCES = \
	gui/gzbd.c \
	gui/gzbd_if.c \
	gui/gzbd_if_dev.c \
	gui/gzbd.h

gzbd_CFLAGS = $(CFLAGS) $(GTK_CFLAGS)
gzbd_LDADD = $(libzbd_ldadd) $(GTK_LIBS)

dist_man8_MANS += gui/gzbd.8
