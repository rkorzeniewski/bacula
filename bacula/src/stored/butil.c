/*
 *
 *  Utility routines for "tool" programs such as bscan, bls,
 *    bextract, ...  
 * 
 *  Normally nothing in this file is called by the Storage   
 *    daemon because we interact more directly with the user
 *    i.e. printf, ...
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

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

#include "bacula.h"
#include "stored.h"


#ifdef DEBUG
char *rec_state_to_str(DEV_RECORD *rec)
{
   static char buf[200]; 
   buf[0] = 0;
   if (rec->state & REC_NO_HEADER) {
      strcat(buf, "Nohdr,");
   }
   if (is_partial_record(rec)) {
      strcat(buf, "partial,");
   }
   if (rec->state & REC_BLOCK_EMPTY) {
      strcat(buf, "empty,");
   }
   if (rec->state & REC_NO_MATCH) {
      strcat(buf, "Nomatch,");
   }
   if (rec->state & REC_CONTINUATION) {
      strcat(buf, "cont,");
   }
   if (buf[0]) {
      buf[strlen(buf)-1] = 0;
   }
   return buf;
}
#endif


/*
 * Setup device, jcr, and prepare to read
 */
DEVICE *setup_to_read_device(JCR *jcr)
{
   DEVICE *dev;
   DEV_BLOCK *block;
   char *p;

   jcr->VolumeName[0] = 0;
   if (strncmp(jcr->dev_name, "/dev/", 5) != 0) {
      /* Try stripping file part */
      p = jcr->dev_name + strlen(jcr->dev_name);
      while (p >= jcr->dev_name && *p != '/')
	 p--;
      if (*p == '/') {
	 strcpy(jcr->VolumeName, p+1);
	 *p = 0;
      }
   }

   dev = init_dev(NULL, jcr->dev_name);
   if (!dev) {
      Emsg1(M_FATAL, 0, "Cannot open %s\n", jcr->dev_name);
      return NULL;
   }
   /* ***FIXME**** init capabilities */
   if (!open_device(dev)) {
      Emsg1(M_FATAL, 0, "Cannot open %s\n", jcr->dev_name);
      return NULL;
   }
   Dmsg0(90, "Device opened for read.\n");

   block = new_block(dev);

   create_vol_list(jcr);

   Dmsg1(100, "Volume=%s\n", jcr->VolumeName);
   if (!acquire_device_for_read(jcr, dev, block)) {
      Emsg0(M_ERROR, 0, dev->errmsg);
      return NULL;
   }
   free_block(block);
   return dev;
}


static void my_free_jcr(JCR *jcr)
{
   return;
}

/*
 * Setup a "daemon" JCR for the various standalone
 *  tools (e.g. bls, bextract, bscan, ...)
 */
JCR *setup_jcr(char *name, char *device, BSR *bsr) 
{
   JCR *jcr = new_jcr(sizeof(JCR), my_free_jcr);
   jcr->VolSessionId = 1;
   jcr->VolSessionTime = (uint32_t)time(NULL);
   jcr->bsr = bsr;
   strcpy(jcr->Job, name);
   jcr->dev_name = get_pool_memory(PM_FNAME);
   strcpy(jcr->dev_name, device);
   return jcr;
}


/*
 * Device got an error, attempt to analyse it
 */
void display_error_status(DEVICE *dev)
{
   uint32_t status;

   Emsg0(M_ERROR, 0, dev->errmsg);
   status_dev(dev, &status);
   Dmsg1(20, "Device status: %x\n", status);
   if (status & MT_EOD)
      Emsg0(M_ERROR_TERM, 0, "Unexpected End of Data\n");
   else if (status & MT_EOT)
      Emsg0(M_ERROR_TERM, 0, "Unexpected End of Tape\n");
   else if (status & MT_EOF)
      Emsg0(M_ERROR_TERM, 0, "Unexpected End of File\n");
   else if (status & MT_DR_OPEN)
      Emsg0(M_ERROR_TERM, 0, "Tape Door is Open\n");
   else if (!(status & MT_ONLINE))
      Emsg0(M_ERROR_TERM, 0, "Unexpected Tape is Off-line\n");
   else
      Emsg2(M_ERROR_TERM, 0, "Read error on Record Header %s: %s\n", dev_name(dev), strerror(errno));
}


extern char *getuser(uid_t uid);
extern char *getgroup(gid_t gid);

void print_ls_output(char *fname, char *link, int type, struct stat *statp)
{
   char buf[1000]; 
   char ec1[30];
   char *p, *f;
   int n;

   p = encode_mode(statp->st_mode, buf);
   n = sprintf(p, "  %2d ", (uint32_t)statp->st_nlink);
   p += n;
   n = sprintf(p, "%-8.8s %-8.8s", getuser(statp->st_uid), getgroup(statp->st_gid));
   p += n;
   n = sprintf(p, "%8.8s ", edit_uint64(statp->st_size, ec1));
   p += n;
   p = encode_time(statp->st_ctime, p);
   *p++ = ' ';
   *p++ = ' ';
   /* Copy file name */
   for (f=fname; *f && (p-buf) < (int)sizeof(buf); )
      *p++ = *f++;
   if (type == FT_LNK) {
      *p++ = ' ';
      *p++ = '-';
      *p++ = '>';
      *p++ = ' ';
      /* Copy link name */
      for (f=link; *f && (p-buf) < (int)sizeof(buf); )
	 *p++ = *f++;
   }
   *p++ = '\n';
   *p = 0;
   fputs(buf, stdout);
}
