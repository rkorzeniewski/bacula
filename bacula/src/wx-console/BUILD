General compiling notes:
 - Fetch latest bacula sources from CVS or bacula-1.34.0 source
   tarball, configure and build it.
 - Extract bacula-wx-gui.tar.gz in bacula/src directory.

Windows/cygwin compiling notes:
 - Build and install wxWindows from sources
   (http://www.wxwindows.org/).  I'm not sure it's necessary but
   this is how I get the thing to work.
 - In bacula/src/wx-console/Makefile.win, modify every
   occurrence of "J:/cygwin" with the path where cygwin is install
   on your system
 ~ Build bacula-wx-gui with "make -f Makefile.win" in bacula/src/wx-console

GTK/Linux compiling notes:
 - Comment out "new" and "delete" operator definitions in
   bacula/src/lib/smartall.c (lines 501-511), because these
   overloads seem to produce segmentation faults with wxWindows, and
   do "make" again in bacula/src/lib
 - Build bacula-wx-gui with "make all" in bacula/src/wx-console
