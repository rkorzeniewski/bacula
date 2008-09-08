#
# Simple Makefile for building test SD plugins for Bacula
#

# No optimization for now for easy debugging
CC = g++ -g -O0 -Wall

SDDIR=../../stored
SRCDIR=../..
LIBDIR=../../lib

.SUFFIXES:    .c .o
.c.o:
	$(CC) -I${SRCDIR} -I${SDDIR} -DTEST_PROGRAM -c $<

test: main example-plugin-sd.so

sd_plugins.o: ${SDDIR}/sd_plugins.h ${SDDIR}/sd_plugins.c
	$(CC)  -I${SRCDIR} -I${SDDIR} -DTEST_PROGRAM -c ${SDDIR}/sd_plugins.c 

main: sd_plugins.o
	$(CC) -L${LIBDIR} sd_plugins.o -o main -lbac -lpthread -lssl -l crypto -ldl

example-plugin-sd.o: example-plugin-sd.c ${SDDIR}/sd_plugins.h
	$(CC) -fPIC -I../.. -I${SDDIR} -c example-plugin-sd.c

example-plugin-sd.so: example-plugin-sd.o 
	$(CC) -shared example-plugin-sd.o -o example-plugin-sd.so

clean:
	rm -f main *.so *.o 1 2 3
