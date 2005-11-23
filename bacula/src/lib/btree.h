/*
 *   Version $Id$
 */
/*
   Copyright (C) 2005 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */


/* ========================================================================
 *
 *   red-black binary tree routines -- btree.h
 *
 *    Kern Sibbald, MMV
 *
 */

#define M_ABORT 1

/*
 * There is a lot of extra casting here to work around the fact
 * that some compilers (Sun and Visual C++) do not accept
 * (bnode *) as an lvalue on the left side of an equal.
 *
 * Loop var through each member of list
 */
#define foreach_btree(var, tree) \
    for(*((bnode **)&(var))=(tree)->first(); (*((bnode **)&(var))=(tree)->next((bnode *)var)); )

#ifdef the_old_way
#define foreach_btree(var, tree) \
        for((var)=(tree)->first(); (((bnode *)(var))=(tree)->next((bnode *)var)); )
#endif

struct bnode;
struct bnode {
   bnode *left;
   bnode *right;
   bnode *parent;
   bool red;
};

class btree : public SMARTALLOC {
   bnode *head;
   uint32_t num_items;
   bool down;
   void left_rotate(bnode *item);
   void right_rotate(bnode *item);
public:
   btree(void);
   ~btree() { destroy(); }
   void init(void);
   bnode *insert(bnode *item, int compare(bnode *item1, bnode *item2));
   bnode *search(bnode *item, int compare(bnode *item1, bnode *item2));
   bnode *first(void);
   bnode *next(bnode *item);
   bnode *any(bnode *item);
   void remove(bnode *item);
   int  size() const;
   void destroy();
};


/*
 * This allows us to do explicit initialization,
 *   allowing us to mix C++ classes inside malloc'ed
 *   C structures. Define before called in constructor.
 */
inline void btree::init()
{
   head = NULL;
   num_items = 0;
}


/* Constructor with link at head of item */
inline btree::btree(void) : head(0), num_items(0)
{
}

inline int btree::size() const
{
   return num_items;
}
