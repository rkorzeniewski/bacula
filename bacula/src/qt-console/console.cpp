/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
 
/*
 *  Connect to a Bacula daemon
 *
 *   Kern Sibbald, January MMVI
 *
 */ 

#include "bat.h"
#include "console.h"

/*
 * Connect to Director. If there are more than one, put up
 * a modal dialog so that the user chooses one.
 */
bool Console::connect(QWidget *textEdit)
{
   JCR jcr;

   m_textEdit = textEdit;

#ifdef xxx
   if (ndir > 1) {
      LockRes();
      foreach_res(dir, R_DIRECTOR) {
         dirs = g_list_append(dirs, dir->hdr.name);
      }
      UnlockRes();
      dir_dialog = create_SelectDirectorDialog();
      combo = lookup_widget(dir_dialog, "combo1");
      dir_select = lookup_widget(dir_dialog, "dirselect");
      if (dirs) {
         gtk_combo_set_popdown_strings(GTK_COMBO(combo), dirs);
      }
      gtk_widget_show(dir_dialog);
      gtk_main();

      if (reply == OK) {
         gchar *ecmd = gtk_editable_get_chars((GtkEditable *)dir_select, 0, -1);
         dir = (DIRRES *)GetResWithName(R_DIRECTOR, ecmd);
         if (ecmd) {
            g_free(ecmd);             /* release director name string */
         }
      }
      if (dirs) {
         g_free(dirs);
      }
      gtk_widget_destroy(dir_dialog);
      dir_dialog = NULL;
   } else {
#endif
      /* Just take the first Director */
      LockRes();
      m_dir = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
      UnlockRes();

   if (!m_dir) {
      return false;
   }

   memset(&jcr, 0, sizeof(jcr));

   set_statusf(_(" Connecting to Director %s:%d"), m_dir->address,dir->DIRport);
   set_textf(_("Connecting to Director %s:%d\n\n"), m_dir->address,dir->DIRport);

   
   LockRes();
   /* If cons==NULL, default console will be used */
   CONRES *cons = (CONRES *)GetNextRes(R_CONSOLE, (RES *)NULL);
   UnlockRes();

#ifdef xxx
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
         set_text(buf, strlen(buf));
         terminate_console(0);
         return true;
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
         set_text(buf, strlen(buf));
         terminate_console(0);
         return true;
      }
   }
#endif

   m_sock = bnet_connect(NULL, 5, 15, _("Director daemon"), m_dir->address,
                          NULL, m_dir->DIRport, 0);
   if (m__sock == NULL) {
      return false;
   }

   jcr.dir_bsock = m_sock;
   if (!authenticate_director(&jcr, m_dir, cons)) {
      set_text(m_sock->msg);
      return false;
   }

   set_status(_(" Initializing ..."));

   bnet_fsend(m_sock, "autodisplay on");

   /* Read and display all initial messages */
   while (bnet_recv(m_sock) > 0) {
      set_text(UA_sock->msg);
   }

   /* Give GUI a chance */
   app->processEvents();

#ifdef xxx
   /* Fill the run_dialog combo boxes */
   job_list      = get_and_fill_combo(run_dialog, "combo_job", ".jobs");
   client_list   = get_and_fill_combo(run_dialog, "combo_client", ".clients");
   fileset_list  = get_and_fill_combo(run_dialog, "combo_fileset", ".filesets");
   messages_list = get_and_fill_combo(run_dialog, "combo_messages", ".msgs");
   pool_list     = get_and_fill_combo(run_dialog, "combo_pool", ".pools");
   storage_list  = get_and_fill_combo(run_dialog, "combo_storage", ".storage");
   type_list     = get_and_fill_combo(run_dialog, "combo_type", ".types");
   level_list    = get_and_fill_combo(run_dialog, "combo_level", ".levels");

   /* Fill the label dialog combo boxes */
   fill_combo(label_dialog, "label_combo_storage", storage_list);
   fill_combo(label_dialog, "label_combo_pool", pool_list);


   /* Fill the restore_dialog combo boxes */
   fill_combo(restore_dialog, "combo_restore_job", job_list);
   fill_combo(restore_dialog, "combo_restore_client", client_list);
   fill_combo(restore_dialog, "combo_restore_fileset", fileset_list);
   fill_combo(restore_dialog, "combo_restore_pool", pool_list);
   fill_combo(restore_dialog, "combo_restore_storage", storage_list);
#endif

   set_status(_(" Connected"));
   return true;
}

#ifdef xxx
void write_director(const gchar *msg)
{
   if (UA_sock) {
      at_prompt = false;
      set_status(_(" Processing command ..."));
      UA_sock->msglen = strlen(msg);
      pm_strcpy(&UA_sock->msg, msg);
      bnet_send(UA_sock);
   }
   if (strcmp(msg, ".quit") == 0 || strcmp(msg, ".exit") == 0) {
      disconnect_from_director((gpointer)NULL);
      gtk_main_quit();
   }
}

extern "C"
void read_director(gpointer data, gint fd, GdkInputCondition condition)
{
   int stat;

   if (!UA_sock || UA_sock->fd != fd) {
      return;
   }
   stat = bnet_recv(UA_sock);
   if (stat >= 0) {
      if (at_prompt) {
         set_text("\n", 1);
         at_prompt = false;
      }
      set_text(UA_sock->msg, UA_sock->msglen);
      return;
   }
   if (is_bnet_stop(UA_sock)) {         /* error or term request */
      gtk_main_quit();
      return;
   }
   /* Must be a signal -- either do something or ignore it */
   if (UA_sock->msglen == BNET_PROMPT) {
      at_prompt = true;
      set_status(_(" At prompt waiting for input ..."));
   }
   if (UA_sock->msglen == BNET_EOD) {
      set_status_ready();
   }
   return;
}

static gint tag;

void start_director_reader(gpointer data)
{

   if (director_reader_running || !UA_sock) {
      return;
   }
   tag = gdk_input_add(UA_sock->fd, GDK_INPUT_READ, read_director, NULL);
   director_reader_running = true;
}

void stop_director_reader(gpointer data)
{
   if (!director_reader_running) {
      return;
   }
   gdk_input_remove(tag);
   director_reader_running = false;
}
#endif
