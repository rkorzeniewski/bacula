#
# Simple Makefile for building test Director plugins for Bacula
#

# No optimization for now for easy debugging
CC = g++ -g -O0 -Wall

.SUFFIXES:    .c .o
.c.o:
	$(CC) -I../.. -DTEST_PROGRAM -c $<

all: dir-plugins example-plugin-dir.so 

dir-plugins: dir-plugins.o dir-plugins.h 
	$(CC) -L../../lib dir-plugins.o -o main -lbac -lpthread -lssl -l crypto -ldl

example-plugin-dir.o: example-plugin-dir.c dir-plugins.h
	$(CC) -fPIC -I../.. -c example-plugin-dir.c

example-plugin-dir.so: example-plugin-dir.o 
	$(CC) -shared example-plugin-dir.o -o example-plugin-dir.so


clean:
	rm -f main *.so *.o 1 2 3
