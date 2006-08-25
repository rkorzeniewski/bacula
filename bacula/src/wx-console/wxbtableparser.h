/*
 *
 *   Class used to parse tables received from director in this format :
 *
 *  +---------+---------+-------------------+
 *  | Header1 | Header2 | ...               |
 *  +---------+---------+-------------------+
 *  |  Data11 | Data12  | ...               |
 *  |  ....   |  ...    | ...               |
 *  +---------+---------+-------------------+
 *
 *    Nicolas Boichat, April 2004
 *
 *    Version $Id$
 */
/*
   Copyright (C) 2000-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#ifndef WXBTABLEPARSER_H
#define WXBTABLEPARSER_H

#include "wx/wxprec.h"

#ifdef __BORLANDC__
   #pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers)
#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

#include "wxbutils.h"

#include <wx/dynarray.h>

/*
 * Allow the use of Object Array (auto-deletion, object returned as themselves
 * and not as pointers)
 */
class wxbArrayString: public wxArrayString, public wxObject {
   public:
      wxbArrayString(int n = 1);
      virtual ~wxbArrayString();
};

WX_DECLARE_OBJARRAY( wxbArrayString, wxbTable );

/*
 * Class used to parse tables received from director. Data can be accessed with
 * the operator [].
 *
 * Example : wxString elem = parser[3][2]; fetches column 2 of element 3.
 */
class wxbTableParser: public wxbTable, public wxbDataParser
{
   public:
      wxbTableParser(bool header = true);
      virtual ~wxbTableParser();

      /*
       *   Receives data to analyse.
       */
      virtual bool Analyse(wxString str, int status);

      /*
       *   Return true table parsing has finished.
       */
      bool hasFinished();

      /*
       *   Returns table header as an array of wxStrings.
       */
      const wxbArrayString& GetHeader();
   private:
      wxbArrayString tableHeader;

      /*
       * 0 - Table has not begun
       * 1 - first +--+ line obtained, header will follow
       * 2 - second +--+ line obtained, data will follow
       * 3 - last +--+ line obtained, table parsing has finished
       */
      int separatorNum;
};

#endif // WXBTABLEPARSER_H
