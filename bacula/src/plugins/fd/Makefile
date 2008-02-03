#
# Simple Makefile for building test plugin-fds for Bacula
#

# No optimization for now for easy debugging
CC = g++ -g -O0 -Wall

.SUFFIXES:    .c .o
.c.o:
	$(CC) -I../.. -I../../filed -DTEST_PROGRAM -c $<

all: fd-plugins example-plugin-fd.so 

fd-plugins: fd-plugins.o fd-plugins.h 
	$(CC) -L../../lib fd-plugins.o -o main -lbac -lpthread -lssl -l crypto -ldl

example-plugin-fd.o: example-plugin-fd.c ../../filed/fd-plugins.h
	$(CC) -fPIC -I../.. -c example-plugin-fd.c

example-plugin-fd.so: example-plugin-fd.o 
	$(CC) -shared example-plugin-fd.o -o example-plugin-fd.so


clean:
	rm -f main *.so *.o 1 2 3
