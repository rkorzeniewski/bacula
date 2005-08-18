/*
 *
 *    Interaction thread between director and the GUI
 *
 *    Nicolas Boichat, April 2004
 *
 *    Version $Id$
 */
/*
   Copyright (C) 2004-2005 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

// http://66.102.9.104/search?q=cache:Djc1mPF3hRoJ:cvs.sourceforge.net/viewcvs.py/audacity/audacity-src/src/AudioIO.cpp%3Frev%3D1.102+macos+x+wxthread&hl=fr

/* Note: this is the only source file in src/wx-console which uses the
 * standard gettext macros. So every translated string passed to wxWidgets
 * must be converted. (wxString(_("..."),wxConvUTF8)) */

#include "console_thread.h" // class's header file

#include <wx/wxprec.h>

#include <wx/thread.h>
#include <wx/file.h>
#include <bacula.h>
#include <jcr.h>

#include "console_conf.h"

#include "csprint.h"

#ifdef HAVE_WIN32
#include <windows.h>
char OK_msg[]   = "2000 OK\n";
char TERM_msg[] = "2999 Terminate\n";
#endif

/* Imported functions */
int authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons);

bool console_thread::inited = false;
bool console_thread::configloaded = false;
wxString console_thread::working_dir = wxT(".");

int numdir = 0;

/*
 * Call-back for reading a passphrase for an encrypted PEM file
 * This function uses getpass(), which uses a static buffer and is NOT thread-safe.
 */
static int tls_pem_callback(char *buf, int size, const void *userdata)
{
#ifdef HAVE_TLS
   const char *prompt = (const char *) userdata;
   char *passwd;

   passwd = getpass(prompt);
   bstrncpy(buf, passwd, size);
   return (strlen(buf));
#else
   buf[0] = 0;
   return 0;
#endif
}


/*
 * Make a quick check to see that we have all the
 * resources needed.
 */
static int check_resources()
{
   int xOK = true;
   DIRRES *director;

   LockRes();

   numdir = 0;
   foreach_res(director, R_DIRECTOR) {
      numdir++;
      /* tls_require implies tls_enable */
      if (director->tls_require) {
         if (have_tls) {
            director->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bacula.\n"));
            xOK = false;
            continue;
         }
      }

      if ((!director->tls_ca_certfile && !director->tls_ca_certdir) && director->tls_enable) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\""
                             " or \"TLS CA Certificate Dir\" are defined for Director \"%s\" in config file.\n"
                             " At least one CA certificate store is required.\n"),
                             director->hdr.name);
         xOK = false;
      }
   }
   
   if (numdir == 0) {
      Jmsg(NULL, M_FATAL, 0, _("No Director resource defined in config file.\n"
                          "Without that I don't how to speak to the Director :-(\n"));
      xOK = false;
   }

   CONRES *cons;
   /* Loop over Consoles */
   foreach_res(cons, R_CONSOLE) {
      /* tls_require implies tls_enable */
      if (cons->tls_require) {
         if (have_tls) {
            cons->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bacula.\n"));
            xOK = false;
            continue;
         }
      }

      if ((!cons->tls_ca_certfile && !cons->tls_ca_certdir) && cons->tls_enable) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\""
                             " or \"TLS CA Certificate Dir\" are defined for Console \"%s\" in config file.\n"),
                             cons->hdr.name);
         xOK = false;
      }
   }
   UnlockRes();
   return xOK;
}


void console_thread::SetWorkingDirectory(wxString w_dir) {
   if ((w_dir.Last() == '/') || (w_dir.Last() == '\\')) {
      console_thread::working_dir = w_dir.Mid(0, w_dir.Length()-1);
   }
   else {
      console_thread::working_dir = w_dir;
   }
}

void console_thread::InitLib() {
   if (WSA_Init() != 0) {
      csprint(_("Error while initializing windows sockets...\n"));
      inited = false;
      return;
   }
   
   init_stack_dump();
   my_name_is(0, NULL, "wx-console");
   working_directory = (const char*) console_thread::working_dir.GetData();
   
   inited = true;
}

void console_thread::FreeLib() {
   if (inited) {
      if (WSACleanup() != 0) {
         csprint(_("Error while cleaning up windows sockets...\n"));
      }
   }
}

wxString errmsg;

/*
 * Format a scanner error message
 */
static void scan_err(const char *file, int line, LEX *lc, const char *msg, ...)
{
   va_list arg_ptr;
   char buf[MAXSTRING];
   char more[MAXSTRING];
   wxString err;
   
   va_start(arg_ptr, msg);
   bvsnprintf(buf, sizeof(buf), msg, arg_ptr);
   va_end(arg_ptr);

   if (lc->line_no > lc->begin_line_no) {
      bsnprintf(more, sizeof(more),
                _("Problem probably begins at line %d.\n"), lc->begin_line_no);
   } else {
      more[0] = 0;
   }

   err.Format(wxString(_("Config error: %s\n            : line %d, col %d of file %s\n%s\n%s"), wxConvUTF8),
      buf, lc->line_no, lc->col_no, lc->fname, lc->line, more);
     
   errmsg << err; 
}

wxString console_thread::LoadConfig(wxString configfile) {
   if (!inited) {
      InitLib();
      if (!inited)
         return wxString(_("Error while initializing library."), wxConvUTF8);
   }
   
   free_config_resources();
   
   MSGS* msgs = (MSGS *)malloc(sizeof(MSGS));
   memset(msgs, 0, sizeof(MSGS));
   for (int i=1; i<=M_MAX; i++) {
#ifndef WIN32
      add_msg_dest(msgs, MD_STDOUT, i, NULL, NULL);
#endif
//    add_msg_dest(msgs, MD_SYSLOG, i, NULL, NULL);
      add_msg_dest(msgs, MD_CONSOLE, i, NULL, NULL);
   }
   
   init_msg(NULL, msgs);
   init_console_msg(console_thread::working_dir.mb_str(*wxConvCurrent));

   errmsg = wxT("");
   if (!parse_config(configfile.mb_str(*wxConvCurrent), &scan_err)) {
      configloaded = false;
      term_msg();
      return errmsg;
   }
   
   if (init_tls() != 0) {
      Jmsg(NULL, M_ERROR_TERM, 0, _("TLS library initialization failed.\n"));
   }

   if (!check_resources()) {
      Jmsg(NULL, M_ERROR_TERM, 0, _("Please correct configuration file.\n"));
   }

   term_msg();
   wxRemoveFile(console_thread::working_dir + wxT("/wx-console.conmsg"));
   init_msg(NULL, NULL);
   
   configloaded = true;
   
   return wxT("");
}

// class constructor
console_thread::console_thread() {
   UA_sock = NULL;
   choosingdirector = false;
}

// class destructor
console_thread::~console_thread() {
   if (UA_sock) {
      bnet_sig(UA_sock, BNET_TERMINATE); /* send EOF */
      bnet_close(UA_sock);
      UA_sock = NULL;
   }
}

/*
 * Thread entry point
 */
void* console_thread::Entry() {
   DIRRES* dir;
   if (!inited) {
      csprint(_("Error : Library not initialized\n"));
      csprint(NULL, CS_END);
      csprint(NULL, CS_DISCONNECTED);
      csprint(NULL, CS_TERMINATED);
      #ifdef HAVE_WIN32
         Exit();
      #endif
      return NULL;
   }
   
   if (!configloaded) {
      csprint(_("Error : No configuration file loaded\n"));
      csprint(NULL, CS_END);
      csprint(NULL, CS_DISCONNECTED);
      csprint(NULL, CS_TERMINATED);
      #ifdef HAVE_WIN32
         Exit();
      #endif
      return NULL;
   }
   
   csprint(_("Connecting...\n"));
  
   int count = 0;
   DIRRES* res[16]; /* Maximum 16 directors */
   
   LockRes();
   foreach_res(dir, R_DIRECTOR) {
      res[count] = dir;
      count++;
      if (count == 16) {
         break;
      }
   }
   UnlockRes();
   
   if (count == 0) {
      csprint(_("Error : No director defined in config file.\n"));
      csprint(NULL, CS_END);
      csprint(NULL, CS_DISCONNECTED);
      csprint(NULL, CS_TERMINATED);
      #ifdef HAVE_WIN32
         Exit();
      #endif
      return NULL;
   } else if (count == 1) {
      directorchoosen = 1;
   } else {
      while (true) {
         csprint(_("Multiple directors found in your config file.\n"));
         for (int i = 0; i < count; i++) {
            if (i < 9) {
               csprint(wxString(wxT("    ")) << (i+1) << wxT(": ") << wxString(res[i]->hdr.name,*wxConvCurrent) << wxT("\n"));
            }
            else {
               csprint(wxString(wxT("   ")) <<  (i+1) << wxT(": ") << wxString(res[i]->hdr.name,*wxConvCurrent) << wxT("\n"));
            }
         }
         csprint(wxString::Format(wxString(_("Please choose a director (1-%d): "), wxConvUTF8), count), CS_DATA);
         csprint(NULL, CS_PROMPT);
         choosingdirector = true;
         directorchoosen = -1;
         while(directorchoosen == -1) {
            bmicrosleep(0, 2000);
            Yield();
         }      
         choosingdirector = false;
         if (directorchoosen != 0) {
            break;
         }
      }
   }
   dir = res[directorchoosen-1];

   memset(&jcr, 0, sizeof(jcr));
   
   jcr.dequeuing = 1; /* TODO: catch messages */

   LockRes();
   /* If cons==NULL, default console will be used */
   CONRES *cons = (CONRES *)GetNextRes(R_CONSOLE, (RES *)NULL);
   UnlockRes();

   char buf[1024];
   /* Initialize Console TLS context */
   if (cons && (cons->tls_enable || cons->tls_require)) {
      /* Generate passphrase prompt */
      bsnprintf(buf, sizeof(buf), _("Passphrase for Console \"%s\" TLS private key: "), cons->hdr.name);

      /* Initialize TLS context:
       * Args: CA certfile, CA certdir, Certfile, Keyfile,
       * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
      cons->tls_ctx = new_tls_context(cons->tls_ca_certfile,
         cons->tls_ca_certdir, cons->tls_certfile,
         cons->tls_keyfile, tls_pem_callback, &buf, NULL, true);

      if (!cons->tls_ctx) {
         bsnprintf(buf, sizeof(buf), _("Failed to initialize TLS context for Console \"%s\".\n"),
            dir->hdr.name);
         csprint(buf);
         return NULL;
      }

   }

   /* Initialize Director TLS context */
   if (dir->tls_enable || dir->tls_require) {
      /* Generate passphrase prompt */
      bsnprintf(buf, sizeof(buf), _("Passphrase for Director \"%s\" TLS private key: "), dir->hdr.name);

      /* Initialize TLS context:
       * Args: CA certfile, CA certdir, Certfile, Keyfile,
       * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
      dir->tls_ctx = new_tls_context(dir->tls_ca_certfile,
         dir->tls_ca_certdir, dir->tls_certfile,
         dir->tls_keyfile, tls_pem_callback, &buf, NULL, true);

      if (!dir->tls_ctx) {
         bsnprintf(buf, sizeof(buf), _("Failed to initialize TLS context for Director \"%s\".\n"),
            dir->hdr.name);
         csprint(buf);
         return NULL;
      }
   }


   UA_sock = bnet_connect(&jcr, 3, 3, _("Director daemon"),
      dir->address, NULL, dir->DIRport, 0);
      
   if (UA_sock == NULL) {
      csprint(_("Failed to connect to the director\n"));
      csprint(NULL, CS_END);
      csprint(NULL, CS_DISCONNECTED);
      csprint(NULL, CS_TERMINATED);
      #ifdef HAVE_WIN32
         Exit();
      #endif
      return NULL;
   }

   csprint(_("Connected\n"));

   jcr.dir_bsock = UA_sock;
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
   
   Write(".messages\n");

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
            csprint(_("<< Heartbeat signal received, answered. >>\n"), CS_DEBUG);
         }
         else {
            csprint(_("<< Unexpected signal received : "), CS_DEBUG);
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

   csprint(_("Connection terminated\n"));
   
   UA_sock = NULL;

   csprint(NULL, CS_TERMINATED);

   #ifdef HAVE_WIN32
      Exit();
   #endif
   
   return NULL;
}

void console_thread::Write(const char* str) 
{
   if (UA_sock) {
       UA_sock->msglen = strlen(str);
       pm_strcpy(&UA_sock->msg, str);
       bnet_send(UA_sock);
   } else if (choosingdirector) {
//      wxString number = str;
//      number.RemoveLast(); /* Removes \n */
      long val;
      
//      if (number.ToLong(&val)) {
      val = atol(str);
      if (val) {
         directorchoosen = (int)val;
      } else {
         directorchoosen = 0;
      }
   }
}

void console_thread::Delete() {
   Write("quit\n");
   if (UA_sock) {
      bnet_sig(UA_sock, BNET_TERMINATE); /* send EOF */
      bnet_close(UA_sock);
      UA_sock = NULL;
      /*csprint(NULL, CS_END);
      csprint(NULL, CS_DISCONNECTED);
      csprint(NULL, CS_TERMINATED);*/
   }
}
