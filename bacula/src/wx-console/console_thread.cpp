/*
 *
 *    Interaction thread between director and the GUI
 *
 *    Nicolas Boichat, April 2004
 *
 */
/*
   Copyright (C) 2004 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "console_thread.h" // class's header file

#include <wx/wxprec.h>

#include <wx/thread.h>
#include <bacula.h>
#include <jcr.h>

#include "console_conf.h"

#include "csprint.h"

#ifdef HAVE_WIN32
#include <windows.h>
DWORD  g_platform_id = VER_PLATFORM_WIN32_WINDOWS;
char OK_msg[]   = "2000 OK\n";
char TERM_msg[] = "2999 Terminate\n";
#endif

#ifdef HAVE_MACOSX
#define CONFIGFILE "/Library/Preferences/org.bacula.wxconsole/wx-console.conf"
#else
#define CONFIGFILE "./wx-console.conf"
#endif

/* Imported functions */
int authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons);

// class constructor
console_thread::console_thread() {
   UA_sock = NULL;
}

// class destructor
console_thread::~console_thread() {
   if (UA_sock) {
      bnet_sig(UA_sock, BNET_TERMINATE); /* send EOF */
      bnet_close(UA_sock);
      UA_sock = NULL;
   }
   if (WSACleanup() == 0) {
      //csprint("Windows sockets cleaned up successfully...\n");
   }
   else {
      csprint("Error while cleaning up windows sockets...\n");
   }
}

/*
 * Thread entry point
 */
void* console_thread::Entry() {
   if (WSA_Init() == 0) {
      //csprint("Windows sockets initialized successfully...\n");
   }
   else {
      csprint("Error while initializing windows sockets...\n");
   }

   csprint("Connecting...\n");

   init_stack_dump();
   my_name_is(0, NULL, "wx-console");
   //textdomain("bacula-console");
   init_msg(NULL, NULL);

   /* TODO (#4#): Allow the user to choose his config file. */
   parse_config(CONFIGFILE);

   LockRes();
   DIRRES *dir = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   UnlockRes();

   memset(&jcr, 0, sizeof(jcr));
   
   jcr.dequeuing = 1; /* TODO: catch messages */

   UA_sock = bnet_connect(&jcr, 3, 3, "Director daemon", dir->address, NULL, dir->DIRport, 0);
   if (UA_sock == NULL) {
      csprint("Failed to connect to the director\n");
      csprint(NULL, CS_END);
      csprint(NULL, CS_DISCONNECTED);
      csprint(NULL, CS_TERMINATED);
      #ifdef HAVE_WIN32
         Exit();
      #endif
      return NULL;
   }

   csprint("Connected\n");

   jcr.dir_bsock = UA_sock;
   LockRes();
   /* If cons==NULL, default console will be used */
   CONRES *cons = (CONRES *)GetNextRes(R_CONSOLE, (RES *)NULL);
   UnlockRes();
   if (!authenticate_director(&jcr, dir, cons)) {
      csprint("ERR=");
      csprint(UA_sock->msg);
      csprint(NULL, CS_END);
      csprint(NULL, CS_DISCONNECTED);
      csprint(NULL, CS_TERMINATED);
      #ifdef HAVE_WIN32
         Exit();
      #endif
      return NULL;
   }
   
   csprint(NULL, CS_CONNECTED);
   
   Write("messages\n");

   int stat;

   /* main loop */
   while(!TestDestroy()) {   /* Tests if thread has been ended */
      if ((stat = bnet_recv(UA_sock)) >= 0) {
         csprint(UA_sock->msg);
      }
      else if (stat == BNET_SIGNAL) {
         if (UA_sock->msglen == BNET_PROMPT) {
            csprint(NULL, CS_PROMPT);
         }
         else if (UA_sock->msglen == BNET_EOD) {
            csprint(NULL, CS_END);
         }
         else if (UA_sock->msglen == BNET_HEARTBEAT) {
            bnet_sig(UA_sock, BNET_HB_RESPONSE);
            csprint("<< Heartbeat signal received, answered. >>\n", CS_DEBUG);
         }
         else {
            csprint("<< Unexpected signal received : ", CS_DEBUG);
            csprint(bnet_sig_to_ascii(UA_sock), CS_DEBUG);
            csprint(">>\n", CS_DEBUG);
         }
      }
      else { /* BNET_HARDEOF || BNET_ERROR */
         csprint(NULL, CS_END);
         break;
      }
           
      if (is_bnet_stop(UA_sock)) {
         csprint(NULL, CS_END);
         break;            /* error or term */
      }
   }
   
   csprint(NULL, CS_DISCONNECTED);

   csprint("Connection terminated\n");
   
   UA_sock = NULL;

   csprint(NULL, CS_TERMINATED);

   #ifdef HAVE_WIN32
      Exit();
   #endif
   
   return NULL;
}

void console_thread::Write(const char* str) {
   if (UA_sock) {
       UA_sock->msglen = strlen(str);
       pm_strcpy(&UA_sock->msg, str);
       bnet_send(UA_sock);
   }
}

void console_thread::Delete() {
   Write("quit\n");
   if (UA_sock) {
      bnet_sig(UA_sock, BNET_TERMINATE); /* send EOF */
      bnet_close(UA_sock);
      UA_sock = NULL;
      csprint(NULL, CS_END);
      csprint(NULL, CS_DISCONNECTED);
      csprint(NULL, CS_TERMINATED);
   }
}
