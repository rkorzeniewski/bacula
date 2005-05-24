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

#ifdef B_VSS_XP
   #pragma message("compile VSS for Windows XP")   
   #define VSSClientGeneric VSSClientXP

   #include "vss/inc/WinXP/vss.h"
   #include "vss/inc/WinXP/vswriter.h"
   #include "vss/inc/WinXP/vsbackup.h"
   
   /* In VSSAPI.DLL */
   typedef HRESULT (STDAPICALLTYPE* t_CreateVssBackupComponents)(OUT IVssBackupComponents **);
   typedef void (APIENTRY* t_VssFreeSnapshotProperties)(IN VSS_SNAPSHOT_PROP*);
   
   static t_CreateVssBackupComponents p_CreateVssBackupComponents = NULL;
   static t_VssFreeSnapshotProperties p_VssFreeSnapshotProperties = NULL;

   #define VSSVBACK_ENTRY "?CreateVssBackupComponents@@YGJPAPAVIVssBackupComponents@@@Z"
#endif

#ifdef B_VSS_W2K3
   #pragma message("compile VSS for Windows 2003")
   #define VSSClientGeneric VSSClient2003

   #include "vss/inc/Win2003/vss.h"
   #include "vss/inc/Win2003/vswriter.h"
   #include "vss/inc/Win2003/vsbackup.h"
   
   /* In VSSAPI.DLL */
   typedef HRESULT (STDAPICALLTYPE* t_CreateVssBackupComponents)(OUT IVssBackupComponents **);
   typedef void (APIENTRY* t_VssFreeSnapshotProperties)(IN VSS_SNAPSHOT_PROP*);
   
   static t_CreateVssBackupComponents p_CreateVssBackupComponents = NULL;
   static t_VssFreeSnapshotProperties p_VssFreeSnapshotProperties = NULL;

   #define VSSVBACK_ENTRY "?CreateVssBackupComponents@@YGJPAPAVIVssBackupComponents@@@Z"
#endif

#include "vss.h"

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



// Constructor

VSSClientGeneric::VSSClientGeneric()
{
   m_hLib = LoadLibraryA("VSSAPI.DLL");
   if (m_hLib) {      
      p_CreateVssBackupComponents = (t_CreateVssBackupComponents)
         GetProcAddress(m_hLib, VSSVBACK_ENTRY);
                                 
      p_VssFreeSnapshotProperties = (t_VssFreeSnapshotProperties)
          GetProcAddress(m_hLib, "VssFreeSnapshotProperties");      
   } 
}



// Destructor
VSSClientGeneric::~VSSClientGeneric()
{
   if (m_hLib)
      FreeLibrary(m_hLib);
}

// Initialize the COM infrastructure and the internal pointers
BOOL VSSClientGeneric::Initialize(DWORD dwContext, BOOL bDuringRestore)
{
   if (!(p_CreateVssBackupComponents && p_VssFreeSnapshotProperties))
      return FALSE;

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

   IVssBackupComponents* pVss = (IVssBackupComponents*) m_pVssObject;
   // Release the IVssBackupComponents interface 
   if (pVss) {
      pVss->Release();
      pVss = NULL;
   }

   // Create the internal backup components object
   hr = p_CreateVssBackupComponents((IVssBackupComponents**) &m_pVssObject);
   if (FAILED(hr))
      return FALSE;

   // We are during restore now?
   m_bDuringRestore = bDuringRestore;

   // Keep the context
   m_dwContext = dwContext;

   return TRUE;
}


void VSSClientGeneric::WaitAndCheckForAsyncOperation(IVssAsync* pAsync)
{
     // Wait until the async operation finishes
    HRESULT hr = pAsync->Wait();

    // Check the result of the asynchronous operation
    HRESULT hrReturned = S_OK;
    hr = pAsync->QueryStatus(&hrReturned, NULL);

    // Check if the async operation succeeded...
    if(FAILED(hrReturned))
    {
      PWCHAR pwszBuffer = NULL;
      DWORD dwRet = ::FormatMessageW(
         FORMAT_MESSAGE_ALLOCATE_BUFFER 
         | FORMAT_MESSAGE_FROM_SYSTEM 
         | FORMAT_MESSAGE_IGNORE_INSERTS,
         NULL, hrReturned, 
         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
         (LPWSTR)&pwszBuffer, 0, NULL);

      // No message found for this error. Just return <Unknown>
      if (dwRet != 0)
      {
         // Convert the message into wstring         
    
         LocalFree(pwszBuffer);         
      }
      throw(hrReturned);
    }
}

BOOL VSSClientGeneric::CreateSnapshots(char* szDriveLetters)
{
   /* szDriveLetters contains all drive letters in uppercase */
   /* if a drive can not being added, it's converted to lowercase in szDriveLetters */
   /* http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vss/base/ivssbackupcomponents_startsnapshotset.asp */
   
   if (!m_pVssObject || m_bBackupIsInitialized)
      return FALSE;      

   IVssBackupComponents* pVss = (IVssBackupComponents*) m_pVssObject;

   // 1. InitializeForBackup
   HRESULT hr = pVss->InitializeForBackup();
   if (FAILED(hr))
      return FALSE;
   
   // 2. SetBackupState
   hr = pVss->SetBackupState(true, true, VSS_BT_FULL, false);
   if (FAILED(hr))
      return FALSE;

   CComPtr<IVssAsync>  pAsync1;
   CComPtr<IVssAsync>  pAsync2;
   VSS_ID latestSnapshotSetID;
   VSS_ID pid;

   // 3. startSnapshotSet

   pVss->StartSnapshotSet(&latestSnapshotSetID);

   // 4. AddToSnapshotSet

   WCHAR szDrive[3];
   szDrive[1] = ':';
   szDrive[2] = 0;

   wstring volume;

   for (size_t i=0; i < strlen (szDriveLetters); i++) {
      szDrive[0] = szDriveLetters[i];
      volume = GetUniqueVolumeNameForPath(szDrive);
      // store uniquevolumname
      if (SUCCEEDED(pVss->AddToSnapshotSet((LPWSTR)volume.c_str(), GUID_NULL, &pid)))
         wcsncpy (m_wszUniqueVolumeName[szDriveLetters[i]-'A'], (LPWSTR) volume.c_str(), MAX_PATH);
      else
         szDriveLetters[i] = tolower (szDriveLetters[i]);               
   }

   // 5. PrepareForBackup

   pVss->PrepareForBackup(&pAsync1);

   // Waits for the async operation to finish and checks the result
   WaitAndCheckForAsyncOperation(pAsync1);


   // 6. DoSnapShotSet

   pVss->DoSnapshotSet(&pAsync2);

   // Waits for the async operation to finish and checks the result
   WaitAndCheckForAsyncOperation(pAsync2); 
   
   /* query snapshot info */
   QuerySnapshotSet(latestSnapshotSetID);

   m_bBackupIsInitialized = true;

   return TRUE;
}

BOOL VSSClientGeneric::CloseBackup()
{
   if (!m_pVssObject)
      return FALSE;

   BOOL bRet = FALSE;
   IVssBackupComponents* pVss = (IVssBackupComponents*) m_pVssObject;
   CComPtr<IVssAsync>  pAsync;
   
   m_bBackupIsInitialized = false;

   if (SUCCEEDED(pVss->BackupComplete(&pAsync))) {
     // Waits for the async operation to finish and checks the result
     WaitAndCheckForAsyncOperation(pAsync);
     bRet = TRUE;     
   }
   else {
      pVss->AbortBackup();
   }

   pVss->Release();
   m_pVssObject = NULL;

   return bRet;
}

// Query all the shadow copies in the given set
void VSSClientGeneric::QuerySnapshotSet(GUID snapshotSetID)
{   
   if (!(p_CreateVssBackupComponents && p_VssFreeSnapshotProperties))
      return;

   memset (m_szShadowCopyName,0,sizeof (m_szShadowCopyName));
   
   if (snapshotSetID == GUID_NULL || m_pVssObject == NULL)
      return;

   IVssBackupComponents* pVss = (IVssBackupComponents*) m_pVssObject;

            
   // Get list all shadow copies. 
   CComPtr<IVssEnumObject> pIEnumSnapshots;
   HRESULT hr = pVss->Query( GUID_NULL, 
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
      p_VssFreeSnapshotProperties(&Snap);
   }
}

