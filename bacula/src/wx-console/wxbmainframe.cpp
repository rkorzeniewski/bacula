/*
 *
 *   Main frame
 *
 *    Nicolas Boichat, April 2004
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

#include "wxbmainframe.h" // class's header file

#include "wxbrestorepanel.h"

#include "csprint.h"

#include "wxwin16x16.xpm"

// ----------------------------------------------------------------------------
// event tables and other macros for wxWindows
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// IDs for the controls and the menu commands
enum
{
   // menu items
   Minimal_Quit = 1,

   // it is important for the id corresponding to the "About" command to have
   // this standard value as otherwise it won't be handled properly under Mac
   // (where it is special and put into the "Apple" menu)
   Minimal_About = wxID_ABOUT,
   TypeText = 2,
   Thread = 3
};

/*
 *   wxbTHREAD_EVENT declaration, used by csprint
 */
BEGIN_DECLARE_EVENT_TYPES()
   DECLARE_EVENT_TYPE(wxbTHREAD_EVENT,       1)
END_DECLARE_EVENT_TYPES()

DEFINE_EVENT_TYPE(wxbTHREAD_EVENT)

// the event tables connect the wxWindows events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
BEGIN_EVENT_TABLE(wxbMainFrame, wxFrame)
   EVT_MENU(Minimal_Quit,  wxbMainFrame::OnQuit)
   EVT_MENU(Minimal_About, wxbMainFrame::OnAbout)
   EVT_TEXT_ENTER(TypeText, wxbMainFrame::OnEnter)
   EVT_CUSTOM(wxbTHREAD_EVENT, Thread, wxbMainFrame::OnPrint)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// wxbThreadEvent
// ----------------------------------------------------------------------------

/*
 *  wxbThreadEvent constructor
 */
wxbThreadEvent::wxbThreadEvent(int id): wxEvent(id, wxbTHREAD_EVENT) {
   m_eventObject = NULL;
}

/*
 *  wxbThreadEvent destructor
 */
wxbThreadEvent::~wxbThreadEvent()
{
   if (m_eventObject != NULL) {
      delete m_eventObject;
   }
}

/*
 *  wxbThreadEvent copy constructor
 */
wxbThreadEvent::wxbThreadEvent(const wxbThreadEvent& te)
{
   this->m_eventType = te.m_eventType;
   this->m_id = te.m_id;
   if (te.m_eventObject != NULL) {
      this->m_eventObject = new wxbPrintObject(*((wxbPrintObject*)te.m_eventObject));
   }
   else {
      this->m_eventObject = NULL;
   }
   this->m_skipped = te.m_skipped;
   this->m_timeStamp = te.m_timeStamp;
}

/*
 *  Must be implemented (abstract in wxEvent)
 */
wxEvent* wxbThreadEvent::Clone() const
{
   return new wxbThreadEvent(*this);
}

/*
 *  Gets the wxbPrintObject attached to this event, containing data sent by director
 */
wxbPrintObject* wxbThreadEvent::GetEventPrintObject()
{
   return (wxbPrintObject*)m_eventObject;
}

/*
 *  Sets the wxbPrintObject attached to this event
 */
void wxbThreadEvent::SetEventPrintObject(wxbPrintObject* object)
{
   m_eventObject = (wxObject*)object;
}

// ----------------------------------------------------------------------------
// main frame
// ----------------------------------------------------------------------------

wxbMainFrame *wxbMainFrame::frame = NULL;

/*
 *  Singleton constructor
 */
wxbMainFrame* wxbMainFrame::CreateInstance(const wxString& title, const wxPoint& pos, const wxSize& size, long style)
{
   frame = new wxbMainFrame(title, pos, size, style);
   return frame;
}

/*
 *  Returns singleton instance
 */
wxbMainFrame* wxbMainFrame::GetInstance()
{
   return frame;
}

/*
 *  Private destructor
 */
wxbMainFrame::~wxbMainFrame()
{
   ct->Delete();
}

/*
 *  Private constructor
 */
wxbMainFrame::wxbMainFrame(const wxString& title, const wxPoint& pos, const wxSize& size, long style)
      : wxFrame(NULL, -1, title, pos, size, style)
{
   ct = NULL;

   // set the frame icon
   SetIcon(wxIcon(wxwin16x16_xpm));

#if wxUSE_MENUS
   // create a menu bar
   wxMenu *menuFile = new wxMenu;

   // the "About" item should be in the help menu
   wxMenu *helpMenu = new wxMenu;
   helpMenu->Append(Minimal_About, _T("&About...\tF1"), _T("Show about dialog"));

   menuFile->Append(Minimal_Quit, _T("E&xit\tAlt-X"), _T("Quit this program"));

   // now append the freshly created menu to the menu bar...
   wxMenuBar *menuBar = new wxMenuBar();
   menuBar->Append(menuFile, _T("&File"));
   menuBar->Append(helpMenu, _T("&Help"));

   // ... and attach this menu bar to the frame
   SetMenuBar(menuBar);
#endif // wxUSE_MENUS

#if wxUSE_STATUSBAR
   // create a status bar just for fun (by default with 1 pane only)
   CreateStatusBar(2);
   SetStatusText(_T("Welcome to Bacula wx-GUI!"));
#endif // wxUSE_STATUSBAR

   wxPanel* global = new wxPanel(this, -1);

   notebook = new wxNotebook(global, -1);

   /* Console */

   wxPanel* consolePanel = new wxPanel(notebook, -1);
   notebook->AddPage(consolePanel, "Console");

   consoleCtrl = new wxTextCtrl(consolePanel,-1,"",wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH);
   consoleCtrl->SetDefaultStyle(wxTextAttr(*wxBLACK, wxNullColour, wxFont(10, wxMODERN, wxNORMAL, wxNORMAL)));

   typeCtrl = new wxTextCtrl(consolePanel,TypeText,"",wxDefaultPosition,wxSize(200,20), wxTE_PROCESS_ENTER);

   wxFlexGridSizer *consoleSizer = new wxFlexGridSizer(2, 1, 0, 0);
   consoleSizer->AddGrowableCol(0);
   consoleSizer->AddGrowableRow(0);

   consoleSizer->Add(consoleCtrl, 1, wxEXPAND | wxALL, 0);
   consoleSizer->Add(typeCtrl, 0, wxEXPAND | wxALL, 0);

   consolePanel->SetAutoLayout( TRUE );
   consolePanel->SetSizer( consoleSizer );
   consoleSizer->SetSizeHints( consolePanel );

   // Creates the list of panels which are included in notebook, and that need to receive director information

   panels = new (wxbPanel*)[2];
   panels[0] = new wxbRestorePanel(notebook);
   panels[1] = NULL;

   for (int i = 0; panels[i] != NULL; i++) {
      notebook->AddPage(panels[i], panels[i]->GetTitle());
   }

   wxBoxSizer* globalSizer = new wxBoxSizer(wxHORIZONTAL);

   globalSizer->Add(new wxNotebookSizer(notebook), 1, wxEXPAND, 0);

   global->SetSizer( globalSizer );
   globalSizer->SetSizeHints( global );

   wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

   sizer->Add(global, 1, wxEXPAND | wxALL, 0);
   SetAutoLayout(true);
   SetSizer( sizer );
   //sizer->SetSizeHints( this );
}

/*
 *  Starts the thread interacting with the director
 */
void wxbMainFrame::StartConsoleThread()
{
   if (ct != NULL) {
      ct->Delete();
   }
   ct = new console_thread();
   ct->Create();
   ct->Run();
}

// event handlers

void wxbMainFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
   // TRUE is to force the frame to close
   Close(TRUE);
}

void wxbMainFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
   wxString msg;
   msg.Printf( _T("Welcome to Bacula wx-GUI.\n (c) 2004 Nicolas Boichat <nicolas@boichat.ch>\n")
            _T("Version : %s"), wxVERSION_STRING);

   wxMessageBox(msg, _T("About Bacula-wx-GUI"), wxOK | wxICON_INFORMATION, this);
}

void wxbMainFrame::OnEnter(wxCommandEvent& WXUNUSED(event))
{
   wxString str = typeCtrl->GetValue() + "\n";
   Send(str);
}

/*
 *  Called when data is arriving from director
 */
void wxbMainFrame::OnPrint(wxbThreadEvent& event) {
   wxbPrintObject* po = event.GetEventPrintObject();

   Print(po->str, po->status);
}

/*
 *  Prints data received from director to the console, and forwards it to the panels
 */
void wxbMainFrame::Print(wxString str, int status)
{
   // CS_DEBUG is often sent by panels, so resend it to them would cause an infinite loop
   if (status != CS_DEBUG) {
      for (int i = 0; panels[i] != NULL; i++) {
         panels[i]->Print(str, status);
       }
   }

   if (status == CS_END) {
      str = "#";
   }

   consoleCtrl->SetDefaultStyle(wxTextAttr(*wxBLACK));
   (*consoleCtrl) << str;
   consoleCtrl->SetInsertionPointEnd();
}

/*
 *  Sends data to the director
 */
void wxbMainFrame::Send(wxString str)
{
   ct->Write((const char*)str);
   typeCtrl->SetValue("");
   consoleCtrl->SetDefaultStyle(wxTextAttr(*wxRED));
   (*consoleCtrl) << str;
}

/*
 *  Used by csprint, which is called by console thread.
 *
 *  In GTK and perhaps X11, only the main thread is allowed to interact with
 *  graphical components, so by firing an event, the main loop will call OnPrint.
 *
 *  Calling OnPrint directly from console thread produces "unexpected async replies".
 */
void firePrintEvent(wxString str, int status)
{
   wxbPrintObject* po = new wxbPrintObject(str, status);

   wxbThreadEvent evt(Thread);
   evt.SetEventPrintObject(po);

   wxbMainFrame::GetInstance()->AddPendingEvent(evt);
}

wxString csBuffer; /* Temporary buffer for receiving data from console thread */

/*
 *  Called by console thread, this function forwards data line by line and end
 *  signals to the GUI.
 */
void csprint(char* str, int status)
{
   if (str != 0) {
      int len = strlen(str);
      bool allnewline = true;
      for (int i = 0; i < len; i++) {
      if (!(allnewline = (str[i] == '\n')))
      break;
      }

      if (allnewline) {
         firePrintEvent(csBuffer << "\n", CS_DATA);
         csBuffer = "";
         for (int i = 1; i < len; i++) {
            firePrintEvent("\n", status);
         }
      }
      else {
         wxStringTokenizer tkz(str, "\n", wxTOKEN_RET_DELIMS | wxTOKEN_RET_EMPTY | wxTOKEN_RET_EMPTY_ALL);

         while ( tkz.HasMoreTokens() ) {
            csBuffer << tkz.GetNextToken();
            if (csBuffer.Length() != 0) {
               if ((csBuffer.GetChar(csBuffer.Length()-1) == '\n') ||
                  (csBuffer.GetChar(csBuffer.Length()-1) == '\r')) {
                  firePrintEvent(csBuffer, status);
                  csBuffer = "";
               }
            }
         }
      }

      if (csBuffer == "$ ") { // Restore console
         firePrintEvent(csBuffer, status);
         csBuffer = "";
      }
   }

   if (status == CS_END) {
      if (csBuffer.Length() != 0) {
         firePrintEvent(csBuffer, CS_DATA);
      }
      csBuffer = "";
      firePrintEvent("", CS_END);
   }
}

