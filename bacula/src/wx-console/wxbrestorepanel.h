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

#ifndef WXBRESTOREPANEL_H
#define WXBRESTOREPANEL_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

#include "wxbtableparser.h"

#include <wx/thread.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/gauge.h>

#include "wxbpanel.h"

#include "wxbtreectrl.h"
#include "wxblistctrl.h"

/*
 * wxbPanel for restoring files
 */
class wxbRestorePanel : public wxbPanel
{
   public:
      wxbRestorePanel(wxWindow* parent);
      ~wxbRestorePanel();

      /* wxbPanel overloadings */
      virtual wxString GetTitle();
      virtual void Print(wxString str, int status);

   private:
/* Commands called by events handler */

      /* The main button has been clicked */
      void CmdStart();

       /* List jobs for a specified client */
      void CmdListJobs();

      /* List files and directories for a specified tree item */
      void CmdList(wxTreeItemId item);

      /* Mark a treeitem (directory) or a listitem (file or directory) */
      void CmdMark(wxTreeItemId treeitem, long listitem);

/* General functions and variables */
      bool ended; /* The last command send has finished */

      long filemessages; /* When restoring, number of files restored */
      long totfilemessages; /* When restoring, number of files to be restored */

      /* When listing a directory, sets if file list must be updated
       * (otherwise only the tree structure is updated)
       */
      bool updatelist;

      wxbTableParser* tableParser; /* Used to parse tables */

      /* Parse a table in tableParser */
      void CreateAndWaitForParser(wxString cmd);

      /* Run a command, and waits until result is fully received. */
      void WaitForEnd(wxString cmd);

      /* Run a dir command, and waits until result is fully received. */
      void WaitForList(wxTreeItemId item, bool updatelist);

      /* Parse dir command results. */
      wxString* ParseList(wxString line);

      /* Sets a list item state, and update its parents and children if it is a directory */
      void SetListItemState(long listitem, int newstate);

      /* Sets a tree item state, and update its children, parents and list (if necessary) */
      void SetTreeItemState(wxTreeItemId item, int newstate);
      
      /* Update a tree item parents' state */
      void UpdateTreeItemState(wxTreeItemId item);

      /* Refresh a tree item, and all its children. */
      void RefreshTree(wxTreeItemId item);

/* Status related */
      enum status_enum
      {
         disabled,  // The panel is not activated
         entered,   // The panel is activated
         choosing,  // The user is choosing files to restore
         listing,   // Dir listing is in progress
         restoring, // Bacula is restoring files
         finished   // Retore done
      };

      status_enum status;

      /* Set current status by enabling/disabling components */
      void setStatus(status_enum newstatus);

/* UI related */
      bool working; // A command is running, discard GUI events
      wxTreeItemId currentTreeItem; // Currently selected tree item

/* Event handling */
      void OnStart(wxEvent& WXUNUSED(event));
      void OnTreeChanging(wxTreeEvent& event);
      void OnTreeExpanding(wxTreeEvent& event);
      void OnTreeChanged(wxTreeEvent& event);
      void OnTreeMarked(wxbTreeMarkedEvent& event);
      void OnListMarked(wxbListMarkedEvent& event);
      void OnListActivated(wxListEvent& event);
      void OnClientChoiceChanged(wxCommandEvent& event);

/* Components */
      wxImageList* imagelist; //image list for tree and list

      wxButton* start;
      wxChoice* clientChoice;
      wxChoice* jobChoice;
      wxbTreeCtrl* tree;
      wxbListCtrl* list;
      wxGauge* gauge;

      DECLARE_EVENT_TABLE();
};

#endif // WXBRESTOREPANEL_H

