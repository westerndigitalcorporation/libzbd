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

rpm: dist
	@echo "Building RPM packages..."
	@mkdir -p $(rpmdir)
	$(RPMBUILD) -ta --clean -D "_topdir $(rpmdir)" libzbd-$(PACKAGE_VERSION).tar.gz
	@mv -f $(rpmdir)/RPMS/$(RPMARCH)/*.rpm $(abs_top_builddir)
	@mv -f $(rpmdir)/SRPMS/*.rpm $(abs_top_builddir)
	@rm -rf $(rpmdir)
	@rm -f libzbd-$(PACKAGE_VERSION).tar.gz
else
rpm:
	@echo "Building RPM packages requires rpmbuild and rpm utilities"
	exit 1
endif

CLEANFILES = *.rpm *.tar.gz
DISTCLEANFILES = *.rpm *.tar.gz configure
