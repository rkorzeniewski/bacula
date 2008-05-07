/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

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
 *   Version $Id$
 *
 *  Helper functions for tree widget formatting
 *
 *   Riccardo Ghetta, May 2008
 *
 */ 

#include <QTreeWidgetItem>
#include <QString>
#include <QStringList>
#include <math.h>
#include "fmtwidgetitem.h"

ItemFormatter::ItemFormatter(QTreeWidgetItem &parent, int indent_level):
wdg(new QTreeWidgetItem(&parent)),
level(indent_level)
{
}

void ItemFormatter::setBoolFld(int index, const QString &fld, bool center)
{
   if (fld.trimmed().toInt())
     setTextFld(index, "Yes", center);
   else
     setTextFld(index, "No", center);
}

void ItemFormatter::setBoolFld(int index, int fld, bool center)
{
   if (fld)
     setTextFld(index, "Yes", center);
   else
     setTextFld(index, "No", center);
}

void ItemFormatter::setTextFld(int index, const QString &fld, bool center)
{
   wdg->setData(index, Qt::UserRole, level);
   if (center) {
      wdg->setTextAlignment(index, Qt::AlignHCenter);
   }
   wdg->setText(index, fld.trimmed());
}

void ItemFormatter::setNumericFld(int index, const QString &fld)
{
   wdg->setData(index, Qt::UserRole, level);
   wdg->setTextAlignment(index, Qt::AlignRight);
   wdg->setText(index, fld.trimmed());
}

void ItemFormatter::setBytesFld(int index, const QString &fld)
{
   static const double KB = 1024.0;
   static const double MB = KB * KB;
   static const double GB = MB * KB;
   static const double TB = GB * KB;

   double dfld = fld.trimmed().toDouble();
   QString msg;
   if (dfld >= TB)
      msg = QString("%1TB").arg(dfld / TB, 0, 'f', 2);
   else if (dfld >= GB)
      msg = QString("%1GB").arg(dfld / GB, 0, 'f', 2);
   else if (dfld >= MB)
      msg = QString("%1MB").arg(dfld / MB, 0, 'f', 2);
   else if (dfld >= KB)
      msg = QString("%1KB").arg(dfld / KB, 0, 'f', 2);
   else
      msg = QString::number(dfld, 'f', 0);
 
   wdg->setData(index, Qt::UserRole, level);
   wdg->setTextAlignment(index, Qt::AlignRight);
   wdg->setText(index, msg);
}

void ItemFormatter::setDurationFld(int index, const QString &fld)
{
   static const double HOUR = 3600;
   static const double DAY = HOUR * 24;
   static const double WEEK = DAY * 7;
   static const double MONTH = DAY * 30;
   static const double YEAR = DAY * 365;

   double dfld = fld.trimmed().toDouble();
   QString msg;
   if (fmod(dfld, YEAR) == 0)
      msg = QString("%1y").arg(dfld / YEAR, 0, 'f', 0);
   else if (fmod(dfld, MONTH) == 0)
      msg = QString("%1m").arg(dfld / MONTH, 0, 'f', 0);
   else if (fmod(dfld, WEEK) == 0)
      msg = QString("%1w").arg(dfld / WEEK, 0, 'f', 0);
   else if (fmod(dfld, DAY) == 0)
      msg = QString("%1d").arg(dfld / DAY, 0, 'f', 0);
   else if (fmod(dfld, HOUR) == 0)
      msg = QString("%1h").arg(dfld / HOUR, 0, 'f', 0);
   else
      msg = QString("%1s").arg(dfld, 0, 'f', 0);
 
   wdg->setData(index, Qt::UserRole, level);
   wdg->setTextAlignment(index, Qt::AlignRight);
   wdg->setText(index, msg);
}

void ItemFormatter::setVolStatusFld(int index, const QString &fld, bool center)
{
  setTextFld(index, fld, center);

   if (fld == "Append" ) {
      wdg->setBackground(index, Qt::green);
   } else if (fld == "Error") {
      wdg->setBackground(index, Qt::red);
   } else if (fld == "Used" || fld == "Full"){
      wdg->setBackground(index, Qt::yellow);
   }
}
