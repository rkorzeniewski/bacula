/*
 * Includes specific to the Director
 *
 *     Kern Sibbald, December MM
 *
 *    Version $Id$
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

#include "dird_conf.h"

#define DIRECTOR_DAEMON 1

/* The following includes are at the bottom of 
 * this file rather than at the top because the
 *  #include "jcr.h" uses the definition of JOB
 * as supplied above.
 */

#include "cats/cats.h"

#include "jcr.h"

#include "protos.h"

/* Globals that dird.c exports */
extern int debug_level;
extern time_t start_time;
extern DIRRES *director;                     /* Director resource */
extern char *working_directory;              /* export our working directory */
extern int FDConnectTimeout;
extern int SDConnectTimeout;

/* From job.c */
void dird_free_jcr(JCR *jcr);
