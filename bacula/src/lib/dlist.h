/*
 *   Version $Id$
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

/* ========================================================================
 * 
 *   Doubly linked list  -- dlist
 *
 */

/* In case you want to specifically specify the offset to the link */
#define OFFSET(item, link) ((char *)(link) - (char *)(item))
#ifdef HAVE_WIN32
/* Extra ***& workaround for VisualC Studio */
#define foreach_dlist(var, list) \
    for((var)=NULL; (*((void **)&(var))=(void*)((list)->next(var))); )
#else
/*
 * Loop var through each member of list
 */
#define foreach_dlist(var, list) \
        for((var)=NULL; (((void *)(var))=(list)->next(var)); )
#endif

struct dlink {
   void *next;
   void *prev;
};


class dlist {
   void *head;
   void *tail;
   int loffset;
   int num_items;
public:
   dlist(void *item, void *link);
   void init(void *item, void *link);
   void prepend(void *item);
   void append(void *item);
   void insert_before(void *item, void *where);
   void insert_after(void *item, void *where);
   void remove(void *item);
   bool empty();
   int  size();
   void *next(void *item);
   void *prev(void *item);
   void destroy();
   void *first();
   void *last();
   void * operator new(size_t);
   void operator delete(void *);
};

/*                            
 * This allows us to do explicit initialization,
 *   allowing us to mix C++ classes inside malloc'ed
 *   C structures. Define before called in constructor.
 */
inline void dlist::init(void *item, void *link) 
{
   head = tail = NULL;
   loffset = (char *)link - (char *)item;
   num_items = 0;
}

/* Constructor */
inline dlist::dlist(void *item, void *link)
{
   this->init(item, link);
}

inline bool dlist::empty()
{
   return head == NULL;
}

inline int dlist::size()
{
   return num_items;
}

   
inline void * dlist::operator new(size_t)
{
   return malloc(sizeof(dlist));
}

inline void dlist::operator delete(void  *item)
{
   ((dlist *)item)->destroy();
   free(item);
}
 

inline void * dlist::first()
{
   return head;
}

inline void * dlist::last()
{
   return tail;
}
