#
# Simple Makefile for building test plugin-fds for Bacula
#

# No optimization for now for easy debugging
CC = g++ -g -O0 -Wall

.SUFFIXES:    .c .o
.c.o:
	$(CC) -I../.. -DTEST_PROGRAM -c $<

all: main plugin-fd.so 

main: main.o plugin-fd.h
	$(CC) -L../../lib main.o -o main -lbac -lpthread -lssl -l crypto -ldl

plugin-fd.o: plugin-fd.c plugin-fd.h
	$(CC) -fPIC -I../.. -c plugin-fd.c

plugin-fd.so: plugin-fd.o
	$(CC) -shared plugin-fd.o -o plugin-fd.so

clean:
	rm -f main *.so *.o
