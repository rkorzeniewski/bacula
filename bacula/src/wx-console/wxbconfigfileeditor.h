/*
 *
 *    Configuration file editor
 *
 *    Nicolas Boichat, May 2004
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

#include "wx/wxprec.h"
#include "wx/wx.h"
#include <wx/dialog.h>

#include <wx/textctrl.h>

class wxbConfigFileEditor : public wxDialog {
public:
        wxbConfigFileEditor(wxWindow* parent, wxString filename);
        virtual ~wxbConfigFileEditor();
private:
   wxString filename;

   wxTextCtrl* textCtrl;

   bool firstpaint;

   void OnSave(wxCommandEvent& event);
   void OnQuit(wxCommandEvent& event);
   void OnPaint(wxPaintEvent& event);

   DECLARE_EVENT_TABLE()
};
