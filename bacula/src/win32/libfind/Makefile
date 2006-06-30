#
# Makefile for win32 bacula executables
# Using MinGW cross-compiler on GNU/Linux
#  
#  Written for Bacula by Howard Thomson, April 2006
# 

include ../Makefile.inc

INCLUDES = \
	$(INCLUDE_GCC) \
	$(INCLUDE_MINGW) \
	$(INCLUDE_PTHREADS) \
	$(INCLUDE_BACULA) \
	$(INCLUDE_ZLIB) \
	$(INCLUDE_VSS) \
	$(INCLUDE_ICONS) \
	$(INCLUDE_OPENSSL)

DEFINES = \
	-DWIN32 \
	$(HAVES)

######################################################################

# Files files in src/lib

LIB_OBJS = \
	$(OBJDIR)/attribs.o \
	$(OBJDIR)/bfile.o \
	$(OBJDIR)/create_file.o \
	$(OBJDIR)/drivetype.o \
	$(OBJDIR)/enable_priv.o \
	$(OBJDIR)/find.o \
	$(OBJDIR)/find_one.o \
	$(OBJDIR)/fstype.o \
	$(OBJDIR)/makepath.o \
	$(OBJDIR)/match.o \
	$(OBJDIR)/save-cwd.o

######################################################################

# Targets

.PHONY: all clean

all: $(LIBDIR)/libfind.a

clean:
	@echo "Cleaning `pwd`"
	$(ECHO_CMD)rm -f $(OBJDIR)/*.[od] $(LIBDIR)/libfind.a

#
# Rules for generating from ../lib
# 

$(LIBDIR)/libfind.a: $(LIB_OBJS)
	@echo "Updating archive $@"
	$(call checkdir,$@)
	$(ECHO_CMD)$(AR) rs $@ $^

include ../Makefile.rules

ifneq ($(MAKECMDGOALS),clean)
include $(patsubst %.o,%.d,$(filter-out %.res,$(LIB_OBJS)))
endif
