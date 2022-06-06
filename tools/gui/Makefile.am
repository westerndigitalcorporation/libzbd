# SPDX-License-Identifier: GPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

polkit_action_in_FILES += gui/org.gnome.gzbd.policy.in
desktop_in_FILES += gui/gzbd.desktop.in
pixmap_FILES += gui/gzbd.png

bin_PROGRAMS += gzbd
gzbd_SOURCES = \
	gui/gzbd.c \
	gui/gzbd_if.c \
	gui/gzbd_if_dev.c \
	gui/gzbd.h

gzbd_CFLAGS = $(CFLAGS) $(GTK_CFLAGS)
gzbd_LDADD = $(libzbd_ldadd) $(GTK_LIBS) -lpthread

dist_man8_MANS += gui/gzbd.8

gui/org.gnome.gzbd.policy: gui/org.gnome.gzbd.policy.in Makefile
	$(bin_subst) < gui/org.gnome.gzbd.policy.in > gui/org.gnome.gzbd.policy

gui/gzbd.desktop: gui/gzbd.desktop.in Makefile
	$(bin_subst) < gui/gzbd.desktop.in > gui/gzbd.desktop
