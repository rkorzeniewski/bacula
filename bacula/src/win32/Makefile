ECHO_CMD=@

DIRS=	dll \
	filed \
	dird \
	stored \
	console \
	wx-console \
	tools

.PHONY: $(DIRS) clean all

all: Makefile.inc $(DIRS)

clean: $(DIRS)
	$(ECHO_CMD)-rmdir release

$(DIRS):
	@if $(MAKE) -C $@ $(MAKECMDGOALS); then \
		echo -e "\n===== Make of $@ succeeded =====\n\n" ; \
	else \
		echo -e "\n!!!!! Make of $@ failed !!!!!\n\n" ; \
	fi

Makefile.inc: Makefile.inc.cross
	@echo Creating $@
	$(ECHO_CMD)sed -e "s^@TOPDIR@^`(cd ../../..;pwd)`^" < Makefile.inc.cross > $@
