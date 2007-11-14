#
# Simple Makefile for building test plugins for Bacula
#

# No optimization for now for easy debugging
CC = g++ -g -O0 -Wall

.SUFFIXES:    .c .o
.c.o:
	$(CC) -I../.. -DTEST_PROGRAM -c $<

all: main plugin-dir.so 

main: main.o plugin-dir.h
	$(CC) -L../../lib main.o -o main -lbac -lpthread -lssl -l crypto -ldl

plugin-dir.o: plugin-dir.c plugin-dir.h
	$(CC) -fPIC -I../.. -c plugin-dir.c

plugin-dir.so: plugin-dir.o
	$(CC) -shared plugin-dir.o -o plugin-dir.so

clean:
	rm -f main *.so *.o
