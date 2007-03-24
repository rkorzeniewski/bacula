/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *   Version $Id: batstack.cpp 4230 2007-02-21 20:07:37Z kerns $
 *
 *   Kern Sibbald, January 2007
 */

#include "batstack.h"

void BatStack::AddTostack()
{
   /* These two lines are for making sure if it is being changed from a window
 * That it has the proper window flag and parent. */
   setWindowFlags(Qt::Widget);
   setParent(m_parent);
   /* This was being done already */
   m_parent->addWidget(this);
   /* Set stacked flag */
   m_stacked=true;
}

void BatStack::RemoveFromstack()
{
   /* Change from a stacked widget to a normal window */
   /* FIXME Will need to make it so that window cannot be closed */
   m_parent->removeWidget(this);
   setWindowFlags(Qt::Window);
   showNormal();
   /* Clear stacked flag */
   m_stacked=false;
}

void BatStack::Togglestack()
{
   if( m_stacked ){
      RemoveFromstack();
   } else {
      AddTostack();
   }
}
