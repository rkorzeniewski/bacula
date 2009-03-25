ECHO_CMD=@

ifeq ($(WIN64),yes)
 DIRS=  lib \
        filed \
        filed/plugins \
        scripts
else
 DIRS=	lib \
	cats \
	filed \
	filed/plugins \
	dird \
	stored \
	console \
	wx-console \
	tools \
	scripts 
endif

.PHONY: $(DIRS) clean all Makefile.inc installer newinstaller

all: Makefile.inc $(DIRS) installer newinstaller


clean: $(DIRS) installer newinstaller
	$(ECHO_CMD)-rm -rf release

newinstaller:
	@if test -f Makefile.inc; then \
	   if $(MAKE) -C $@ $(MAKECMDGOALS); then \
		echo "\n===== Make of $@ succeeded =====\n\n" ; \
	   else  \
		echo "\n!!!!! Make of $@ failed !!!!!\n\n" ; \
	   fi ; \
	fi

installer:
	@if test -f Makefile.inc; then \
	   if $(MAKE) -C $@ $(MAKECMDGOALS); then \
		echo "\n===== Make of $@ succeeded =====\n\n" ; \
	   else  \
		echo "\n!!!!! Make of $@ failed !!!!!\n\n" ; \
	   fi ; \
	fi

$(DIRS):
	@if test -f Makefile.inc; then \
	   if $(MAKE) -C $@ $(MAKECMDGOALS); then \
		echo "\n===== Make of $@ succeeded =====\n\n" ; \
	   else  \
		echo "\n!!!!! Make of $@ failed !!!!!\n\n" ; \
	   fi ; \
	fi

Makefile.inc:
	@echo Creating $@
	$(ECHO_CMD)TOPDIR=`(cd ../../..;pwd)`; \
	if test "$(WIN64)" = yes -a -e ../../../cross-tools/mingw-w64/bin/x86_64-pc-mingw32-gcc; then \
		BINDIR=$${TOPDIR}/cross-tools/mingw-w64/bin; \
		INCDIR=$${TOPDIR}/cross-tools/mingw-w64/x86_64-pc-mingw32/include; \
		DLLDIR=$${TOPDIR}/cross-tools/mingw-w64/x86_64-pc-mingw32/bin; \
		DEPKGSDIR=depkgs-mingw-w64; \
		DEPKGSDIR32=depkgs-mingw32; \
	elif test -e ../../../cross-tools/mingw32/bin/mingw32-gcc; then \
		BINDIR=$${TOPDIR}/cross-tools/mingw32/bin; \
		INCDIR=$${TOPDIR}/cross-tools/mingw32/mingw32/include; \
		DLLDIR=$${TOPDIR}/cross-tools/mingw32/mingw32/bin; \
		DEPKGSDIR=depkgs-mingw32; \
		DEPKGSDIR32=depkgs-mingw32; \
	else \
		echo "\nThe GCC cross compiler isn't installed."; \
		echo "You must run build-win32-cross-tools and build-dependencies first.\n"; \
		exit 1; \
	fi ; \
	BUILDDIR=`(pwd)`; \
	MAINDIR=`(cd ../..;pwd)`; \
	sed \
		-e "s^@WIN64@^$${WIN64}^" \
		-e "s^@WIN32DEPKGS@^$${DEPKGSDIR}^" \
		-e "s^@WIN32DEPKGS32@^$${DEPKGSDIR32}^" \
		-e "s^@WIN32BUILDDIR@^$${BUILDDIR}^" \
		-e "s^@WIN32MAINDIR@^$${MAINDIR}^" \
		-e "s^@WIN32TOPDIR@^$${TOPDIR}^" \
		-e "s^@WIN32BINDIR@^$${BINDIR}^" \
		-e "s^@WIN32INCDIR@^$${INCDIR}^" \
		-e "s^@WIN32DLLDIR@^$${DLLDIR}^" < Makefile.inc.in > $@
