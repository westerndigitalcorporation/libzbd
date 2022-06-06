Name:		libzbd
Version:	2.0.3
Release:	1%{?dist}
Summary:	A library to control zoned block devices

License:	LGPLv3+ and GPLv3+
URL:		https://github.com/westerndigitalcorporation/%{name}
Source0:	%{url}/releases/download/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:	desktop-file-utils
BuildRequires:	gtk3-devel
BuildRequires:	autoconf
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

desktop-file-validate %{buildroot}/%{_datadir}/applications/gzbd.desktop
desktop-file-validate %{buildroot}/%{_datadir}/applications/gzbd-viewer.desktop

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
%{_datadir}/polkit-1/actions/org.gnome.gzbd.policy
%{_datadir}/applications/gzbd.desktop
%{_datadir}/pixmaps/gzbd.png
%{_bindir}/gzbd-viewer
%{_datadir}/polkit-1/actions/org.gnome.gzbd-viewer.policy
%{_datadir}/applications/gzbd-viewer.desktop
%{_datadir}/pixmaps/gzbd-viewer.png
%{_mandir}/man8/gzbd.8*
%{_mandir}/man8/gzbd-viewer.8*
%license LICENSES/GPL-3.0-or-later.txt

%changelog
* Wed Mar 23 2022 Damien Le Moal <damien.lemoal@wdc.com> 2.0.3-1
- Update to version 2.0.3
* Wed Dec 15 2021 Damien Le Moal <damien.lemoal@wdc.com> 2.0.2-1
- Update to version 2.0.2
