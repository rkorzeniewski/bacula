/*
 *
 *   Bacula Gnome Tray Monitor
 *
 *     Nicolas Boichat, August MMIV
 *
 *     Version $Id$
 */

/*
   Copyright (C) 2004 Kern Sibbald and John Walker

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
int authenticate_storage_daemon(JCR *jcr, MONITOR *monitor, STORE* store);

/* Forward referenced functions */
void writecmd(const char* command);

/* Static variables */
static char *configfile = NULL;
static BSOCK *D_sock = NULL;
static MONITOR *monitor;
static POOLMEM *args;
static JCR jcr;
static int nitems = 0;
static monitoritem items[32];
static monitoritem* currentitem;

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
static void TrayIconDaemonChanged(GtkWidget *widget, monitoritem* data);
static void TrayIconActivate(GtkWidget *widget, gpointer data);
static void TrayIconExit(unsigned int activateTime, unsigned int button);
static void TrayIconPopupMenu(unsigned int button, unsigned int activateTime);
static void MonitorAbout(GtkWidget *widget, gpointer data);
static gboolean delete_event(GtkWidget *widget, GdkEvent  *event, gpointer   data);

static gint timerTag;
static EggStatusIcon *mTrayIcon;
static GtkWidget *mTrayMenu;
static GtkWidget *window;
static GtkWidget *textview;
static GtkTextBuffer *buffer;

#define CONFIG_FILE "./tray-monitor.conf"   /* default configuration file */

static void usage()
{
   fprintf(stderr, _(
"Copyright (C) 2000-2004 Kern Sibbald and John Walker\n"
"Written by Nicolas Boichat (2004)\n"
"\nVersion: " VERSION " (" BDATE ") %s %s %s\n\n"
"Usage: tray-monitor [-c config_file] [-d debug_level] [-f filed | -s stored]\n"
"       -c <file>     set configuration file to file\n"
"       -dnn          set debug level to nn\n"
"       -t            test - read configuration and exit\n"
"       -f <filed>    monitor <filed>\n"
"       -s <stored>   monitor <stored>\n"
"       -?            print this message.\n"  
"\n"), HOST_OS, DISTNAME, DISTVER);
}

static GtkWidget *new_image_button(const gchar *stock_id, 
                                   const gchar *label_text) {
    GtkWidget *button;
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *image;

    button = gtk_button_new();
   
    box = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(box), 2);
    image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
    label = gtk_label_new(label_text);
    
    gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 3);

    gtk_widget_show(image);
    gtk_widget_show(label);

    gtk_widget_show(box);

    gtk_container_add(GTK_CONTAINER(button), box);
    
    return button;
}

/*********************************************************************
 *
 *	   Main Bacula Tray Monitor -- User Interface Program
 *
 */
int main(int argc, char *argv[])
{
   int ch;
   bool test_config = false;
   char* deffiled = NULL;
   char* defstored = NULL;
   CLIENT* filed;
   STORE* stored;

   init_stack_dump();
   my_name_is(argc, argv, "tray-monitor");
   textdomain("bacula");
   init_msg(NULL, NULL);
   working_directory = "/tmp";
   args = get_pool_memory(PM_FNAME);

   while ((ch = getopt(argc, argv, "bc:d:th?f:s:")) != -1) {
      switch (ch) {
      case 'c':                    /* configuration file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
         break;
         
      case 'f':
         if ((deffiled != NULL) || (defstored != NULL)) {
            fprintf(stderr, "Error: You can only use one -f <filed> or -s <stored> parameter\n");
            exit(1);
         }
         deffiled = bstrdup(optarg);
         break;
         
      case 's':
         if ((deffiled != NULL) || (defstored != NULL)) {
            fprintf(stderr, "Error: You can only use one -f <filed> or -s <stored> parameter\n");
            exit(1);
         }
         defstored = bstrdup(optarg);
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

      case 'h':
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

   currentitem = NULL;
   
   LockRes();
   nitems = 0;
   foreach_res(filed, R_CLIENT) {
      items[nitems].type = R_CLIENT;
      items[nitems].resource = filed;
      if ((deffiled != NULL) && (strcmp(deffiled, filed->hdr.name) == 0)) {
         currentitem = &(items[nitems]);
      }
      nitems++;
   }
   foreach_res(stored, R_STORAGE) {
      items[nitems].type = R_STORAGE;
      items[nitems].resource = stored;
      if ((defstored != NULL) && (strcmp(defstored, stored->hdr.name) == 0)) {
         currentitem = &(items[nitems]);
      }
      nitems++;
   }
   UnlockRes();
     
   if (nitems == 0) {
      Emsg1(M_ERROR_TERM, 0, _("No Client nor Storage resource defined in %s\n\
Without that I don't how to get status from the File or Storage Daemon :-(\n"), configfile);
   }
   
   if ((deffiled != NULL) || (defstored != NULL)) {
     if (currentitem == NULL) {
        fprintf(stderr, "Error: The file or storage daemon specified by parameters doesn't exists on your configuration file. Exiting...\n");
        exit(1);
     }
   }
   else {
      currentitem = &(items[0]);
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
   g_signal_connect(G_OBJECT(mTrayIcon), "popup-menu", G_CALLBACK(TrayIconPopupMenu), NULL);
   g_object_unref(G_OBJECT(pixbuf));

   mTrayMenu = gtk_menu_new();
   
   GtkWidget *entry;
   
   entry = gtk_menu_item_new_with_label("Open status window...");
   g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(TrayIconActivate), NULL);
   gtk_menu_shell_append(GTK_MENU_SHELL(mTrayMenu), entry);
   
   gtk_menu_shell_append(GTK_MENU_SHELL(mTrayMenu), gtk_separator_menu_item_new());
   
   GtkWidget *submenu = gtk_menu_new();
   
   entry = gtk_menu_item_new_with_label("Set monitored daemon");
   gtk_menu_item_set_submenu(GTK_MENU_ITEM(entry), submenu);
   gtk_menu_shell_append(GTK_MENU_SHELL(mTrayMenu), entry);
      
   GSList *group = NULL;
   
   GString *str;   
   for (int i = 0; i < nitems; i++) {
      switch (items[i].type) {
      case R_CLIENT:
         str = g_string_new(((CLIENT*)(items[i].resource))->hdr.name);
         g_string_append(str, " (FD)");
         break;
      case R_STORAGE:
         str = g_string_new(((STORE*)(items[i].resource))->hdr.name);
         g_string_append(str, " (SD)");
         break;
      default:
         continue;
      }
      entry = gtk_radio_menu_item_new_with_label(group, str->str);
      g_string_free(str, TRUE);
      
      group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(entry));
      if (currentitem == &(items[i])) {
         gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(entry), TRUE);
      }
      
      g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(TrayIconDaemonChanged), &(items[i]));
      gtk_menu_shell_append(GTK_MENU_SHELL(submenu), entry);         
   }
   
   gtk_menu_shell_append(GTK_MENU_SHELL(mTrayMenu), gtk_separator_menu_item_new());
   
   entry = gtk_menu_item_new_with_label("Exit");
   g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(TrayIconExit), NULL);
   gtk_menu_shell_append(GTK_MENU_SHELL(mTrayMenu), entry);
   
   gtk_widget_show_all(mTrayMenu);
   
   timerTag = g_timeout_add( 5000, fd_read, NULL );
      
   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   
   gtk_window_set_title(GTK_WINDOW(window), "Bacula tray monitor");
   
   g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(delete_event), NULL);
   //g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy), NULL);
   
   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   
   GtkWidget* vbox = gtk_vbox_new(FALSE, 10);
   
   textview = gtk_text_view_new();

   buffer = gtk_text_buffer_new(NULL);

   gtk_text_buffer_set_text(buffer, "", -1);

   PangoFontDescription *font_desc = pango_font_description_from_string ("Fixed 10");
   gtk_widget_modify_font(textview, font_desc);
   pango_font_description_free(font_desc);
   
   gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 20);
   gtk_text_view_set_right_margin(GTK_TEXT_VIEW(textview), 20);
   
   gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
   
   gtk_text_view_set_buffer(GTK_TEXT_VIEW(textview), buffer);
   
   gtk_box_pack_start(GTK_BOX(vbox), textview, TRUE, FALSE, 0);
      
   GtkWidget* hbox = gtk_hbox_new(FALSE, 10);
         
   GtkWidget* button = new_image_button("gtk-help", "About");
   g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(MonitorAbout), NULL);
   
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
   
   button = new_image_button("gtk-close", "Close");
   g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(gtk_widget_hide), G_OBJECT(window));
   
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
   
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
   
   gtk_container_add(GTK_CONTAINER (window), vbox);
   
   gtk_widget_show_all(vbox);
   
   fd_read(NULL);
   
   gtk_main();
      
   if (D_sock) {
      writecmd("quit");
      bnet_sig(D_sock, BNET_TERMINATE); /* send EOF */
      bnet_close(D_sock);
   }

   free_pool_memory(args);
   (void)WSACleanup();		     /* Cleanup Windows sockets */
   return 0;
}

static void MonitorAbout(GtkWidget *widget, gpointer data) {
#if HAVE_GTK_2_4
   GtkWidget* about = gtk_message_dialog_new_with_markup(GTK_WINDOW(window),GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _(
      "<span size='x-large' weight='bold'>Bacula Tray Monitor</span>\n\n"
      "Copyright (C) 2004 Kern Sibbald and John Walker\n"
      "Written by Nicolas Boichat\n"
      "\n<small>Version: " VERSION " (" BDATE ") %s %s %s</small>"
   ), HOST_OS, DISTNAME, DISTVER);
#else
   GtkWidget* about = gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _(
      "Bacula Tray Monitor\n\n"
      "Copyright (C) 2004 Kern Sibbald and John Walker\n"
      "Written by Nicolas Boichat\n"
      "\nVersion: " VERSION " (" BDATE ") %s %s %s"
   ), HOST_OS, DISTNAME, DISTVER); 
#endif
   gtk_dialog_run(GTK_DIALOG(about));
   gtk_widget_destroy(about);
}

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data ) {
   gtk_widget_hide(window);
   return TRUE; /* do not destroy the window */
}

static void TrayIconDaemonChanged(GtkWidget *widget, monitoritem* data) {
   if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) { // && (data != currentitem)
      printf("!!%s\n", ((STORE*)data->resource)->hdr.name);
      if (D_sock) {
         writecmd("quit");
         bnet_sig(D_sock, BNET_TERMINATE); /* send EOF */
         bnet_close(D_sock);
         D_sock = NULL;
      }
      currentitem = data;
      fd_read(NULL);
   }
}

static void TrayIconActivate(GtkWidget *widget, gpointer data) {
    gtk_widget_show(window);
}

static void TrayIconPopupMenu(unsigned int activateTime, unsigned int button) {
  gtk_menu_popup(GTK_MENU(mTrayMenu), NULL, NULL, NULL, NULL, 1, 0);
  gtk_widget_show_all(mTrayMenu);
}

static void TrayIconExit(unsigned int activateTime, unsigned int button) {
   gtk_main_quit();
}

static int authenticate_daemon(JCR *jcr) {
   switch (currentitem->type) {
   case R_CLIENT:
      return authenticate_file_daemon(jcr, monitor, (CLIENT*)currentitem->resource);
      break;
   case R_STORAGE:
      return authenticate_storage_daemon(jcr, monitor, (STORE*)currentitem->resource);
      break;
   default:
      printf("Error, currentitem is not a Client or a Storage..\n");
      gtk_main_quit();
      return FALSE;
   }
}

static gboolean fd_read(gpointer data) {
   int stat;
   int statuschanged = 0;
   GtkTextBuffer *newbuffer = gtk_text_buffer_new(NULL);
   GtkTextIter start, stop, nstart, nstop;
   
   gtk_text_buffer_set_text (newbuffer, "", -1);
      
   if (!D_sock) {
      memset(&jcr, 0, sizeof(jcr));
      
      CLIENT* filed;
      STORE* stored;
      
      switch (currentitem->type) {
      case R_CLIENT:
         filed = (CLIENT*)currentitem->resource;      
         trayMessage("Connecting to Client %s:%d\n", filed->address, filed->FDport);
         D_sock = bnet_connect(NULL, 3, 3, "File daemon", filed->address, NULL, filed->FDport, 0);
         jcr.file_bsock = D_sock;
         break;
      case R_STORAGE:
         stored = (STORE*)currentitem->resource;      
         trayMessage("Connecting to Storage %s:%d\n", stored->address, stored->SDport);
         D_sock = bnet_connect(NULL, 3, 3, "Storage daemon", stored->address, NULL, stored->SDport, 0);
         jcr.store_bsock = D_sock;
         break;
      default:
         printf("Error, currentitem is not a Client or a Storage..\n");
         gtk_main_quit();
         return FALSE;
      }
      
      if (D_sock == NULL) {
         writeToTextBuffer(newbuffer, "Cannot connect to daemon.\n");
         changeIcon(error);
         return 1;
      }
      
      if (!authenticate_daemon(&jcr)) {
         writeToTextBuffer(newbuffer, "ERR=%s\n", D_sock->msg);
         D_sock = NULL;
         changeIcon(error);
         return 0;
      }
   
      trayMessage("Opened connection with File daemon.\n");
   }
      
   writecmd("status");
   
   while(1) {
      if ((stat = bnet_recv(D_sock)) >= 0) {
         writeToTextBuffer(newbuffer, D_sock->msg);
         if (strstr(D_sock->msg, " is running.") != NULL) {
            changeIcon(running);
            statuschanged = 1;
         }
         else if (strstr(D_sock->msg, "No Jobs running.") != NULL) {
            changeIcon(idle);
            statuschanged = 1;
         }
      }
      else if (stat == BNET_SIGNAL) {
         if (D_sock->msglen == BNET_EOD) {
            if (statuschanged == 0) {
               changeIcon(warn);
            }
            break;
         }
         else if (D_sock->msglen == BNET_HEARTBEAT) {
            bnet_sig(D_sock, BNET_HB_RESPONSE);
            writeToTextBuffer(newbuffer, "<< Heartbeat signal received, answered. >>");
         }
         else {
            writeToTextBuffer(newbuffer, "<< Unexpected signal received : %s >>", bnet_sig_to_ascii(D_sock));
         }
      }
      else { /* BNET_HARDEOF || BNET_ERROR */
         writeToTextBuffer(newbuffer, "<ERROR>\n");
         D_sock = NULL;
         changeIcon(error);
         break;
      }
           
      if (is_bnet_stop(D_sock)) {
         writeToTextBuffer(newbuffer, "<STOP>\n");
         D_sock = NULL;
         changeIcon(error);
         break;            /* error or term */
      }
   }
   
   /* Keep the selection if necessary */
   if (gtk_text_buffer_get_selection_bounds(buffer, &start, &stop)) {
      gtk_text_buffer_get_iter_at_offset(newbuffer, &nstart, gtk_text_iter_get_offset(&start));
      gtk_text_buffer_get_iter_at_offset(newbuffer, &nstop,  gtk_text_iter_get_offset(&stop ));
      
#if HAVE_GTK_2_4
      gtk_text_buffer_select_range(newbuffer, &nstart, &nstop);
#else
      gtk_text_buffer_move_mark(newbuffer, gtk_text_buffer_get_mark(newbuffer, "insert"), &nstart);
      gtk_text_buffer_move_mark(newbuffer, gtk_text_buffer_get_mark(newbuffer, "selection_bound"), &nstop);
#endif
   }

   g_object_unref(buffer);
   
   buffer = newbuffer;
   gtk_text_view_set_buffer(GTK_TEXT_VIEW(textview), buffer);
      
   return 1;
}

void writecmd(const char* command) {
   if (D_sock) {
      D_sock->msglen = strlen(command);
      pm_strcpy(&D_sock->msg, command);
      bnet_send(D_sock);
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
   egg_status_icon_set_from_pixbuf(mTrayIcon, pixbuf);
   
   gtk_window_set_icon(GTK_WINDOW(window), pixbuf);
   
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

