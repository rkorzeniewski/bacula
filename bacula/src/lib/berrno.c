/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *  Bacula errno handler
 *
 *    berrno is a simplistic errno handler that works for
 *      Unix, Win32, and Bacula bpipes.
 *
 *    See berrno.h for how to use berrno.
 *
 *   Written by Kern Sibbald, July MMIV
 *
 *
 */

#include "bacula.h"

#ifndef HAVE_WIN32
extern const char *get_signal_name(int sig);
extern int num_execvp_errors;
extern int execvp_errors[];
#endif

const char *berrno::bstrerror()
{
   *m_buf = 0;
#ifdef HAVE_WIN32
   if (m_berrno & (b_errno_win32)) {
      format_win32_message();
      return (const char *)m_buf;
   }
#else
   int stat = 0;

   if (m_berrno & b_errno_exit) {
      stat = (m_berrno & ~b_errno_exit);       /* remove bit */
      if (stat == 0) {
         return _("Child exited normally.");    /* this really shouldn't happen */
      } else {
         /* Maybe an execvp failure */
         if (stat >= 200) {
            if (stat < 200 + num_execvp_errors) {
               m_berrno = execvp_errors[stat - 200];
            } else {
               return _("Unknown error during program execvp");
            }
         } else {
            Mmsg(m_buf, _("Child exited with code %d"), stat);
            return m_buf;
         }
         /* If we drop out here, m_berrno is set to an execvp errno */
      }
   }
   if (m_berrno & b_errno_signal) {
      stat = (m_berrno & ~b_errno_signal);        /* remove bit */
      Mmsg(m_buf, _("Child died from signal %d: %s"), stat, get_signal_name(stat));
      return m_buf;
   }
#endif
   /* Normal errno */
   if (b_strerror(m_berrno, m_buf, sizeof_pool_memory(m_buf)) < 0) {
      return _("Invalid errno. No error message possible.");
   }
   return m_buf;
}

void berrno::format_win32_message()
{
#ifdef HAVE_WIN32
   LPVOID msg;
   FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
       FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
       NULL,
       GetLastError(),
       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
       (LPTSTR)&msg,
       0,
       NULL);
   pm_strcpy(&m_buf, (const char *)msg);
   LocalFree(msg);
#endif
}

#ifdef TEST_PROGRAM

int main()
{
}
#endif
