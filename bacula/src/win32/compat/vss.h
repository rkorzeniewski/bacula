/*                               -*- Mode: C -*-
 * vss.h --
 */

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

/*
 *
 * Author          : Thorsten Engel
 * Created On      : Fri May 06 21:44:00 2006 
 */

#ifndef __VSS_H_
#define __VSS_H_

// some forward declarations
struct IVssAsync;

class VSSClient
{
public:
    VSSClient();
    ~VSSClient();

    // Backup Process
    BOOL InitializeForBackup();
    virtual BOOL CreateSnapshots(char* szDriveLetters) = 0;
    virtual BOOL CloseBackup() = 0;
    virtual const char* GetDriverName() = 0;
    BOOL GetShadowPath (const char* szFilePath, char* szShadowPath, int nBuflen);

    const size_t GetWriterCount();
    const char* GetWriterInfo(size_t nIndex);
    const int   GetWriterState(size_t nIndex);
         
private:
    virtual BOOL Initialize(DWORD dwContext, BOOL bDuringRestore = FALSE) = 0;
    virtual void WaitAndCheckForAsyncOperation(IVssAsync*  pAsync) = 0;
    virtual void QuerySnapshotSet(GUID snapshotSetID) = 0;

protected:
    HMODULE                         m_hLib;

    BOOL                            m_bCoInitializeCalled;
    DWORD                           m_dwContext;

    IUnknown*                       m_pVssObject;
    GUID                            m_uidCurrentSnapshotSet;
    // TRUE if we are during restore
    BOOL                            m_bDuringRestore;
    BOOL                            m_bBackupIsInitialized;

    // drive A will be stored on position 0,Z on pos. 25
    WCHAR                           m_wszUniqueVolumeName[26][MAX_PATH]; // approx. 7 KB
    char /* in utf-8 */             m_szShadowCopyName[26][MAX_PATH*2]; // approx. 7 KB

    void*                           m_pVectorWriterStates;
    void*                           m_pVectorWriterInfo;
};

class VSSClientXP:public VSSClient
{
public:
   VSSClientXP();
   virtual ~VSSClientXP();
   virtual BOOL CreateSnapshots(char* szDriveLetters);
   virtual BOOL CloseBackup();
   virtual const char* GetDriverName() { return "VSS WinXP"; };
private:
   virtual BOOL Initialize(DWORD dwContext, BOOL bDuringRestore);
   virtual void WaitAndCheckForAsyncOperation(IVssAsync* pAsync);
   virtual void QuerySnapshotSet(GUID snapshotSetID);
   BOOL CheckWriterStatus();   
};

class VSSClient2003:public VSSClient
{
public:
   VSSClient2003();
   virtual ~VSSClient2003();
   virtual BOOL CreateSnapshots(char* szDriveLetters);
   virtual BOOL CloseBackup();   
   virtual const char* GetDriverName() { return "VSS Win 2003"; };
private:
   virtual BOOL Initialize(DWORD dwContext, BOOL bDuringRestore);
   virtual void WaitAndCheckForAsyncOperation(IVssAsync*  pAsync);
   virtual void QuerySnapshotSet(GUID snapshotSetID);
   BOOL CheckWriterStatus();
};


#endif /* __VSS_H_ */
