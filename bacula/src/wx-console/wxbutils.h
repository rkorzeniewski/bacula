/*
 *
 *   wxbDataParser, class that receives and analyses data
 *   wxbPanel, main frame's notebook panels
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

#ifndef WXBPANEL_H
#define WXBPANEL_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

/*
 *  abstract class that can receive director information.
 */
class wxbDataParser
{
   public:
      /* Creates a new wxbDataParser, and register it in wxbMainFrame */
      wxbDataParser();

      /* Destroy a wxbDataParser, and unregister it in wxbMainFrame */
      virtual ~wxbDataParser();

      /*
       *   Receives director information, forwarded by wxbMainFrame.
       */
      virtual void Print(wxString str, int status) = 0;
};

/*
 *  abstract panel that can receive director information.
 */
class wxbPanel : public wxPanel
{
   public:
      wxbPanel(wxWindow* parent) : wxPanel(parent) {}

      /*
       *   Tab title in the notebook.
       */
      virtual wxString GetTitle() = 0;
      
      /*
       *   Enable or disable this panel
       */
      virtual void EnablePanel(bool enable = true) = 0;
};

/*
 *  Receives director information, and splits it by line.
 * 
 * datatokenizer[0] retrieves first line
 */
class wxbDataTokenizer: public wxbDataParser, public wxArrayString
{
   public:
      /* Creates a new wxbDataTokenizer */
      wxbDataTokenizer();

      /* Destroy a wxbDataTokenizer */
      virtual ~wxbDataTokenizer();

      /*
       *   Receives director information, forwarded by wxbMainFrame.
       */
      virtual void Print(wxString str, int status);
      
      /* Returns true if the last signal received was an end signal, 
       * indicating that no more data is available */
      bool hasFinished();
      
   private:
      bool finished;
      wxString buffer;
};

#endif // WXBPANEL_H

