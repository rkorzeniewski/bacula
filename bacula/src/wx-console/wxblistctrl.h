/*
 *
 *   Custom list control, which send "list marked" events when the user right-
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

#ifndef WXBLISTCTRL_H
#define WXBLISTCTRL_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

#include <wx/listctrl.h>

BEGIN_DECLARE_EVENT_TYPES()
   DECLARE_LOCAL_EVENT_TYPE(wxbLIST_MARKED_EVENT,       1)
END_DECLARE_EVENT_TYPES()

/* Customized list event, used for marking events */
class wxbListMarkedEvent: public wxEvent {
   public:
      wxbListMarkedEvent(int id);
      ~wxbListMarkedEvent();
      wxbListMarkedEvent(const wxbListMarkedEvent& te);
      virtual wxEvent *Clone() const;

};

typedef void (wxEvtHandler::*wxListMarkedEventFunction)(wxbListMarkedEvent&);

#define EVT_LIST_MARKED_EVENT(id, fn) \
    DECLARE_EVENT_TABLE_ENTRY( \
        wxbLIST_MARKED_EVENT, id, wxID_ANY, \
        (wxObjectEventFunction)(wxEventFunction)(wxListMarkedEventFunction)&fn, \
        (wxObject *) NULL \
    ),

/* Customized list, which transmit double clicks on images */
class wxbListCtrl: public wxListCtrl {
   public:
      wxbListCtrl(wxWindow* parent, wxEvtHandler* handler, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
      ~wxbListCtrl();

   private:
      void OnDoubleClicked(wxMouseEvent& event);
      void OnRightClicked(wxMouseEvent& event);

      wxEvtHandler* handler;

      DECLARE_EVENT_TABLE();
};

#endif // WXBTREECTRL_H
