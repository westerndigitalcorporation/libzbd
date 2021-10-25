# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Copyright (c) 2020 Western Digital Corporation or its affiliates.
Name:		libzbd
Version:	1.5.0
Release:	1%{?dist}
Summary:	A library to control zoned block devices

License:	LGPLv3+ and GPLv3+
URL:		https://github.com/westerndigitalcorporation/%{name}
Source0:	%{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:	gtk3-devel
BuildRequires:	autoconf
BuildRequires:	autoconf-archive
BuildRequires:	automake
BuildRequires:	libtool
BuildRequires:	make
BuildRequires:	gcc

%description
libzbd is a library providing functions simplifying the management and
use of zoned block devices using the kernel ioctl interface defined in
/usr/include/linux/blkzoned.h.

# Development headers package
%package devel
Summary: Development header files for libzbd
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
This package provides development header files for libzbd.

# Command line tools package
%package tools
Summary: Command line tools using libzbd
Requires: %{name}%{?_isa} = %{version}-%{release}

%description tools
This package provides command line tools using libzbd.

# Graphic tools package
%package gtk-tools
Summary: GTK tools using libzbd
Requires: %{name}%{?_isa} = %{version}-%{release}
Requires: gtk3

%description gtk-tools
This package provides command line tools using libzbd.

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
%exclude %{_libdir}/pkgconfig
%license LICENSES/LGPL-3.0-or-later.txt
%doc README.md

%files devel
%{_includedir}/*
%{_libdir}/pkgconfig
%license LICENSES/LGPL-3.0-or-later.txt

%files tools
%{_bindir}/zbd
%{_mandir}/man8/zbd.*
%license LICENSES/GPL-3.0-or-later.txt

%files gtk-tools
%{_bindir}/gz*
%{_mandir}/man8/gz*
%license LICENSES/GPL-3.0-or-later.txt

%changelog
* Tue Aug 17 2021 Damien Le Moal <damien.lemoal@wdc.com> 1.5.0-1
- Version 1.5.0 packages
