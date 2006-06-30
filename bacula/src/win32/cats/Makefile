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
	$(INCLUDE_OPENSSL) \
	$(INCLUDE_MYSQL)

DEFINES = \
	-DWIN32 \
	$(HAVES)

######################################################################

# Files in src/cats

LIB_OBJS = \
	$(OBJDIR)/bdb.o \
	$(OBJDIR)/bdb_create.o \
	$(OBJDIR)/bdb_delete.o \
	$(OBJDIR)/bdb_find.o \
	$(OBJDIR)/bdb_get.o \
	$(OBJDIR)/bdb_list.o \
	$(OBJDIR)/bdb_update.o \
	$(OBJDIR)/mysql.o \
	$(OBJDIR)/postgresql.o \
	$(OBJDIR)/sql.o \
	$(OBJDIR)/sql_create.o \
	$(OBJDIR)/sql_delete.o \
	$(OBJDIR)/sql_find.o \
	$(OBJDIR)/sql_get.o \
	$(OBJDIR)/sql_list.o \
	$(OBJDIR)/sql_update.o \
	$(OBJDIR)/sqlite.o

######################################################################

# Targets

.PHONY: all clean

all: $(LIBDIR)/libcats.a

clean:
	@echo "Cleaning `pwd`"
	$(ECHO_CMD)rm -f $(OBJDIR)/*.[od] $(LIBDIR)/libcats.a

#
# Rules
# 

$(LIBDIR)/libcats.a: $(LIB_OBJS)
	@echo "Updating archive $@"
	$(call checkdir,$@)
	$(ECHO_CMD)$(AR) rs $@ $^ $(LIB_OBJS)

include ../Makefile.rules

ifneq ($(MAKECMDGOALS),clean)
include $(patsubst %.o,%.d,$(filter-out %.res,$(LIB_OBJS)))
endif
