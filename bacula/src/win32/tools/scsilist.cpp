/*
 * scsilist.cpp - Outputs the contents of a ScsiDeviceList.
 *
 * Author: Robert Nelson, August, 2006 <robertn@the-nelsons.org>
 *
 * Version $Id$
 *
 * Copyright (C) 2006 Kern Sibbald
 *
 * This file was contributed to the Bacula project by Robert Nelson.
 *
 * Robert Nelson has been granted a perpetual, worldwide,
 * non-exclusive, no-charge, royalty-free, irrevocable copyright
 * license to reproduce, prepare derivative works of, publicly
 * display, publicly perform, sublicense, and distribute the original
 * work contributed by Robert Nelson to the Bacula project in source 
 * or object form.
 *
 * If you wish to license contributions from Robert Nelson
 * under an alternate open source license please contact
 * Robert Nelson <robertn@the-nelsons.org>.
 */
/*
   Copyright (C) 2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#if defined(_MSC_VER) && defined(_DEBUG)
#include <afx.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <tchar.h>

#include "ScsiDeviceList.h"

#if defined(_MSC_VER) && defined(_DEBUG)
#define  new   DEBUG_NEW
#endif

int _tmain(int /* argc */, _TCHAR* /* argv */[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
   CMemoryState InitialMemState, FinalMemState, DiffMemState;

   InitialMemState.Checkpoint();

   {
#endif

   CScsiDeviceList   DeviceList;

   if (!DeviceList.Populate())
   {
      DeviceList.PrintLastError();
      return 1;
   }

#define  HEADING \
   _T("Device                        Type     Physical     Name\n") \
   _T("======                        ====     ========     ====\n")

   _fputts(HEADING, stdout);

   for (DWORD index = 0; index < DeviceList.size(); index++) {

      CScsiDeviceListEntry &entry = DeviceList[index];

      if (entry.GetType() != CScsiDeviceListEntry::Disk) {

         _tprintf(_T("%-28s  %-7s  %-11s  %-27s\n"), 
                  entry.GetIdentifier(),
                  entry.GetTypeName(),
                  entry.GetDevicePath(),
                  entry.GetDeviceName());
      }
   }

#if defined(_MSC_VER) && defined(_DEBUG)
   }

   InitialMemState.DumpAllObjectsSince();

   FinalMemState.Checkpoint();
   DiffMemState.Difference(InitialMemState, FinalMemState);
   DiffMemState.DumpStatistics();
#endif

   return 0;
}
