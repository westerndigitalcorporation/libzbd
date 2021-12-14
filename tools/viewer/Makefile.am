# SPDX-License-Identifier: GPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

AM_LDFLAGS = -lm

polkit_action_in_FILES += viewer/org.gnome.gzbd-viewer.policy.in
desktop_in_FILES += viewer/gzbd-viewer.desktop.in
pixmap_FILES += viewer/gzbd-viewer.png

bin_PROGRAMS += gzbd-viewer
gzbd_viewer_SOURCES = \
	viewer/gzbd_viewer.c \
	viewer/gzbd_viewer_if.c \
	viewer/gzbd_viewer.h

gzbd_viewer_CFLAGS = $(CFLAGS) $(GTK_CFLAGS)
gzbd_viewer_LDADD = $(libzbd_ldadd) $(GTK_LIBS)

dist_man8_MANS += viewer/gzbd-viewer.8

viewer/org.gnome.gzbd-viewer.policy: viewer/org.gnome.gzbd-viewer.policy.in Makefile
	$(bin_subst) < viewer/org.gnome.gzbd-viewer.policy.in > viewer/org.gnome.gzbd-viewer.policy

viewer/gzbd-viewer.desktop: viewer/gzbd-viewer.desktop.in Makefile
	$(bin_subst) < viewer/gzbd-viewer.desktop.in > viewer/gzbd-viewer.desktop
