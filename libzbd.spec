# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2020 Western Digital Corporation or its affiliates.
Name:		libzbd
Version:	1.5.0
Release:	1%{?dist}
Summary:	A library to control zoned block devices

License:	LGPLv3+ and GPLv3+
URL:		https://github.com/westerndigitalcorporation/%{name}
Source0:	https://github.com/westerndigitalcorporation/%{name}/archive/refs/tags/v%{version}.tar.gz

BuildRoot:	%{_topdir}/BUILDROOT/
BuildRequires:	autoconf,autoconf-archive,automake,libtool

%description
libzbd is a library providing functions simplifying the management and
use of zoned block devices using the kernel ioctl interface defined in
/usr/include/linux/blkzoned.h.

%package devel
Summary: Development header files for libzbd
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
This package provides development header files for libzbd.

%prep
%autosetup

%build
sh autogen.sh
%configure --libdir="%{_libdir}" --includedir="%{_includedir}"
%make_build

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
make install PREFIX=%{_prefix} DESTDIR=$RPM_BUILD_ROOT 
chmod -x $RPM_BUILD_ROOT%{_mandir}/man8/*.8

find $RPM_BUILD_ROOT -name '*.la' -delete

%ldconfig_scriptlets

%files
%{_libdir}/*
%{_bindir}/*
%{_mandir}/man8/*
%exclude %{_libdir}/pkgconfig

%files devel
%{_includedir}/*
%{_libdir}/pkgconfig

%license LICENSES/LGPL-3.0-or-later.txt LICENSES/GPL-3.0-or-later.txt
%doc README.md

%changelog
* Tue Aug 17 2021 Damien Le Moal <damien.lemoal@wdc.com> 1.5.0-1
- Version 1.5.0 packages
