//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//
//  This file is part of the VNC system.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//
// If the source code for the VNC system is not available from the place 
// whence you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.
//
// This file has been adapted to the Win32 version of Bacula
// by Kern E. Sibbald.  Many thanks to ATT and James Weatherall,
// the original author, for providing an excellent template.
//
// Copyright (2000-2003) Kern E. Sibbald
//


// winService

// Implementation of service-oriented functionality of Bacula
// I.e. command line options that contact a running version of
// Bacula and ask it to do something (show about, show status,
// show events, ...)


#include <lmcons.h>
#include "winbacula.h"
#include "winservice.h"
#include "wintray.h"

// Error message logging
void LogErrorMsg(char *message);

// OS-SPECIFIC ROUTINES

// Create an instance of the bacService class to cause the static fields to be
// initialised properly

bacService init;

DWORD   g_platform_id;
BOOL    g_impersonating_user = 0;

bacService::bacService()
{
    OSVERSIONINFO osversioninfo;
    osversioninfo.dwOSVersionInfoSize = sizeof(osversioninfo);

    // Get the current OS version
    if (!GetVersionEx(&osversioninfo))
            g_platform_id = 0;
    else
            g_platform_id = osversioninfo.dwPlatformId;
}


// IsWin95 - returns a BOOL indicating whether the current OS is Win95
BOOL
bacService::IsWin95()
{
   return (g_platform_id == VER_PLATFORM_WIN32_WINDOWS);
}

// IsWinNT - returns a bool indicating whether the current OS is WinNT
BOOL
bacService::IsWinNT()
{
   return (g_platform_id == VER_PLATFORM_WIN32_NT);
}

// Internal routine to find the  Bacula menu class window and
// post a message to it!

BOOL
PostToBacula(UINT message, WPARAM wParam, LPARAM lParam)
{
   // Locate the hidden Bacula menu window
   HWND hservwnd = FindWindow(MENU_CLASS_NAME, NULL);
   if (hservwnd == NULL)
           return FALSE;

   // Post the message to Bacula
   PostMessage(hservwnd, message, wParam, lParam);
   return TRUE;
}


// Static routine to show the Properties dialog for a currently-running
// copy of Bacula, (usually a servicified version.)

BOOL
bacService::ShowProperties()
{
#ifdef properties_implemented
   // Post to the Bacula menu window
   if (!PostToBacula(MENU_PROPERTIES_SHOW, 0, 0)) {
      MessageBox(NULL, "No existing instance of Bacula could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }
#endif
   return TRUE;
}

// Static routine to show the Default Properties dialog for a currently-running
// copy of Bacula, (usually a servicified version.)

BOOL
bacService::ShowDefaultProperties()
{
#ifdef properties_implemented
   // Post to the Bacula menu window
   if (!PostToBacula(MENU_DEFAULT_PROPERTIES_SHOW, 0, 0)) {
      MessageBox(NULL, "No existing instance of Bacula could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

#endif
   return TRUE;
}

// Static routine to show the About dialog for a currently-running
// copy of Bacula, (usually a servicified version.)

BOOL
bacService::ShowAboutBox()
{
   // Post to the Bacula menu window
   if (!PostToBacula(MENU_ABOUTBOX_SHOW, 0, 0)) {
      MessageBox(NULL, "No existing instance of Bacula could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }
   return TRUE;
}

// Static routine to show the Status dialog for a currently-running
// copy of Bacula, (usually a servicified version.)

BOOL
bacService::ShowStatus()
{
   // Post to the Bacula menu window
   if (!PostToBacula(MENU_STATUS_SHOW, 0, 0)) {
      MessageBox(NULL, "No existing instance of Bacula could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }
   return TRUE;
}

// Static routine to show the Events dialog for a currently-running
// copy of Bacula, (usually a servicified version.)

BOOL
bacService::ShowEvents()
{
   // Post to the Bacula menu window
   if (!PostToBacula(MENU_EVENTS_SHOW, 0, 0)) {
      MessageBox(NULL, "No existing instance of Bacula could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }
   return TRUE;
}


// Static routine to tell a locally-running instance of the server
// to connect out to a new client

BOOL
bacService::PostAddNewClient(unsigned long ipaddress)
{
   // Post to the Bacula menu window
   if (!PostToBacula(MENU_ADD_CLIENT_MSG, 0, ipaddress)) {
      MessageBox(NULL, "No existing instance of Bacula could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   return TRUE;
}

// SERVICE-MODE ROUTINES

// Service-mode defines:

// Executable name
#define BAC_APPNAME            "bacula"

// Internal service name
#define BAC_SERVICENAME        "Bacula"

// Displayed service name
#define BAC_SERVICEDISPLAYNAME "Bacula File Server"

// List of other required services ("dependency 1\0dependency 2\0\0")
// *** These need filling in properly
#define BAC_DEPENDENCIES       ""

// Internal service state
SERVICE_STATUS          g_srvstatus;       // current status of the service
SERVICE_STATUS_HANDLE   g_hstatus;
DWORD                   g_error = 0;
DWORD                   g_servicethread = 0;
char*                   g_errortext[256];

// Forward defines of internal service functions
void WINAPI ServiceMain(DWORD argc, char **argv);

DWORD WINAPI ServiceWorkThread(LPVOID lpwThreadParam);
void ServiceStop();
void WINAPI ServiceCtrl(DWORD ctrlcode);

bool WINAPI CtrlHandler (DWORD ctrltype);

BOOL ReportStatus(DWORD state, DWORD exitcode, DWORD waithint);

// ROUTINE TO QUERY WHETHER THIS PROCESS IS RUNNING AS A SERVICE OR NOT

BOOL    g_servicemode = FALSE;

BOOL
bacService::RunningAsService()
{
   return g_servicemode;
}

BOOL
bacService::KillRunningCopy()
{
   while (PostToBacula(WM_CLOSE, 0, 0)) {
   }
   return TRUE;
}


// ROUTINE TO POST THE HANDLE OF THE CURRENT USER TO THE RUNNING Bacula, IN ORDER
// THAT IT CAN LOAD THE APPROPRIATE SETTINGS.  THIS IS USED ONLY BY THE SVCHELPER
// OPTION, WHEN RUNNING UNDER NT
BOOL
bacService::PostUserHelperMessage()
{
   // - Check the platform type
   if (!IsWinNT())
           return TRUE;

   // - Get the current process ID
   DWORD processId = GetCurrentProcessId();

   // - Post it to the existing Bacula
   if (!PostToBacula(MENU_SERVICEHELPER_MSG, 0, (LPARAM)processId))
           return FALSE;

   // - Wait until it's been used
   return TRUE;
}

// ROUTINE TO PROCESS AN INCOMING INSTANCE OF THE ABOVE MESSAGE
BOOL
bacService::ProcessUserHelperMessage(WPARAM wParam, LPARAM lParam) {
   // - Check the platform type
   if (!IsWinNT() || !bacService::RunningAsService())
           return TRUE;

   // - Close the HKEY_CURRENT_USER key, to force NT to reload it for the new user
   // NB: Note that this is _really_ dodgy if ANY other thread is accessing the key!
   if (RegCloseKey(HKEY_CURRENT_USER) != ERROR_SUCCESS) {
           return FALSE;
   }

   // - Revert to our own identity
   RevertToSelf();
   g_impersonating_user = FALSE;

   // - Open the specified process
   HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)lParam);
   if (processHandle == NULL) {
           return FALSE;
   }

   // - Get the token for the given process
   HANDLE userToken = NULL;
   if (!OpenProcessToken(processHandle, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE, &userToken)) {
           CloseHandle(processHandle);
           return FALSE;
   }
   CloseHandle(processHandle);

   // - Set this thread to impersonate them
   if (!ImpersonateLoggedOnUser(userToken)) {
           CloseHandle(userToken);
           return FALSE;
   }
   CloseHandle(userToken);

   g_impersonating_user = TRUE;
   return TRUE;
}

// SERVICE MAIN ROUTINE
int
bacService::BaculaServiceMain()
{
   // Mark that we are a service
   g_servicemode = TRUE;

   // How to run as a service depends upon the OS being used
   switch (g_platform_id) {

           // Windows 95/98
   case VER_PLATFORM_WIN32_WINDOWS:
      {
      // Obtain a handle to the kernel library
      HINSTANCE kerneldll = LoadLibrary("KERNEL32.DLL");
      if (kerneldll == NULL) {
         MessageBox(NULL, "KERNEL32.DLL not found: Bacula service not started", 
             "Bacula Service", MB_OK);
         break;
      }

      // And find the RegisterServiceProcess function
      DWORD (*RegisterService)(DWORD, DWORD);
      RegisterService = (DWORD (*)(DWORD, DWORD))
              GetProcAddress(kerneldll, "RegisterServiceProcess");
      if (RegisterService == NULL) {
         MessageBox(NULL, "Registry service not fond: Bacula service not started",
            "Bacula Service", MB_OK);
         break;
      }
      
      // Register this process with the OS as a service!
      RegisterService(0, 1);

      // Run the service itself
      BaculaAppMain();

      // Then remove the service from the system service table
      RegisterService(0, 0);

      // Free the kernel library
      FreeLibrary(kerneldll);

      // *** If we don't kill the process directly here, then 
      // for some reason, Bacula crashes...
 //         ExitProcess(0);
      break;
      }
           // Windows NT
   case VER_PLATFORM_WIN32_NT:
      {
      // Create a service entry table
      SERVICE_TABLE_ENTRY dispatchTable[] = {
              {BAC_SERVICENAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
              {NULL, NULL}
      };

      // Call the service control dispatcher with our entry table
      if (!StartServiceCtrlDispatcher(dispatchTable))
              LogErrorMsg("StartServiceCtrlDispatcher failed.");
      break;
      }
   }
   return 0;
}

// SERVICE MAIN ROUTINE - NT ONLY !!!
// NT ONLY !!!
void WINAPI ServiceMain(DWORD argc, char **argv)
{
    DWORD dwThreadID;

        // Register the service control handler
    g_hstatus = RegisterServiceCtrlHandler(BAC_SERVICENAME, ServiceCtrl);

    if (g_hstatus == 0) {
       MessageBox(NULL, "Contact Register Service Handler failure",
          "Bacula service", MB_OK);
       return;
    }

        // Set up some standard service state values
    g_srvstatus.dwServiceType = SERVICE_WIN32 | SERVICE_INTERACTIVE_PROCESS;
    g_srvstatus.dwServiceSpecificExitCode = 0;

        // Give this status to the SCM
    if (!ReportStatus(
        SERVICE_START_PENDING,  // Service state
        NO_ERROR,                               // Exit code type
        15000))                                 // Hint as to how long Bacula should have hung before you assume error
        {
        ReportStatus(
                        SERVICE_STOPPED,
                        g_error,
            0);
                return;
        }

        // Now start the service for real
    (void)CreateThread(NULL, 0, ServiceWorkThread, NULL, 0, &dwThreadID);
    return;
}

// SERVICE START ROUTINE - thread that calls BaculaAppMain
//   NT ONLY !!!!
DWORD WINAPI ServiceWorkThread(LPVOID lpwThreadParam)
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // Save the current thread identifier
    g_servicethread = GetCurrentThreadId();

    // report the status to the service control manager.
    //
    if (!ReportStatus(
        SERVICE_RUNNING,       // service state
        NO_ERROR,              // exit code
        0)) {                  // wait hint
       MessageBox(NULL, "Report Service failure", "Bacula Service", MB_OK);
       return 0;
    }

      // Get a token for this process. 
       
      if (!OpenProcessToken(GetCurrentProcess(), 
              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
// Forge onward anyway in the hopes of succeeding.
//       MessageBox(NULL, "System shutdown failed: OpenProcessToken", "shutdown", MB_OK);
//       exit(1);
      } 

      // Get the LUID for the backup privilege. 
       
      LookupPrivilegeValue(NULL, SE_BACKUP_NAME, 
              &tkp.Privileges[0].Luid); 

       
      tkp.PrivilegeCount = 1;  // one privilege to set    
      tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
       
      // Get the shutdown privilege for this process. 
       
      AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, 
              (PTOKEN_PRIVILEGES)NULL, 0); 
       
      // Cannot test the return value of AdjustTokenPrivileges. 
       
      if (GetLastError() != ERROR_SUCCESS) {
//       MessageBox(NULL, "System shutdown failed: AdjustTokePrivileges", "shutdown", MB_OK);
      } 
     
      // Get the LUID for the restore privilege. 
       
      LookupPrivilegeValue(NULL, SE_RESTORE_NAME, 
              &tkp.Privileges[0].Luid); 

       
      tkp.PrivilegeCount = 1;  // one privilege to set    
      tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
       
      // Get the shutdown privilege for this process. 
       
      AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, 
              (PTOKEN_PRIVILEGES)NULL, 0); 
       
      // Cannot test the return value of AdjustTokenPrivileges. 
       
      if (GetLastError() != ERROR_SUCCESS) {
//       MessageBox(NULL, "System shutdown failed: AdjustTokePrivileges", "shutdown", MB_OK);
      } 

    /* Call Bacula main code */
    BaculaAppMain();

    // Mark that we're no longer running
    g_servicethread = 0;

    // Tell the service manager that we've stopped.
    ReportStatus(
                SERVICE_STOPPED,
                g_error,
                0);
    return 0;
}

// SERVICE STOP ROUTINE - post a quit message to the relevant thread
void ServiceStop()
{
   // Post a quit message to the main service thread
   if (g_servicethread != 0) {
      PostThreadMessage(g_servicethread, WM_QUIT, 0, 0);
   }
}

// SERVICE INSTALL ROUTINE
int
bacService::InstallService()
{
   const int pathlength = 2048;
   char path[pathlength];
   char servicecmd[pathlength];
   int len;

   // Get the filename of this executable
   if (GetModuleFileName(NULL, path, pathlength-(strlen(BaculaRunService)+2)) == 0) {
      MessageBox(NULL, "Unable to install Bacula service", szAppName, MB_ICONEXCLAMATION | MB_OK);
      return 0;
   }

   // Append the service-start flag to the end of the path:
   if ((int)strlen(path) + 20 + (int)strlen(BaculaRunService) < pathlength) {
      sprintf(servicecmd, "\"%s\" %s -c %s", path, BaculaRunService, path);
      len = strlen(servicecmd) - 1;
      for ( ; len > 0; len--) {
         if (servicecmd[len] == '\\') {
            servicecmd[len] = 0;
            break;
         }
         servicecmd[len] = 0;
      }
      strcat(servicecmd, "\\bacula-fd.conf");

   } else {
      MessageBox(NULL, "Service command length too long. Service not registered.",
          szAppName, MB_ICONEXCLAMATION | MB_OK);
      return 0;
   }

   // How to add the Bacula service depends upon the OS
   switch (g_platform_id) {

           // Windows 95/98
   case VER_PLATFORM_WIN32_WINDOWS:
      // Locate the RunService registry entry
      HKEY runservices;
      if (RegCreateKey(HKEY_LOCAL_MACHINE, 
              "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices",
              &runservices) != ERROR_SUCCESS) {
         MessageBox(NULL, "The System Registry could not be updated - the Bacula service was not installed", szAppName, MB_ICONEXCLAMATION | MB_OK);
         break;
      }

      // Attempt to add a Bacula key
      if (RegSetValueEx(runservices, szAppName, 0, REG_SZ, (unsigned char *)servicecmd, strlen(servicecmd)+1) != ERROR_SUCCESS) {
         RegCloseKey(runservices);
         MessageBox(NULL, "The Bacula service could not be installed", szAppName, MB_ICONEXCLAMATION | MB_OK);
         break;
      }

      RegCloseKey(runservices);

      // We have successfully installed the service!
      MessageBox(NULL,
              "The Bacula File service was successfully installed.\n"
              "The service may be started by double clicking on the\n"
              "Bacula \"Start\" icon and will be automatically\n"
              "be run the next time this machine is rebooted. ",
              szAppName,
              MB_ICONINFORMATION | MB_OK);

#ifdef needed
      // Run the service...
      STARTUPINFO si;
      si.cb = sizeof(si);
      si.cbReserved2 = 0;
      si.lpReserved = NULL;
      si.lpReserved2 = NULL;
      si.dwFlags = 0;
      si.lpTitle = NULL;
      PROCESS_INFORMATION pi;
      if (!CreateProcess(NULL, servicecmd,   // Program name & path
              NULL, NULL,                    // Security attributes
              FALSE,                         // Inherit handles?
              NORMAL_PRIORITY_CLASS,         // Extra startup flags
              NULL,                          // Environment table
              NULL,                          // Current directory
              &si,
              &pi
              )) {
         MessageBox(NULL, "CreateProcess: the Bacula service failed to start", szAppName, MB_ICONSTOP | MB_OK);
         break;
      }
#endif
      break;

           // Windows NT
   case VER_PLATFORM_WIN32_NT:
      SC_HANDLE   hservice;
      SC_HANDLE   hsrvmanager;

      // Open the default, local Service Control Manager database
      hsrvmanager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
          if (hsrvmanager == NULL) {
             MessageBox(NULL,
                "The Service Control Manager could not be contacted - the Bacula service was not installed",
                szAppName, MB_ICONEXCLAMATION | MB_OK);
             break;
          }

          // Create an entry for the Bacula service
          hservice = CreateService(
                  hsrvmanager,                    // SCManager database
                  BAC_SERVICENAME,                // name of service
                  BAC_SERVICEDISPLAYNAME,         // name to display
                  SERVICE_ALL_ACCESS,             // desired access
                  SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
                                                                          // service type
                  SERVICE_AUTO_START,             // start type
                  SERVICE_ERROR_NORMAL,           // error control type
                  servicecmd,                     // service's binary
                  NULL,                           // no load ordering group
                  NULL,                           // no tag identifier
                  BAC_DEPENDENCIES,               // dependencies
                  NULL,                           // LocalSystem account
                  NULL);                          // no password
          CloseServiceHandle(hsrvmanager);
          if (hservice == NULL) {
             MessageBox(NULL,
                 "The Bacula service could not be installed",
                  szAppName, MB_ICONEXCLAMATION | MB_OK);
             break;
          }
          CloseServiceHandle(hservice);

          // Now install the servicehelper registry setting...
          // Locate the RunService registry entry
          HKEY runapps;
          if (RegCreateKey(HKEY_LOCAL_MACHINE, 
                  "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  &runapps) != ERROR_SUCCESS) {
             MessageBox(NULL, "WARNING: Unable to install the ServiceHelper hook\nGlobal user-specific registry settings will not be loaded", 
                szAppName, MB_ICONEXCLAMATION | MB_OK);
          } else {
             char servicehelpercmd[pathlength];

             // Append the service-helper-start flag to the end of the path:
             if ((int)strlen(path) + 4 + (int)strlen(BaculaRunServiceHelper) < pathlength)
                sprintf(servicehelpercmd, "\"%s\" %s", path, BaculaRunServiceHelper);
             else
                return 0;

             // Add the upsserviceHelper entry
             if (RegSetValueEx(runapps, szAppName, 0, REG_SZ,
                  (unsigned char *)servicehelpercmd, strlen(servicehelpercmd)+1) != ERROR_SUCCESS)
             {
                MessageBox(NULL, "WARNING:Unable to install the ServiceHelper hook\nGlobal user-specific registry settings will not be loaded", szAppName, MB_ICONEXCLAMATION | MB_OK);
             }
             RegCloseKey(runapps);
          }

          // Everything went fine
          MessageBox(NULL,
                  "The Bacula File service was successfully installed.\n"
                  "The service may be started from the Control Panel and will\n"
                  "automatically be run the next time this machine is rebooted.",
                  szAppName,
                  MB_ICONINFORMATION | MB_OK);
          break;
   default:
          MessageBox(NULL, 
                   "Unknown Windows operating system.\n"     
                   "Cannot install Bacula service.\n",
                    szAppName, MB_ICONEXCLAMATION | MB_OK);
           break;     
   };

   return 0;
}

// SERVICE REMOVE ROUTINE
int
bacService::RemoveService()
{
        // How to remove the Bacula service depends upon the OS
        switch (g_platform_id) {

                // Windows 95/98
        case VER_PLATFORM_WIN32_WINDOWS:
           // Locate the RunService registry entry
           HKEY runservices;
           if (RegOpenKey(HKEY_LOCAL_MACHINE, 
                   "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices",
                   &runservices) != ERROR_SUCCESS) {
              MessageBox(NULL, 
                 "Could not find registry entry.\nService probably not registerd - the Bacula service was not removed", szAppName, MB_ICONEXCLAMATION | MB_OK);
           } else {
              // Attempt to delete the Bacula key
              if (RegDeleteValue(runservices, szAppName) != ERROR_SUCCESS) {
                 RegCloseKey(runservices);
                 MessageBox(NULL, "Could not delete Registry key.\nThe Bacula service could not be removed", szAppName, MB_ICONEXCLAMATION | MB_OK);
              }

              RegCloseKey(runservices);
              break;
           }

           // Try to kill any running copy of Bacula
           if (!KillRunningCopy()) {
              MessageBox(NULL,
                  "Bacula could not be contacted, probably not running",
                  szAppName, MB_ICONEXCLAMATION | MB_OK);
              break;
           }

           // We have successfully removed the service!
           MessageBox(NULL, "The Bacula service has been removed", szAppName, MB_ICONINFORMATION | MB_OK);
           break;

                // Windows NT
        case VER_PLATFORM_WIN32_NT:
           SC_HANDLE   hservice;
           SC_HANDLE   hsrvmanager;

           // Attempt to remove the service-helper hook
           HKEY runapps;
           if (RegOpenKey(HKEY_LOCAL_MACHINE, 
                   "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                   &runapps) == ERROR_SUCCESS)
           {
                   // Attempt to delete the Bacula key
                   if (RegDeleteValue(runapps, szAppName) != ERROR_SUCCESS)
                   {
                           MessageBox(NULL, "WARNING:The ServiceHelper hook entry could not be removed from the registry", szAppName, MB_ICONEXCLAMATION | MB_OK);
                   }
                   RegCloseKey(runapps);
           }

           // Open the SCM
           hsrvmanager = OpenSCManager(
              NULL,                   // machine (NULL == local)
              NULL,                   // database (NULL == default)
              SC_MANAGER_ALL_ACCESS   // access required
              );
           if (hsrvmanager) {
              hservice = OpenService(hsrvmanager, BAC_SERVICENAME, SERVICE_ALL_ACCESS);
              if (hservice != NULL)
              {
                 SERVICE_STATUS status;

                 // Try to stop the Bacula service
                 if (ControlService(hservice, SERVICE_CONTROL_STOP, &status))
                 {
                    while(QueryServiceStatus(hservice, &status)) {
                            if (status.dwCurrentState == SERVICE_STOP_PENDING)
                                    Sleep(1000);
                            else
                                    break;
                    }

                    if (status.dwCurrentState != SERVICE_STOPPED)
                            MessageBox(NULL, "The Bacula service could not be stopped", szAppName, MB_ICONEXCLAMATION | MB_OK);
                 }

                 // Now remove the service from the SCM
                 if(DeleteService(hservice))
                    MessageBox(NULL, "The Bacula service has been removed", szAppName, MB_ICONINFORMATION | MB_OK);
                 else
                    MessageBox(NULL, "The Bacula service could not be removed", szAppName, MB_ICONEXCLAMATION | MB_OK);

                 CloseServiceHandle(hservice);
              }
              else
                 MessageBox(NULL, "The Bacula service could not be found", szAppName, MB_ICONEXCLAMATION | MB_OK);

              CloseServiceHandle(hsrvmanager);
           }
           else
              MessageBox(NULL, "The SCM could not be contacted - the Bacula service was not removed", szAppName, MB_ICONEXCLAMATION | MB_OK);
           break;
        }
        return 0;
}

// USEFUL SERVICE SUPPORT ROUTINES

// Service control routine
void WINAPI ServiceCtrl(DWORD ctrlcode)
{
        // What control code have we been sent?
    switch(ctrlcode)
    {

        case SERVICE_CONTROL_STOP:
                // STOP : The service must stop
                g_srvstatus.dwCurrentState = SERVICE_STOP_PENDING;
        ServiceStop();
        break;

    case SERVICE_CONTROL_INTERROGATE:
                // QUERY : Service control manager just wants to know our state
                break;

        default:
                // Control code not recognised
                break;

    }

        // Tell the control manager what we're up to.
    ReportStatus(g_srvstatus.dwCurrentState, NO_ERROR, 0);
}

// Service manager status reporting
BOOL ReportStatus(DWORD state,
                  DWORD exitcode,
                  DWORD waithint)
{
    static DWORD checkpoint = 1;
    BOOL result = TRUE;

    // If we're in the start state then we don't want the control manager
    // sending us control messages because they'll confuse us.
    if (state == SERVICE_START_PENDING)
       g_srvstatus.dwControlsAccepted = 0;
    else
       g_srvstatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    // Save the new status we've been given
    g_srvstatus.dwCurrentState = state;
    g_srvstatus.dwWin32ExitCode = exitcode;
    g_srvstatus.dwWaitHint = waithint;

    // Update the checkpoint variable to let the SCM know that we
    // haven't died if requests take a long time
    if ((state == SERVICE_RUNNING) || (state == SERVICE_STOPPED))
            g_srvstatus.dwCheckPoint = 0;
    else
    g_srvstatus.dwCheckPoint = checkpoint++;

    // Tell the SCM our new status
    if (!(result = SetServiceStatus(g_hstatus, &g_srvstatus)))
            LogErrorMsg("SetServiceStatus failed");

    return result;
}

// Error reporting
void LogErrorMsg(char *message)
{
    char        msgbuff[256];
    HANDLE      heventsrc;
    char *      strings[2];

    // Save the error code
    g_error = GetLastError();

    // Use event logging to log the error
    heventsrc = RegisterEventSource(NULL, BAC_SERVICENAME);

    sprintf(msgbuff, "%s error: %ld", BAC_SERVICENAME, g_error);
    strings[0] = msgbuff;
    strings[1] = message;

    if (heventsrc != NULL) {
       MessageBeep(MB_OK);

       ReportEvent(
               heventsrc,              // handle of event source
               EVENTLOG_ERROR_TYPE,    // event type
               0,                      // event category
               0,                      // event ID
               NULL,                   // current user's SID
               2,                      // strings in 'strings'
               0,                      // no bytes of raw data
               (const char **)strings, // array of error strings
               NULL);                  // no raw data

       DeregisterEventSource(heventsrc);
    }
}
