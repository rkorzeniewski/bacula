/*
 *
 *  ansi_label.c routines to handle ANSI (and perhaps one day IBM)
 *   tape labels.
 *
 *   Kern Sibbald, MMV
 *
 *
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2005 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

/* Forward referenced functions */
static char *ansi_date(time_t td, char *buf);
static bool same_label_names(char *bacula_name, char *ansi_name);

/*
 * We read an ANSI label and compare the Volume name. We require
 * a VOL1 record of 80 characters followed by a HDR1 record containing
 * BACULA.DATA in the filename field. We then read up to 3 more 
 * header records (they are not required) and an EOF, at which
 * point, all is good.
 *
 * Returns:
 *    VOL_OK		Volume name OK
 *    VOL_NO_LABEL	No ANSI label on Volume
 *    VOL_IO_ERROR	I/O error on read
 *    VOL_NAME_ERROR	Wrong name in VOL1 record
 *    VOL_LABEL_ERROR	Probably an ANSI label, but something wrong
 *	
 */ 
int read_ansi_ibm_label(DCR *dcr) 
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   char label[80];		      /* tape label */
   int stat, i;
   char *VolName = dcr->VolumeName;

   /*
    * Read VOL1, HDR1, HDR2 labels, but ignore the data
    *  If tape read the following EOF mark, on disk do
    *  not read.
    */
   Dmsg0(000, "Read ansi label.\n");
   if (!dev->is_tape()) {
      return VOL_OK;
   }

   dev->label_type = B_BACULA_LABEL;  /* assume Bacula label */

   /* Read a maximum of 5 records VOL1, HDR1, ... HDR4 */
   for (i=0; i < 6; i++) {
      do {
	 stat = read(dev->fd, label, sizeof(label));
      } while (stat == -1 && errno == EINTR);
      if (stat < 0) {
	 berrno be;
	 clrerror_dev(dev, -1);
         Dmsg1(000, "Read device got: ERR=%s\n", be.strerror());
         Mmsg2(jcr->errmsg, _("Read error on device %s in ANSI label. ERR=%s\n"),
	    dev->dev_name, be.strerror());
         Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
	 dev->VolCatInfo.VolCatErrors++;
	 return VOL_IO_ERROR;
      }
      if (stat == 0) {
	 if (dev->at_eof()) {
	    dev->state |= ST_EOT;
            Dmsg0(000, "EOM on ANSI label\n");
            Mmsg0(jcr->errmsg, _("Insane! End of tape while reading ANSI label.\n"));
            return VOL_LABEL_ERROR;   /* at EOM this shouldn't happen */
	 } else {
	    dev->set_eof();
	 }
      }
      switch (i) {
      case 0:			      /* Want VOL1 label */
         if (stat != 80 || strncmp("VOL1", label, 4) != 0) {
            Dmsg0(000, "No VOL1 label\n");
            Mmsg0(jcr->errmsg, _("No VOL1 label while reading ANSI label.\n"));
	    return VOL_NO_LABEL;   /* No ANSI label */
	 }

	 dev->label_type = B_ANSI_LABEL;

	 /* Compare Volume Names allow special wild card */
         if (VolName && *VolName && *VolName != '*') { 
	    if (!same_label_names(VolName, &label[4])) {
	       char *p = &label[4];
	       char *q = dev->VolHdr.VolName;
               for (int i=0; *p != ' ' && i < 6; i++) {
		  *q++ = *p++;
	       }
	       *q = 0;
               Dmsg2(000, "Wanted ANSI Vol %s got %6s\n", VolName, dev->VolHdr.VolName);
               Mmsg2(jcr->errmsg, "Wanted ANSI Volume \"%s\" got \"%s\"\n", VolName, dev->VolHdr.VolName);
	       return VOL_NAME_ERROR;
	    }
	 }
	 break;
      case 1:
         if (stat != 80 || strncmp("HDR1", label, 4) != 0) {
            Dmsg0(000, "No HDR1 label\n");
            Mmsg0(jcr->errmsg, _("No HDR1 label while reading ANSI label.\n"));
	    return VOL_LABEL_ERROR;
	 }
         if (strncmp("BACULA.DATA", &label[4], 11) != 0) {
            Dmsg1(000, "HD1 not Bacula label. Wanted  BACULA.DATA got %11s\n",
	       &label[4]);
            Mmsg1(jcr->errmsg, _("ANSI Volume \"%s\" does not belong to Bacula.\n"),
	       dev->VolHdr.VolName);
	    return VOL_NAME_ERROR;     /* Not a Bacula label */
	 }
	 break;
      case 2:
         if (stat != 80 || strncmp("HDR2", label, 4) != 0) {
            Dmsg0(000, "No HDR2 label\n");
            Mmsg0(jcr->errmsg, _("No HDR2 label while reading ANSI label.\n"));
	    return VOL_LABEL_ERROR;
	 }
	 break;
      default:
	 if (stat == 0) {
            Dmsg0(000, "ANSI label OK\n");
	    return VOL_OK;
	 }
         if (stat != 80 || strncmp("HDR", label, 3) != 0) {
            Dmsg0(000, "Unknown or bad ANSI label record.\n");
            Mmsg0(jcr->errmsg, _("Unknown or bad ANSI label record.\n"));
	    return VOL_LABEL_ERROR;
	 }
	 break;
      }
   }
   Dmsg0(000, "Too many records in ANSI label.\n");
   Mmsg0(jcr->errmsg, _("Too many records in while reading ANSI label.\n"));
   return VOL_LABEL_ERROR;
}  

/*
 * Write an ANSI or IBM 80 character tape label
 *   Assume we are positioned at the beginning of the tape.
 *   Returns:  true of OK
 *	       false if error
 */
bool write_ansi_ibm_label(DCR *dcr, const char *VolName)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   char label[80];		      /* tape label */
   char date[20];		      /* ansi date buffer */
   time_t now;
   int len, stat, label_type;

   /*
    * If the Device requires a specific label type use it,
    * otherwise, use the type requested by the Director
    */
   if (dcr->device->label_type != B_BACULA_LABEL) {
      label_type = dcr->device->label_type;   /* force label type */
   } else {
      label_type = dcr->VolCatInfo.LabelType; /* accept Dir type */
   }

   switch (label_type) {
   case B_BACULA_LABEL:
      return true;
   case B_ANSI_LABEL:
   case B_IBM_LABEL:
      ser_declare;
      Dmsg1(000, "Write ANSI label type=%d\n", label_type);
      len = strlen(VolName);
      if (len > 6) {
         Jmsg1(jcr, M_FATAL, 0, _("ANSI Volume label name \"%s\" longer than 6 chars.\n"),
	    VolName);
	 return false;
      }
      memset(label, ' ', sizeof(label));
      ser_begin(label, sizeof(label));
      ser_bytes("VOL1", 4);
      ser_bytes(VolName, len);
      label[79] = '3';                /* ANSI label flag */
      /* Write VOL1 label */
      stat = write(dev->fd, label, sizeof(label));
      if (stat != sizeof(label)) {
	 berrno be;
         Jmsg1(jcr, M_FATAL, 0,  _("Could not write ANSI VOL1 label. ERR=%s\n"),
	    be.strerror());
	 return false;
      }
      /* Now construct HDR1 label */
      ser_begin(label, sizeof(label));
      ser_bytes("HDR1", 4);
      ser_bytes("BACULA.DATA", 11);            /* Filename field */
      ser_begin(&label[21], sizeof(label)-21); /* fileset field */
      ser_bytes(VolName, len);	      /* write Vol Ser No. */
      ser_begin(&label[27], sizeof(label)-27);
      ser_bytes("00010001000100", 14);  /* File section, File seq no, Generation no */
      now = time(NULL);
      ser_bytes(ansi_date(now, date), 6); /* current date */
      ser_bytes(ansi_date(now - 24 * 3600, date), 6); /* created yesterday */
      ser_bytes(" 000000Bacula              ", 27);
      /* Write HDR1 label */
      stat = write(dev->fd, label, sizeof(label));
      if (stat != sizeof(label)) {
	 berrno be;
         Jmsg1(jcr, M_FATAL, 0, _("Could not write ANSI HDR1 label. ERR=%s\n"),
	    be.strerror());
	 return false;
      }
      /* Now construct HDR2 label */
      memset(label, ' ', sizeof(label));
      ser_begin(label, sizeof(label));
      ser_bytes("HDR2F3200032000", 15);
      /* Write HDR1 label */
      stat = write(dev->fd, label, sizeof(label));
      if (stat != sizeof(label)) {
	 berrno be;
         Jmsg1(jcr, M_FATAL, 0, _("Could not write ANSI HDR1 label. ERR=%s\n"),
	    be.strerror());
	 return false;
      }
      if (weof_dev(dev, 1) < 0) {
         Jmsg(jcr, M_FATAL, 0, _("Error writing EOF to tape. ERR=%s"), dev->errmsg);
	 return false;
      }
      return true;
   default:
      Jmsg0(jcr, M_ABORT, 0, _("write_ansi_ibm_label called for non-ANSI/IBM type\n"));
      return false; /* should not get here */
   }
}

/* Check a Bacula Volume name against an ANSI Volume name */
static bool same_label_names(char *bacula_name, char *ansi_name)
{
   char *a = ansi_name;
   char *b = bacula_name;
   /* Six characters max */
   for (int i=0; i < 6; i++) {
      if (*a == *b) {
	 a++;
	 b++;
	 continue;
      }
      /* ANSI labels are blank filled, Bacula's are zero terminated */
      if (*a == ' ' && *b == 0) {
	 return true;
      }
      return false;
   }
   /* Reached 6 characters */
   b++;
   if (*b == 0) {
      return true;
   }
   return false;
}


static char *ansi_date(time_t td, char *buf)
{
   struct tm *tm;

   if (td == 0) {
      td = time(NULL);
   }
   tm = gmtime(&td);
   bsnprintf(buf, 10, " %05d ", 1000 * (tm->tm_year + 1900 - 2000) + tm->tm_yday);
   return buf;
}
