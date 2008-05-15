#ifndef _FMTWIDGETITEM_H_
#define _FMTWIDGETITEM_H_
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
 *   TreeView formatting helpers - Riccardo Ghetta, May 2008 
 */

class QTreeWidgetItem;
class QTableWidget;
class QTableWidgetItem;
class QString;
class QBrush;

/*
 * base class for formatters
 *
 */
class ItemFormatterBase
{
public:
  enum BYTES_CONVERSION {
    BYTES_CONVERSION_NONE,
    BYTES_CONVERSION_IEC,
    BYTES_CONVERSION_SI,
  };

public:
   virtual ~ItemFormatterBase(); 

   /* Prints Yes if fld is != 0, No otherwise. Centers field if center true*/
   void setBoolFld(int index, const QString &fld, bool center = true);
   void setBoolFld(int index, int fld, bool center = true);

   /* Normal text field. Centers field if center true*/
   virtual void setTextFld(int index, const QString &fld, bool center = false) = 0;

   /* Right-aligned text field.*/
   void setNumericFld(int index, const QString &fld);

   /* fld value interpreted as bytes and formatted with size suffixes */
   void setBytesFld(int index, const QString &fld);

   /* fld value interpreted as seconds and formatted with y,m,w,h suffixes */
   void setDurationFld(int index, const QString &fld);

   /* fld value interpreted as volume status. Colored accordingly */
   void setVolStatusFld(int index, const QString &fld, bool center = true);
  
   /* fld value interpreted as job status. Colored accordingly */
   void setJobStatusFld(int index, const QString &shortStatus, const QString &longstatus, 
			bool center = true);
  
   /* fld value interpreted as job type. */
   void setJobTypeFld(int index, const QString &fld, bool center = false);
  
   /* fld value interpreted as job level. */
   void setJobLevelFld(int index, const QString &fld, bool center = false);
  
   static void setBytesConversion(BYTES_CONVERSION b) {
      cnvFlag = b;
   }
   static BYTES_CONVERSION getBytesConversion() {
      return cnvFlag;
   }

protected:
   /* only derived classes can create one of these */
   ItemFormatterBase();

   virtual void setTextAlignment(int index, int align) = 0;
   virtual void setBackground(int index, const QBrush &) = 0;

private:
   /* bytes formatted as power-of-two with IEC suffixes (KiB, MiB, and so on) */
   static QString convertBytesIEC(qint64 fld);

   /* bytes formatted as power-of-ten with SI suffixes (kB, MB, and so on) */
   static QString convertBytesSI(qint64 fld);

private:
   static BYTES_CONVERSION cnvFlag;
};

/*
 * This class can be used instead of QTreeWidgetItem (it allocates one internally,
 * to format data fields.
 * All setXXXFld routines receive a column index and the unformatted string value.
 */
class TreeItemFormatter : public ItemFormatterBase
{
public:

   TreeItemFormatter(QTreeWidgetItem &parent, int indent_level);

   virtual void setTextFld(int index, const QString &fld, bool center = false);

   /* access internal widget */
   QTreeWidgetItem *widget() { return wdg; }
   const QTreeWidgetItem *widget() const { return wdg; }

protected:
   virtual void setTextAlignment(int index, int align);
   virtual void setBackground(int index, const QBrush &);

private:
   QTreeWidgetItem *wdg;
   int level;
};

/*
 * This class can be used instead of QTableWidgetItem (it allocates one internally,
 * to format data fields.
 * All setXXXFld routines receive the column and the unformatted string value.
 */
class TableItemFormatter : public ItemFormatterBase
{
public:

   TableItemFormatter(QTableWidget &parent, int row);

   virtual void setTextFld(int col, const QString &fld, bool center = false);

   /* access internal widget at column col*/
   QTableWidgetItem *widget(int col);
   const QTableWidgetItem *widget(int col) const;

protected:
   virtual void setTextAlignment(int index, int align);
   virtual void setBackground(int index, const QBrush &);

private:
   QTableWidget *parent;
   int row;
   QTableWidgetItem *last;
};

#endif /* _FMTWIDGETITEM_H_ */
