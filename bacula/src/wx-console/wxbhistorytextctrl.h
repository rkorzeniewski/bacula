/*
 *
 *   Text control with an history of commands typed
 *
 *    Nicolas Boichat, July 2004
 *
 *    Version $Id$
 */
/*
   Copyright (C) 2004-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#ifndef WXBHISTORYTEXTCTRL
#define WXBHISTORYTEXTCTRL

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

#include <wx/treectrl.h>
#include <wx/hashmap.h>

WX_DECLARE_STRING_HASH_MAP( wxString, wxbCommands );

class wxbHistoryTextCtrl: public wxTextCtrl {
   public:
      wxbHistoryTextCtrl(wxStaticText* help, wxWindow* parent, wxWindowID id,
         const wxString& value = wxT(""), const wxPoint& pos = wxDefaultPosition,
         const wxSize& size = wxDefaultSize,
         const wxValidator& validator = wxDefaultValidator,
         const wxString& name = wxTextCtrlNameStr);

      void HistoryAdd(wxString cmd);

      void AddCommand(wxString cmd, wxString description);
      void ClearCommandList();

      virtual void SetValue(const wxString& value);
   private:
      wxArrayString history;
      wxbCommands commands;
      int index;
      wxStaticText* help;

      void OnKeyUp(wxKeyEvent& event);
      void OnKeyDown(wxKeyEvent& event);

      DECLARE_EVENT_TABLE();
};

#endif //WXBHISTORYTEXTCTRL
