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
rpmdir = $(abs_top_builddir)/rpmbuild

pkgconf_DATA = libzbd.pc
pkginclude_HEADERS =
noinst_HEADERS =
bin_PROGRAMS =
lib_LTLIBRARIES =
noinst_PROGRAMS =

dist_man8_MANS =

libzbd_ldadd = $(LDADD) libzbd.la

pkginclude_HEADERS += include/libzbd/zbd.h

include lib/Makemodule.am

include tools/zbd/Makemodule.am
if BUILD_GUI
include tools/gzbd/Makemodule.am
include tools/gzbd-viewer/Makemodule.am
endif

if BUILDING_RPM
EXTRA_DIST += libzbd.spec
RPMARCH=`$(RPM) --eval %_target_cpu`

spec: libzbd.spec.in
	@echo "Generating rpm spec file..."
	@echo "Version:        $(PACKAGE_VERSION)" > libzbd.spec
	@cat libzbd.spec.in | grep -v '# ' >> libzbd.spec

rpm: spec dist
	@echo "Building RPM packages..."
	@mkdir -p $(rpmdir)
	$(RPMBUILD) -ta --clean -D "_topdir $(rpmdir)" libzbd-$(PACKAGE_VERSION).tar.gz
	@mv -f $(rpmdir)/RPMS/$(RPMARCH)/*.rpm $(abs_top_builddir)
	@mv -f $(rpmdir)/SRPMS/*.rpm $(abs_top_builddir)
	@rm -rf $(rpmdir)
	@rm -f libzbd-$(PACKAGE_VERSION).tar.gz
	@rm -f libzbd.spec
else
rpm:
	@echo "To build RPM packages, run configure with --enable-rpm"
	exit 1
endif
