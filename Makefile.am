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

rpm: dist
	@mkdir -p $(rpmdir)/BUILD
	@mkdir -p $(rpmdir)/RPMS
	@mkdir -p $(rpmdir)/SOURCES
	@mkdir -p $(rpmdir)/SPECS
	@mkdir -p $(rpmdir)/SRPMS
	@mv libzbd-$(PACKAGE_VERSION).tar.gz $(rpmdir)/SOURCES
	@echo "Version:        $(PACKAGE_VERSION)" > $(rpmdir)/SPECS/libzbd.spec
	@cat $(rpmdir)/libzbd.spec.in >> $(rpmdir)/SPECS/libzbd.spec
	@rpmbuild -bs -v \
		--target="$(build_cpu)" \
		-D "_topdir $(rpmdir)" \
		--nodebuginfo "$(rpmdir)/SPECS/libzbd.spec"
	@mv -f $(rpmdir)/SRPMS/*.rpm $(abs_top_builddir)
	@rpmbuild -bb -v \
		--target="$(build_cpu)" \
		-D "_topdir $(rpmdir)" \
		--nodebuginfo "$(rpmdir)/SPECS/libzbd.spec"
	@mv -f $(rpmdir)/RPMS/$(build_cpu)/*.rpm $(abs_top_builddir)
	@rm -rf $(rpmdir)/SRPMS
	@rm -rf $(rpmdir)/SPECS
	@rm -rf $(rpmdir)/SOURCES
	@rm -rf $(rpmdir)/RPMS
	@rm -rf $(rpmdir)/BUILDROOT
	@rm -rf $(rpmdir)/BUILD

