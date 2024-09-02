%global min_xulrunner_version 90.9.1

%define system_nspr       1
%define system_pixman     1

Name:       qtmozembed-qt5
Summary:    Qt embeddings for Gecko
Version:    1.53.9
Release:    1
License:    MPLv2.0
URL:        https://github.com/sailfishos/qtmozembed/
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5OpenGL)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5QuickTest)
%if %{system_nspr}
BuildRequires:  pkgconfig(nspr) >= 4.13.1
%endif

%if %{system_pixman}
BuildRequires:  pkgconfig(pixman-1) >= 0.19.2
%endif
BuildRequires:  xulrunner-qt5-devel >= %{min_xulrunner_version}
BuildRequires:  qt5-default
BuildRequires:  qt5-qttools
BuildRequires:  pkgconfig(systemsettings) >= 0.5.25
Requires:       xulrunner-qt5 >= %{min_xulrunner_version}
Requires:       nemo-qml-plugin-systemsettings >= 0.5.25
Requires:       embedlite-components-qt5 >= 1.22.32

%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}

%description
Qt embeddings for Gecko browser engine

%package devel
Requires:   %{name} = %{version}-%{release}
Summary:    Headers for qtmozembed

%description devel
Development files for qtmozembed.

%package tests
Summary:    Unit tests for QtMozEmbed tests
Requires:   %{name} = %{version}-%{release}
Requires:   qt5-qtdeclarative-import-qttest
Requires:   nemo-test-tools

%description tests
This package contains QML unit tests for QtMozEmbed library

%prep
%setup -q -n %{name}-%{version}

%build

CONFIGURE_VARIABLE=""

%if %{system_nspr}
  CONFIGURE_VARIABLE="with-system-nspr"
%endif

%qtc_qmake5 -r VERSION=%{version} CONFIG+=${CONFIGURE_VARIABLE}
%qtc_make %{?_smp_mflags}

%install
%qmake5_install
%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%license LICENSE.txt
%{_libdir}/*.so.*
%{_libdir}/qt5/qml/Qt5Mozilla/*

%files devel
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_libdir}/pkgconfig
%{_includedir}/*

%files tests
%defattr(-,root,root,-)
/opt/tests/qtmozembed/*
