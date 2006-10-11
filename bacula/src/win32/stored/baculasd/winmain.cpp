/*
   Copyright (C) 2000-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

   This file is patterned after the VNC Win32 code by ATT
  
*/

#include <unistd.h>
#include <ctype.h>

#include "bacula.h"
#include "winbacula.h"
#include "wintray.h"
#include "winservice.h"
#include <signal.h>
#include <pthread.h>

extern int BaculaMain(int argc, char *argv[]);
extern void terminate_stored(int sig);
extern DWORD g_error;
extern BOOL ReportStatus(DWORD state, DWORD exitcode, DWORD waithint);
extern void d_msg(const char *, int, int, const char *, ...);

/* Globals */
HINSTANCE       hAppInstance;
const char      *szAppName = "Bacula-sd";
DWORD           mainthreadId;
bool            silent = false;

/* Imported variables */
extern DWORD    g_servicethread;

#define MAX_COMMAND_ARGS 100
static char *command_args[MAX_COMMAND_ARGS] = {"bacula-sd", NULL};
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
   char *wordPtr, *tempPtr;
   int i, quote;
   DWORD dwCharsWritten;

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
   if (*szCmdLine != '"' && *wordPtr == '"') {
      szCmdLine = wordPtr + 1;
   }

   /* Build Unix style argc *argv[] */      

   /* Don't NULL command_args[0] !!! */
   for (i=1;i<MAX_COMMAND_ARGS;i++)
      command_args[i] = NULL;

   char *pszArgs = bstrdup(szCmdLine);
   wordPtr = pszArgs;
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
    */
   for (i = 0; i < (int)strlen(szCmdLine); i++) {
      if (szCmdLine[i] <= ' ') {
         continue;
      }

      if (szCmdLine[i] != '/') {
         break;
      }

      /* Now check for command-line arguments */

      /* /silent install quietly -- no prompts */
      if (strnicmp(&szCmdLine[i], BaculaSilent, sizeof(BaculaSilent) - 1) == 0) {
         silent = true;
         i += sizeof(BaculaSilent) - 1;
         continue;
      }

      /* /service start service */
      if (strnicmp(&szCmdLine[i], BaculaRunService, sizeof(BaculaRunService) - 1) == 0) {
         /* Run Bacula as a service */
         return bacService::BaculaServiceMain();
      }
      /* /run  (this is the default if no command line arguments) */
      if (strnicmp(&szCmdLine[i], BaculaRunAsUserApp, sizeof(BaculaRunAsUserApp) - 1) == 0) {
         /* Bacula is being run as a user-level program */
         if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            AllocConsole();
         }
         WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\r\n", 2, &dwCharsWritten, NULL);

         return BaculaAppMain();
      }
      /* /install */
      if (strnicmp(&szCmdLine[i], BaculaInstallService, sizeof(BaculaInstallService) - 1) == 0) {
         /* Install Bacula as a service */
         return bacService::InstallService(&szCmdLine[i + sizeof(BaculaInstallService) - 1]);
      }
      /* /remove */
      if (strnicmp(&szCmdLine[i], BaculaRemoveService, sizeof(BaculaRemoveService) - 1) == 0) {
         /* Remove the Bacula service */
         return bacService::RemoveService();
      }

      /* /about */
      if (strnicmp(&szCmdLine[i], BaculaShowAbout, sizeof(BaculaShowAbout) - 1) == 0) {
         /* Show Bacula's about box */
         return bacService::ShowAboutBox();
      }

      /* /status */
      if (strnicmp(&szCmdLine[i], BaculaShowStatus, sizeof(BaculaShowStatus) - 1) == 0) {
         /* Show Bacula's status box */                             
         return bacService::ShowStatus();
      }

      /* /kill */
      if (strnicmp(&szCmdLine[i], BaculaKillRunningCopy, sizeof(BaculaKillRunningCopy) - 1) == 0) {
         /* Kill running copy of Bacula */
         return bacService::KillRunningCopy();
      }

      /* /help */
      if (strnicmp(&szCmdLine[i], BaculaShowHelp, sizeof(BaculaShowHelp) - 1) == 0) {
         MessageBox(NULL, BaculaUsageText, _("Bacula Usage"), MB_OK|MB_ICONINFORMATION);
         return 0;
      }
      
      MessageBox(NULL, szCmdLine, _("Bad Command Line Options"), MB_OK);

      /* Show the usage dialog */
      MessageBox(NULL, BaculaUsageText, _("Bacula Usage"), MB_OK | MB_ICONINFORMATION);
      return 1;
   }

   /* If no arguments were given then just run */
   if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
      AllocConsole();
   }
   WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "\r\n", 2, &dwCharsWritten, NULL);

   return BaculaAppMain();
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

   /* Create tray icon & menu if we're running as an app */
   bacMenu *menu = new bacMenu();
   if (menu == NULL) {
//    log_error_message("Could not create sys tray menu");
      PostQuitMessage(0);
   }

   /* Now enter the Windows message handling loop until told to quit! */
   MSG msg;
   while (GetMessage(&msg, NULL, 0,0) ) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   if (menu != NULL) {
      delete menu;
   }

   if (old_servicethread != 0) { /* started as NT service */
      /* Mark that we're no longer running */
      g_servicethread = 0;

      /* Tell the service manager that we've stopped. */
      ReportStatus(SERVICE_STOPPED, g_error, 0);
   }  
   /* Tell main program to go away */
   terminate_stored(0);

   /* Should not get here */
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
 /* DWORD dwThreadID; */
   pthread_t tid;

   InitWinAPIWrapper();

   WSA_Init();

   /* Set this process to be the last application to be shut down. */
   if (p_SetProcessShutdownParameters) {
      p_SetProcessShutdownParameters(0x100, 0);
   }

   HWND hservwnd = FindWindow(MENU_CLASS_NAME, NULL);
   if (hservwnd != NULL) {
      /* We don't allow multiple instances! */
      MessageBox(NULL, _("Another instance of Bacula is already running"), szAppName, MB_OK);
      _exit(0);
   }

   /* Create a thread to handle the Windows messages */
   pthread_create(&tid, NULL,  Main_Msg_Loop, (void *)0);

   /* Call the "real" Bacula */
   BaculaMain(num_command_args, command_args);
   PostQuitMessage(0);
   WSACleanup();
   _exit(0);
}
