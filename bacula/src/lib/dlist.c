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
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

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
   ((dlink *)(((char *)item)+loffset))->next = NULL;
   ((dlink *)(((char *)item)+loffset))->prev = tail;
   if (tail) {
      ((dlink *)(((char *)tail)+loffset))->next = item;
   }
   tail = item;
   if (head == NULL) {		      /* if empty list, */
      head = item;		      /* item is head as well */
   }
   num_items++;
}

/*
 * Append an item to the list
 */
void dlist::prepend(void *item) 
{
   ((dlink *)(((char *)item)+loffset))->next = head;
   ((dlink *)(((char *)item)+loffset))->prev = NULL;
   if (head) {
      ((dlink *)(((char *)head)+loffset))->prev = item;
   }
   head = item;
   if (tail == NULL) {		      /* if empty list, */		      
      tail = item;		      /* item is tail too */
   }
   num_items++;
}

void dlist::insert_before(void *item, void *where)	 
{
   dlink *where_link = (dlink *)((char *)where+loffset);     

   ((dlink *)(((char *)item)+loffset))->next = where;
   ((dlink *)(((char *)item)+loffset))->prev = where_link->prev;

   if (where_link->prev) {
      ((dlink *)(((char *)(where_link->prev))+loffset))->next = item;
   }
   where_link->prev = item;
   if (head == where) {
      head = item;
   }
   num_items++;
}

void dlist::insert_after(void *item, void *where)	
{
   dlink *where_link = (dlink *)((char *)where+loffset);     

   ((dlink *)(((char *)item)+loffset))->next = where_link->next;
   ((dlink *)(((char *)item)+loffset))->prev = where;

   if (where_link->next) {
      ((dlink *)(((char *)(where_link->next))+loffset))->prev = item;
   }
   where_link->next = item;
   if (tail == where) {
      tail = item;
   }
   num_items++;
}


void dlist::remove(void *item)
{
   void *xitem;
   dlink *ilink = (dlink *)(((char *)item)+loffset);   /* item's link */
   if (item == head) {
      head = ilink->next;
      if (head) {
	 ((dlink *)(((char *)head)+loffset))->prev = NULL;
      }
      if (item == tail) {
	 tail = ilink->prev;
      }
   } else if (item == tail) {
      tail = ilink->prev;
      if (tail) {
	 ((dlink *)(((char *)tail)+loffset))->next = NULL;
      }
   } else {
      xitem = ilink->next;
      ((dlink *)(((char *)xitem)+loffset))->prev = ilink->prev;
      xitem = ilink->prev;
      ((dlink *)(((char *)xitem)+loffset))->next = ilink->next;
   }
   num_items--;
   if (num_items == 0) {
      head = tail = NULL;
   }
}

void * dlist::next(void *item)
{
   if (item == NULL) {
      return head;
   }
   return ((dlink *)(((char *)item)+loffset))->next;
}

void * dlist::prev(void *item)
{
   if (item == NULL) {
      return tail;
   }
   return ((dlink *)(((char *)item)+loffset))->prev;
}


/* Destroy the list contents */
void dlist::destroy()
{
   for (void *n=head; n; ) {
      void *ni = ((dlink *)(((char *)n)+loffset))->next;
      free(n);
      n = ni;
   }
   num_items = 0;
   head = tail = NULL;
}



#ifdef TEST_PROGRAM

struct MYJCR {
   char *buf;
   dlink link;
};

int main()
{
   char buf[30];
   dlist *jcr_chain;
   MYJCR *jcr = NULL;
   MYJCR *save_jcr = NULL;
   MYJCR *next_jcr;

   jcr_chain = (dlist *)malloc(sizeof(dlist));
   jcr_chain->init(jcr, &jcr->link);
    
   printf("Prepend 20 items 0-19\n");
   for (int i=0; i<20; i++) {
      sprintf(buf, "This is dlist item %d", i);
      jcr = (MYJCR *)malloc(sizeof(MYJCR));
      jcr->buf = bstrdup(buf);
      jcr_chain->prepend(jcr);
      if (i == 10) {
	 save_jcr = jcr;
      }
   }

   next_jcr = (MYJCR *)jcr_chain->next(save_jcr);
   printf("11th item=%s\n", next_jcr->buf);
   jcr = (MYJCR *)malloc(sizeof(MYJCR));
   jcr->buf = save_jcr->buf;
   printf("Remove 10th item\n");
   jcr_chain->remove(save_jcr);
   printf("Re-insert 10th item\n");
   jcr_chain->insert_before(jcr, next_jcr);
   
   printf("Print remaining list.\n");
   foreach_dlist (jcr, jcr_chain) {
      printf("Dlist item = %s\n", jcr->buf);
      free(jcr->buf);
   }
   jcr_chain->destroy();
   free(jcr_chain);

   jcr_chain = new dlist(jcr, &jcr->link);
   printf("append 20 items 0-19\n");
   for (int i=0; i<20; i++) {
      sprintf(buf, "This is dlist item %d", i);
      jcr = (MYJCR *)malloc(sizeof(MYJCR));
      jcr->buf = bstrdup(buf);
      jcr_chain->append(jcr);
      if (i == 10) {
	 save_jcr = jcr;
      }
   }

   next_jcr = (MYJCR *)jcr_chain->next(save_jcr);
   printf("11th item=%s\n", next_jcr->buf);
   jcr = (MYJCR *)malloc(sizeof(MYJCR));
   jcr->buf = save_jcr->buf;
   printf("Remove 10th item\n");
   jcr_chain->remove(save_jcr);
   printf("Re-insert 10th item\n");
   jcr_chain->insert_before(jcr, next_jcr);
   
   printf("Print remaining list.\n");
   foreach_dlist (jcr, jcr_chain) {
      printf("Dlist item = %s\n", jcr->buf);
      free(jcr->buf);
   }

   delete jcr_chain;

   sm_dump(false);

}
#endif
