# 
# gui Makefile.in
#
# Version $Id$
# 

# autoconf/Make.common.in        -*- Makefile -*-
# release date (man), LSM date, version number/name, current maintainer
DATE="26 January 2007"
LSMDATE=@LSMDATE@
VERSION=2.0.2
VERNAME=bacula-$(VERSION)#
MAINT=Kern Sibbald#
MAINTEMAIL=<kern@sibbald.com>#
#-------------------------------------------------------------------------

SHELL = /bin/sh

# Installation target directories & other installation stuff
prefix = /usr/local
exec_prefix = ${prefix}
binprefix =
manprefix =
sbindir = ${exec_prefix}/sbin
sysconfdir = ${prefix}/etc
scriptdir = @scriptdir@
mandir = ${prefix}/man/man1
manext = 1

# Tools & program stuff
MV = /bin/mv
RM = /bin/rm
RMF = /bin/rm -f
CP = /bin/cp
SED = @SED@
AWK = /usr/bin/gawk
ECHO = /bin/echo
CMP = @CMP@
INSTALL = /usr/bin/install -c
# add the -s to the following in PRODUCTION mode
INSTALL_PROGRAM = /usr/bin/install -c -m @SBINPERM@
INSTALL_DATA = /usr/bin/install -c -m 644
INSTALL_SCRIPT = /usr/bin/install -c -m @SBINPERM@
INSTALL_CONFIG = /usr/bin/install -c -m 640


# End of common section of the Makefile
#-------------------------------------------------------------------------

srcdir =	.

.PATH:		.
topdir = .
thisdir = .


first_rule: all
dummy:

MKDIR = $(topdir)/autoconf/mkinstalldirs

#-------------------------------------------------------------------------

all: Makefile


depend:
	

#-------------------------------------------------------------------------


install:

uninstall:

configure: autoconf/configure.in autoconf/aclocal.m4 autoconf/acconfig.h autoconf/config.h.in
	cd $(srcdir);
	${RMF} -f config.cache config.log config.out config.status src/config.h
	autoconf --prepend-include=$(srcdir)/autoconf \
	autoconf/configure.in > configure
	chmod 755 configure

Makefile: Makefile.in
	cd $(topdir) \
	    && CONFIG_FILES=$(thisdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status

Makefiles:
	$(SHELL) config.status

clean:
	@$(RMF) *~ 1 2 3 diff */diff  *.tar.gz
	@$(RMF) config.log config.out config.status
	@rm -rf autom4te.cache

# clean for distribution
distclean: clean
	@$(RMF) -rf CVS

devclean: clean
	@for subdir in freebsd linux solaris; do \
	    if [ -f $${subdir}/Makefile ]; then \
		     (cd $${subdir}; $(MAKE) devclean) \
	    fi; \

release-bacula-web:
	(cwd=`pwd`; rm -rf /tmp/bacula-web-$(VERSION); \
	  cd ..; cp -rp $$cwd/bacula-web /tmp/bacula-web-$(VERSION); \
	  cd /tmp/bacula-web-$(VERSION); \
	  make clean; \
	  find . -name ".#*" -exec rm -rf {} \; ; \
	  find . -name "CVS" -exec rm -rf {} \; ; \
	  find . -name ".cvsignore" -exec rm -rf {} \; ; \
	  rm -rf autom4te.cache; \
	  cd ..; \
	  tar cvfz $$cwd/bacula-web-$(VERSION).tar.gz bacula-web-$(VERSION); \
	  rm -rf /tmp/bacula-web-$(VERSION); \
	  echo " "; \
	  echo "bacula-web release is in: $$cwd/bacula-web-$(VERSION).tar.gz"; \
	  echo " ")

release-bimagemgr:
	(cwd=`pwd`; rm -rf /tmp/bimagemgr-$(VERSION); \
	  cd ..; cp -rp $$cwd/bimagemgr /tmp/bimagemgr-$(VERSION); \
	  cd /tmp/bimagemgr-$(VERSION); \
	  make clean; \
	  find . -name ".#*" -exec rm -rf {} \; ; \
	  find . -name "CVS" -exec rm -rf {} \; ; \
	  find . -name ".cvsignore" -exec rm -rf {} \; ; \
	  rm -rf autom4te.cache; \
	  cd ..; \
	  tar cvfz $$cwd/bimagemgr-$(VERSION).tar.gz bimagemgr-$(VERSION); \
	  rm -rf /tmp/bimagemgr-$(VERSION); \
	  echo " "; \
	  echo "bimagemgr release is in: $$cwd/bimagemgr-$(VERSION).tar.gz"; \
	  echo " ")

release-brestore:
	(cwd=`pwd`; rm -rf /tmp/brestore-$(VERSION); \
	  cd ..; cp -rp $$cwd/brestore /tmp/brestore-$(VERSION); \
	  cd /tmp/brestore-$(VERSION); \
	  make clean; \
	  find . -name ".#*" -exec rm -rf {} \; ; \
	  find . -name "CVS" -exec rm -rf {} \; ; \
	  find . -name ".cvsignore" -exec rm -rf {} \; ; \
	  rm -rf autom4te.cache; \
	  cd ..; \
	  tar cvfz $$cwd/brestore-$(VERSION).tar.gz brestore-$(VERSION); \
	  rm -rf /tmp/brestore-$(VERSION); \
	  echo " "; \
	  echo "brestore release is in: $$cwd/brestore-$(VERSION).tar.gz"; \
	  echo " ")

release-bweb:
	(cwd=`pwd`; rm -rf /tmp/bweb-$(VERSION); \
	  cd ..; cp -rp $$cwd/bweb /tmp/bweb-$(VERSION); \
	  cd /tmp/bweb-$(VERSION); \
	  make clean; \
	  find . -name ".#*" -exec rm -rf {} \; ; \
	  find . -name "CVS" -exec rm -rf {} \; ; \
	  find . -name ".cvsignore" -exec rm -rf {} \; ; \
	  rm -rf autom4te.cache; \
	  cd ..; \
	  tar cvfz $$cwd/bweb-$(VERSION).tar.gz bweb-$(VERSION); \
	  rm -rf /tmp/bweb-$(VERSION); \
	  echo " "; \
	  echo "bweb release is in: $$cwd/bweb-$(VERSION).tar.gz"; \
	  echo " ")

release: clean release-bacula-web release-bimagemgr release-brestore release-bweb

# ------------------------------------------------------------------------
