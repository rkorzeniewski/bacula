#
# Makefile for win32 bacula executables
# Using MinGW cross-compiler on GNU/Linux
#
#  Written by Robert Nelson, June 2006
#

include ../Makefile.inc

VERSION := $(shell sed -ne 's/.*[ \t]VERSION[ \t][ \t]*"\(.*\)"/\1/p' < ../../version.h)

DEFINES := \
	-DVERSION=$(VERSION) \
	-DOUT_DIR=..\\release \
	-DDOC_DIR=$(DOCDIR) \
	-DBUILD_TOOLS=MinGW \
	-DMINGW_BIN=$(MINGW_BIN) \
	-DDEPKGS_BIN=. \
	-DBACULA_BIN=. \
	-DCATS_DIR=..\\cats \
	-DSCRIPT_DIR=..\\scripts

INSTALL_EXE := ../release/winbacula-$(VERSION).exe

BACULA_BINARIES := \
	bacula.dll \
	cats_mysql.dll \
	cats_pgsql.dll \
	cats_bdb.dll \
	bacula-dir.exe \
	bacula-fd.exe \
	bacula-sd.exe \
	bconsole.exe \
	bcopy.exe \
	bextract.exe \
	bls.exe \
	bscan.exe \
	bsleep.exe \
	bsmtp.exe \
	btape.exe \
	dbcheck.exe \
	scsilist.exe \
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
	sed.exe \
	tapeinfo.exe \
	wxbase26_gcc_bacula.dll \
	wxmsw26_core_gcc_bacula.dll

NONGCC_BINARIES := \
	libmysql.dll

NONGCC_LIBRARIES := \
	libpq.dll

SCRIPTS := \
	../scripts/mtx-changer.cmd \
	../cats/create_bdb_database.cmd \
	../cats/drop_bdb_database.cmd \
	../cats/make_bdb_tables.cmd \
	../cats/drop_bdb_tables.cmd \
	../cats/update_bdb_tables.cmd \
	../cats/grant_bdb_privileges.cmd \
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
	../cats/create_postgresql_database.cmd \
	../cats/drop_postgresql_database.cmd \
	../cats/make_postgresql_tables.cmd \
	../cats/make_postgresql_tables.sql \
	../cats/drop_postgresql_tables.cmd \
	../cats/drop_postgresql_tables.sql \
	../cats/update_postgresql_tables.cmd \
	../cats/update_postgresql_tables.sql \
	../cats/grant_postgresql_privileges.cmd \
	../cats/grant_postgresql_privileges.sql \
	../cats/make_catalog_backup.cmd \
	../cats/delete_catalog_backup.cmd

DIRD_FILES := \
	query.sql

SSL_FILES := \
	openssl.cnf

##########################################################################

# Targets

.PHONY: all clean installer

all:		$(INSTALL_EXE)

installer:	$(INSTALL_EXE)

clean:
	@echo "Cleaning `pwd`"
	$(CMD_ECHO)-rm -f $(INSTALL_EXE) $(BACULA_BINARIES) $(DEPKGS_BINARIES) $(NONGCC_BINARIES) $(NONGCC_LIBRARIES) $(SSL_FILES) $(DIRD_FILES)
	$(CMD_ECHO)-rm -f $(BACULA_BINARIES) $(addsuffix .dbg,$(basename $(BACULA_BINARIES)))
	$(CMD_ECHO)-rm -f $(DEPKGS_BINARIES) $(addsuffix .dbg,$(basename $(DEPKGS_BINARIES)))
	$(CMD_ECHO)-rm -f *.exe

#
# Rules
#

define Convert_Binary
$$(notdir $(1)): $(1)
	$(ECHO_CMD)cp -f $$^ $$@ ; \
	$(STAB2CV) $$@
endef

define Copy_Binary
$$(notdir $(1)): $(1)
	$(ECHO_CMD)cp -f $$^ $$@
endef

$(foreach file,$(addprefix $(DEPKGS)/bin/, $(DEPKGS_BINARIES)),$(eval $(call Convert_Binary,$(file))))

$(foreach file,$(addprefix $(DEPKGS)/bin/, $(NONGCC_BINARIES)),$(eval $(call Copy_Binary,$(file))))

$(foreach file,$(addprefix $(DEPKGS)/lib/, $(NONGCC_LIBRARIES)),$(eval $(call Copy_Binary,$(file))))

$(foreach file,$(addprefix $(BINDIR)/, $(BACULA_BINARIES)),$(eval $(call Convert_Binary,$(file))))

$(foreach file,$(addprefix $(DEPKGS)/ssl/, $(SSL_FILES)),$(eval $(call Copy_Binary,$(file))))

$(foreach file,$(addprefix ../../dird/, $(DIRD_FILES)),$(eval $(call Copy_Binary,$(file))))

$(INSTALL_EXE): winbacula.nsi $(BACULA_BINARIES) $(SCRIPTS) $(DEPKGS_BINARIES) $(NONGCC_BINARIES) $(NONGCC_LIBRARIES) $(SSL_FILES) $(DIRD_FILES)
	NSISDIR=$(NSIS_DIR) \
	$(NSIS_DIR)/makensis -V3 $(DEFINES) winbacula.nsi

include ../Makefile.rules
