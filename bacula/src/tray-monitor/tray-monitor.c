/*
 *
 *   Bacula Gnome Tray Monitor
 *
 *     Nicolas Boichat, August MMIV
 *
 *     Version $Id$
 */

/*
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"
#include "tray-monitor.h"

#include "eggstatusicon.h"
#include <gtk/gtk.h>

#include "idle.xpm"
#include "error.xpm"
#include "running.xpm"
#include "saving.xpm"
#include "warn.xpm"

/* Imported functions */
int authenticate_file_daemon(JCR *jcr, MONITOR *monitor, CLIENT* client);

/* Forward referenced functions */
static void terminate_console(int sig);
void writecmd(const char* command);

/* Static variables */
static char *configfile = NULL;
static BSOCK *FD_sock = NULL;
static MONITOR *monitor;
static POOLMEM *args;

/* UI variables and functions */
gboolean fd_read(gpointer data);

static EggStatusIcon* mTrayIcon;

#define CONFIG_FILE "./tray-monitor.conf"   /* default configuration file */

static void usage()
{
   fprintf(stderr, _(
"Copyright (C) 2000-2004 Kern Sibbald and John Walker\n"
"\nVersion: " VERSION " (" BDATE ") %s %s %s\n\n"
"Usage: tray-monitor [-s] [-c config_file] [-d debug_level]\n"
"       -c <file>   set configuration file to file\n"
"       -dnn        set debug level to nn\n"
"       -s          no signals\n"
"       -t          test - read configuration and exit\n"
"       -?          print this message.\n"  
"\n"), HOST_OS, DISTNAME, DISTVER);
}

/*********************************************************************
 *
 *	   Main Bacula Tray Monitor -- User Interface Program
 *
 */
int main(int argc, char *argv[])
{
   int ch, nfiled;
   bool test_config = false;
   JCR jcr;
   CLIENT* filed;

   init_stack_dump();
   my_name_is(argc, argv, "tray-monitor");
   textdomain("bacula");
   init_msg(NULL, NULL);
   working_directory = "/tmp";
   args = get_pool_memory(PM_FNAME);

   while ((ch = getopt(argc, argv, "bc:d:r:st?")) != -1) {
      switch (ch) {
      case 'c':                    /* configuration file */
	 if (configfile != NULL) {
	    free(configfile);
	 }
	 configfile = bstrdup(optarg);
	 break;

      case 'd':
	 debug_level = atoi(optarg);
	 if (debug_level <= 0) {
	    debug_level = 1;
	 }
	 break;

      case 't':
	 test_config = true;
	 break;

      case '?':
      default:
	 usage();
	 exit(1);
      }  
   }
   argc -= optind;
   argv += optind;

   if (argc) {
      usage();
      exit(1);
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   parse_config(configfile);

   LockRes();
   nfiled = 0;
   foreach_res(filed, R_CLIENT) {
      nfiled++;
   }
   UnlockRes();
   if (nfiled != 1) {
      Emsg1(M_ERROR_TERM, 0, _("No Client resource defined in %s (or more than one)\n\
Without that I don't how to get status from the Client :-(\n"), configfile);
   }

   if (test_config) {
      terminate_console(0);
      exit(0);
   }
   
   LockRes();
   filed = (CLIENT*)GetNextRes(R_CLIENT, NULL);
   UnlockRes();

   memset(&jcr, 0, sizeof(jcr));

   (void)WSA_Init();			    /* Initialize Windows sockets */
      
   printf(_("Connecting to Client %s:%d\n"), filed->address, filed->FDport);
   FD_sock = bnet_connect(NULL, 5, 15, "File daemon", filed->address, 
			  NULL, filed->FDport, 0);
   if (FD_sock == NULL) {
      terminate_console(0);
      return 1;
   }
   jcr.file_bsock = FD_sock;
   
   LockRes();
   monitor = (MONITOR*)GetNextRes(R_MONITOR, (RES *)NULL);
   UnlockRes();
   
   if (!authenticate_file_daemon(&jcr, monitor, filed)) {
      fprintf(stderr, "ERR=%s", FD_sock->msg);
      terminate_console(0);
      return 1;
   }

   printf("Opened connection with File daemon\n");

   writecmd("status");

   gtk_init (&argc, &argv);
   
   GdkPixbuf* pixbuf = gdk_pixbuf_new_from_xpm_data(xpm_idle);
   // This should be ideally replaced by a completely libpr0n-based icon rendering.
   mTrayIcon = egg_status_icon_new_from_pixbuf(pixbuf);
/*   g_signal_connect(G_OBJECT(mTrayIcon), "activate", G_CALLBACK(TrayIconActivate), this);
   g_signal_connect(G_OBJECT(mTrayIcon), "popup-menu", G_CALLBACK(TrayIconPopupMenu), this);*/
   g_object_unref(G_OBJECT(pixbuf));

   gtk_idle_add( fd_read, NULL );
      
   gtk_main();
   
   if (FD_sock) {
      bnet_sig(FD_sock, BNET_TERMINATE); /* send EOF */
      bnet_close(FD_sock);
   }

   terminate_console(0);
   return 0;
}

gboolean fd_read(gpointer data) {
   int stat;
   
   while(1) {
      if ((stat = bnet_recv(FD_sock)) >= 0) {
         printf(FD_sock->msg);
      }
      else if (stat == BNET_SIGNAL) {
         if (FD_sock->msglen == BNET_PROMPT) {
            printf("<PROMPT>");
         }
         else if (FD_sock->msglen == BNET_EOD) {
            printf("<END>");
            writecmd("status");
            sleep(1);
            return 1;
         }
         else if (FD_sock->msglen == BNET_HEARTBEAT) {
            bnet_sig(FD_sock, BNET_HB_RESPONSE);
            printf("<< Heartbeat signal received, answered. >>");
         }
         else {
            printf("<< Unexpected signal received : %s >>", bnet_sig_to_ascii(FD_sock));
         }
      }
      else { /* BNET_HARDEOF || BNET_ERROR */
         printf("<ERROR>");
         break;
      }
           
      if (is_bnet_stop(FD_sock)) {
         printf("<STOP>");
         break;            /* error or term */
      }
   }
   return 0;
}

/* Cleanup and then exit */
static void terminate_console(int sig)
{

   static bool already_here = false;

   if (already_here) {		      /* avoid recursive temination problems */
      exit(1);
   }
   already_here = true;
   free_pool_memory(args);
   (void)WSACleanup();		     /* Cleanup Windows sockets */
   if (sig != 0) {
      exit(1);
   }
   return;
}

void writecmd(const char* command) {
   FD_sock->msglen = strlen(command);
   pm_strcpy(&FD_sock->msg, command);
   bnet_send(FD_sock);
}
