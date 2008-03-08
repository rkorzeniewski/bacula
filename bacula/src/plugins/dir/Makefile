#
# Simple Makefile for building test Dir plugins for Bacula
#

# No optimization for now for easy debugging
CC = g++ -g -O0 -Wall

DIRDIR=../../dird
SRCDIR=../..
LIBDIR=../../lib

.SUFFIXES:    .c .o
.c.o:
	$(CC) -I${SRCDIR} -I${DIRDIR} -DTEST_PROGRAM -c $<

test: main example-plugin-dir.so

dir-plugins.o: ${DIRDIR}/dir-plugins.h ${DIRDIR}/dir-plugins.c
	$(CC)  -I${SRCDIR} -I${DIRDIR} -DTEST_PROGRAM -c ${DIRDIR}/dir-plugins.c 

main: dir-plugins.o
	$(CC) -L${LIBDIR} dir-plugins.o -o main -lbac -lpthread -lssl -l crypto -ldl

example-plugin-dir.o: example-plugin-dir.c ${DIRDIR}/dir-plugins.h
	$(CC) -fPIC -I../.. -I${DIRDIR} -c example-plugin-dir.c

example-plugin-dir.so: example-plugin-dir.o 
	$(CC) -shared example-plugin-dir.o -o example-plugin-dir.so

clean:
	rm -f main *.so *.o 1 2 3
