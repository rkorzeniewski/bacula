/*
 *
 *    csprint header file, used by console_thread to send events back to the GUI.
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

#ifndef CSPRINT_H
#define CSPRINT_H

#define CS_DATA          1 /* data has been received */
#define CS_END           2 /* no data to receive anymore */
#define CS_PROMPT        3 /* prompt signal received */
#define CS_CONNECTED     4 /* the socket is now connected */
#define CS_DISCONNECTED  5 /* the socket is now disconnected */

#define CS_REMOVEPROMPT  6 /* remove the prompt (#), when automatic messages are comming */

#define CS_DEBUG        10 /* used to print debug messages */
#define CS_TERMINATED   99 /* used to signal that the thread is terminated */

/* function called by console_thread to send events back to the GUI */
class wxString;

void csprint(const char* str, int status=CS_DATA);
void csprint(wxString str, int status=CS_DATA);

#endif
