/*
 *
 *   wxbPanel for restoring files
 *
 *    Nicolas Boichat, April-May 2004
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
/* Note concerning "done" output (modifiable marked with +)
Run Restore job
+JobName:    RestoreFiles
Bootstrap:  /var/lib/bacula/restore.bsr
+Where:      /tmp/bacula-restores
+Replace:    always
+FileSet:    Full Set
+Client:     tom-fd
+Storage:    File
+When:       2004-04-18 01:18:56
+Priority:   10
OK to run? (yes/mod/no):mod
Parameters to modify:
     1: Level (not appropriate)
     2: Storage (automatic ?)
     3: Job (no)
     4: FileSet (yes)
     5: Client (yes : "The defined Client resources are:\n\t1: velours-fd\n\t2: tom-fd\nSelect Client (File daemon) resource (1-2):")
     6: When (yes : "Please enter desired start time as YYYY-MM-DD HH:MM:SS (return for now):")
     7: Priority (yes : "Enter new Priority: (positive integer)")
     8: Bootstrap (?)
     9: Where (yes : "Please enter path prefix for restore (/ for none):")
    10: Replace (yes : "Replace:\n 1: always\n 2: ifnewer\n 3: ifolder\n 4: never\n 
          Select replace option (1-4):")
    11: JobId (no)
Select parameter to modify (1-11):
       */

#include "wxbrestorepanel.h"

#include "wxbmainframe.h"

#include "csprint.h"

#include <wx/choice.h>
#include <wx/datetime.h>

#include <wx/timer.h>

#include "unmarked.xpm"
#include "marked.xpm"
#include "partmarked.xpm"

/* A macro named Yield is defined under MinGW */
#undef Yield

/*
 *  Class which is stored in the tree and in the list to keep informations
 *  about the element.
 */
class wxbTreeItemData : public wxTreeItemData {
   public:
      wxbTreeItemData(wxString path, wxString name, int marked, long listid = -1);
      wxbTreeItemData(wxString path, wxString name, wxString marked, long listid = -1);
      ~wxbTreeItemData();
      wxString GetPath();
      wxString GetName();
      
      int GetMarked();
      void SetMarked(int marked);
      void SetMarked(wxString marked);
      
      long GetListId();

      static int GetMarkedStatus(wxString file);
   private:
      wxString* path; /* Full path */
      wxString* name; /* File name */
      int marked; /* 0 - Not Marked, 1 - Marked, 2 - Some file under is marked */
      long listid; /* list ID : >-1 if this data is in the list (and/or on the tree) */
};

wxbTreeItemData::wxbTreeItemData(wxString path, wxString name, int marked, long listid): wxTreeItemData() {
   this->path = new wxString(path);
   this->name = new wxString(name);
   this->marked = marked;
   this->listid = listid;
}

wxbTreeItemData::wxbTreeItemData(wxString path, wxString name, wxString marked, long listid): wxTreeItemData() {
   this->path = new wxString(path);
   this->name = new wxString(name);
   SetMarked(marked);
   this->listid = listid;
}

wxbTreeItemData::~wxbTreeItemData() {
   delete path;
   delete name;
}

int wxbTreeItemData::GetMarked() {
   return marked;
}

void wxbTreeItemData::SetMarked(wxString marked) {
   if (marked == "*") {
      this->marked = 1;
   }
   else if (marked == "+") {
      this->marked = 2;
   }
   else {
      this->marked = 0;
   }
}

void wxbTreeItemData::SetMarked(int marked) {
   this->marked = marked;
}

long wxbTreeItemData::GetListId() {
   return listid;
}

wxString wxbTreeItemData::GetPath() {
   return *path;
}

wxString wxbTreeItemData::GetName() {
   return *name;
}

/*wxbTreeItemData* wxbTreeItemData::GetChild(wxString dirname) {
   int marked = GetMarkedStatus(dirname);
   return new wxbTreeItemData(path + (marked ? dirname.Mid(1) : dirname), marked);
}*/

int wxbTreeItemData::GetMarkedStatus(wxString file) {
   if (file.Length() == 0)
      return 0;
   
   switch (file.GetChar(0)) {
       case '*':
          return 1;
       case '+':
          return 2;
       default:
          return 0;
    }
}

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

enum
{
   RestoreStart = 1,
   RestoreCancel = 2,
   TreeCtrl = 3,
   ListCtrl = 4,
   ConfigOk = 5,
   ConfigApply = 6,
   ConfigCancel = 7,
   ConfigWhere = 8,
   ConfigReplace = 9,
   ConfigWhen = 10,
   ConfigPriority = 11,
   ConfigClient = 12,
   ConfigFileset = 13,
   ConfigStorage = 14,
   ConfigJobName = 15,
   ConfigPool = 16,
   TreeAdd = 17,
   TreeRemove = 18,
   TreeRefresh = 19,
   ListAdd = 20,
   ListRemove = 21,
   ListRefresh = 22
};

BEGIN_EVENT_TABLE(wxbRestorePanel, wxPanel)
   EVT_BUTTON(RestoreStart, wxbRestorePanel::OnStart)
   EVT_BUTTON(RestoreCancel, wxbRestorePanel::OnCancel)
   
   EVT_TREE_SEL_CHANGING(TreeCtrl, wxbRestorePanel::OnTreeChanging)
   EVT_TREE_SEL_CHANGED(TreeCtrl, wxbRestorePanel::OnTreeChanged)
   EVT_TREE_ITEM_EXPANDING(TreeCtrl, wxbRestorePanel::OnTreeExpanding)
   EVT_TREE_MARKED_EVENT(TreeCtrl, wxbRestorePanel::OnTreeMarked)
   EVT_BUTTON(TreeAdd, wxbRestorePanel::OnTreeAdd)
   EVT_BUTTON(TreeRemove, wxbRestorePanel::OnTreeRemove)
   EVT_BUTTON(TreeRefresh, wxbRestorePanel::OnTreeRefresh)

   EVT_LIST_ITEM_ACTIVATED(ListCtrl, wxbRestorePanel::OnListActivated)
   EVT_LIST_MARKED_EVENT(ListCtrl, wxbRestorePanel::OnListMarked)
   EVT_LIST_ITEM_SELECTED(ListCtrl, wxbRestorePanel::OnListChanged)
   EVT_LIST_ITEM_DESELECTED(ListCtrl, wxbRestorePanel::OnListChanged)
   EVT_BUTTON(ListAdd, wxbRestorePanel::OnListAdd)
   EVT_BUTTON(ListRemove, wxbRestorePanel::OnListRemove)
   EVT_BUTTON(ListRefresh, wxbRestorePanel::OnListRefresh)
  
   EVT_TEXT(ConfigWhere, wxbRestorePanel::OnConfigUpdated)
   EVT_TEXT(ConfigWhen, wxbRestorePanel::OnConfigUpdated)
   EVT_TEXT(ConfigPriority, wxbRestorePanel::OnConfigUpdated)
   EVT_CHOICE(ConfigWhen, wxbRestorePanel::OnConfigUpdated)
   EVT_CHOICE(ConfigReplace, wxbRestorePanel::OnConfigUpdated)
   EVT_CHOICE(ConfigClient, wxbRestorePanel::OnConfigUpdated)
   EVT_CHOICE(ConfigFileset, wxbRestorePanel::OnConfigUpdated)
   EVT_CHOICE(ConfigStorage, wxbRestorePanel::OnConfigUpdated)
   EVT_CHOICE(ConfigJobName, wxbRestorePanel::OnConfigUpdated)
   EVT_CHOICE(ConfigPool, wxbRestorePanel::OnConfigUpdated)
   
   EVT_BUTTON(ConfigOk, wxbRestorePanel::OnConfigOk)
   EVT_BUTTON(ConfigApply, wxbRestorePanel::OnConfigApply)
   EVT_BUTTON(ConfigCancel, wxbRestorePanel::OnConfigCancel)
END_EVENT_TABLE()

/*
 *  wxbRestorePanel constructor
 */
wxbRestorePanel::wxbRestorePanel(wxWindow* parent): wxbPanel(parent) {
   imagelist = new wxImageList(16, 16, TRUE, 3);
   imagelist->Add(wxIcon(unmarked_xpm));
   imagelist->Add(wxIcon(marked_xpm));
   imagelist->Add(wxIcon(partmarked_xpm));

   wxFlexGridSizer* mainSizer = new wxFlexGridSizer(3, 1, 10, 10);
   mainSizer->AddGrowableCol(0);
   mainSizer->AddGrowableRow(1);

   wxFlexGridSizer *firstSizer = new wxFlexGridSizer(1, 2, 10, 10);

   firstSizer->AddGrowableCol(0);
   firstSizer->AddGrowableRow(0);

   start = new wxButton(this, RestoreStart, "Enter restore mode", wxDefaultPosition, wxSize(150, 30));
   firstSizer->Add(start, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 10);

   cancel = new wxButton(this, RestoreCancel, "Cancel restore", wxDefaultPosition, wxSize(150, 30));
   firstSizer->Add(cancel, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_RIGHT, 10);

   wxString elist[1];

/*   clientChoice = new wxChoice(this, ClientChoice, wxDefaultPosition, wxSize(150, 30), 0, elist);
   firstSizer->Add(clientChoice, 1, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 10);

   jobChoice = new wxChoice(this, -1, wxDefaultPosition, wxSize(150, 30), 0, elist);
   firstSizer->Add(jobChoice, 1, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL, 10);*/

   mainSizer->Add(firstSizer, 1, wxEXPAND, 10);

   treelistPanel = new wxSplitterWindow(this);

   wxPanel* treePanel = new wxPanel(treelistPanel);
   wxFlexGridSizer *treeSizer = new wxFlexGridSizer(2, 1, 0, 0);
   treeSizer->AddGrowableCol(0);
   treeSizer->AddGrowableRow(0);

   tree = new wxbTreeCtrl(treePanel, this, TreeCtrl, wxDefaultPosition, wxSize(200,50));  
   tree->SetImageList(imagelist);
   
   treeSizer->Add(tree, 1, wxEXPAND, 0);
   
   wxBoxSizer *treeCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
   treeadd = new wxButton(treePanel, TreeAdd, "Add", wxDefaultPosition, wxSize(60, 25));
   treeCtrlSizer->Add(treeadd, 0, wxLEFT | wxRIGHT, 3);
   treeremove = new wxButton(treePanel, TreeRemove, "Remove", wxDefaultPosition, wxSize(60, 25));
   treeCtrlSizer->Add(treeremove, 0, wxLEFT | wxRIGHT, 3);
   treerefresh = new wxButton(treePanel, TreeRefresh, "Refresh", wxDefaultPosition, wxSize(60, 25));
   treeCtrlSizer->Add(treerefresh, 0, wxLEFT | wxRIGHT, 3);
   
   treeSizer->Add(treeCtrlSizer, 1, wxALIGN_CENTER_HORIZONTAL, 0);
   
   treePanel->SetSizer(treeSizer);
   
   wxPanel* listPanel = new wxPanel(treelistPanel);
   wxFlexGridSizer *listSizer = new wxFlexGridSizer(2, 1, 0, 0);
   listSizer->AddGrowableCol(0);
   listSizer->AddGrowableRow(0);
   
   list = new wxbListCtrl(listPanel, this, ListCtrl, wxDefaultPosition, wxSize(200,50));
   //treelistSizer->Add(list, 1, wxEXPAND, 10);

   list->SetImageList(imagelist, wxIMAGE_LIST_SMALL);

   wxListItem info;
   info.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_FORMAT);
   info.SetText("M");
   info.SetAlign(wxLIST_FORMAT_CENTER);
   list->InsertColumn(0, info);
   
   info.SetText("Filename");
   info.SetAlign(wxLIST_FORMAT_LEFT);
   list->InsertColumn(1, info);

   info.SetText("Size");
   info.SetAlign(wxLIST_FORMAT_RIGHT);   
   list->InsertColumn(2, info);

   info.SetText("Date");
   info.SetAlign(wxLIST_FORMAT_LEFT);
   list->InsertColumn(3, info);

   info.SetText("Perm.");
   info.SetAlign(wxLIST_FORMAT_LEFT);
   list->InsertColumn(4, info);
   
   info.SetText("User");
   info.SetAlign(wxLIST_FORMAT_RIGHT);
   list->InsertColumn(5, info);
   
   info.SetText("Group");
   info.SetAlign(wxLIST_FORMAT_RIGHT);
   list->InsertColumn(6, info);
    
   listSizer->Add(list, 1, wxEXPAND, 0);
   
   wxBoxSizer *listCtrlSizer = new wxBoxSizer(wxHORIZONTAL);
   listadd = new wxButton(listPanel, ListAdd, "Add", wxDefaultPosition, wxSize(60, 25));
   listCtrlSizer->Add(listadd, 0, wxLEFT | wxRIGHT, 5);
   listremove = new wxButton(listPanel, ListRemove, "Remove", wxDefaultPosition, wxSize(60, 25));
   listCtrlSizer->Add(listremove, 0, wxLEFT | wxRIGHT, 5);
   listrefresh = new wxButton(listPanel, ListRefresh, "Refresh", wxDefaultPosition, wxSize(60, 25));
   listCtrlSizer->Add(listrefresh, 0, wxLEFT | wxRIGHT, 5);
   
   listSizer->Add(listCtrlSizer, 1, wxALIGN_CENTER_HORIZONTAL, 0);
   
   listPanel->SetSizer(listSizer);
   
   treelistPanel->SplitVertically(treePanel, listPanel, 210);
   
   treelistPanel->SetMinimumPaneSize(210);
     
   treelistPanel->Show(false);
   
   wxbConfig* config = new wxbConfig();
   config->Add(new wxbConfigParam("Job Name", ConfigJobName, choice, 0, elist));
   config->Add(new wxbConfigParam("Client", ConfigClient, choice, 0, elist));
   config->Add(new wxbConfigParam("Fileset", ConfigFileset, choice, 0, elist));
   config->Add(new wxbConfigParam("Pool", ConfigPool, choice, 0, elist));
   config->Add(new wxbConfigParam("Storage", ConfigStorage, choice, 0, elist));
   config->Add(new wxbConfigParam("Before", ConfigWhen, choice, 0, elist));
   
   configPanel = new wxbConfigPanel(this, config, "Please configure parameters concerning files to restore :", RestoreStart, RestoreCancel, -1);
   
   configPanel->Show(true);
   configPanel->Enable(false);
   
   config = new wxbConfig();
   config->Add(new wxbConfigParam("Job Name", -1, text, ""));
   config->Add(new wxbConfigParam("Bootstrap", -1, text, ""));
   config->Add(new wxbConfigParam("Where", ConfigWhere, modifiableText, ""));
   wxString erlist[] = {"always", "if newer", "if older", "never"};
   config->Add(new wxbConfigParam("Replace", ConfigReplace, choice, 4, erlist));
   config->Add(new wxbConfigParam("Fileset", ConfigFileset, choice, 0, erlist));
   config->Add(new wxbConfigParam("Client", ConfigClient, choice, 0, erlist));
   config->Add(new wxbConfigParam("Storage", ConfigStorage, choice, 0, erlist));
   config->Add(new wxbConfigParam("When", ConfigWhen, modifiableText, ""));
   config->Add(new wxbConfigParam("Priority", ConfigPriority, modifiableText, ""));
   
   restorePanel = new wxbConfigPanel(this, config, "Please configure parameters concerning files restoration :", ConfigOk, ConfigCancel, ConfigApply);
    
   restorePanel->Show(false);
   
   centerSizer = new wxBoxSizer(wxHORIZONTAL);
   //centerSizer->Add(treelistPanel, 1, wxEXPAND | wxADJUST_MINSIZE);
      
   mainSizer->Add(centerSizer, 1, wxEXPAND, 10);

   gauge = new wxGauge(this, -1, 1, wxDefaultPosition, wxSize(200,20));

   mainSizer->Add(gauge, 1, wxEXPAND, 5);
   gauge->SetValue(0);
   gauge->Enable(false);

   SetSizer(mainSizer);
   mainSizer->SetSizeHints(this);

   SetStatus(disabled);

   for (int i = 0; i < 7; i++) {
      list->SetColumnWidth(i, 70);
   }

   working = false;
   SetCursor(*wxSTANDARD_CURSOR);

   markWhenListingDone = false;
   
   cancelled = 0;
}

/*
 *  wxbRestorePanel destructor
 */
wxbRestorePanel::~wxbRestorePanel() {
   delete imagelist;
}

/*----------------------------------------------------------------------------
   wxbPanel overloadings
  ----------------------------------------------------------------------------*/

wxString wxbRestorePanel::GetTitle() {
   return "Restore";
}

void wxbRestorePanel::EnablePanel(bool enable) {
   if (enable) {
      if (status == disabled) {
         SetStatus(activable);
      }
   }
   else {
      SetStatus(disabled);
   }
}

/*----------------------------------------------------------------------------
   Commands called by events handler
  ----------------------------------------------------------------------------*/

/* The main button has been clicked */
void wxbRestorePanel::CmdStart() {
   unsigned int i;
   if (status == activable) {
      wxbMainFrame::GetInstance()->SetStatusText("Getting parameters list.");
      wxbDataTokenizer* dt = WaitForEnd(".clients\n", true, false);
      wxString str;

      configPanel->ClearRowChoices("Client");
      restorePanel->ClearRowChoices("Client");
      
      if (dt->GetCount() == 0) {
         wxbMainFrame::GetInstance()->SetStatusText("Error : no clients returned by the director.");
         return;
      }
      
      for (i = 0; i < dt->GetCount(); i++) {
         str = (*dt)[i];
         str.RemoveLast();
         configPanel->AddRowChoice("Client", str);
         restorePanel->AddRowChoice("Client", str);
      }
          
      delete dt;
      
      if (cancelled) {
         cancelled = 2;
         return;
      }
      
      dt = WaitForEnd(".filesets\n", true, false);
      
      configPanel->ClearRowChoices("Fileset");
      restorePanel->ClearRowChoices("Fileset");
    
      if (dt->GetCount() == 0) {
         wxbMainFrame::GetInstance()->SetStatusText("Error : no filesets returned by the director.");
         return;
      }
      
      for (i = 0; i < dt->GetCount(); i++) {
         str = (*dt)[i];
         str.RemoveLast();
         configPanel->AddRowChoice("Fileset", str);
         restorePanel->AddRowChoice("Fileset", str);
      }
      
      delete dt;
      
      if (cancelled) {
         cancelled = 2;
         return;
      }
      
      dt = WaitForEnd(".storage\n", true, false);
    
      configPanel->ClearRowChoices("Storage");
      restorePanel->ClearRowChoices("Storage");
    
      if (dt->GetCount() == 0) {
         wxbMainFrame::GetInstance()->SetStatusText("Error : no storage returned by the director.");
         return;
      }
      
      for (i = 0; i < dt->GetCount(); i++) {
         str = (*dt)[i];
         str.RemoveLast();
         configPanel->AddRowChoice("Storage", str);
         restorePanel->AddRowChoice("Storage", str);
      }
      
      delete dt;
      
      if (cancelled) {
         cancelled = 2;
         return;
      }
      
      dt = WaitForEnd(".jobs\n", true, false);
    
      configPanel->ClearRowChoices("Job Name");
    
      if (dt->GetCount() == 0) {
         wxbMainFrame::GetInstance()->SetStatusText("Error : no jobs returned by the director.");
         return;
      }
      
      for (i = 0; i < dt->GetCount(); i++) {
         str = (*dt)[i];
         str.RemoveLast();
         configPanel->AddRowChoice("Job Name", str);
      }
      
      configPanel->SetRowString("Job Name", "RestoreFiles");
      
      delete dt;
      
      if (cancelled) {
         cancelled = 2;
         return;
      }
      
      dt = WaitForEnd(".pools\n", true, false);
    
      configPanel->ClearRowChoices("Pool");
    
      if (dt->GetCount() == 0) {
         wxbMainFrame::GetInstance()->SetStatusText("Error : no jobs returned by the director.");
         return;
      }
      
      for (i = 0; i < dt->GetCount(); i++) {
         str = (*dt)[i];
         str.RemoveLast();
         configPanel->AddRowChoice("Pool", str);
      }
         
      delete dt; 
      
      if (cancelled) {
         cancelled = 2;
         return;
      }

      SetStatus(entered);

      UpdateFirstConfig();
           
      wxbMainFrame::GetInstance()->SetStatusText("Please configure your restore parameters.");
   }
   else if (status == entered) {
/*      if (clientChoice->GetStringSelection().Length() < 1) {
         wxbMainFrame::GetInstance()->SetStatusText("Please select a client.");
         return;
      }
      if (jobChoice->GetStringSelection().Length() < 1) {
         wxbMainFrame::GetInstance()->SetStatusText("Please select a restore date.");
         return;
      }*/
      wxbMainFrame::GetInstance()->SetStatusText("Building restore tree...");
      
      SetStatus(choosing);
      
      WaitForPrompt(wxString("restore") <<
         " client=\"" << configPanel->GetRowString("Client") <<
         "\" fileset=\"" << configPanel->GetRowString("Fileset") <<
         "\" pool=\"" << configPanel->GetRowString("Pool") <<
         "\" storage=\"" << configPanel->GetRowString("Storage") << "\"\n");
      WaitForPrompt("6\n");
      //WaitForEnd();
      /*wxbPromptParser *pp = WaitForPrompt(wxString() << configPanel->GetRowString("Before") << "\n", true);
      int client = pp->getChoices()->Index(configPanel->GetRowString("Client"));
      if (client == wxNOT_FOUND) {
         wxbMainFrame::GetInstance()->SetStatusText("Failed to find the selected client.");
         return;
      }
      delete pp;*/
      
      wxbTableParser* tableparser = new wxbTableParser();
      wxbDataTokenizer* dt = new wxbDataTokenizer(true);
      
      wxbMainFrame::GetInstance()->Send(wxString() << configPanel->GetRowString("Before") << "\n");
   
      while (!tableparser->hasFinished() && !dt->hasFinished()) {
         wxTheApp->Yield(true);
         ::wxUsleep(100);
      }
      
      wxString str;

      if (dt->hasFinished() && !tableparser->hasFinished()) {
         str = "";
         if (dt->GetCount() > 1) {
            str = (*dt)[dt->GetCount()-2];
            str.RemoveLast();
         }
         wxbMainFrame::GetInstance()->SetStatusText(wxString("Error while starting restore: ") << str);
         delete dt;
         delete tableparser;
         SetStatus(finished);
         return;
      }
           
      int tot = 0;
      long l;
      
      for (i = 0; i < tableparser->GetCount(); i++) {
         str = (*tableparser)[i][2];
         str.Replace(",", "");
         if (str.ToLong(&l)) {
            tot += l;
         }
      }
           
      gauge->SetValue(0);
      gauge->SetRange(tot);
      
      /*wxbMainFrame::GetInstance()->Print(
               wxString("[") << tot << "]", CS_DEBUG);*/
      
      wxDateTime base = wxDateTime::Now();
      wxDateTime newdate;
      int done = 0;
      int willdo = 0;
      unsigned int lastindex = 0;
      
      int var = 0;
      
      while (!dt->hasFinished()) {
         newdate = wxDateTime::Now();
         if (newdate.Subtract(base).GetMilliseconds() > 10 ) {
            base = newdate;
            for (; lastindex < dt->GetCount(); lastindex++) {
               if (((*dt)[lastindex].Find("Building directory tree for JobId ") == 0) && 
                     ((i = (*dt)[lastindex].Find(" ...")) > 0)) {
                  str = (*dt)[lastindex].Mid(34, i-34);
                  for (i = 0; i < tableparser->GetCount(); i++) {
                     if (str == (*tableparser)[i][0]) {
                        str = (*tableparser)[i][2];
                        str.Replace(",", "");
                        if (str.ToLong(&l)) {
                           done += willdo;
                           willdo += l;
                           var = (willdo-done)/3;
                        }
                        break;
                     }
                  }
               }
            }
            
            if (gauge->GetValue() <= done) {
               gauge->SetValue(done);
               if (var < 0)
                  var = -var;
            }
            else if (gauge->GetValue() >= willdo) {
               gauge->SetValue(willdo);
               if (var > 0)
                  var = -var;
            }
            
            gauge->SetValue(gauge->GetValue()+var);
            
            /*wxbMainFrame::GetInstance()->Print(
               wxString("[") << gauge->GetValue() << "/" << done
                  << "-" << willdo << "]", CS_DEBUG);*/
         }
         wxTheApp->Yield(true);
         ::wxUsleep(1);
      }

      gauge->SetValue(tot);
      wxTheApp->Yield(true);
      gauge->SetValue(0);
      
      delete dt;
      delete tableparser;

      if (cancelled) {
         cancelled = 2;
         return;
      }

      WaitForEnd("unmark *\n");
      wxTreeItemId root = tree->AddRoot(configPanel->GetRowString("Client"), -1, -1, new wxbTreeItemData("/", configPanel->GetRowString("Client"), 0));
      currentTreeItem = root;
      tree->Refresh();
      tree->SelectItem(root);
      CmdList(root);
      wxbMainFrame::GetInstance()->SetStatusText("Right click on a file or on a directory, or double-click on its mark to add it to the restore list.");
      tree->Expand(root);
   }
   else if (status == choosing) {
      EnableConfig(false);
      
      totfilemessages = 0;
      wxbDataTokenizer* dt;
           
      int j;
      
      dt = new wxbDataTokenizer(true);
      WaitForPrompt("done\n");

      SetStatus(configuring);

      for (i = 0; i < dt->GetCount(); i++) {
         if ((j = (*dt)[i].Find(" files selected to be restored.")) > -1) {
            (*dt)[i].Mid(0, j).ToLong(&totfilemessages);
            break;
         }

         if ((j = (*dt)[i].Find(" file selected to be restored.")) > -1) {
            (*dt)[i].Mid(0, j).ToLong(&totfilemessages);
            break;
         }
      }
      
      wxbMainFrame::GetInstance()->SetStatusText(
         wxString("Please configure your restore (") 
            << totfilemessages <<  " files selected to be restored)...");
      
      UpdateSecondConfig(dt);
      
      delete dt;
      
      EnableConfig(true);
      restorePanel->EnableApply(false);

      if (totfilemessages == 0) {
         wxbMainFrame::GetInstance()->Print("Restore failed : no file selected.\n", CS_DEBUG);
         wxbMainFrame::GetInstance()->SetStatusText("Restore failed : no file selected.");
         SetStatus(finished);
         return;
      }
   }
   else if (status == configuring) {
      cancel->Enable(false);
      jobid = "";
      EnableConfig(false);
    
      wxbMainFrame::GetInstance()->SetStatusText("Restoring, please wait...");
    
      wxbDataTokenizer* dt;
    
      SetStatus(restoring);
      WaitForEnd("yes\n");

      gauge->SetValue(0);
      gauge->SetRange(totfilemessages);

      wxDateTime currenttime;
      
      dt = WaitForEnd("time\n", true);
      wxStringTokenizer ttkz((*dt)[0], " ", wxTOKEN_STRTOK);
      if ((currenttime.ParseDate(ttkz.GetNextToken()) == NULL) || // Date
           (currenttime.ParseTime(ttkz.GetNextToken()) == NULL)) { // Time
         currenttime.SetYear(1990); // If parsing fails, set currenttime to a dummy date
      }
      else {
         currenttime -= wxTimeSpan::Seconds(30); //Adding a 30" tolerance
      }
      delete dt;
    
      wxDateTime scheduledtime;
      wxStringTokenizer stkz(restorePanel->GetRowString("When"), " ", wxTOKEN_STRTOK);
      
      if ((scheduledtime.ParseDate(stkz.GetNextToken()) == NULL) || // Date
           (scheduledtime.ParseTime(stkz.GetNextToken()) == NULL)) { // Time
         scheduledtime.SetYear(2090); // If parsing fails, set scheduledtime to a dummy date
      }

      if (scheduledtime.Subtract(currenttime).IsLongerThan(wxTimeSpan::Seconds(150))) {
         wxbMainFrame::GetInstance()->Print("Restore is scheduled in more than two minutes, wx-console will not wait for its completion.\n", CS_DEBUG);
         wxbMainFrame::GetInstance()->SetStatusText("Restore is scheduled in more than two minutes, wx-console will not wait for its completion.");
         SetStatus(finished);
         return;
      }

      wxString cmd = "list jobid=";

      wxString jobname = restorePanel->GetRowString("Job Name");

      wxStopWatch sw;

      wxbTableParser* tableparser;

      while (true) {
         tableparser = CreateAndWaitForParser("list jobs\n");
         
         wxDateTime jobtime;
         
         if (jobname == (*tableparser)[tableparser->GetCount()-1][1]) {
            wxStringTokenizer jtkz((*tableparser)[tableparser->GetCount()-1][2], " ", wxTOKEN_STRTOK);
            if ((jobtime.ParseDate(jtkz.GetNextToken()) != NULL) && // Date
                  (jobtime.ParseTime(jtkz.GetNextToken()) != NULL)) { // Time
               if (jobtime.IsLaterThan(currenttime)) {
                  jobid = (*tableparser)[tableparser->GetCount()-1][0];
                  cmd << jobid << "\n";
                  delete tableparser;
                  cancel->Enable(true);
                  break;
               }
            }
         }
   
         delete tableparser;
         
         wxStopWatch sw2;
         while (sw2.Time() < 2000) {
            wxTheApp->Yield(true);
            ::wxUsleep(100);
         }
         if (sw.Time() > 60000) {
            wxbMainFrame::GetInstance()->Print("The restore job has not been created within one minute, wx-console will not wait for its completion anymore.\n", CS_DEBUG);
            wxbMainFrame::GetInstance()->SetStatusText("The restore job has not been created within one minute, wx-console will not wait for its completion anymore.");
            SetStatus(finished);
            cancel->Enable(true);
            return;
         }
      }
      
      long filemessages = 0;

      while (true) {
         tableparser = CreateAndWaitForParser(cmd);
         if ((*tableparser)[0][7] != "C") {
            break;
         }
         delete tableparser;

         dt = WaitForEnd("messages\n", true);
         
         for (unsigned int i = 0; i < dt->GetCount(); i++) {
            wxStringTokenizer tkz((*dt)[i], " ", wxTOKEN_STRTOK);
   
            wxDateTime datetime;
   
            //   Date    Time   name:   perm      ?   user   grp      size    date     time
            //04-Apr-2004 17:19 Tom-fd: -rwx------   1 nicolas  None     514967 2004-03-20 20:03:42  filename
   
            if (datetime.ParseDate(tkz.GetNextToken()) != NULL) { // Date
               if (datetime.ParseTime(tkz.GetNextToken()) != NULL) { // Time
                  if (tkz.GetNextToken().Last() == ':') { // name:
                  tkz.GetNextToken(); // perm
                  tkz.GetNextToken(); // ?
                  tkz.GetNextToken(); // user
                  tkz.GetNextToken(); // grp
                  tkz.GetNextToken(); // size
                  if (datetime.ParseDate(tkz.GetNextToken()) != NULL) { //date
                        if (datetime.ParseTime(tkz.GetNextToken()) != NULL) { //time
                           filemessages++;
                           //wxbMainFrame::GetInstance()->Print(wxString("(") << filemessages << ")", CS_DEBUG);
                           gauge->SetValue(filemessages);
                        }
                     }
                  }
               }
            }
         }
         
         delete dt;

         wxbMainFrame::GetInstance()->SetStatusText(wxString("Restoring, please wait (") << filemessages << " of " << totfilemessages << " files done)...");

         wxStopWatch sw2;
         while (sw2.Time() < 10000) {  
            wxTheApp->Yield(true);
            ::wxUsleep(100);
         }
      }

      WaitForEnd("messages\n");

      gauge->SetValue(totfilemessages);

      if ((*tableparser)[0][7] == "T") {
         wxbMainFrame::GetInstance()->Print("Restore done successfully.\n", CS_DEBUG);
         wxbMainFrame::GetInstance()->SetStatusText("Restore done successfully.");
      }
      else {
         wxbMainFrame::GetInstance()->Print("Restore failed, please look at messages.\n", CS_DEBUG);
         wxbMainFrame::GetInstance()->SetStatusText("Restore failed, please look at messages in console.");
      }
      delete tableparser;
      SetStatus(finished);
   }
}

/* The cancel button has been clicked */
void wxbRestorePanel::CmdCancel() {
   cancelled = 1;
   
   if (status == restoring) {
      if (jobid != "") {
         wxbMainFrame::GetInstance()->Send(wxString("cancel job=") << jobid << "\n");
      }
      cancel->Enable(true);
      return;
   }
   
   wxStopWatch sw;
   while ((working) && (cancelled != 2)) {
      wxTheApp->Yield(true);
      ::wxUsleep(100);
      if (sw.Time() > 30000) { /* 30 seconds timeout */
         if (status == choosing) {
            wxbMainFrame::GetInstance()->Send("quit\n");
         }
         else if (status == configuring) {
            wxbMainFrame::GetInstance()->Send("no\n");
         }
         else if (status == restoring) {
            
         }
         SetStatus(finished);
         ::wxUsleep(1000);
         return;
      }
   }
   
   switch (status) {
   case choosing:
      wxbMainFrame::GetInstance()->Send("quit\n");      
      break;
   case configuring:
      wxbMainFrame::GetInstance()->Send("no\n");      
      break;
   default:
      break;
   }
   ::wxUsleep(1000);
   SetStatus(finished);
}

/* Apply configuration changes */

/*   1: Level (not appropriate)
 *   2: Storage (yes)
 *   3: Job (no)
 *   4: FileSet (yes)
 *   5: Client (yes)
 *   6: When (yes : "Please enter desired start time as YYYY-MM-DD HH:MM:SS (return for now):")
 *   7: Priority (yes : "Enter new Priority: (positive integer)")
 *   8: Bootstrap (?)
 *   9: Where (yes : "Please enter path prefix for restore (/ for none):")
 *  10: Replace (yes : "Replace:\n 1: always\n 2: ifnewer\n 3: ifolder\n 4: never\n 
 *         Select replace option (1-4):")
 *  11: JobId (no)
 */

void wxbRestorePanel::CmdConfigApply() {
   if (cfgUpdated == 0) return;
   
   wxbMainFrame::GetInstance()->SetStatusText("Applying restore configuration changes...");
   
   EnableConfig(false);
   
   wxbDataTokenizer* dt = NULL;
   
   bool failed = false;
   
   while (cfgUpdated > 0) {
      if (cancelled) {
         cancelled = 2;
         return;
      }
      wxString def; //String to send if can't use our data
      if ((cfgUpdated >> ConfigWhere) & 1) {
         WaitForPrompt("mod\n"); /* TODO: check results */
         WaitForPrompt("9\n");
         dt = new wxbDataTokenizer(true);
         WaitForPrompt(restorePanel->GetRowString("Where") + "\n");
         def = "/tmp";
         cfgUpdated = cfgUpdated & (~(1 << ConfigWhere));
      }
      else if ((cfgUpdated >> ConfigReplace) & 1) {
         WaitForPrompt("mod\n"); /* TODO: check results */
         WaitForPrompt("10\n");
         dt = new wxbDataTokenizer(true);
         WaitForPrompt(wxString() << (restorePanel->GetRowSelection("Replace")+1) << "\n");
         def = "1";
         cfgUpdated = cfgUpdated & (~(1 << ConfigReplace));
      }
      else if ((cfgUpdated >> ConfigWhen) & 1) {
         WaitForPrompt("mod\n"); /* TODO: check results */
         WaitForPrompt("6\n");
         dt = new wxbDataTokenizer(true);
         WaitForPrompt(restorePanel->GetRowString("When") + "\n");
         def = "";
         cfgUpdated = cfgUpdated & (~(1 << ConfigWhen));
      }
      else if ((cfgUpdated >> ConfigPriority) & 1) {
         WaitForPrompt("mod\n"); /* TODO: check results */
         WaitForPrompt("7\n");
         dt = new wxbDataTokenizer(true);
         WaitForPrompt(restorePanel->GetRowString("Priority") + "\n");
         def = "10";
         cfgUpdated = cfgUpdated & (~(1 << ConfigPriority));
      }
      else if ((cfgUpdated >> ConfigClient) & 1) {
         WaitForPrompt("mod\n"); /* TODO: check results */
         wxbPromptParser *pp = WaitForPrompt("5\n", true);
         int client = pp->getChoices()->Index(restorePanel->GetRowString("Client"));
         if (client == wxNOT_FOUND) {
            wxbMainFrame::GetInstance()->SetStatusText("Failed to find the selected client.");
            failed = true;
            client = 1;
         }
         delete pp;
         dt = new wxbDataTokenizer(true);
         WaitForPrompt(wxString() << client << "\n");
         def = "1";
         cfgUpdated = cfgUpdated & (~(1 << ConfigClient));
      }
      else if ((cfgUpdated >> ConfigFileset) & 1) {
         WaitForPrompt("mod\n"); /* TODO: check results */
         wxbPromptParser *pp = WaitForPrompt("4\n", true);
         int fileset = pp->getChoices()->Index(restorePanel->GetRowString("Fileset"));
         if (fileset == wxNOT_FOUND) {
            wxbMainFrame::GetInstance()->SetStatusText("Failed to find the selected fileset.");
            failed = true;
            fileset = 1;
         }
         delete pp;
         dt = new wxbDataTokenizer(true);
         WaitForPrompt(wxString() << fileset << "\n");
         def = "1";
         cfgUpdated = cfgUpdated & (~(1 << ConfigFileset));
      }
      else if ((cfgUpdated >> ConfigStorage) & 1) {
         WaitForPrompt("mod\n"); /* TODO: check results */
         wxbPromptParser *pp = WaitForPrompt("2\n", true);
         int fileset = pp->getChoices()->Index(restorePanel->GetRowString("Storage"));
         if (fileset == wxNOT_FOUND) {
            wxbMainFrame::GetInstance()->SetStatusText("Failed to find the selected storage.");
            failed = true;
            fileset = 1;
         }
         delete pp;
         dt = new wxbDataTokenizer(true);
         WaitForPrompt(wxString() << fileset << "\n");
         def = "1";
         cfgUpdated = cfgUpdated & (~(1 << ConfigFileset));
      }
      else {
         cfgUpdated = 0;
         break;
      }
                 
      unsigned int i;
      for (i = 0; i < dt->GetCount(); i++) {
         if ((*dt)[i].Find("Run Restore job") == 0) {
            break;
         }
      }
      
      if (i == dt->GetCount()) {
         delete dt;   
         dt = WaitForEnd(def + "\n", true);
         failed = true;
      }
   }
   UpdateSecondConfig(dt); /* TODO: Check result */
   
   EnableConfig(true);

   if (!failed) {
      wxbMainFrame::GetInstance()->SetStatusText("Restore configuration changes were applied.");
   }

   delete dt;
}

/* Cancel restore */
void wxbRestorePanel::CmdConfigCancel() {
   WaitForEnd("no\n");
   wxbMainFrame::GetInstance()->Print("Restore cancelled.\n", CS_DEBUG);
   wxbMainFrame::GetInstance()->SetStatusText("Restore cancelled.");
   SetStatus(finished);
}

/* List jobs for a specified client */
void wxbRestorePanel::CmdListJobs() {
   if (status == entered) {
      configPanel->ClearRowChoices("Before");
      WaitForPrompt("query\n");
      WaitForPrompt("6\n");
      wxbTableParser* tableparser = CreateAndWaitForParser(configPanel->GetRowString("Client") + "\n");

      for (int i = tableparser->GetCount()-1; i > -1; i--) {
         wxString str = (*tableparser)[i][3];
         wxDateTime datetime;
         const char* chr;
         if ( ( (chr = datetime.ParseDate(str.GetData()) ) != NULL ) && ( datetime.ParseTime(++chr) != NULL ) ) {
            datetime += wxTimeSpan::Seconds(1);
            //wxbMainFrame::GetInstance()->Print(wxString("-") << datetime.Format("%Y-%m-%d %H:%M:%S"), CS_DEBUG);
            configPanel->AddRowChoice("Before", datetime.Format("%Y-%m-%d %H:%M:%S"));
         }
         /*else {
         jobChoice->Append("Invalid");
         }*/
      }
      
      delete tableparser;

      configPanel->SetRowSelection("Before", 0);
   }
}

/* List files and directories for a specified tree item */
void wxbRestorePanel::CmdList(wxTreeItemId item) {
   if (status == choosing) {
      list->DeleteAllItems();

      if (!item.IsOk()) {
         return;
      }
      UpdateTreeItem(item, (tree->GetSelection() == item), false);
    
      if (list->GetItemCount() >= 1) {
         int firstwidth = list->GetSize().GetWidth(); 
         for (int i = 2; i < 7; i++) {
            list->SetColumnWidth(i, wxLIST_AUTOSIZE);
            firstwidth -= list->GetColumnWidth(i);
         }
       
         list->SetColumnWidth(0, 18);
         firstwidth -= 18;
         list->SetColumnWidth(1, wxLIST_AUTOSIZE);
         if (list->GetColumnWidth(1) < firstwidth) {
            list->SetColumnWidth(1, firstwidth-25);
         }
      }
   }
}

/* Mark a treeitem (directory) or a listitem (file or directory) */
void wxbRestorePanel::CmdMark(wxTreeItemId treeitem, long* listitems, int listsize, int state) {
   if (status == choosing) {
      wxbTreeItemData** itemdata;
      int itemdatasize = 0;
      if (listsize == 0) {
         itemdata = new wxbTreeItemData*[1];
         itemdatasize = 1;
      }
      else {
         itemdata = new wxbTreeItemData*[listsize];
         itemdatasize = listsize;
      }
      
      if (listitems != NULL) {
         for (int i = 0; i < listsize; i++) {
            itemdata[i] = (wxbTreeItemData*)list->GetItemData(listitems[i]);
         }
      }
      else if (treeitem.IsOk()) {
         itemdata[0] = (wxbTreeItemData*)tree->GetItemData(treeitem);
      }
      else {
         delete[] itemdata;
         return;
      }

      if (itemdata[0] == NULL) { //Should never happen
         delete[] itemdata;
         return;
      }

      wxString dir = itemdata[0]->GetPath();
      wxString file;

      if (dir != "/") {
         if (dir.GetChar(dir.Length()-1) == '/') {
            dir.RemoveLast();
         }

         int i = dir.Find('/', TRUE);
         if (i == -1) {
            file = dir;
            dir = "/";
         }
         else { /* first dir below root */
            file = dir.Mid(i+1);
            dir = dir.Mid(0, i+1);
         }
      }
      else {
         dir = "/";
         file = "*";
      }

      if (state == -1) {
         bool marked = false;
         bool unmarked = false;
         state = 0;
         for (int i = 0; i < itemdatasize; i++) {
            switch(itemdata[i]->GetMarked()) {
            case 0:
               unmarked = true;
               break;
            case 1:
               marked = true;
               break;
            case 2:
               marked = true;
               unmarked = true;
               break;
            default:
               break;
            }
            if (marked && unmarked)
               break;
         }
         if (marked) {
            if (unmarked) {
               state = 1;
            }
            else {
               state = 0;
            }
         }
         else {
            state = 1;
         }
      }

      WaitForEnd(wxString("cd \"") << dir << "\"\n");
      WaitForEnd(wxString((state==1) ? "mark" : "unmark") << " \"" << file << "\"\n");

      /* TODO: Check commands results */

      /*if ((dir == "/") && (file == "*")) {
            itemdata->SetMarked((itemdata->GetMarked() == 1) ? 0 : 1);
      }*/

      if (listitems == NULL) { /* tree item state changed */
         SetTreeItemState(treeitem, state);
         /*treeitem = tree->GetSelection();
         UpdateTree(treeitem, true);
         treeitem = tree->GetItemParent(treeitem);*/
      }
      else {
         for (int i = 0; i < listsize; i++) {
            SetListItemState(listitems[i], state);
         }
         /*UpdateTree(treeitem, (tree->GetSelection() == treeitem));
         treeitem = tree->GetItemParent(treeitem);*/
      }

      /*while (treeitem.IsOk()) {
         WaitForList(treeitem, false);
         treeitem = tree->GetItemParent(treeitem);
      }*/
      
      delete[] itemdata;
   }
}

/*----------------------------------------------------------------------------
   General functions
  ----------------------------------------------------------------------------*/

/* Parse a table in tableParser */
wxbTableParser* wxbRestorePanel::CreateAndWaitForParser(wxString cmd) {
   wxbTableParser* tableParser = new wxbTableParser();

   wxbMainFrame::GetInstance()->Send(cmd);

   //time_t base = wxDateTime::Now().GetTicks();
   while (!tableParser->hasFinished()) {
      //innerThread->Yield();
      wxTheApp->Yield(true);
      ::wxUsleep(100);
      //if (base+15 < wxDateTime::Now().GetTicks()) break;
   }
   return tableParser;
}

/* Run a command, and waits until prompt result is fully received,
 * if keepresults is true, returns a valid pointer to a wxbPromptParser
 * containing the data. */
wxbPromptParser* wxbRestorePanel::WaitForPrompt(wxString cmd, bool keepresults) {
   wxbPromptParser* promptParser = new wxbPromptParser();
   
   wxbMainFrame::GetInstance()->Send(cmd);
    
   //time_t base = wxDateTime::Now().GetTicks();
   while (!promptParser->hasFinished()) {
      //innerThread->Yield();
      wxTheApp->Yield(true);
      ::wxUsleep(100);
      //if (base+15 < wxDateTime::Now().GetTicks()) break;
   }
     
   if (keepresults) {
      return promptParser;
   }
   else {
      delete promptParser;
      return NULL;
   }  
}

/* Run a command, and waits until result is fully received. */
wxbDataTokenizer* wxbRestorePanel::WaitForEnd(wxString cmd, bool keepresults, bool linebyline) {
   wxbDataTokenizer* datatokenizer = new wxbDataTokenizer(linebyline);

   wxbMainFrame::GetInstance()->Send(cmd);
   
   //wxbMainFrame::GetInstance()->Print("(<WFE)", CS_DEBUG);
   
   //time_t base = wxDateTime::Now().GetTicks();
   while (!datatokenizer->hasFinished()) {
      //innerThread->Yield();
      wxTheApp->Yield(true);
      ::wxUsleep(100);
      //if (base+15 < wxDateTime::Now().GetTicks()) break;
   }
   
   //wxbMainFrame::GetInstance()->Print("(>WFE)", CS_DEBUG);
   
   if (keepresults) {
      return datatokenizer;
   }
   else {
      delete datatokenizer;
      return NULL;
   }
}

/* Run a dir command, and waits until result is fully received. */
void wxbRestorePanel::UpdateTreeItem(wxTreeItemId item, bool updatelist, bool recurse) {
//   this->updatelist = updatelist;
   wxbDataTokenizer* dt;

   dt = WaitForEnd(wxString("cd \"") << 
      static_cast<wxbTreeItemData*>(tree->GetItemData(item))
         ->GetPath() << "\"\n", false);

   /* TODO: check command result */
   
   //delete dt;

   status = listing;

   if (updatelist)
      list->DeleteAllItems();
   dt = WaitForEnd("dir\n", true);
   
   wxString str;
   
   for (unsigned int i = 0; i < dt->GetCount(); i++) {
      str = (*dt)[i];
      
      if (str.Find("cwd is:") == 0) { // Sometimes cd command result "infiltrate" into listings.
         break;
      }

      str.RemoveLast();

      wxString* file = ParseList(str);
      
      if (file == NULL)
            break;

      wxTreeItemId treeid;

      if (file[8].GetChar(file[8].Length()-1) == '/') {
         wxString itemStr;

         long cookie;
         treeid = tree->GetFirstChild(item, cookie);

         bool updated = false;

         while (treeid.IsOk()) {
            itemStr = tree->GetItemText(treeid);
            if (file[8] == itemStr) {
               int stat = wxbTreeItemData::GetMarkedStatus(file[6]);
               if (static_cast<wxbTreeItemData*>(tree->GetItemData(treeid))->GetMarked() != stat) {
                  tree->SetItemImage(treeid, stat, wxTreeItemIcon_Normal);
                  tree->SetItemImage(treeid, stat, wxTreeItemIcon_Selected);
                  static_cast<wxbTreeItemData*>(tree->GetItemData(treeid))->SetMarked(file[6]);
               }
               if ((recurse) && (tree->IsExpanded(treeid))) {
                  UpdateTreeItem(treeid, false, true);
               }
               updated = true;
               break;
            }
            treeid = tree->GetNextChild(item, cookie);
         }

         if (!updated) {
            int img = wxbTreeItemData::GetMarkedStatus(file[6]);
            treeid = tree->AppendItem(item, file[8], img, img, new wxbTreeItemData(file[7], file[8], file[6]));
         }
      }

      if (updatelist) {
         long ind = list->InsertItem(list->GetItemCount(), wxbTreeItemData::GetMarkedStatus(file[6]));
         wxbTreeItemData* data = new wxbTreeItemData(file[7], file[8], file[6], ind);
         data->SetId(treeid);
         list->SetItemData(ind, (long)data);
         list->SetItem(ind, 1, file[8]); // filename
         list->SetItem(ind, 2, file[4]); //Size
         list->SetItem(ind, 3, file[5]); //date
         list->SetItem(ind, 4, file[0]); //perm
         list->SetItem(ind, 5, file[2]); //user
         list->SetItem(ind, 6, file[3]); //grp
      }

      delete[] file;
   }
   
   delete dt;
   
   tree->Refresh();
   status = choosing;
}

/* Parse dir command results. */
wxString* wxbRestorePanel::ParseList(wxString line) {
   //drwx------  11 1003    42949672      0  2001-07-30 16:45:14 *filename
   //+ 10    ++ 4++   10   ++   8  ++   8  + +      19         + *+ ->
   //0       10  14         24      32       42                  62

   if (line.Length() < 63)
      return NULL;

   wxString* ret = new wxString[9];

   ret[0] = line.Mid(0, 10).Trim();
   ret[1] = line.Mid(10, 4).Trim();
   ret[2] = line.Mid(14, 10).Trim();
   ret[3] = line.Mid(24, 8).Trim();
   ret[4] = line.Mid(32, 8).Trim();
   ret[5] = line.Mid(42, 19).Trim();
   ret[6] = line.Mid(62, 1);
   ret[7] = line.Mid(63).Trim();

   if (ret[6] == " ") ret[6] = "";

   if (ret[7].GetChar(ret[7].Length()-1) == '/') {
      ret[8] = ret[7];
      ret[8].RemoveLast();
      ret[8] = ret[7].Mid(ret[8].Find('/', true)+1);
   }
   else {
      ret[8] = ret[7].Mid(ret[7].Find('/', true)+1);
   }

   return ret;
}

/* Sets a list item state, and update its parents and children if it is a directory */
void wxbRestorePanel::SetListItemState(long listitem, int newstate) {
   wxbTreeItemData* itemdata = (wxbTreeItemData*)list->GetItemData(listitem);
   
   wxTreeItemId treeitem;
   
   itemdata->SetMarked(newstate);
   list->SetItemImage(listitem, newstate, 0); /* TODO: Find what these ints are for */
   list->SetItemImage(listitem, newstate, 1);
      
   if ((treeitem = itemdata->GetId()).IsOk()) {
      SetTreeItemState(treeitem, newstate);
   }
   else {
      UpdateTreeItemState(tree->GetSelection());
   }
}

/* Sets a tree item state, and update its children, parents and list (if necessary) */
void wxbRestorePanel::SetTreeItemState(wxTreeItemId item, int newstate) {
   long cookie;
   wxTreeItemId currentChild = tree->GetFirstChild(item, cookie);

   wxbTreeItemData* itemdata;

   while (currentChild.IsOk()) {
      itemdata = (wxbTreeItemData*)tree->GetItemData(currentChild);
      int state = itemdata->GetMarked();
      
      if (state != newstate) {
         itemdata->SetMarked(newstate);
         tree->SetItemImage(currentChild, newstate, wxTreeItemIcon_Normal);
         tree->SetItemImage(currentChild, newstate, wxTreeItemIcon_Selected);
      }
      
      currentChild = tree->GetNextChild(item, cookie);
   }
     
   itemdata = (wxbTreeItemData*)tree->GetItemData(item);  
   itemdata->SetMarked(newstate);
   tree->SetItemImage(item, newstate, wxTreeItemIcon_Normal);
   tree->SetItemImage(item, newstate, wxTreeItemIcon_Selected);
   tree->Refresh();
   
   if (tree->GetSelection() == item) {
      for (long i = 0; i < list->GetItemCount(); i++) {
         list->SetItemImage(i, newstate, 0); /* TODO: Find what these ints are for */
         list->SetItemImage(i, newstate, 1);
      }
   }
   
   UpdateTreeItemState(tree->GetItemParent(item));
}

/* Update a tree item state, and its parents' state */
void wxbRestorePanel::UpdateTreeItemState(wxTreeItemId item) {  
   if (!item.IsOk()) {
      return;
   }
   
   int state = 0;
       
   long cookie;
   wxTreeItemId currentChild = tree->GetFirstChild(item, cookie);

   bool onechildmarked = false;
   bool onechildunmarked = false;

   while (currentChild.IsOk()) {
      state = ((wxbTreeItemData*)tree->GetItemData(currentChild))->GetMarked();
      switch (state) {
      case 0:
         onechildunmarked = true;
         break;
      case 1:
         onechildmarked = true;
         break;
      case 2:
         onechildmarked = true;
         onechildunmarked = true;
         break;
      }
      
      if (onechildmarked && onechildunmarked) {
         break;
      }
      
      currentChild = tree->GetNextChild(item, cookie);
   }
   
   if (tree->GetSelection() == item) {
      for (long i = 0; i < list->GetItemCount(); i++) {
         state = ((wxbTreeItemData*)list->GetItemData(i))->GetMarked();
         
         switch (state) {
         case 0:
            onechildunmarked = true;
            break;
         case 1:
            onechildmarked = true;
            break;
         case 2:
            onechildmarked = true;
            onechildunmarked = true;
            break;
         }
         
         if (onechildmarked && onechildunmarked) {
            break;
         }
      }
   }
   
   state = 0;
   
   if (onechildmarked && onechildunmarked) {
      state = 2;
   }
   else if (onechildmarked) {
      state = 1;
   }
   else if (onechildunmarked) {
      state = 0;
   }
   else { // no child, don't change anything
      UpdateTreeItemState(tree->GetItemParent(item));
      return;
   }
   
   wxbTreeItemData* itemdata = (wxbTreeItemData*)tree->GetItemData(item);
      
   itemdata->SetMarked(state);
   tree->SetItemImage(item, state, wxTreeItemIcon_Normal);
   tree->SetItemImage(item, state, wxTreeItemIcon_Selected);
   
   UpdateTreeItemState(tree->GetItemParent(item));
}

/* Refresh the whole tree. */
void wxbRestorePanel::RefreshTree() {
   /* Save current selection */
   wxArrayString current;
   
   wxTreeItemId item = currentTreeItem;
   
   while ((item.IsOk()) && (item != tree->GetRootItem())) {
      current.Add(tree->GetItemText(item));
      item = tree->GetItemParent(item);
   }

   /* Update the tree */
   UpdateTreeItem(tree->GetRootItem(), false, true);

   /* Reselect the former selected item */   
   item = tree->GetRootItem();
   
   if (current.Count() == 0) {
      tree->SelectItem(item);
      return;
   }
   
   bool match;
   
   for (int i = current.Count()-1; i >= 0; i--) {
      long cookie;
      wxTreeItemId currentChild = tree->GetFirstChild(item, cookie);
      
      match = false;
      
      while (currentChild.IsOk()) {
         if (tree->GetItemText(currentChild) == current[i]) {
            item = currentChild;
            match = true;
            break;
         }
   
         currentChild = tree->GetNextChild(item, cookie);
      }
      
      if (!match) break;
   }
   
   UpdateTreeItem(item, true, false); /* Update the list */
   
   tree->SelectItem(item);
}

void wxbRestorePanel::RefreshList() {
   if (currentTreeItem.IsOk()) {
      UpdateTreeItem(currentTreeItem, true, false); /* Update the list */
   }
}

/* Update first config, adapting settings to the job name selected */
void wxbRestorePanel::UpdateFirstConfig() {
   configPanel->Enable(false);
   wxbDataTokenizer* dt = WaitForEnd(wxString(".defaults job=") + configPanel->GetRowString("Job Name") + "\n", true, false);
   /* job=RestoreFiles
    * pool=Default
    * messages=Standard
    * client=***
    * storage=File
    * where=/tmp/bacula-restores
    * level=0
    * type=Restore
    * fileset=Full Set */
   
   wxString name, str;
   unsigned int i;
   int j;
   wxString client;
   bool dolistjobs = false;
   
   for (i = 0; i < dt->GetCount(); i++) {
      str = (*dt)[i];
      if ((j = str.Find('=')) > -1) {
         name = str.Mid(0, j);
         if (name == "pool") {
            configPanel->SetRowString("Pool", str.Mid(j+1));
         }
         else if (name == "client") {
            str = str.Mid(j+1);
            if ((str != configPanel->GetRowString("Client")) || (configPanel->GetRowString("Before") == "")) {
               configPanel->SetRowString("Client", str);
               dolistjobs = true;
            }
         }
         else if (name == "storage") {
            configPanel->SetRowString("Storage", str.Mid(j+1));
         }
         else if (name == "fileset") {
            configPanel->SetRowString("Fileset", str.Mid(j+1));
         }
      }
   }
      
   delete dt;
   
   if (dolistjobs) {
      //wxTheApp->Yield(false);
      CmdListJobs();
   }
   configPanel->Enable(true);
}

/* 
 * Update second config.
 * 
 * Run Restore job
 * JobName:    RestoreFiles
 * Bootstrap:  /var/lib/bacula/restore.bsr
 * Where:      /tmp/bacula-restores
 * Replace:    always
 * FileSet:    Full Set
 * Client:     tom-fd
 * Storage:    File
 * When:       2004-04-18 01:18:56
 * Priority:   10
 * OK to run? (yes/mod/no):
 * 
 */
bool wxbRestorePanel::UpdateSecondConfig(wxbDataTokenizer* dt) {
   unsigned int i;
   for (i = 0; i < dt->GetCount(); i++) {
      if ((*dt)[i].Find("Run Restore job") == 0)
         break;
   }
   
   if ((i + 10) > dt->GetCount()) {
      return false;
   }
   
   int k;
   
   if ((k = (*dt)[++i].Find("JobName:")) != 0) return false;
   restorePanel->SetRowString("Job Name", (*dt)[i].Mid(10).Trim(false).RemoveLast());
   if ((k = (*dt)[++i].Find("Bootstrap:")) != 0) return false;
   restorePanel->SetRowString("Bootstrap", (*dt)[i].Mid(10).Trim(false).RemoveLast());
   if ((k = (*dt)[++i].Find("Where:")) != 0) return false;
   restorePanel->SetRowString("Where", (*dt)[i].Mid(10).Trim(false).RemoveLast());
   
   if ((k = (*dt)[++i].Find("Replace:")) != 0) return false;
   wxString str = (*dt)[i].Mid(10).Trim(false).RemoveLast();
   if (str == "always") restorePanel->SetRowSelection("Replace", 0);
   else if (str == "ifnewer") restorePanel->SetRowSelection("Replace", 1);
   else if (str == "ifolder") restorePanel->SetRowSelection("Replace", 2);
   else if (str == "never") restorePanel->SetRowSelection("Replace", 3);
   else restorePanel->SetRowSelection("Replace", 0);

   if ((k = (*dt)[++i].Find("FileSet:")) != 0) return false;
   restorePanel->SetRowString("Fileset", (*dt)[i].Mid(10).Trim(false).RemoveLast());
   if ((k = (*dt)[++i].Find("Client:")) != 0) return false;
   restorePanel->SetRowString("Client", (*dt)[i].Mid(10).Trim(false).RemoveLast());
   if ((k = (*dt)[++i].Find("Storage:")) != 0) return false;
   restorePanel->SetRowString("Storage", (*dt)[i].Mid(10).Trim(false).RemoveLast());
   if ((k = (*dt)[++i].Find("When:")) != 0) return false;
   restorePanel->SetRowString("When", (*dt)[i].Mid(10).Trim(false).RemoveLast());
   if ((k = (*dt)[++i].Find("Priority:")) != 0) return false;
   restorePanel->SetRowString("Priority", (*dt)[i].Mid(10).Trim(false).RemoveLast());
   cfgUpdated = 0;
   
   restorePanel->Layout();
   
   return true;
}

/*----------------------------------------------------------------------------
   Status function
  ----------------------------------------------------------------------------*/

/* Set current status by enabling/disabling components */
void wxbRestorePanel::SetStatus(status_enum newstatus) {
   switch (newstatus) {
   case disabled:
      centerSizer->Remove(configPanel);
      centerSizer->Remove(restorePanel);
      centerSizer->Remove(treelistPanel);
      treelistPanel->Show(false);
      restorePanel->Show(false);
      centerSizer->Add(configPanel, 1, wxEXPAND);
      configPanel->Show(true);
      configPanel->Layout();
      centerSizer->Layout();
      this->Layout();
      start->SetLabel("Enter restore mode");
      start->Enable(false);
      configPanel->Enable(false);
      tree->Enable(false);
      list->Enable(false);
      gauge->Enable(false);
      cancel->Enable(false);
      cfgUpdated = 0;
      cancelled = 0;
      break;
   case finished:
      centerSizer->Remove(configPanel);
      centerSizer->Remove(restorePanel);
      centerSizer->Remove(treelistPanel);
      treelistPanel->Show(false);
      restorePanel->Show(false);
      centerSizer->Add(configPanel, 1, wxEXPAND);
      configPanel->Show(true);
      configPanel->Layout();
      centerSizer->Layout();
      this->Layout();
      tree->DeleteAllItems();
      list->DeleteAllItems();
      configPanel->ClearRowChoices("Client");
      configPanel->ClearRowChoices("Before");
      wxbMainFrame::GetInstance()->EnablePanels();
      newstatus = activable;
   case activable:
      cancelled = 0;
      start->SetLabel("Enter restore mode");
      start->Enable(true);
      configPanel->Enable(false);
      tree->Enable(false);
      list->Enable(false);
      gauge->Enable(false);
      cancel->Enable(false);
      cfgUpdated = 0;
      break;
   case entered:
      wxbMainFrame::GetInstance()->DisablePanels(this);
      gauge->SetValue(0);
      start->Enable(false);
      //start->SetLabel("Choose files to restore");
      configPanel->Enable(true);
      tree->Enable(false);
      list->Enable(false);
      cancel->Enable(true);
      cfgUpdated = 0;
      break;
   case listing:
      
      break;
   case choosing:
      start->Enable(true);
      start->SetLabel("Restore");
      centerSizer->Remove(configPanel);
      configPanel->Show(false);
      centerSizer->Add(treelistPanel, 1, wxEXPAND);
      treelistPanel->Show(true);
      treelistPanel->Layout();
      centerSizer->Layout();
      this->Layout();
      tree->Enable(true);
      list->Enable(true);
      working = false;
      SetCursor(*wxSTANDARD_CURSOR);
      break;
   case configuring:
      start->Enable(false);
      configPanel->Enable(false);
      tree->Enable(false);
      list->Enable(false);
      centerSizer->Remove(treelistPanel);
      treelistPanel->Show(false);
      centerSizer->Add(restorePanel, 1, wxEXPAND);
      restorePanel->Show(true);
      restorePanel->Layout();
      centerSizer->Layout();
      this->Layout();
      restorePanel->EnableApply(false);
      break;
   case restoring:
      start->SetLabel("Restoring...");
      gauge->Enable(true);
      gauge->SetValue(0);
      start->Enable(false);
      configPanel->Enable(false);
      tree->Enable(false);
      list->Enable(false);
      SetCursor(*wxHOURGLASS_CURSOR);
      working = true;
      break;
   }
   status = newstatus;
}

/*----------------------------------------------------------------------------
   UI related
  ----------------------------------------------------------------------------*/

void wxbRestorePanel::EnableConfig(bool enable) {
   restorePanel->Enable(enable);
}

/*----------------------------------------------------------------------------
   Event handling
  ----------------------------------------------------------------------------*/

void wxbRestorePanel::OnCancel(wxCommandEvent& WXUNUSED(event)) {
   cancel->Enable(false);
   SetCursor(*wxHOURGLASS_CURSOR);
   CmdCancel();
   SetCursor(*wxSTANDARD_CURSOR);
}

void wxbRestorePanel::OnStart(wxCommandEvent& WXUNUSED(event)) {
   if (working) {
      return;
   }
   SetCursor(*wxHOURGLASS_CURSOR);
   working = true;
   CmdStart();
   working = false;
   SetCursor(*wxSTANDARD_CURSOR);
}

void wxbRestorePanel::OnTreeChanging(wxTreeEvent& event) {
   if (working) {
      event.Veto();
   }
}

void wxbRestorePanel::OnTreeExpanding(wxTreeEvent& event) {
   if (working) {
      event.Veto();
      return;
   }
   //working = true;
   //CmdList(event.GetItem());
   if (tree->GetSelection() != event.GetItem()) {
      tree->SelectItem(event.GetItem());
   }
   //working = false;
}

void wxbRestorePanel::OnTreeChanged(wxTreeEvent& event) {
   if (working) {
      return;
   }
   if (currentTreeItem == event.GetItem()) {
      return;
   }
   treeadd->Enable(false);
   treeremove->Enable(false);
   treerefresh->Enable(false);
   SetCursor(*wxHOURGLASS_CURSOR);
   markWhenListingDone = false;
   working = true;
   currentTreeItem = event.GetItem();
   CmdList(event.GetItem());
   if (markWhenListingDone) {
      CmdMark(event.GetItem(), NULL, 0);
      tree->Refresh();
   }
   working = false;
   SetCursor(*wxSTANDARD_CURSOR);
   if (event.GetItem().IsOk()) {
      int status = ((wxbTreeItemData*)tree->GetItemData(event.GetItem()))->GetMarked();
      treeadd->Enable(status != 1);
      treeremove->Enable(status != 0);
   }
   treerefresh->Enable(true);
}

void wxbRestorePanel::OnTreeMarked(wxbTreeMarkedEvent& event) {
   csprint("Tree marked", CS_DEBUG);
   if (working) {
      if (tree->GetSelection() == event.GetItem()) {
         markWhenListingDone = !markWhenListingDone;
      }
      return;
   }
   SetCursor(*wxHOURGLASS_CURSOR);
   working = true;
   CmdMark(event.GetItem(), NULL, 0);
   //event.Skip();
   tree->Refresh();
   working = false;
   if (event.GetItem().IsOk()) {
      int status = ((wxbTreeItemData*)tree->GetItemData(event.GetItem()))->GetMarked();
      treeadd->Enable(status != 1);
      treeremove->Enable(status != 0);
   }
   SetCursor(*wxSTANDARD_CURSOR);
}

void wxbRestorePanel::OnTreeAdd(wxCommandEvent& event) {
   if (working) {
      return;
   }
   
   if (currentTreeItem.IsOk()) {
      SetCursor(*wxHOURGLASS_CURSOR);
      working = true;
      CmdMark(currentTreeItem, NULL, 0, 1);
      tree->Refresh();
      working = false;
      treeadd->Enable(0);
      treeremove->Enable(1);
      SetCursor(*wxSTANDARD_CURSOR);
   }
}

void wxbRestorePanel::OnTreeRemove(wxCommandEvent& event) {
   if (working) {
      return;
   }
   
   if (currentTreeItem.IsOk()) {
      SetCursor(*wxHOURGLASS_CURSOR);
      working = true;
      CmdMark(currentTreeItem, NULL, 0, 0);
      tree->Refresh();
      working = false;
      treeadd->Enable(1);
      treeremove->Enable(0);
      SetCursor(*wxSTANDARD_CURSOR);
   }
}

void wxbRestorePanel::OnTreeRefresh(wxCommandEvent& event) {
   if (working) {
      return;
   }
   
   RefreshTree();
}

void wxbRestorePanel::OnListMarked(wxbListMarkedEvent& event) {
   if (working) {
      //event.Skip();
      return;
   }
   
   if (list->GetSelectedItemCount() == 0) {
      return;
   }
   
   SetCursor(*wxHOURGLASS_CURSOR);
   working = true;  
   
   long* items = new long[list->GetSelectedItemCount()];
   
   int num = 0;
   
   long item = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   while (item != -1) {
      items[num] = item;
      num++;
      item = list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   }
     
   CmdMark(wxTreeItemId(), items, num);
   
   delete[] items;
   
   wxListEvent listevt;
   
   OnListChanged(listevt);
   
   event.Skip();
   tree->Refresh();
   working = false;
   SetCursor(*wxSTANDARD_CURSOR);
}

void wxbRestorePanel::OnListActivated(wxListEvent& event) {
   if (working) {
      //event.Skip();
      return;
   }
   SetCursor(*wxHOURGLASS_CURSOR);
   working = true;
   long item = event.GetIndex();
//   long item = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_FOCUSED);
   if (item > -1) {
      wxbTreeItemData* itemdata = (wxbTreeItemData*)list->GetItemData(item);
      wxString name = itemdata->GetName();
      event.Skip();

      wxString itemStr;

      long cookie;

      if (name.GetChar(name.Length()-1) == '/') {
         wxTreeItemId currentChild = tree->GetFirstChild(currentTreeItem, cookie);

         while (currentChild.IsOk()) {
            wxString name2 = tree->GetItemText(currentChild);
            if (name2 == name) {
               //tree->UnselectAll();
               working = false;
               SetCursor(*wxSTANDARD_CURSOR);
               tree->Expand(currentTreeItem);
               tree->SelectItem(currentChild);
               //tree->Refresh();
               return;
            }
            currentChild = tree->GetNextChild(currentTreeItem, cookie);
         }
      }
   }
   working = false;
   SetCursor(*wxSTANDARD_CURSOR);
}

void wxbRestorePanel::OnListChanged(wxListEvent& event) {
   listadd->Enable(false);
   listremove->Enable(false);
   
   bool marked = false;
   bool unmarked = false;
   
   long item = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   while (item != -1) {
      switch (((wxbTreeItemData*)list->GetItemData(item))->GetMarked()) {
      case 0:
         unmarked = true;
         break;
      case 1:
         marked = true;
         break;
      case 2:
         marked = true;
         unmarked = true;
         break;
      default:
         break;
         // Should never happen
      }
      if (marked && unmarked) break;
      item = list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   }
   
   listadd->Enable(unmarked);
   listremove->Enable(marked);
}

void wxbRestorePanel::OnListAdd(wxCommandEvent& WXUNUSED(event)) {
   if (working) {
      return;
   }
   
   SetCursor(*wxHOURGLASS_CURSOR);
   working = true;
   
   long* items = new long[list->GetSelectedItemCount()];
   
   int num = 0;
   
   long item = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   while (item != -1) {
      items[num] = item;
      num++;
      item = list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   }
     
   CmdMark(wxTreeItemId(), items, num, 1);
   
   delete[] items;
   
   tree->Refresh();
   working = false;
   SetCursor(*wxSTANDARD_CURSOR);
   
   listadd->Enable(false);
   listremove->Enable(true);
}

void wxbRestorePanel::OnListRemove(wxCommandEvent& WXUNUSED(event)) {
   if (working) {
      return;
   }
   
   SetCursor(*wxHOURGLASS_CURSOR);
   working = true;
   
   long* items = new long[list->GetSelectedItemCount()];
   
   int num = 0;
   
   long item = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   while (item != -1) {
      items[num] = item;
      num++;
      item = list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   }
     
   CmdMark(wxTreeItemId(), items, num, 0);
   
   delete[] items;
   
   tree->Refresh();
   working = false;
   SetCursor(*wxSTANDARD_CURSOR);
   
   listadd->Enable(true);
   listremove->Enable(false);
}

void wxbRestorePanel::OnListRefresh(wxCommandEvent& WXUNUSED(event)) {
   if (working) {
      return;
   }
   
   RefreshList();
}

void wxbRestorePanel::OnConfigUpdated(wxCommandEvent& event) {
   if (status == entered) {
      if (event.GetId() == ConfigJobName) {
         if (working) {
            return;
         }
         SetCursor(*wxHOURGLASS_CURSOR);
         working = true;
         UpdateFirstConfig();
         working = false;
         SetCursor(*wxSTANDARD_CURSOR);
      }
      else if (event.GetId() == ConfigClient) {
         if (working) {
            return;
         }
         SetCursor(*wxHOURGLASS_CURSOR);
         working = true;
         configPanel->Enable(false);
         CmdListJobs();
         configPanel->Enable(true);
         working = false;
         SetCursor(*wxSTANDARD_CURSOR);
      }
      cfgUpdated = cfgUpdated | (1 << event.GetId());
   }
   else if (status == configuring) {
      restorePanel->EnableApply(true);
      cfgUpdated = cfgUpdated | (1 << event.GetId());
   }
}

void wxbRestorePanel::OnConfigOk(wxCommandEvent& WXUNUSED(event)) {
   if (status != configuring) return;
   if (working) {
      return;
   }
   SetCursor(*wxHOURGLASS_CURSOR);
   working = true;
   CmdStart();
   working = false;
   SetCursor(*wxSTANDARD_CURSOR);
}

void wxbRestorePanel::OnConfigApply(wxCommandEvent& WXUNUSED(event)) {
   if (status != configuring) return;
   if (working) {
      return;
   }
   SetCursor(*wxHOURGLASS_CURSOR);
   working = true;
   CmdConfigApply();
   if (cfgUpdated == 0) {
      restorePanel->EnableApply(false);
   }
   working = false;  
   SetCursor(*wxSTANDARD_CURSOR);
}

void wxbRestorePanel::OnConfigCancel(wxCommandEvent& WXUNUSED(event)) {
   if (status != configuring) return;
   if (working) {
      return;
   }
   SetCursor(*wxHOURGLASS_CURSOR);
   working = true;
   CmdConfigCancel();
   working = false;
   SetCursor(*wxSTANDARD_CURSOR);
}

