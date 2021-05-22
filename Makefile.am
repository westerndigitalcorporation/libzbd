# SPDX-License-Identifier: LGPL-3.0-or-later
#
# SPDX-FileCopyrightText: 2020 Western Digital Corporation or its affiliates.

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = lib tools

EXTRA_DIST = autogen.sh \
	     README.md \
	     LICENSES/GPL-3.0-or-later.txt \
	     LICENSES/LGPL-3.0-or-later.txt

if BUILDING_RPM
rpmdir = $(abs_top_builddir)/rpmbuild

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
