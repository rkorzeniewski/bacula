/*
 *  Bacula errno handler
 *
 *    berrno is a simplistic errno handler that works for
 *	Unix, Win32, and Bacula bpipes.
 *
 *    See berrno.h for how to use berrno.
 *
 *   Kern Sibbald, July MMIV
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2004 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"

const char *berrno::strerror()
{
#ifdef HAVE_WIN32
   LPVOID msg;

   if (errnum && b_errno_win32) {
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	  FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	  NULL,
	  GetLastError(),
	  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	  (LPTSTR)&msg;
	  0,
	  NULL);

      pm_strcpy(&buf_, msg);
      LocalFree(msg);
      return (const char *)buf_;
   }
#endif
   if (bstrerror(berrno_, buf_, 1024) < 0) {
      return "Invalid errno. No error message possible."; 
   }
   return (const char *)buf_;
}


#ifdef TEST_PROGRAM


struct FILESET {
   alist mylist;
};

int main()
{
   FILESET *fileset;
   char buf[30];
   alist *mlist;

   fileset = (FILESET *)malloc(sizeof(FILESET));
   memset(fileset, 0, sizeof(FILESET));
   fileset->mylist.init();

   printf("Manual allocation/destruction of list:\n");
   
   for (int i=0; i<20; i++) {
      sprintf(buf, "This is item %d", i);
      fileset->mylist.append(bstrdup(buf));
   } 
   for (int i=0; i< fileset->mylist.size(); i++) {
      printf("Item %d = %s\n", i, (char *)fileset->mylist[i]);  
   }
   fileset->mylist.destroy();
   free(fileset);

   printf("Allocation/destruction using new delete\n");
   mlist = new alist(10);

   for (int i=0; i<20; i++) {
      sprintf(buf, "This is item %d", i);
      mlist->append(bstrdup(buf));
   } 
   for (int i=0; i< mlist->size(); i++) {
      printf("Item %d = %s\n", i, (char *)mlist->get(i));  
   }

   delete mlist;


   sm_dump(false);

}
#endif
