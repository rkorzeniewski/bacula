/*
 *  Bacula doubly linked list routines. 	 
 *
 *    dlist is a doubly linked list with the links being in the
 *	 list data item.
 *		  
 *   Kern Sibbald, July MMIII
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

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

/* ===================================================================
 *    dlist
 */
/*
 * Append an item to the list
 */
void dlist::append(void *item) 
{
   ((dlink *)((char *)item+loffset))->next = NULL;
   ((dlink *)((char *)item+loffset))->prev = tail;
   if (tail) {
      ((dlink *)((char *)tail+loffset))->next = item;
   }
   tail = item;
   if (head == NULL) {		      /* if empty list, */
      head = item;		      /* item is head as well */
   }
}

/*
 * Append an item to the list
 */
void dlist::prepend(void *item) 
{
   ((dlink *)((char *)item+loffset))->next = head;
   ((dlink *)((char *)item+loffset))->prev = NULL;
   if (head) {
      ((dlink *)((char *)head+loffset))->prev = item;
   }
   head = item;
   if (tail == NULL) {		      /* if empty list, */		      
      tail = item;		      /* item is tail too */
   }
}

void dlist::remove(void *item)
{
   void *xitem;
   dlink *ilink = (dlink *)((char *)item+loffset);   /* item's link */
   if (item == head) {
      head = ilink->next;
      ((dlink *)((char *)head+loffset))->prev = NULL;
   } else if (item == tail) {
      tail = ilink->prev;
      ((dlink *)((char *)tail+loffset))->next = NULL;
   } else {
      xitem = ilink->next;
      ((dlink *)((char *)xitem+loffset))->prev = ilink->prev;
      xitem = ilink->prev;
      ((dlink *)((char *)xitem+loffset))->next = ilink->next;
   }
   free(item);
}

void * dlist::next(void *item)
{
   if (item == NULL) {
      return head;
   }
   return ((dlink *)((char *)item+loffset))->next;
}

/* Destroy the list and its contents */
void dlist::destroy()
{
   for (void *n=head; n; ) {
      void *ni = ((dlink *)((char *)n+loffset))->next;
      free(n);
      n = ni;
   }
}



#ifdef TEST_PROGRAM

struct MYJCR {
   char *buf;
   dlist link;
};

int main()
{
   char buf[30];
   dlist *jcr_chain;
   MYJCR *save_jcr = NULL;

   jcr_chain = (dlist *)malloc(sizeof(dlist));
   jcr_chain->init((int)&MYJCR::link);
    
   printf("Prepend 20 items 0-19\n");
   for (int i=0; i<20; i++) {
      MYJCR *jcr;
      sprintf(buf, "This is dlist item %d", i);
      jcr = (MYJCR *)malloc(sizeof(MYJCR));
      jcr->buf = bstrdup(buf);
      jcr_chain->prepend(jcr);
      if (i == 10) {
	 save_jcr = jcr;
      }
   }

   printf("Remove 10th item\n");
   free(save_jcr->buf);
   jcr_chain->remove(save_jcr);
   
   printf("Print remaining list.\n");
   for (MYJCR *jcr=NULL; (jcr=(MYJCR *)jcr_chain->next(jcr)); ) {
      printf("Dlist item = %s\n", jcr->buf);
      free(jcr->buf);
   }
   jcr_chain->destroy();
   free(jcr_chain);

   sm_dump(False);

}
#endif
