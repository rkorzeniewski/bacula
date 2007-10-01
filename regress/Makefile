#
# Makefile for Bacula regression testing
#
# Before running, you must create a file named config containing
#   the configuration parameters.  Create it by doing:
# 
#  cp prototype.conf config
#
# Then edit config and set the value for what are appropriate for you.
#		 

first_rule: all

all:

setup: bacula sed

# 
# Some machines cannot handle the sticky bit and other garbage that
#  is in weird-files, so we load and run it only on Linux machines.
#
bacula: all
	@rm -rf bin build weird-files tmp
	@rm -f w.tar.gz w.tar
	@cp weird-files.tar.gz w.tar.gz
	@-gunzip w.tar.gz
	@-tar xf w.tar
	@rm -f w.tar.gz w.tar
	@rm -rf tmp working
	mkdir tmp working
	echo "Doing: scripts/setup"
	scripts/setup

sed:
	echo "Doing: scripts/do_sed"
	scripts/do_sed

# Run all non-root userid tests
test: 
	./all-non-root-tests

# run all file and tape tests
full_test:
	./all-tape-and-file-tests

# These tests require you to run as root
root_test:
	./all-root-tests

clean:
	scripts/cleanup
	rm -f tmp/file-list
	rm -fr tmp/* working/*
	rm -f test.out
	rm -f diff
	rm -f 1 2 3 scripts/1 scripts/2 scripts/3 tests/1 tests/2 tests/3
	find . -name .#* -exec rm -rf {} \;

# Reset our userid after running as root
reset:
	chown -R ${USER}:${USER} . tmp working
	scripts/cleanup
	rm -f tmp/* working/*

distclean: clean
	rm -rf bin build weird-files weird-files weird-files2 tmp working
	rm -f scripts/*.conf
		      
