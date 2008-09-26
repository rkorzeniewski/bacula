#
# Simple Makefile for building test FD plugins for Bacula
#
# Version $Id: Makefile.in 7638 2008-09-25 14:04:17Z kerns $
#
#
# This file is pulled in by all the Unix Bacula Makefiles
#   so it has all the "common" definitions
#

DATE="25 September 2008"
LSMDATE=25Sep08
VERSION=2.5.4
VERNAME=bacula-$(VERSION)#
MAINT=Kern Sibbald#
MAINTEMAIL=<kern@sibbald.com>#
WEBMAINT=#
WEBMAINTEMAIL=#
WEBPAGE=#
FTPSITENAME=#
FTPSITEDIR=#
#-------------------------------------------------------------------------

SHELL = /bin/sh

# Installation target directories & other installation stuff
prefix = 
exec_prefix = 
binprefix =
manprefix =
datarootdir = ${prefix}/share
sbindir = /home/kern/bacula/bin
sysconfdir = /home/kern/bacula/bin
scriptdir = /home/kern/bacula/bin
mandir = /home/kern/bacula/bin
manext = 8

NO_ECHO = @

# Tools & program stuff
CC = gcc
CPP = gcc -E
CXX = /usr/bin/g++
MV = /bin/mv
RM = /bin/rm
RMF = /bin/rm -f
CP = /bin/cp
SED = /bin/sed
AWK = /usr/bin/mawk
ECHO = /bin/echo
CMP = /usr/bin/cmp
TBL = /usr/bin/tbl
AR = /usr/bin/ar
RANLIB = ranlib
MKDIR = /home/kern/bacula/k/autoconf/mkinstalldirs
INSTALL = /usr/bin/install -c
# add the -s to the following in PRODUCTION mode
INSTALL_PROGRAM = /usr/bin/install -c -m 0754
INSTALL_DATA = /usr/bin/install -c -m 644
INSTALL_SCRIPT = /usr/bin/install -c -m 0754
INSTALL_CONFIG = /usr/bin/install -c -m 640

# Flags & libs
CFLAGS = -g -O2 -Wall -fno-strict-aliasing -fno-exceptions -fno-rtti 

CPPFLAGS =  -fno-strict-aliasing -fno-exceptions -fno-rtti 
LDFLAGS = 
TTOOL_LDFLAGS = 
#DEFS = -DHAVE_CONFIG_H 
LIBS = -lpthread -ldl 
WRAPLIBS = -lwrap
DINCLUDE = 
DLIB = 
DB_LIBS = -L/home/kern/bacula/depkgs/sqlite3 -lsqlite3
PYTHON_LIBS = -L/usr/lib/python2.5/config -lpython2.5 -lutil -lrt 
PYTHON_INC = -I/usr/include/python2.5
OPENSSL_LIBS = -lssl -lcrypto
BDB_CPPFLAGS = 
BDB_LIBS = 


# Windows (cygwin) flags 
WCFLAGS = 
WLDFLAGS = 

# X Include directory
#XINC =  @XPM_CFLAGS@

# extra libraries needed by X on some systems, X library location
#XLIB =  @XPM_LIBS@ -lX11 

# End of common section of the Makefile
#-------------------------------------------------------------------------


# No optimization for now for easy debugging

FDDIR=../../filed
SRCDIR=../..
LIBDIR=../../lib

.SUFFIXES:    .c .o
.c.o:
	$(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) -I${SRCDIR} -I${FDDIR} -DTEST_PROGRAM -c $<

all: bpipe-fd.so

test: main example-plugin-fd.so

fd_plugins.o: ${FDDIR}/fd_plugins.h ${FDDIR}/fd_plugins.c
	$(CXX) -I${SRCDIR} -I${FDDIR} -DTEST_PROGRAM -c ${FDDIR}/fd_plugins.c 

main: fd_plugins.o
	$(CXX) $(LDFLAGS) -L${LIBDIR} fd_plugins.o -o main -lbac -lpthread -lssl -l crypto -ldl

example-plugin-fd.o: example-plugin-fd.c ${FDDIR}/fd_plugins.h
	$(CXX) -fPIC -I../.. -I${FDDIR} -c example-plugin-fd.c

example-plugin-fd.so: example-plugin-fd.o 
	$(CXX) $(LDFLAGS) -shared example-plugin-fd.o -o example-plugin-fd.so

bpipe-fd.o: bpipe-fd.c ${FDDIR}/fd_plugins.h
	$(CXX) $(DEFS) $(DEBUG) $(CPPFLAGS) -fPIC -I../.. -I${FDDIR} -c bpipe-fd.c

bpipe-fd.so: bpipe-fd.o 
	$(CXX) $(LDFLAGS) -shared bpipe-fd.o -o bpipe-fd.so

clean:
	rm -f main *.so *.o 1 2 3
