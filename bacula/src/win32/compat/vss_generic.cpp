//                              -*- Mode: C++ -*-
// vss.cpp -- Interface to Volume Shadow Copies (VSS)
//
// Copyright transferred from MATRIX-Computer GmbH to
//   Kern Sibbald by express permission.
//
//  Copyright (C) 2005-2006 Kern Sibbald
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  version 2 as amended with additional clauses defined in the
//  file LICENSE in the main source directory.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
//  the file LICENSE for additional details.
//
//
// Author          : Thorsten Engel
// Created On      : Fri May 06 21:44:00 2005


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

// STL includes
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
using namespace std;

#include <atlcomcli.h>
#include <objbase.h>

// Used for safe string manipulation
#include <strsafe.h>

#include "../../lib/winapi.h"

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

    if (!p_GetVolumePathNameW || !p_GetVolumePathNameW((LPCWSTR)path.c_str(), volumeRootPath, MAX_PATH))
      return L"";
    
    // Get the volume name alias (might be different from the unique volume name in rare cases)
    if (!p_GetVolumeNameForVolumeMountPointW || !p_GetVolumeNameForVolumeMountPointW(volumeRootPath, volumeName, MAX_PATH))
       return L"";
    
    // Get the unique volume name    
    if (!p_GetVolumeNameForVolumeMountPointW(volumeName, volumeUniqueName, MAX_PATH))
       return L"";
    
    return volumeUniqueName;
}


// Helper macro for quick treatment of case statements for error codes
#define GEN_MERGE(A, B) A##B
#define GEN_MAKE_W(A) GEN_MERGE(L, A)

#define CHECK_CASE_FOR_CONSTANT(value)                      \
    case value: return (GEN_MAKE_W(#value));


// Convert a writer status into a string
inline const WCHAR* GetStringFromWriterStatus(VSS_WRITER_STATE eWriterStatus)
{
    switch (eWriterStatus) {
    CHECK_CASE_FOR_CONSTANT(VSS_WS_STABLE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_WAITING_FOR_FREEZE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_WAITING_FOR_THAW);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_WAITING_FOR_POST_SNAPSHOT);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_WAITING_FOR_BACKUP_COMPLETE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_IDENTIFY);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_PREPARE_BACKUP);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_PREPARE_SNAPSHOT);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_FREEZE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_THAW);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_POST_SNAPSHOT);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_BACKUP_COMPLETE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_PRE_RESTORE);
    CHECK_CASE_FOR_CONSTANT(VSS_WS_FAILED_AT_POST_RESTORE);
    
    default:
        return L"Error or Undefined";
    }
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
   if (!(p_CreateVssBackupComponents && p_VssFreeSnapshotProperties)) {
      errno = ENOSYS;
      return FALSE;
   }

   HRESULT hr;
   // Initialize COM 
   if (!m_bCoInitializeCalled)  {
      if (FAILED(CoInitialize(NULL))) {
         errno = b_errno_win32;
         return FALSE;
      }
      m_bCoInitializeCalled = true;
   }

   // Initialize COM security
   if (!m_bCoInitializeSecurityCalled) {
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

      if (FAILED(hr)) {         
         errno = b_errno_win32;
         return FALSE;
      }
      m_bCoInitializeSecurityCalled = true;      
   }
   
   // Release the IVssBackupComponents interface 
   if (m_pVssObject) {
      m_pVssObject->Release();
      m_pVssObject = NULL;
   }

   // Create the internal backup components object
   hr = p_CreateVssBackupComponents((IVssBackupComponents**) &m_pVssObject);
   if (FAILED(hr)) {      
      errno = b_errno_win32;
      return FALSE;
   }

#ifdef B_VSS_W2K3
   if (dwContext != VSS_CTX_BACKUP) {
      hr = ((IVssBackupComponents*) m_pVssObject)->SetContext(dwContext);
      if (FAILED(hr)) {
         errno = b_errno_win32;
         return FALSE;
      }
   }
#endif

   if (!bDuringRestore) {
       // 1. InitializeForBackup
       hr = ((IVssBackupComponents*) m_pVssObject)->InitializeForBackup();
       if (FAILED(hr)) {
           errno = b_errno_win32; 
           return FALSE;
       }
 
       // 2. SetBackupState
       hr = ((IVssBackupComponents*) m_pVssObject)->SetBackupState(true, true, VSS_BT_FULL, false);
       if (FAILED(hr)) {
           errno = b_errno_win32;
           return FALSE;
       }
       
       CComPtr<IVssAsync>  pAsync1;       
       // 3. GatherWriterMetaData
       hr = ((IVssBackupComponents*) m_pVssObject)->GatherWriterMetadata(&pAsync1);
       if (FAILED(hr)) {
           errno = b_errno_win32;
           return FALSE;
       }
        // Waits for the async operation to finish and checks the result
       WaitAndCheckForAsyncOperation(pAsync1);
   }


   // We are during restore now?
   m_bDuringRestore = bDuringRestore;

   // Keep the context
   m_dwContext = dwContext;

   return TRUE;
}


BOOL VSSClientGeneric::WaitAndCheckForAsyncOperation(IVssAsync* pAsync)
{
   // Wait until the async operation finishes
   // unfortunately we can't use a timeout here yet.
   // the interface would allow it on W2k3,
   // but it is not implemented yet....

   HRESULT hr;

   // Check the result of the asynchronous operation
   HRESULT hrReturned = S_OK;

   int timeout = 600; // 10 minutes....

   int queryErrors = 0;
   do {
      if (hrReturned != S_OK) 
         Sleep(1000);
   
      hrReturned = S_OK;
      hr = pAsync->QueryStatus(&hrReturned, NULL);
   
      if (FAILED(hr)) 
         queryErrors++;
   } while ((timeout-- > 0) && (hrReturned == VSS_S_ASYNC_PENDING));

   if (hrReturned == VSS_S_ASYNC_FINISHED)
      return TRUE;

   
#ifdef DEBUG
   // Check if the async operation succeeded...
   if(hrReturned != VSS_S_ASYNC_FINISHED) {   
      PWCHAR pwszBuffer = NULL;
      DWORD dwRet = ::FormatMessageW(
                        FORMAT_MESSAGE_ALLOCATE_BUFFER 
                        | FORMAT_MESSAGE_FROM_SYSTEM 
                        | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL, hrReturned, 
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
                        (LPWSTR)&pwszBuffer, 0, NULL);

      if (dwRet != 0) {         
         LocalFree(pwszBuffer);         
      }      
      errno = b_errno_win32;
   }
#endif

   return FALSE;
}

BOOL VSSClientGeneric::CreateSnapshots(char* szDriveLetters)
{
   /* szDriveLetters contains all drive letters in uppercase */
   /* if a drive can not being added, it's converted to lowercase in szDriveLetters */
   /* http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vss/base/ivssbackupcomponents_startsnapshotset.asp */
   
   if (!m_pVssObject || m_bBackupIsInitialized) {
      errno = ENOSYS;
      return FALSE;  
   }

   m_uidCurrentSnapshotSet = GUID_NULL;

   IVssBackupComponents *pVss = (IVssBackupComponents*)m_pVssObject;

   /* startSnapshotSet */

   pVss->StartSnapshotSet(&m_uidCurrentSnapshotSet);

   /* AddToSnapshotSet */

   WCHAR szDrive[3];
   szDrive[1] = ':';
   szDrive[2] = 0;

   wstring volume;

   CComPtr<IVssAsync>  pAsync1;
   CComPtr<IVssAsync>  pAsync2;   
   VSS_ID pid;

   for (size_t i=0; i < strlen (szDriveLetters); i++) {
      szDrive[0] = szDriveLetters[i];
      volume = GetUniqueVolumeNameForPath(szDrive);
      // store uniquevolumname
      if (SUCCEEDED(pVss->AddToSnapshotSet((LPWSTR)volume.c_str(), GUID_NULL, &pid)))
         wcsncpy (m_wszUniqueVolumeName[szDriveLetters[i]-'A'], (LPWSTR) volume.c_str(), MAX_PATH);
      else
      {            
         szDriveLetters[i] = tolower (szDriveLetters[i]);               
      }
   }

   /* PrepareForBackup */
   if (FAILED(pVss->PrepareForBackup(&pAsync1))) {      
      return FALSE;   
   }
   
   // Waits for the async operation to finish and checks the result
   WaitAndCheckForAsyncOperation(pAsync1);

   /* get latest info about writer status */
   if (!CheckWriterStatus()) {
      return FALSE;
   }

   /* DoSnapShotSet */   
   if (FAILED(pVss->DoSnapshotSet(&pAsync2))) {      
      return FALSE;   
   }

   // Waits for the async operation to finish and checks the result
   WaitAndCheckForAsyncOperation(pAsync2); 
   
   /* query snapshot info */   
   QuerySnapshotSet(m_uidCurrentSnapshotSet);

   m_bBackupIsInitialized = true;

   return TRUE;
}

BOOL VSSClientGeneric::CloseBackup()
{
   BOOL bRet = FALSE;
   if (!m_pVssObject)
      errno = ENOSYS;
   else {
      IVssBackupComponents* pVss = (IVssBackupComponents*) m_pVssObject;
      CComPtr<IVssAsync>  pAsync;
      
      m_bBackupIsInitialized = false;

      if (SUCCEEDED(pVss->BackupComplete(&pAsync))) {
         // Waits for the async operation to finish and checks the result
         WaitAndCheckForAsyncOperation(pAsync);
         bRet = TRUE;     
      } else {
         errno = b_errno_win32;
         pVss->AbortBackup();
      }

      /* get latest info about writer status */
      CheckWriterStatus();

      if (m_uidCurrentSnapshotSet != GUID_NULL) {
         VSS_ID idNonDeletedSnapshotID = GUID_NULL;
         LONG lSnapshots;

         pVss->DeleteSnapshots(
            m_uidCurrentSnapshotSet, 
            VSS_OBJECT_SNAPSHOT_SET,
            FALSE,
            &lSnapshots,
            &idNonDeletedSnapshotID);

         m_uidCurrentSnapshotSet = GUID_NULL;
      }

      pVss->Release();
      m_pVssObject = NULL;
   }

   // Call CoUninitialize if the CoInitialize was performed sucesfully
   if (m_bCoInitializeCalled) {
      CoUninitialize();
      m_bCoInitializeCalled = false;
   }

   return bRet;
}

// Query all the shadow copies in the given set
void VSSClientGeneric::QuerySnapshotSet(GUID snapshotSetID)
{   
   if (!(p_CreateVssBackupComponents && p_VssFreeSnapshotProperties)) {
      errno = ENOSYS;
      return;
   }

   memset (m_szShadowCopyName,0,sizeof (m_szShadowCopyName));
   
   if (snapshotSetID == GUID_NULL || m_pVssObject == NULL) {
      errno = ENOSYS;
      return;
   }

   IVssBackupComponents* pVss = (IVssBackupComponents*) m_pVssObject;
               
   // Get list all shadow copies. 
   CComPtr<IVssEnumObject> pIEnumSnapshots;
   HRESULT hr = pVss->Query( GUID_NULL, 
         VSS_OBJECT_NONE, 
         VSS_OBJECT_SNAPSHOT, 
         &pIEnumSnapshots );    

   // If there are no shadow copies, just return
   if (FAILED(hr)) {
      errno = b_errno_win32;
      return;   
   }

   // Enumerate all shadow copies. 
   VSS_OBJECT_PROP Prop;
   VSS_SNAPSHOT_PROP& Snap = Prop.Obj.Snap;
   
   while (true) {
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
               wcsncpy (m_szShadowCopyName[ch],Snap.m_pwszSnapshotDeviceObject, MAX_PATH-1);               
               break;
            }
         }
      }
      p_VssFreeSnapshotProperties(&Snap);
   }
   errno = 0;
}

// Check the status for all selected writers
BOOL VSSClientGeneric::CheckWriterStatus()
{
    /* 
    http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vss/base/ivssbackupcomponents_startsnapshotset.asp
    */
    IVssBackupComponents* pVss = (IVssBackupComponents*) m_pVssObject;
    DestroyWriterInfo();

    // Gather writer status to detect potential errors
    CComPtr<IVssAsync>  pAsync;
    
    HRESULT hr = pVss->GatherWriterStatus(&pAsync);
    if (FAILED(hr)) {
       errno = b_errno_win32;
       return FALSE;
    } 

    // Waits for the async operation to finish and checks the result
    WaitAndCheckForAsyncOperation(pAsync);
      
    unsigned cWriters = 0;

    hr = pVss->GetWriterStatusCount(&cWriters);
    if (FAILED(hr)) {
       errno = b_errno_win32;
       return FALSE;
    }

    int nState;
    
    // Enumerate each writer
    for (unsigned iWriter = 0; iWriter < cWriters; iWriter++) {
        VSS_ID idInstance = GUID_NULL;
        VSS_ID idWriter= GUID_NULL;
        VSS_WRITER_STATE eWriterStatus = VSS_WS_UNKNOWN;
        CComBSTR bstrWriterName;
        HRESULT hrWriterFailure = S_OK;

        // Get writer status
        hr = pVss->GetWriterStatus(iWriter,
                             &idInstance,
                             &idWriter,
                             &bstrWriterName,
                             &eWriterStatus,
                             &hrWriterFailure);
        if (FAILED(hr)) {
            /* unknown */            
            nState = 0;
        }
        else {            
            switch(eWriterStatus) {
            case VSS_WS_FAILED_AT_IDENTIFY:
            case VSS_WS_FAILED_AT_PREPARE_BACKUP:
            case VSS_WS_FAILED_AT_PREPARE_SNAPSHOT:
            case VSS_WS_FAILED_AT_FREEZE:
            case VSS_WS_FAILED_AT_THAW:
            case VSS_WS_FAILED_AT_POST_SNAPSHOT:
            case VSS_WS_FAILED_AT_BACKUP_COMPLETE:
            case VSS_WS_FAILED_AT_PRE_RESTORE:
            case VSS_WS_FAILED_AT_POST_RESTORE:
    #ifdef B_VSS_W2K3
            case VSS_WS_FAILED_AT_BACKUPSHUTDOWN:
    #endif
                /* failed */                
                nState = -1;
                break;

            default:
                /* ok */
                nState = 1;
            }
        }

        /* store text info */
        CComBSTR str;
        char szBuf[16];
        itoa(eWriterStatus, szBuf, 16);
                
        str = "\"";
        str.Append (bstrWriterName);
        str.Append ("\", State: 0x");
        str.Append (szBuf);
        str.Append (" (");
        str.Append (GetStringFromWriterStatus(eWriterStatus));
        str.Append (")");
               
        AppendWriterInfo(nState, CW2A(str));
     //   SysFreeString (bstrWriterName);
    }

    hr = pVss->FreeWriterStatus();

    if (FAILED(hr)) {
        errno = b_errno_win32;
        return FALSE;
    } 

    errno = 0;
    return TRUE;
}
