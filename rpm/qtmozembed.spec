%define qtmozembedversion 1.0.0

Name:       qtmozembed
Summary:    Qt MozEmbed
Version:    1.0.3+master
Release:    10.19.1.jolla
Group:      Applications/Internet
License:    Mozilla License
URL:        http://www.mozilla.com
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(QtCore) >= 4.6.0
BuildRequires:  pkgconfig(QtOpenGL)
BuildRequires:  pkgconfig(QtGui)
BuildRequires:  pkgconfig(QJson)
BuildRequires:  pkgconfig(libxul-embedding)
BuildRequires:  pkgconfig(nspr)
BuildRequires:  pkgconfig(QtTest)
BuildRequires:  qtest-qml-devel

%description
Mozilla XUL runner

%package devel
Group: Development/Tools/Other
Requires: qtmozembed
Summary: Headers for qtmozembed

%description devel
Development files for qtmozembed.

%package tests
Summary:    Unit tests for QtMozEmbed tests
Group:      Applications/Multimedia
Requires:   %{name} = %{version}-%{release}
Requires:   embedlite-components >= 1.0.10

%description tests
This package contains QML unit tests for QtMozEmbed library

%prep
%setup -q -n %{name}-%{version}

%build
qmake
%{__make} %{?jobs:MOZ_MAKE_FLAGS="-j%jobs"}

%install
%{__make} install INSTALL_ROOT=%{buildroot}
%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root,-)
%{_libdir}/*.so
%{_libdir}/pkgconfig
%{_includedir}/*

%files tests
%defattr(-,root,root,-)
# >> files tests
/opt/tests/qtmozembed/*
%{_libdir}/qt4/bin/*
# << files tests

%changelog
* Wed Mar 20 2013 Tatiana Meshkova <tanya.meshkova@gmail.com> - 1.0.3
- [browser] Fixed linking flag for autotests to link it with local built library. Contributes to JB#5879
* Tue Mar 19 2013 Tatiana Meshkova <tanya.meshkova@gmail.com> - 1.0.1
- [qtmozembed] Created basic tests sceleton (copypasted from gallery). Contributes to JB#5485
- [qtmozembed] Drop circular build dependency on itself

