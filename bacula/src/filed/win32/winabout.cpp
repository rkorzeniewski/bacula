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

#include "winbacula.h"
#include "winabout.h"

bacAbout::bacAbout()
{
   visible = FALSE;
}

bacAbout::~bacAbout()
{
}

void bacAbout::Show(BOOL show)
{
   if (show && !visible) {
      DialogBoxParam(hAppInstance, MAKEINTRESOURCE(IDD_ABOUT), NULL,
         (DLGPROC)DialogProc, (LONG)this);
   }
}

BOOL CALLBACK
bacAbout::DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
   /* The dialog-box this pointer is in USERDATA */
   bacAbout *_this = (bacAbout *)GetWindowLong(hwnd, GWL_USERDATA);

   switch (uMsg) {
   case WM_INITDIALOG:
      /* Retrieve the Dialog box parameter */
      SetWindowLong(hwnd, GWL_USERDATA, lParam);
      _this = (bacAbout *)lParam;

      /* Show the dialog */
      SetForegroundWindow(hwnd);
      _this->visible = TRUE;
      return TRUE;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case IDCANCEL:
      case IDOK:
         EndDialog(hwnd, TRUE);
         _this->visible = FALSE;
         return TRUE;
      }
      break;

   case WM_DESTROY:
      EndDialog(hwnd, FALSE);
      _this->visible = FALSE;
      return TRUE;
   }
   return 0;
}
