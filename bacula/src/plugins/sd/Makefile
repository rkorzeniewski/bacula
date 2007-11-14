#
# Simple Makefile for building test plugin-sds for Bacula
#

# No optimization for now for easy debugging
CC = g++ -g -O0 -Wall

.SUFFIXES:    .c .o
.c.o:
	$(CC) -I../.. -DTEST_PROGRAM -c $<

all: main plugin-sd.so 

main: main.o plugin-sd.h
	$(CC) -L../../lib main.o -o main -lbac -lpthread -lssl -l crypto -ldl

plugin-sd.o: plugin-sd.c plugin-sd.h
	$(CC) -fPIC -I../.. -c plugin-sd.c

plugin-sd.so: plugin-sd.o
	$(CC) -shared plugin-sd.o -o plugin-sd.so

clean:
	rm -f main *.so *.o
