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
#include <wx/stattext.h>

#include "wxbutils.h"

#include "wxbtreectrl.h"
#include "wxblistctrl.h"

class wxbTreeListPanel;

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
      virtual void EnablePanel(bool enable = true);

   private:
/* Commands called by events handler */

      /* The main button has been clicked */
      void CmdStart();

      /* Apply configuration changes */
      void CmdConfigApply();

      /* Cancel restore */
      void CmdConfigCancel();

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

      /* Parse a table in tableParser */
      wxbTableParser* CreateAndWaitForParser(wxString cmd);

      /* Run a command, and waits until result is fully received,
       * if keepresults is true, returns a valid pointer to a wxbDataTokenizer
       * containing the data. */
      wxbDataTokenizer* WaitForEnd(wxString cmd, bool keepresults = false, bool linebyline = true);

      /* Run a command, and waits until prompt result is fully received,
       * if keepresults is true, returns a valid pointer to a wxbPromptParser
       * containing the data. */
      wxbPromptParser* WaitForPrompt(wxString cmd, bool keepresults = false);

      /* Run a dir command, and waits until result is fully received. */
      void UpdateTreeItem(wxTreeItemId item, bool updatelist);

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
      
      /* Update config */
      bool UpdateConfig(wxbDataTokenizer* dt);
      
/* Status related */
      enum status_enum
      {
         disabled,    // The panel is not activatable
         activable,   // The panel is activable, but not activated
         entered,     // The panel is activated
         choosing,    // The user is choosing files to restore
         listing,     // Dir listing is in progress
         configuring, // The user is configuring restore process
         restoring,   // Bacula is restoring files
         finished     // Retore done (state will change in activable)
      };

      status_enum status;

      /* Set current status by enabling/disabling components */
      void SetStatus(status_enum newstatus);

/* UI related */
      bool working; // A command is running, discard GUI events
      wxTreeItemId currentTreeItem; // Currently selected tree item

      /* Enable or disable config controls status */
      void EnableConfig(bool enable);

/* Event handling */
      void OnStart(wxEvent& WXUNUSED(event));
      void OnTreeChanging(wxTreeEvent& event);
      void OnTreeExpanding(wxTreeEvent& event);
      void OnTreeChanged(wxTreeEvent& event);
      void OnTreeMarked(wxbTreeMarkedEvent& event);
      void OnListMarked(wxbListMarkedEvent& event);
      void OnListActivated(wxListEvent& event);
      void OnClientChoiceChanged(wxCommandEvent& event);
      void OnConfigUpdated(wxCommandEvent& event);
      void OnConfigOk(wxEvent& WXUNUSED(event));
      void OnConfigApply(wxEvent& WXUNUSED(event));
      void OnConfigCancel(wxEvent& WXUNUSED(event));

/* Components */
      wxBoxSizer *centerSizer; /* Center sizer */
      wxbTreeListPanel *treelistPanel; /* Panel which contains tree and list */
      wxPanel *restorePanel; /* Panel which contains restore options */

      wxImageList* imagelist; //image list for tree and list

      wxButton* start;
      wxChoice* clientChoice;
      wxChoice* jobChoice;
      wxbTreeCtrl* tree;
      wxbListCtrl* list;
      wxGauge* gauge;

      wxButton*     cfgOk;
      wxButton*     cfgApply;
      wxButton*     cfgCancel;
      
      long cfgUpdated; //keeps which config fields are updated
      
      wxStaticText* cfgJobname;
      wxStaticText* cfgBootstrap;
      wxTextCtrl*   cfgWhere;
      wxChoice*     cfgReplace;
      wxChoice*     cfgFileset;
      wxChoice*     cfgClient;
      wxStaticText* cfgStorage;
      wxTextCtrl*   cfgWhen;
      wxTextCtrl*   cfgPriority;

      friend class wxbTreeListPanel;

      DECLARE_EVENT_TABLE();    
};

class wxbTreeListPanel: public wxPanel {
public:
     wxbTreeListPanel(wxbRestorePanel* parent);
private:
     void OnTreeMarked(wxbTreeMarkedEvent& event);
     void OnListMarked(wxbListMarkedEvent& event);
     DECLARE_EVENT_TABLE(); 
     wxbRestorePanel* parent;
};

#endif // WXBRESTOREPANEL_H

