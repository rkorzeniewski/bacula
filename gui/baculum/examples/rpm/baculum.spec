Summary:	Baculum WebGUI tool for Bacula Community program
Name:		baculum
Version:	7.0.20150315git
Release:	1%{?dist}
License:	AGPLv3
Group:		Applications/Internet
Source:		%{name}-%{version}.tar.gz
URL:		http://bacula.org
Requires:	lighttpd
Requires:	lighttpd-fastcgi
Requires:	php
Requires:	php-bcmath
Requires:	php-common
Requires:	php-mbstring
Requires:	php-mysqlnd
Requires:	php-pdo
Requires:	php-pgsql
Requires:	php-xml
Requires(post):	chkconfig
Requires(preun):chkconfig

%description
The Baculum program allows the user to administrate and manage Bacula work.
By using Baculum is possible to execute backup/restore operations, monitor
current Bacula jobs, media management and others. Baculum has integrated web
console that communicates with Bacula bconsole program.

%prep
%autosetup

%build

%files
%defattr(-,lighttpd,lighttpd)
%attr(-,lighttpd,lighttpd) /usr/share/baculum/htdocs
%attr(-,lighttpd,lighttpd) /etc/baculum
%attr(755,root,root) /etc/rc.d/init.d/baculum

%install
mkdir -p %{buildroot}/usr/share/baculum/htdocs
mkdir -p %{buildroot}/etc/baculum
mkdir -p %{buildroot}/etc/rc.d/init.d

cp -ra . %{buildroot}/usr/share/baculum/htdocs
install -m 640 examples/rpm/baculum.lighttpd.conf %{buildroot}/etc/baculum/
install -m 600 examples/rpm/baculum.users %{buildroot}/etc/baculum/
install -m 755 examples/rpm/baculum.startup %{buildroot}/etc/rc.d/init.d/baculum


%post
ln -s /etc/baculum/baculum.users /usr/share/baculum/htdocs/protected/Data/baculum.users
/sbin/chkconfig --add /etc/rc.d/init.d/baculum

%preun
if [ $1 -eq 0 ] ; then
    /sbin/service baculum stop
    /sbin/chkconfig --del baculum
fi
