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
class IVssBackupComponents;
struct IVssAsync;

class VSSClient
{
public:
    VSSClient();
    ~VSSClient();

    // Backup Process
    BOOL InitializeForBackup();
    BOOL CreateSnapshots(char* szDriveLetters);
    BOOL GetShadowPath (const char* szFilePath, char* szShadowPath, int nBuflen);
    BOOL CloseBackup();
    
private:

   BOOL Initialize(DWORD dwContext, bool bDuringRestore = false);
   void WaitAndCheckForAsyncOperation(IVssAsync*  pAsync);
   void QuerySnapshotSet(GUID snapshotSetID);

private:

    bool                            m_bCoInitializeCalled;
    DWORD                           m_dwContext;

    IVssBackupComponents*           m_pVssObject;
    // TRUE if we are during restore
    bool                            m_bDuringRestore;
    bool                            m_bBackupIsInitialized;

    // drive A will be stored on position 0,Z on pos. 25
    WCHAR                           m_wszUniqueVolumeName[26][MAX_PATH]; // approx. 7 KB
    char /* in utf-8 */             m_szShadowCopyName[26][MAX_PATH*2]; // approx. 7 KB
};

// define global VssClient
extern VSSClient g_VSSClient;


#endif /* __VSS_H_ */
