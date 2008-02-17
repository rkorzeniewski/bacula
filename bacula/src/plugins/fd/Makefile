#
# Simple Makefile for building test plugin-fds for Bacula
#

# No optimization for now for easy debugging
CC = g++ -g -O0 -Wall

FDDIR=../../filed
SRCDIR=../..
LIBDIR=../../lib

.SUFFIXES:    .c .o
.c.o:
	$(CC) -I${SRCDIR} -I${FDDOR} -DTEST_PROGRAM -c $<

all: bpipe-fd.so

test: main example-plugin-fd.so

fd-plugins.o: ${FDDIR}/fd-plugins.h ${FDDIR}/fd-plugins.c
	$(CC)  -I${SRCDIR} -I${FDDIR} -DTEST_PROGRAM -c ${FDDIR}/fd-plugins.c 

main: fd-plugins.o
	$(CC) -L${LIBDIR} fd-plugins.o -o main -lbac -lpthread -lssl -l crypto -ldl

example-plugin-fd.o: example-plugin-fd.c ${FDDIR}/fd-plugins.h
	$(CC) -fPIC -I../.. -I${FDDIR} -c example-plugin-fd.c

example-plugin-fd.so: example-plugin-fd.o 
	$(CC) -shared example-plugin-fd.o -o example-plugin-fd.so

bpipe-fd.o: bpipe-fd.c ${FDDIR}/fd-plugins.h
	$(CC) -fPIC -I../.. -I${FDDIR} -c bpipe-fd.c

bpipe-fd.so: bpipe-fd.o 
	$(CC) -shared bpipe-fd.o -o bpipe-fd.so

clean:
	rm -f main *.so *.o 1 2 3
