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

/* 
 * Array list -- much like a simplified STL vector
 *   array of pointers to inserted items
 */
class alist {
   void **items;
   int num_items;
   int max_items;
   int num_grow;
   bool own_items;
public:
   alist(int num = 1, bool own=true);
   void init(int num = 1, bool own=true);
   void append(void *item);
   void *get(int index);
   void * operator [](int index) const;
   int size();
   void destroy();
   void grow(int num);
   void * operator new(size_t);
   void operator delete(void *);
};

inline void * alist::operator [](int index) const {
   if (index < 0 || index >= num_items) {
      return NULL;
   }
   return items[index];
}

/*			      
 * This allows us to do explicit initialization,
 *   allowing us to mix C++ classes inside malloc'ed
 *   C structures. Define before called in constructor.
 */
inline void alist::init(int num, bool own) {
   items = NULL;
   num_items = 0;
   max_items = 0;
   num_grow = num;
   own_items = own;
}

/* Constructor */
inline alist::alist(int num, bool own) {
   this->init(num, own);
}
   


/* Current size of list */
inline int alist::size()
{
   return num_items;
}

/* How much to grow by each time */
inline void alist::grow(int num) 
{
   num_grow = num;
}

inline void * alist::operator new(size_t)
{
   return malloc(sizeof(alist));
}

inline void alist::operator delete(void  *item)
{
   ((alist *)item)->destroy();
   free(item);
}
