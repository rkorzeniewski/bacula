# 
# gui Makefile.in
#
# Version $Id$
# 

# autoconf/Make.common.in	 -*- Makefile -*-
# release date (man), LSM date, version number/name, current maintainer
DATE="19 May 2005"
LSMDATE=@LSMDATE@
VERSION=1.7.0
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
	@$(RMF) *~ 1 2 3

# clean for distribution
distclean: clean
	@$(RMF) -r CVS

devclean: clean
	@for subdir in freebsd linux solaris; do \
	    if [ -f $${subdir}/Makefile ]; then \
		     (cd $${subdir}; $(MAKE) devclean) \
	    fi; \
# ------------------------------------------------------------------------
