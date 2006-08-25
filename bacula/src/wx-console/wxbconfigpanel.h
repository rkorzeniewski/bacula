/*
 *
 *   Config panel, used to specify parameters (for example clients, filesets... in restore)
 *
 *    Nicolas Boichat, April 2004
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

#ifndef WXBCONFIGPANEL_H
#define WXBCONFIGPANEL_H

#include "wxbutils.h"
#include <wx/panel.h>
#include <wx/choice.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <wx/dynarray.h>

enum wxbConfigType {
   text,
   modifiableText,
   choice
};

class wxbConfigParam {
   public:
      /* Create a new config parameter */
      wxbConfigParam(wxString title, wxWindowID id, wxbConfigType type, wxString value);
      wxbConfigParam(wxString title, wxWindowID id, wxbConfigType type, int n, wxString values[]);
     ~wxbConfigParam();

     void AddControl(wxWindow* parent, wxSizer* sizer);

     wxString GetValue();
     void SetValue(wxString str);

     int GetIndex();
     void SetIndex(int ind);

     void Clear();
     void Add(wxString value);

     wxString GetTitle();

   private:
      wxString value;
      wxString* values;
      int nvalues;

      wxString title;

      wxWindowID id;

      wxbConfigType type;

      wxChoice* choicectrl;
      wxTextCtrl* textctrl;
      wxStaticText* statictext;
};

WX_DECLARE_OBJARRAY(wxbConfigParam, wxbConfig);

class wxbConfigPanel : public wxPanel {
public:
   /* Creates a new config panel, config must be allocated with new */
        wxbConfigPanel(wxWindow* parent, wxbConfig* config, wxString title, wxWindowID ok, wxWindowID cancel, wxWindowID apply = -1);
        ~wxbConfigPanel();

   void SetRowString(const wxChar* title, wxString value);
   wxString GetRowString(const wxChar* title);
   int GetRowSelection(const wxChar* title);
   void SetRowSelection(const wxChar* title, int ind);

   void ClearRowChoices(const wxChar* title);
   void AddRowChoice(const wxChar* title, wxString value);

   /* If enable is true, enables apply button, and disables ok button */
   void EnableApply(bool enable = true);

private:
   /* Keep the last index accessed, for optimization */
   unsigned int last;

   wxbConfig* config;
   wxButton* cfgOk;
   wxButton* cfgCancel;
   wxButton* cfgApply;

   int FindRow(const wxChar* title);
};

#endif
