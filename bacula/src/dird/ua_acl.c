/*
 *
 *   Bacula Director -- User Agent Access Control List (ACL) handling
 *
 *     Kern Sibbald, January MMIV  
 *
 *   Version  $Id$
 */

/*
   Copyright (C) 2004 Kern Sibbald and John Walker

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
#include "dird.h"

/*  
 * Check if access is permitted to item in acl	  
 */
bool acl_access_ok(UAContext *ua, int acl, char *item)
{
   return acl_access_ok(ua, acl, item, strlen(item));
}


bool acl_access_ok(UAContext *ua, int acl, char *item, int len)
{

   /* If no console resource => default console and all is permitted */
   if (!ua->cons) {
      Dmsg0(400, "Root cons access OK.\n");
      return true;		      /* No cons resource -> root console OK for everything */
   }

   alist *list = ua->cons->ACL_lists[acl];
   if (!list) {
      return false;		      /* List empty, reject everything */
   }

   /* Special case *all* gives full access */
   if (list->size() == 1 && strcasecmp("*all*", (char *)list->get(0)) == 0) {
      return true;
   }

   /* Search list for item */
   for (int i=0; i<list->size(); i++) {
      if (strncasecmp(item, (char *)list->get(i), len) == 0) {
         Dmsg3(400, "Found %s in %d %s\n", item, acl, (char *)list->get(i));
	 return true;
      }
   }
   return false;
}
