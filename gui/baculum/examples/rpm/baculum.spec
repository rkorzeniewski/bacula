Summary:	WebGUI tool for Bacula Community program
Name:		baculum
Version:	7.0.6
Release:	0.1.a%{?dist}
License:	AGPLv3
Group:		Applications/Internet
URL:		http://bacula.org/
Source0:	http://www.bacula.org/downloads/baculum/baculum-7.0.6a.tar.gz
BuildRequires:	systemd-units
Requires:	lighttpd
Requires:	lighttpd-fastcgi
Requires:	bacula-console
Requires:	php >= 5.3.4
Requires:	php-bcmath
Requires:	php-common
Requires:	php-mbstring
Requires:	php-mysqlnd
Requires:	php-pdo
Requires:	php-pgsql
Requires:	php-xml
Requires(post):	/sbin/chkconfig, policycoreutils-python
Requires(preun):/sbin/service, /sbin/chkconfig, policycoreutils-python
BuildArch: noarch

%description
The Baculum program allows the user to administrate and manage Bacula work.
By using Baculum is possible to execute backup/restore operations, monitor
current Bacula jobs, media management and others. Baculum has integrated web
console that communicates with Bacula bconsole program.

%package selinux
Summary:		SELinux module for Baculum WebGUI tool
Requires:		%name = %version-%release
Group:			Applications/Internet
Requires(post):		policycoreutils-python
Requires(preun):	policycoreutils-python

%description selinux
This package provides an SELinux module for Baculum WebGUI tool.
You should install this package if you are using SELinux, that Baculum
can be run in enforcing mode.

%prep
%autosetup

%build

%files
%defattr(-,lighttpd,lighttpd)
%attr(-,lighttpd,lighttpd) %{_localstatedir}/cache/%{name}/
%attr(750,lighttpd,lighttpd) %{_var}/log/%{name}/
%{_unitdir}/baculum.service
%{_datadir}/%{name}/
%config %{_sysconfdir}/%{name}/%{name}.lighttpd.conf
%config(noreplace) %{_sysconfdir}/%{name}/%{name}.users
%license LICENSE
%doc AUTHORS INSTALL README

%files selinux
%defattr(-,lighttpd,lighttpd)

%install
mkdir -p %{buildroot}%{_datadir}/%{name}/htdocs
mkdir -p %{buildroot}%{_sysconfdir}/%{name}
mkdir -p %{buildroot}%{_unitdir}
mkdir -p %{buildroot}%{_localstatedir}/cache/%{name}
mkdir -p %{buildroot}%{_var}/log/%{name}

cp -ra framework protected themes index.php AUTHORS INSTALL LICENSE README %{buildroot}%{_datadir}/%{name}/htdocs
ln -s  %{_localstatedir}/cache/%{name} %{buildroot}%{_datadir}/%{name}/htdocs/assets
ln -s  %{_localstatedir}/cache/%{name} %{buildroot}%{_datadir}/%{name}/htdocs/protected/runtime
install -m 640 examples/rpm/baculum.lighttpd.conf %{buildroot}%{_sysconfdir}/%{name}/
install -m 600 examples/rpm/baculum.users %{buildroot}%{_sysconfdir}/%{name}/%{name}.users
install -m 644 examples/rpm/baculum.service %{buildroot}%{_unitdir}/

%post
[ -e %{_datadir}/baculum/htdocs/protected/Data/baculum.users ] ||
    ln -s %{_sysconfdir}/baculum/baculum.users %{_datadir}/%{name}/htdocs/protected/Data/baculum.users
[ -e %{_datadir}/baculum/htdocs/assets ] ||
    ln -s %{_localstatedir}/cache/%{name} %{_datadir}/%{name}/htdocs/assets
[ -e %{_datadir}/baculum/htdocs/protected/runtime ] ||
    ln -s %{_localstatedir}/cache/%{name} %{_datadir}/%{name}/htdocs/protected/runtime

%systemd_post baculum.service

%post selinux
if [ $1 -le 1 ] ; then
    semanage port -a -t http_port_t -p tcp 9095
    semanage fcontext -a -t httpd_cache_t '%{_localstatedir}/cache/%{name}(/.*)?' 2>/dev/null || :
    restorecon -R %{_localstatedir}/cache/%{name} || :
    semanage fcontext -a -t httpd_sys_rw_content_t '%{_datadir}/%{name}/htdocs/protected/Data(/.*)?' 2>/dev/null || :
    restorecon '%{_datadir}/%{name}/htdocs/protected/Data' || :
    chcon -t httpd_user_content_rw_t '%{_datadir}/%{name}/htdocs/protected/Data/baculum.users' || :
    chcon -t httpd_user_content_rw_t '%{_sysconfdir}/%{name}/baculum.users' || :
    setsebool -P httpd_can_network_connect 1 || :
fi

%preun
%systemd_preun baculum.service
if [ $1 -lt 1 ] ; then
    [ ! -e %{_datadir}/%{name}/htdocs/protected/Data/baculum.users ] ||
	rm %{_datadir}/%{name}/htdocs/protected/Data/baculum.users
    [ ! -e %{_datadir}/%{name}/htdocs/protected/Data/settings.conf ] ||
	rm %{_datadir}/%{name}/htdocs/protected/Data/settings.conf
    [ ! -e %{_datadir}/%{name}/htdocs/protected/Data/baculum.log ] ||
	rm %{_datadir}/%{name}/htdocs/protected/Data/baculum*.log
fi

%postun
%systemd_postun_with_restart baculum.service

%postun selinux
if [ $1 -eq 0 ] ; then
    semanage port -d -t http_port_t -p tcp 9095
    semanage fcontext -d -t httpd_sys_rw_content_t '%{_datadir}/%{name}/htdocs/protected/Data(/.*)?' 2>/dev/null || :
    semanage fcontext -d -t httpd_cache_t '%{_localstatedir}/cache/%{name}(/.*)?' 2>/dev/null || :
    setsebool -P httpd_can_network_connect 0 || :
fi

%changelog
 * Tue Mar 24 2015 Marcin Haba <marcin.haba@bacula.pl> - 7.0.6-0.1.a
 - Spec create