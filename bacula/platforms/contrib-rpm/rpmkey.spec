# rpm public key package
# Copyright (C) 2006 Kern Sibbald
#
#

# replace the string yournam with your name
# first initial and lastname, ie. sbarninger
%define pubkeyname yourname

# replace below with your name and email address
Packager: Your Name <your-email@site.org>

Summary: The %{pubkeyname} rpm public key
Name: rpmkey-%{pubkeyname}
Version: 0.1
Release: 1
License: GPL v2
Group: System/Packages
Source0: %{pubkeyname}.asc
BuildArch: noarch
BuildRoot: %{_tmppath}/%{name}-root

%description
The %{pubkeyname} rpm public key. If you trust %{pubkeyname} component
and you want to import this key to the RPM database, then install this 
RPM. After installing this package you may import the key into your rpm
database, as root:

rpm --import %{_libdir}/rpm/gnupg/%{pubkeyname}.asc

%prep
%setup -c -T a1

%build

%install
mkdir -p %{buildroot}%{_libdir}/rpm/gnupg
cp -a %{SOURCE0} %{buildroot}%{_libdir}/rpm/gnupg

%files
%defattr(-, root, root)
%{_libdir}/rpm/gnupg/%{pubkeyname}.asc


%changelog
* Sun Jul 16 2006 D. Scott Barninger <barninger@fairfieldcomputers.com
- initial spec file

