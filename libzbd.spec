Name:		libzbd
Version:	2.0.1
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
use of zoned block devices using the kernel ioctl interface.

# Development headers package
%package devel
Summary: Development header files for libzbd
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
This package provides development header files for libzbd.

# Command line tools package
%package cli-tools
Summary: Command line tools using libzbd
Requires: %{name}%{?_isa} = %{version}-%{release}

%description cli-tools
This package provides command line tools using libzbd.

# Graphic tools package
%package gtk-tools
Summary: GTK tools using libzbd
Requires: %{name}%{?_isa} = %{version}-%{release}

%description gtk-tools
This package provides GTK-based graphical tools using libzbd.

%prep
%autosetup

%build
sh autogen.sh
%configure --libdir="%{_libdir}" --includedir="%{_includedir}"
%make_build

%install
%make_install PREFIX=%{_prefix}
chmod -x ${RPM_BUILD_ROOT}%{_mandir}/man8/*.8*

find ${RPM_BUILD_ROOT} -name '*.la' -delete

%ldconfig_scriptlets

%files
%{_libdir}/*.so.*
%exclude %{_libdir}/*.a
%exclude %{_libdir}/pkgconfig/*.pc
%license LICENSES/LGPL-3.0-or-later.txt
%doc README.md

%files devel
%{_includedir}/*
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc
%license LICENSES/LGPL-3.0-or-later.txt

%files cli-tools
%{_bindir}/zbd
%{_mandir}/man8/zbd.8*
%license LICENSES/GPL-3.0-or-later.txt

%files gtk-tools
%{_bindir}/gzbd
%{_bindir}/gzbd-viewer
%{_mandir}/man8/gzbd.8*
%{_mandir}/man8/gzbd-viewer.8*
%license LICENSES/GPL-3.0-or-later.txt

%changelog
* Sat Dec 11 2021 Damien Le Moal <damien.lemoal@wdc.com> 2.0.1-1
- Version 2.0.1 packages
