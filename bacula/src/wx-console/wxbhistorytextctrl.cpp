/*
 *
 *   Text control with an history of commands typed
 *
 *    Nicolas Boichat, July 2004
 *
 */
/*
   Copyright (C) 2004 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
 
#include "wxbhistorytextctrl.h"

BEGIN_EVENT_TABLE(wxbHistoryTextCtrl, wxTextCtrl)
   EVT_KEY_UP(wxbHistoryTextCtrl::OnKeyUp)
END_EVENT_TABLE()

wxbHistoryTextCtrl::wxbHistoryTextCtrl(wxWindow* parent, wxWindowID id, 
      const wxString& value, const wxPoint& pos, 
      const wxSize& size , 
      const wxValidator& validator, 
      const wxString& name): 
         wxTextCtrl(parent, id, value, pos, size, 
            wxTE_PROCESS_ENTER, validator, name) {
   index = 0;
   history.Add("");
}

void wxbHistoryTextCtrl::HistoryAdd(wxString cmd) {
   if (cmd == "") return;
   index = history.Count();
   history[index-1] = cmd;
   history.Add("");
}

void wxbHistoryTextCtrl::OnKeyUp(wxKeyEvent& event) {
   if (event.m_keyCode == WXK_UP) {
      if (index > 0) {
         if (index == ((int)history.Count()-1)) {
            history[index] = GetValue();
         }
         index--;
         SetValue(history[index]);
         SetInsertionPointEnd();
      }
   }
   else if (event.m_keyCode == WXK_DOWN) {
      if (index < ((int)history.Count()-1)) {
         index++;
         SetValue(history[index]);
         SetInsertionPointEnd();
      }      
   }
   else {
      event.Skip();
   }
}


