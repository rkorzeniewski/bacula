/*
 *
 *    Configuration file editor
 *
 *    Nicolas Boichat, May 2004
 *
 *    Version $Id$
 */
/*
   Copyright (C) 2004-2005 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#include "wxbconfigfileeditor.h"

#include <wx/file.h>
#include <wx/filename.h>

enum
{
   Save = 1,
   Quit = 2
};

BEGIN_EVENT_TABLE(wxbConfigFileEditor, wxDialog)
   EVT_BUTTON(Save, wxbConfigFileEditor::OnSave)
   EVT_BUTTON(Quit, wxbConfigFileEditor::OnQuit)
END_EVENT_TABLE()

wxbConfigFileEditor::wxbConfigFileEditor(wxWindow* parent, wxString filename):
      wxDialog(parent, -1, _("Config file editor"), wxDefaultPosition, wxSize(500, 300),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
   this->filename = filename;
   
   textCtrl = new wxTextCtrl(this,-1,wxT(""),wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_RICH | wxTE_DONTWRAP);
   wxFont font(10, wxMODERN, wxNORMAL, wxNORMAL);
#if defined __WXGTK12__ && !defined __WXGTK20__ // Fix for "chinese" fonts under gtk+ 1.2
   font.SetDefaultEncoding(wxFONTENCODING_ISO8859_1);
#endif
   textCtrl->SetDefaultStyle(wxTextAttr(*wxBLACK, wxNullColour, font));

   wxFlexGridSizer *mainSizer = new wxFlexGridSizer(2, 1, 0, 0);
   mainSizer->AddGrowableCol(0);
   mainSizer->AddGrowableRow(0);
   
   wxBoxSizer *bottomsizer = new wxBoxSizer(wxHORIZONTAL);
   bottomsizer->Add(new wxButton(this, Save, _("Save and close")), 0, wxALL, 10);
   bottomsizer->Add(new wxButton(this, Quit, _("Close without saving")), 0, wxALL, 10);
   
   mainSizer->Add(textCtrl, 1, wxEXPAND);
   mainSizer->Add(bottomsizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
   
   this->SetSizer(mainSizer);
   
   wxFileName filen(filename);
   
   if (!filen.FileExists()) {
      (*textCtrl) << wxT("#\n");
      (*textCtrl) << _("# Bacula wx-console Configuration File\n");
      (*textCtrl) << wxT("#\n");
      (*textCtrl) << wxT("\n");
      (*textCtrl) << wxT("Director {\n");
      (*textCtrl) << wxT("  Name = <hostname>-dir\n");
      (*textCtrl) << wxT("  DIRport = 9101\n");
      (*textCtrl) << wxT("  address = <hostname>\n");
      (*textCtrl) << wxT("  Password = \"<dir_password>\"\n");
      (*textCtrl) << wxT("}\n");
   }
   else {
      wxFile file(filename);
      char buffer[2049];
      off_t len;
      while ((len = file.Read(buffer, 2048)) > -1) {
         buffer[len] = 0;
         (*textCtrl) << wxString(buffer,wxConvLocal);
         if (file.Eof())
            break;
      }
      file.Close();
   }
}

wxbConfigFileEditor::~wxbConfigFileEditor() {
   
}

void wxbConfigFileEditor::OnSave(wxCommandEvent& event) {
   wxFile file(filename, wxFile::write);
   if (!file.IsOpened()) {
      wxMessageBox(wxString::Format(_("Unable to write to %s\n"), filename.c_str()),
                        _("Error while saving"),
                        wxOK | wxICON_ERROR, this);
      EndModal(wxCANCEL);
      return;
   }
   
   file.Write(textCtrl->GetValue(),wxConvLocal);
   
   file.Flush();
   file.Close();
   
   EndModal(wxOK);
}

void wxbConfigFileEditor::OnQuit(wxCommandEvent& event) {
   EndModal(wxCANCEL);
}
