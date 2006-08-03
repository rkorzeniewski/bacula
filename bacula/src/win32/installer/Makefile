#
# Makefile for win32 bacula executables
# Using MinGW cross-compiler on GNU/Linux
#
#  Written by Robert Nelson, June 2006
#

include ../Makefile.inc

VERSION := $(shell sed -ne 's/.*[ \t]VERSION[ \t]\+"\(.*\)"/\1/p' < ../../version.h)

DEFINES := \
	-DVERSION=$(VERSION) \
	-DDOCDIR=$(DOCDIR) \
	-DMINGW_BIN=$(MINGW_BIN) 

INSTALL_EXE := winbacula-$(VERSION).exe

BACULA_BINARIES := \
	bacula.dll \
	bacula-dir.exe \
	bacula-fd.exe \
	bacula-sd.exe \
	bconsole.exe \
	bcopy.exe \
	bextract.exe \
	bls.exe \
	bscan.exe \
	btape.exe \
	dbcheck.exe \
	wx-console.exe

DEPKGS_BINARIES := \
	libeay32.dll \
	pthreadGCE.dll \
	ssleay32.dll \
	zlib1.dll \
	openssl.exe \
	loaderinfo.exe \
	mt.exe \
	mtx.exe \
	scsitape.exe \
	tapeinfo.exe \
	wxbase26_gcc_bacula.dll \
	wxmsw26_core_gcc_bacula.dll

NONGCC_BINARIES := \
	libmysql.dll

SCRIPTS := \
	../scripts/mtx-changer.cmd.in \
	../cats/create_mysql_database.cmd \
	../cats/drop_mysql_database.cmd \
	../cats/make_mysql_tables.cmd \
	../cats/make_mysql_tables.sql \
	../cats/drop_mysql_tables.cmd \
	../cats/drop_mysql_tables.sql \
	../cats/update_mysql_tables.cmd \
	../cats/update_mysql_tables.sql \
	../cats/grant_mysql_privileges.cmd \
	../cats/grant_mysql_privileges.sql \
	../cats/make_catalog_backup.cmd \
	../cats/delete_catalog_backup.cmd

##########################################################################

# Targets

.PHONY: all clean installer

all:

installer:	$(INSTALL_EXE)

clean:
	@echo "Cleaning `pwd`"
	$(CMD_ECHO)-rm -f $(INSTALL_EXE) $(BACULA_BINARIES) $(DEPKGS_BINARIES) $(NONGCC_BINARIES)
	$(CMD_ECHO)-rm -f $(BACULA_BINARIES) $(addsuffix .dbg,$(basename $(BACULA_BINARIES)))
	$(CMD_ECHO)-rm -f $(DEPKGS_BINARIES) $(addsuffix .dbg,$(basename $(DEPKGS_BINARIES)))

#
# Rules
#

define Strip_Binary
$$(notdir $(1)): $(1)
	$(ECHO_CMD)cp -f $$^ $$@
	$$(call makedbg,$$@)
endef

define Copy_Binary
$$(notdir $(1)): $(1)
	$(ECHO_CMD)cp -f $$^ $$@
endef

$(foreach file,$(addprefix $(DEPKGS)/bin/, $(DEPKGS_BINARIES)),$(eval $(call Strip_Binary,$(file))))

$(foreach file,$(addprefix $(DEPKGS)/bin/, $(NONGCC_BINARIES)),$(eval $(call Copy_Binary,$(file))))

$(foreach file,$(addprefix $(BINDIR)/, $(BACULA_BINARIES)),$(eval $(call Strip_Binary,$(file))))

$(INSTALL_EXE): winbacula.nsi $(BACULA_BINARIES) $(SCRIPTS) $(DEPKGS_BINARIES) $(NONGCC_BINARIES)
	NSISDIR=$(NSIS_DIR) \
	$(NSIS_DIR)/makensis -V3 $(DEFINES) winbacula.nsi

include ../Makefile.rules
