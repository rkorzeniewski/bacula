//                              -*- Mode: C++ -*-
// vss.cpp -- Interface to Volume Shadow Copies (VSS)
//
// Copyright transferred from MATRIX-Computer GmbH to
//   Kern Sibbald by express permission.
//
// Copyright (C) 2004-2005 Kern Sibbald
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License as
//   published by the Free Software Foundation; either version 2 of
//   the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//   General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free
//   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
//   MA 02111-1307, USA.
//
// Author          : Thorsten Engel
// Created On      : Fri May 06 21:44:00 2006


#include <stdio.h>
#include <basetsd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <process.h>
#include <direct.h>
#include <winsock2.h>
#include <windows.h>
#include <wincon.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <conio.h>
#include <process.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <malloc.h>
#include <setjmp.h>
#include <direct.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>


// STL includes
#include <vector>
#include <algorithm>
#include <string>
#include <fstream>
using namespace std;   

#include <atlcomcli.h>
#include <objbase.h>


// Used for safe string manipulation
#include <strsafe.h>

#include "vss/inc/WinXP/vss.h"
#include "vss/inc/WinXP/vswriter.h"
#include "vss/inc/WinXP/vsbackup.h"

#include "vss.h"

#pragma comment(lib,"C:/Development/bacula/bacula/src/win32/compat/vss/lib/WinXP/obj/i386/vssapi.lib")
#pragma comment(lib,"C:/Development/bacula/bacula/src/win32/compat/vss/lib/WinXP/obj/i386/vss_uuid.lib")
#pragma comment(lib,"atlsd.lib")



// define global VssClient
VSSClient g_VSSClient;

/* 
 *
 * some helper functions 
 *
 *
 */

// Append a backslash to the current string 
inline wstring AppendBackslash(wstring str)
{
    if (str.length() == 0)
        return wstring(L"\\");
    if (str[str.length() - 1] == L'\\')
        return str;
    return str.append(L"\\");
}

// Get the unique volume name for the given path
inline wstring GetUniqueVolumeNameForPath(wstring path)
{
    _ASSERTE(path.length() > 0);

    // Add the backslash termination, if needed
    path = AppendBackslash(path);

    // Get the root path of the volume
    WCHAR volumeRootPath[MAX_PATH];
    WCHAR volumeName[MAX_PATH];
    WCHAR volumeUniqueName[MAX_PATH];

    if (!GetVolumePathNameW((LPCWSTR)path.c_str(), volumeRootPath, MAX_PATH))
      return L'\0';
    
    // Get the volume name alias (might be different from the unique volume name in rare cases)
    if (!GetVolumeNameForVolumeMountPointW(volumeRootPath, volumeName, MAX_PATH))
       return L'\0';
    
    // Get the unique volume name    
    if (!GetVolumeNameForVolumeMountPointW(volumeName, volumeUniqueName, MAX_PATH))
       return L'\0';
    
    return volumeUniqueName;
}


// we need something like a map

// Constructor
VSSClient::VSSClient()
{
    m_bCoInitializeCalled = false;
    m_dwContext = VSS_CTX_BACKUP;
    m_bDuringRestore = false;
    m_bBackupIsInitialized = false;
    m_pVssObject = NULL;
    memset (m_wszUniqueVolumeName,0,sizeof (m_wszUniqueVolumeName));
    memset (m_szShadowCopyName,0,sizeof (m_szShadowCopyName));
}

// Destructor
VSSClient::~VSSClient()
{
   // Release the IVssBackupComponents interface 
   // WARNING: this must be done BEFORE calling CoUninitialize()
   if (m_pVssObject) {
      m_pVssObject->Release();
      m_pVssObject = NULL;
   }

   // Call CoUninitialize if the CoInitialize was performed sucesfully
   if (m_bCoInitializeCalled)
      CoUninitialize();
}

BOOL VSSClient::InitializeForBackup()
{
    return Initialize (VSS_CTX_BACKUP);
}

// Initialize the COM infrastructure and the internal pointers
BOOL VSSClient::Initialize(DWORD dwContext, bool bDuringRestore)
{
   HRESULT hr;
   // Initialize COM 
   if (!m_bCoInitializeCalled)  {
      if (FAILED(CoInitialize(NULL)))
         return FALSE;

      m_bCoInitializeCalled = true;

      // Initialize COM security
      hr =
         CoInitializeSecurity(
         NULL,                           //  Allow *all* VSS writers to communicate back!
         -1,                             //  Default COM authentication service
         NULL,                           //  Default COM authorization service
         NULL,                           //  reserved parameter
         RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  //  Strongest COM authentication level
         RPC_C_IMP_LEVEL_IDENTIFY,       //  Minimal impersonation abilities 
         NULL,                           //  Default COM authentication settings
         EOAC_NONE,                      //  No special options
         NULL                            //  Reserved parameter
         );

      if (FAILED(hr))
         return FALSE;
   }

   // Release the IVssBackupComponents interface 
   if (m_pVssObject) {
      m_pVssObject->Release();
      m_pVssObject = NULL;
   }

   // Create the internal backup components object
   hr = CreateVssBackupComponents(&m_pVssObject);
   if (FAILED(hr))
      return FALSE;

   // We are during restore now?
   m_bDuringRestore = bDuringRestore;
/*
   // Call either Initialize for backup or for restore
   if (m_bDuringRestore)  {
      hr = m_pVssObject->InitializeForRestore(CComBSTR(L""));
      if (FAILED(hr))
         return FALSE;
   }
   else
   {
      // Initialize for backup
      hr = m_pVssObject->InitializeForBackup();
      if (FAILED(hr))
         return FALSE;
   }

   

   // Set various properties per backup components instance
   hr = m_pVssObject->SetBackupState(true, true, VSS_BT_FULL, false);
   if (FAILED(hr))
      return FALSE;
*/
// Keep the context
   m_dwContext = dwContext;

   return TRUE;
}


void VSSClient::WaitAndCheckForAsyncOperation(IVssAsync* pAsync)
{
     // Wait until the async operation finishes
    HRESULT hr = pAsync->Wait();

    // Check the result of the asynchronous operation
    HRESULT hrReturned = S_OK;
    hr = pAsync->QueryStatus(&hrReturned, NULL);

    // Check if the async operation succeeded...
    if(FAILED(hrReturned))
    {
        throw(hrReturned);
    }
}

BOOL VSSClient::CreateSnapshots(char* szDriveLetters)
{
   /* szDriveLetters contains all drive letters in uppercase */
   /* if a drive can not being added, it's converted to lowercase in szDriveLetters */
   /* http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vss/base/ivssbackupcomponents_startsnapshotset.asp */
   
   if (!m_pVssObject || m_bBackupIsInitialized)
      return FALSE;      

   m_bBackupIsInitialized = true;

   // 1. InitializeForBackup
   HRESULT hr = m_pVssObject->InitializeForBackup();
   if (FAILED(hr))
      return FALSE;
   
   // 2. SetBackupState
   hr = m_pVssObject->SetBackupState(true, true, VSS_BT_FULL, false);
   if (FAILED(hr))
      return FALSE;

   CComPtr<IVssAsync>  pAsync;
   VSS_ID latestSnapshotSetID;
   VSS_ID pid;

   // 3. startSnapshotSet

   m_pVssObject->StartSnapshotSet(&latestSnapshotSetID);

   // 4. AddToSnapshotSet

   WCHAR szDrive[3];
   szDrive[1] = ':';
   szDrive[2] = 0;

   wstring volume;

   for (size_t i=0; i < strlen (szDriveLetters); i++) {
      szDrive[0] = szDriveLetters[i];
      volume = GetUniqueVolumeNameForPath(szDrive);
      // store uniquevolumname
      if (SUCCEEDED(m_pVssObject->AddToSnapshotSet((LPWSTR)volume.c_str(), GUID_NULL, &pid)))
         wcsncpy (m_wszUniqueVolumeName[szDriveLetters[i]-'A'], (LPWSTR) volume.c_str(), MAX_PATH);
      else
         szDriveLetters[i] = tolower (szDriveLetters[i]);               
   }

   // 5. PrepareForBackup

   m_pVssObject->PrepareForBackup(&pAsync);

   // Waits for the async operation to finish and checks the result
   WaitAndCheckForAsyncOperation(pAsync);


   // 6. DoSnapShotSet
   
   pAsync = NULL;
   m_pVssObject->DoSnapshotSet(&pAsync);

   // Waits for the async operation to finish and checks the result
   WaitAndCheckForAsyncOperation(pAsync); 
   
   /* query snapshot info */
   QuerySnapshotSet(latestSnapshotSetID);

   return TRUE;
}

BOOL VSSClient::CloseBackup()
{
   if (!m_pVssObject || !m_bBackupIsInitialized)
      return FALSE;

   m_bBackupIsInitialized = false;

   CComPtr<IVssAsync>  pAsync;

   if (SUCCEEDED(m_pVssObject->BackupComplete(&pAsync))) {
     // Waits for the async operation to finish and checks the result
     WaitAndCheckForAsyncOperation(pAsync);
   }
   else return FALSE;
   
   return TRUE;
}

// Query all the shadow copies in the given set
void VSSClient::QuerySnapshotSet(GUID snapshotSetID)
{   
   memset (m_szShadowCopyName,0,sizeof (m_szShadowCopyName));
   
   if (snapshotSetID == GUID_NULL || m_pVssObject == NULL)
      return;
            
   // Get list all shadow copies. 
   CComPtr<IVssEnumObject> pIEnumSnapshots;
   HRESULT hr = m_pVssObject->Query( GUID_NULL, 
         VSS_OBJECT_NONE, 
         VSS_OBJECT_SNAPSHOT, 
         &pIEnumSnapshots );    

   // If there are no shadow copies, just return
   if (hr == S_FALSE) {
      return;
   } 

   // Enumerate all shadow copies. 
   VSS_OBJECT_PROP Prop;
   VSS_SNAPSHOT_PROP& Snap = Prop.Obj.Snap;
   
   while(true)
   {
      // Get the next element
      ULONG ulFetched;
      hr = pIEnumSnapshots->Next( 1, &Prop, &ulFetched );

      // We reached the end of list
      if (ulFetched == 0)
         break;

      // Print the shadow copy (if not filtered out)
      if (Snap.m_SnapshotSetId == snapshotSetID)  {
         for (char ch='A'-'A';ch<='Z'-'A';ch++) {
            if (wcscmp(Snap.m_pwszOriginalVolumeName, m_wszUniqueVolumeName[ch]) == 0) {               
               WideCharToMultiByte(CP_UTF8,0,Snap.m_pwszSnapshotDeviceObject,-1,m_szShadowCopyName[ch],MAX_PATH*2,NULL,NULL);               
               break;
            }
         }
      }
      ::VssFreeSnapshotProperties(&Snap);
   }
}

BOOL VSSClient::GetShadowPath (const char* szFilePath, char* szShadowPath, int nBuflen)
{
   /* check for valid pathname */
   BOOL bIsValidName;
   
   bIsValidName = strlen (szFilePath) > 3;
   if (bIsValidName)
      bIsValidName &= isalpha (szFilePath[0]) &&
                      szFilePath[1]==':' && 
                      szFilePath[2]=='\\';

   if (bIsValidName) {
      int nDriveIndex = toupper(szFilePath[0])-'A';
      if (m_szShadowCopyName[nDriveIndex][0] != 0) {
         strncpy (szShadowPath, m_szShadowCopyName[nDriveIndex], nBuflen);
         nBuflen -= (int) strlen (m_szShadowCopyName[nDriveIndex]);
         strncat (szShadowPath, szFilePath+2,nBuflen);

         return TRUE;
      }
   }
   
   strncpy (szShadowPath,  szFilePath, nBuflen);
   return FALSE;   
}