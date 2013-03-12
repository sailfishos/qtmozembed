%define qtmozembedversion 1.0.0

Name:       qtmozembed
Summary:    Qt MozEmbed
Version:    %{qtmozembedversion}
Release:    1
Group:      Applications/Internet
License:    Mozilla License
URL:        http://www.mozilla.com
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(QtCore) >= 4.6.0
BuildRequires:  pkgconfig(QtOpenGL)
BuildRequires:  pkgconfig(QtGui)
BuildRequires:  pkgconfig(xulrunner-devel)

%description
Mozilla XUL runner

%package devel
Group: Development/Tools/Other
Requires: qtmozembed
Summary: Headers for qtmozembed

%description devel
Development files for qtmozembed.

%package misc
Group: Development/Tools/Other
Requires: qtmozembed
Summary: Misc files for qtmozembed

%description misc
Tests and misc files for qtmozembed

%prep
%setup -q -n %{name}-%{version}

%build
cp -rf embedding/embedlite/config/mozconfig.merqtqtmozembed mozconfig
%ifarch i586
echo "ac_add_options --disable-libjpeg-turbo" >> mozconfig
%else
echo "ac_add_options --with-arm-kuser" >> mozconfig
echo "ac_add_options --with-float-abi=toolchain-default" >> mozconfig
# No need for this, this should be managed by toolchain
echo "ac_add_options --with-thumb=yes" >> mozconfig
%endif
export MOZCONFIG=mozconfig
%{__make} -f client.mk build_all %{?jobs:MOZ_MAKE_FLAGS="-j%jobs"}

%install
export MOZCONFIG=mozconfig
%{__make} -f client.mk install DESTDIR=%{buildroot}
%{__chmod} +x %{buildroot}%{_libdir}/qtmozembed-%{greversion}/*.so

%files
%defattr(-,root,root,-)
%attr(755,-,-) %{_bindir}/*
%dir %{_libdir}/qtmozembed-%{greversion}/dictionaries
%{_libdir}/qtmozembed-%{greversion}/*.so
%{_libdir}/qtmozembed-%{greversion}/omni.ja
%{_libdir}/qtmozembed-%{greversion}/dependentlibs.list
%{_libdir}/qtmozembed-%{greversion}/dictionaries/*

%files devel
%defattr(-,root,root,-)
%{_datadir}/*
%{_libdir}/qtmozembed-devel-%{greversion}
%{_libdir}/pkgconfig
%{_includedir}/*

%files misc
%defattr(-,root,root,-)
%{_libdir}/qtmozembed-%{greversion}/*
%exclude %{_libdir}/qtmozembed-%{greversion}/*.so
%exclude %{_libdir}/qtmozembed-%{greversion}/omni.ja
%exclude %{_libdir}/qtmozembed-%{greversion}/dependentlibs.list
%exclude %{_libdir}/qtmozembed-%{greversion}/dictionaries/*

%changelog
