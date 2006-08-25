/*
 *
 *   Custom tree control, which send "tree marked" events when the user right-
 *   click on a item, or double-click on a mark.
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

#ifndef WXBTREECTRL_H
#define WXBTREECTRL_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

#include <wx/treectrl.h>

BEGIN_DECLARE_EVENT_TYPES()
   DECLARE_LOCAL_EVENT_TYPE(wxbTREE_MARKED_EVENT,       618)
END_DECLARE_EVENT_TYPES()

/* Customized tree event, used for marking events */
class wxbTreeMarkedEvent: public wxEvent {
   public:
      wxbTreeMarkedEvent(int id, wxTreeItemId& item);
      ~wxbTreeMarkedEvent();
      wxbTreeMarkedEvent(const wxbTreeMarkedEvent& te);
      virtual wxEvent *Clone() const;

      wxTreeItemId GetItem();
   private:
      wxTreeItemId item;
};

typedef void (wxEvtHandler::*wxTreeMarkedEventFunction)(wxbTreeMarkedEvent&);

#define EVT_TREE_MARKED_EVENT(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( \
        wxbTREE_MARKED_EVENT, id, wxID_ANY, \
        (wxObjectEventFunction)(wxEventFunction)(wxTreeMarkedEventFunction)&fn, \
        (wxObject *) NULL \
    ),

/* Customized tree, which transmit double clicks on images */
class wxbTreeCtrl: public wxTreeCtrl {
   public:
      wxbTreeCtrl(wxWindow* parent, wxEvtHandler* handler, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
      ~wxbTreeCtrl();

   private:
      void OnDoubleClicked(wxMouseEvent& event);
      void OnRightClicked(wxMouseEvent& event);

      wxEvtHandler* handler;

      DECLARE_EVENT_TABLE();
};

#endif // WXBTREECTRL_H
