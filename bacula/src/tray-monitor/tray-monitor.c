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
void writecmd(const char* command);

/* Static variables */
static char *configfile = NULL;
static BSOCK *FD_sock = NULL;
static MONITOR *monitor;
static POOLMEM *args;
static JCR jcr;
static CLIENT* filed;

/* UI variables and functions */
enum stateenum {
   error,
   idle,
   running,
   saving,
   warn
};

stateenum currentstatus = warn;

static gboolean fd_read(gpointer data);
void trayMessage(const char *fmt,...);
void changeIcon(stateenum status);
void writeToTextBuffer(GtkTextBuffer *buffer, const char *fmt,...);

/* Callbacks */
static void TrayIconActivate(GtkWidget *widget, gpointer data);
static gboolean delete_event(GtkWidget *widget, GdkEvent  *event, gpointer   data);

static gint timerTag;
static EggStatusIcon* mTrayIcon;
GtkWidget *window;
GtkWidget *textview;
GtkTextBuffer *buffer;

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
      exit(0);
   }
   
   (void)WSA_Init();			    /* Initialize Windows sockets */

   LockRes();
   monitor = (MONITOR*)GetNextRes(R_MONITOR, (RES *)NULL);
   UnlockRes();
   
   gtk_init (&argc, &argv);
   
   GdkPixbuf* pixbuf = gdk_pixbuf_new_from_xpm_data(xpm_warn);
   // This should be ideally replaced by a completely libpr0n-based icon rendering.
   mTrayIcon = egg_status_icon_new_from_pixbuf(pixbuf);
   g_signal_connect(G_OBJECT(mTrayIcon), "activate", G_CALLBACK(TrayIconActivate), NULL);
/*   g_signal_connect(G_OBJECT(mTrayIcon), "popup-menu", G_CALLBACK(TrayIconPopupMenu), this);*/
   g_object_unref(G_OBJECT(pixbuf));

   timerTag = g_timeout_add( 5000, fd_read, NULL );
        
   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   
   gtk_window_set_title(GTK_WINDOW(window), "Bacula tray monitor");
   
   g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event), NULL);
   //g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy), NULL);
   
   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
      
   textview = gtk_text_view_new();

   buffer = gtk_text_buffer_new(NULL);

   gtk_text_buffer_set_text(buffer, "", -1);

   PangoFontDescription *font_desc = pango_font_description_from_string ("Fixed 10");
   gtk_widget_modify_font(textview, font_desc);
   pango_font_description_free (font_desc);
   
   gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
   
   gtk_text_view_set_buffer(GTK_TEXT_VIEW(textview), buffer);
      
   gtk_container_add(GTK_CONTAINER (window), textview);
   
   gtk_widget_show(textview);
   
   fd_read(NULL);
   
   gtk_main();
      
   if (FD_sock) {
      writecmd("status");
      bnet_sig(FD_sock, BNET_TERMINATE); /* send EOF */
      bnet_close(FD_sock);
   }

   free_pool_memory(args);
   (void)WSACleanup();		     /* Cleanup Windows sockets */
   return 0;
}

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data ) {
   gtk_widget_hide(window);
   return TRUE; /* do not destroy the window */
}

static void TrayIconActivate(GtkWidget *widget, gpointer data) {
    gtk_widget_show(window);
}

static gboolean fd_read(gpointer data) {
   int stat;
   int statuschanged = 0;
   GtkTextBuffer *newbuffer = gtk_text_buffer_new(NULL);
   GtkTextIter start, stop, nstart, nstop;
   
   gtk_text_buffer_set_text (newbuffer, "", -1);
      
   if (!FD_sock) {
      LockRes();
      filed = (CLIENT*)GetNextRes(R_CLIENT, NULL);
      UnlockRes();
   
      memset(&jcr, 0, sizeof(jcr));
            
      writeToTextBuffer(newbuffer, "Connecting to Client %s:%d\n", filed->address, filed->FDport);
      FD_sock = bnet_connect(NULL, 3, 3, "File daemon", filed->address, 
               NULL, filed->FDport, 0);
      if (FD_sock == NULL) {
         changeIcon(error);
         return 1;
      }
      jcr.file_bsock = FD_sock;
      
      if (!authenticate_file_daemon(&jcr, monitor, filed)) {
         writeToTextBuffer(newbuffer, "ERR=%s", FD_sock->msg);
         FD_sock = NULL;
         changeIcon(error);
         return 0;
      }
   
      trayMessage("Opened connection with File daemon");
   }
      
   writecmd("status");
   
   while(1) {
      if ((stat = bnet_recv(FD_sock)) >= 0) {
         writeToTextBuffer(newbuffer, FD_sock->msg);
         if (strstr(FD_sock->msg, " is running.") != NULL) {
            changeIcon(running);
            statuschanged = 1;
         }
         else if (strstr(FD_sock->msg, "No Jobs running.") != NULL) {
            changeIcon(idle);
            statuschanged = 1;
         }
      }
      else if (stat == BNET_SIGNAL) {
         if (FD_sock->msglen == BNET_EOD) {
            if (statuschanged == 0) {
               changeIcon(warn);
            }
            break;
         }
         else if (FD_sock->msglen == BNET_HEARTBEAT) {
            bnet_sig(FD_sock, BNET_HB_RESPONSE);
            writeToTextBuffer(newbuffer, "<< Heartbeat signal received, answered. >>");
         }
         else {
            writeToTextBuffer(newbuffer, "<< Unexpected signal received : %s >>", bnet_sig_to_ascii(FD_sock));
         }
      }
      else { /* BNET_HARDEOF || BNET_ERROR */
         writeToTextBuffer(newbuffer, "<ERROR>\n");
         FD_sock = NULL;
         changeIcon(error);
         break;
      }
           
      if (is_bnet_stop(FD_sock)) {
         writeToTextBuffer(newbuffer, "<STOP>\n");
         FD_sock = NULL;
         changeIcon(error);
         break;            /* error or term */
      }
   }
   
   /* Keep the selection if necessary */
   if (gtk_text_buffer_get_selection_bounds(buffer, &start, &stop)) {
      gtk_text_buffer_get_iter_at_offset(newbuffer, &nstart, gtk_text_iter_get_offset(&start));
      gtk_text_buffer_get_iter_at_offset(newbuffer, &nstop,  gtk_text_iter_get_offset(&stop ));
      gtk_text_buffer_select_range(newbuffer, &nstart, &nstop);
   }

   buffer = newbuffer;
   gtk_text_view_set_buffer(GTK_TEXT_VIEW(textview), buffer);
      
   return 1;
}

void writecmd(const char* command) {
   if (FD_sock) {
      FD_sock->msglen = strlen(command);
      pm_strcpy(&FD_sock->msg, command);
      bnet_send(FD_sock);
   }
}

/* Note: Does not seem to work either on Gnome nor KDE... */
void trayMessage(const char *fmt,...) {
   char buf[3000];
   va_list arg_ptr;
   
   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), (char *)fmt, arg_ptr);
   va_end(arg_ptr);
   
   egg_tray_icon_send_message(egg_status_icon_get_tray_icon(mTrayIcon), 5000, (const char*)&buf, -1);
}

void changeIcon(stateenum status) {
   if (status == currentstatus)
      return;

   const char** xpm;

   switch (status) {
   case error:
      xpm = (const char**)&xpm_error;
      break;
   case idle:
      xpm = (const char**)&xpm_idle;
      break;
   case running:
      xpm = (const char**)&xpm_running;
      break;
   case saving:
      xpm = (const char**)&xpm_saving;
      break;
   case warn:
      xpm = (const char**)&xpm_warn;
      break;
   default:
      xpm = NULL;
      break;
   }
   
   GdkPixbuf* pixbuf = gdk_pixbuf_new_from_xpm_data(xpm);
   // This should be ideally replaced by a completely libpr0n-based icon rendering.
   egg_status_icon_set_from_pixbuf(mTrayIcon, pixbuf);
   
   currentstatus = status;
}

void writeToTextBuffer(GtkTextBuffer *buffer, const char *fmt,...) {
   char buf[3000];
   va_list arg_ptr;
   GtkTextIter iter;
   
   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), (char *)fmt, arg_ptr);
   va_end(arg_ptr);
   
   gtk_text_buffer_get_end_iter(buffer, &iter);
   gtk_text_buffer_insert(buffer, &iter, buf, -1);
}

