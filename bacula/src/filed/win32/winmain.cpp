/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

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

   This file is patterned after the VNC Win32 code by ATT
  
   Copyright (2000) Kern E. Sibbald
*/

#include <unistd.h>
#include <lmcons.h>
#include <ctype.h>
#include "winbacula.h"
#include "wintray.h"
#include "winservice.h"
#include <signal.h>
#include <pthread.h>

extern int BaculaMain(int argc, char **argv);
extern int terminate_filed(int sig);
extern DWORD g_error;
extern BOOL ReportStatus(DWORD state, DWORD exitcode, DWORD waithint);


HINSTANCE       hAppInstance;
const char      *szAppName = "Bacula";
DWORD           mainthreadId;

/* Imported variables */
extern DWORD    g_servicethread;

#define MAX_COMMAND_ARGS 100
static char *command_args[MAX_COMMAND_ARGS] = {"bacula-fd", NULL};
static int num_command_args = 1;
static pid_t main_pid;
static pthread_t main_tid;

/*
 * WinMain parses the command line and either calls the main App
 * routine or, under NT, the main service routine.
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   PSTR CmdLine, int iCmdShow)
{
   char *szCmdLine = CmdLine;
   char *wordPtr,*tempPtr;
   int i,quote;

   /* Save the application instance and main thread id */
   hAppInstance = hInstance;
   mainthreadId = GetCurrentThreadId();

   main_pid = getpid();
   main_tid = pthread_self();

   /*
    * Funny things happen with the command line if the
    * execution comes from c:/Program Files/bacula/bacula.exe
    * We get a command line like: Files/bacula/bacula.exe" options
    * I.e. someone stops scanning command line on a space, not
    * realizing that the filename is quoted!!!!!!!!!!
    * So if first character is not a double quote and
    * the last character before first space is a double
    * quote, we throw away the junk.
    */
   wordPtr = szCmdLine;
   while (*wordPtr && *wordPtr != ' ')
      wordPtr++;
   if (wordPtr > szCmdLine)      /* backup to char before space */
      wordPtr--;
   /* if first character is not a quote and last is, junk it */
   if (*szCmdLine != '"' && *wordPtr == '"')
      szCmdLine = wordPtr + 1;
   //      MessageBox(NULL, szCmdLine, "Cmdline", MB_OK);

   /* Build Unix style argc *argv[] */      

   /* Don't NULL command_args[0] !!! */
   for (i=1;i<MAX_COMMAND_ARGS;i++)
      command_args[i] = NULL;

   wordPtr = szCmdLine;
   quote = 0;
   while  (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
      wordPtr++;
   if (*wordPtr == '\"') {
      quote = 1;
      wordPtr++;
   } else if (*wordPtr == '/') {
      /* Skip Windows options */
      while (*wordPtr && (*wordPtr != ' ' && *wordPtr != '\t'))
         wordPtr++;
      while  (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
         wordPtr++;
   }
   if (*wordPtr) {
      while (*wordPtr && num_command_args < MAX_COMMAND_ARGS) {
         tempPtr = wordPtr;
         if (quote) {
            while (*tempPtr && *tempPtr != '\"')
            tempPtr++;
            quote = 0;
         } else {
            while (*tempPtr && *tempPtr != ' ')
            tempPtr++;
         }
         if (*tempPtr)
            *(tempPtr++) = '\0';
         command_args[num_command_args++] = wordPtr;
         wordPtr = tempPtr;
         while (*wordPtr && (*wordPtr == ' ' || *wordPtr == '\t'))
            wordPtr++;
         if (*wordPtr == '\"') {
            quote = 1;
            wordPtr++;
         }
      }
   }

   /*
    * Now process Windows command line options
    *   as defined by ATT
    *
    * Make the command-line lowercase and parse it
    */
   for (i = 0; i < (int)strlen(szCmdLine); i++) {
      szCmdLine[i] = tolower(szCmdLine[i]);
   }

   BOOL argfound = FALSE;
   for (i = 0; i < (int)strlen(szCmdLine); i++) {
      if (szCmdLine[i] <= ' ') {
         continue;
      }

      if (szCmdLine[i] == '-') {
         while (szCmdLine[i] && szCmdLine[i] != ' ') {
            i++;
         }
         continue;
      }

      argfound = TRUE;

      // Now check for command-line arguments

      // /servicehelper
      //  Used on NT to connect to Bacula
      if (strncmp(&szCmdLine[i], BaculaRunServiceHelper, strlen(BaculaRunServiceHelper)) == 0) {
         // NB : This flag MUST be parsed BEFORE "-service", otherwise it will match
         // the wrong option!  (This code should really be replaced with a simple
         // parser machine and parse-table...)

         // Run the Bacula Service Helper app
         bacService::PostUserHelperMessage();
         return 0;
      }
      // /service
      if (strncmp(&szCmdLine[i], BaculaRunService, strlen(BaculaRunService)) == 0) {
         // Run Bacula as a service
         return bacService::BaculaServiceMain();
      }
      // /run  (this is the default if no command line arguments)
      if (strncmp(&szCmdLine[i], BaculaRunAsUserApp, strlen(BaculaRunAsUserApp)) == 0) {
         // Bacula is being run as a user-level program
         return BaculaAppMain();
      }
      // /install
      if (strncmp(&szCmdLine[i], BaculaInstallService, strlen(BaculaInstallService)) == 0) {
         // Install Bacula as a service
         bacService::InstallService();
         i+=strlen(BaculaInstallService);
         continue;
      }
      // /remove
      if (strncmp(&szCmdLine[i], BaculaRemoveService, strlen(BaculaRemoveService)) == 0) {
         // Remove the Bacula service
         bacService::RemoveService();
         i+=strlen(BaculaRemoveService);
         continue;
      }

      // /about
      if (strncmp(&szCmdLine[i], BaculaShowAbout, strlen(BaculaShowAbout)) == 0) {
         // Show the About dialog of an existing instance of Bacula
         bacService::ShowAboutBox();
         i+=strlen(BaculaShowAbout);
         continue;
      }

      // /status
      if (strncmp(&szCmdLine[i], BaculaShowStatus, strlen(BaculaShowStatus)) == 0) {
         // Show the Status dialog of an existing instance of Bacula
         bacService::ShowStatus();
         i+=strlen(BaculaShowStatus);
         continue;
      }

      // /events
      if (strncmp(&szCmdLine[i], BaculaShowEvents, strlen(BaculaShowEvents)) == 0) {
         // Show the Events dialog of an existing instance of Bacula
         bacService::ShowEvents();
         i+=strlen(BaculaShowEvents);
         continue;
      }


      // /kill
      if (strncmp(&szCmdLine[i], BaculaKillRunningCopy, strlen(BaculaKillRunningCopy)) == 0) {
         // Kill any already running copy of Bacula
         bacService::KillRunningCopy();
         i+=strlen(BaculaKillRunningCopy);
         continue;
      }

      // /help
      if (strncmp(&szCmdLine[i], BaculaShowHelp, strlen(BaculaShowHelp)) == 0) {
         MessageBox(NULL, BaculaUsageText, "Bacula Usage", MB_OK | MB_ICONINFORMATION);
         i+=strlen(BaculaShowHelp);
         continue;
      }
      
      MessageBox(NULL, szCmdLine, "Bad Command Line Options", MB_OK);

      // Show the usage dialog
      MessageBox(NULL, BaculaUsageText, "Bacula Usage", MB_OK | MB_ICONINFORMATION);
      break;
   }

   // If no arguments were given then just run
   if (!argfound) {
      BaculaAppMain();
   }
   return 0;
}


/*
 * Called as a thread from BaculaAppMain()
 * Here we handle the Windows messages
 */
//DWORD WINAPI Main_Msg_Loop(LPVOID lpwThreadParam)
void *Main_Msg_Loop(LPVOID lpwThreadParam) 
{
   DWORD old_servicethread = g_servicethread;


   pthread_detach(pthread_self());

   /* Since we are the only thread with a message loop
    * mark ourselves as the service thread so that
    * we can receive all the window events.
    */
   g_servicethread = GetCurrentThreadId();

   // Create tray icon & menu if we're running as an app
   bacMenu *menu = new bacMenu();
   if (menu == NULL) {
//    log_error_message("Could not create sys tray menu");
      PostQuitMessage(0);
   }


   // Now enter the Windows message handling loop until told to quit!
   MSG msg;
   while (GetMessage(&msg, NULL, 0,0) ) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   if (menu != NULL) {
      delete menu;
   }

   if (old_servicethread != 0) { /* started as NT service */
      // Mark that we're no longer running
      g_servicethread = 0;

      // Tell the service manager that we've stopped.
      ReportStatus(SERVICE_STOPPED, g_error, 0);
   }   
   pthread_kill(main_tid, SIGTERM);   /* ask main thread to terminate */
   sleep(1);
   kill(main_pid, SIGTERM);           /* ask main thread to terminate */
   _exit(0);
}
 

/*
 * This is the main routine for Bacula when running as an application
 * (under Windows 95 or Windows NT)
 * Under NT, Bacula can also run as a service.  The BaculaServerMain routine,
 * defined in the bacService header, is used instead when running as a service.
 */
int BaculaAppMain()
{
// DWORD dwThreadID;
   pthread_t tid;

   // Set this process to be the last application to be shut down.
   SetProcessShutdownParameters(0x100, 0);

   HWND hservwnd = FindWindow(MENU_CLASS_NAME, NULL);
   if (hservwnd != NULL) {
      // We don't allow multiple instances!
      MessageBox(NULL, "Another instance of Bacula is already running", szAppName, MB_OK);
      _exit(0);
   }

   // Create a thread to handle the Windows messages
// (void)CreateThread(NULL, 0, Main_Msg_Loop, NULL, 0, &dwThreadID);
   pthread_create(&tid, NULL, Main_Msg_Loop, (void *)0);

   // Call the "real" Bacula
   BaculaMain(num_command_args, command_args);
   PostQuitMessage(0);
   _exit(0);
}
