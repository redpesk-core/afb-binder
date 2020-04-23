#---------------------------------------------
# spec file for package afb-binder
#---------------------------------------------

Name:           afb-binder
Version:        1.2
Release:        0%{?dist}
License:        GPLv3
Summary:        Application framework binder
Group:          Development/Libraries/C and C++
Url:            https://github.com/redpesk/afb-libafb
Source:         %{name}-%{version}.tar.gz
BuildRequires:  make
BuildRequires:  cmake
BuildRequires:  gcc-c++

BuildRequires:  pkgconfig(afb-libafb)
BuildRequires:  pkgconfig(json-c)

# this is the compatibility with afb-daemon
# set it to 0 to remove any afb-daemon references
%global provide_afb_daemon 1

%if 0%{?provide_afb_daemon}
Provides:       afb-daemon
%endif

%description
The application framework binder

#---------------------------------------------
%package devel
Summary:        Application framework binder compatibility for developpers
Requires:       %{name} = %{version}
Requires:       pkgconfig(afb-binding)
%if 0%{?provide_afb_daemon}
Provides:       pkgconfig(afb-daemon) = %{version}
%endif

%description devel
Application framework binder compatibility with afb-daemon for developpers

#---------------------------------------------
%prep
%setup -q -n %{name}-%{version}

%build
%cmake %{?provide_afb_daemon:-DPROVIDE_AFB_DAEMON=ON} .
%__make %{?_smp_mflags}

%install
%make_install

%post

%postun

#---------------------------------------------
%files
%defattr(-,root,root)
%{_bindir}/afb-binder
%dir %{_datarootdir}/afb-binder/monitoring
%{_datarootdir}/afb-binder/monitoring/*

%if 0%{?provide_afb_daemon}
%{_bindir}/afb-daemon
%endif

#---------------------------------------------
%files devel
%defattr(-,root,root)
%if 0%{?provide_afb_daemon}
%{_libdir}/pkgconfig/*.pc
%endif

#---------------------------------------------
%changelog
* Thu Apr 23 2020 jobol
- initial creation
