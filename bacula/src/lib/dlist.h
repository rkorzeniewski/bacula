/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *  Written by Kern Sibbald MMIV
 *
 */


/* ========================================================================
 *
 *   Doubly linked list  -- dlist
 *
 *   See the end of the file for the dlistString class which
 *     facilitates storing strings in a dlist.
 *
 *    Kern Sibbald, MMIV and MMVII
 *
 */

#define M_ABORT 1

/* In case you want to specifically specify the offset to the link */
#define OFFSET(item, link) (int)((char *)(link) - (char *)(item))
/*
 * There is a lot of extra casting here to work around the fact
 * that some compilers (Sun and Visual C++) do not accept
 * (void *) as an lvalue on the left side of an equal.
 *
 * Loop var through each member of list
 */
#ifdef HAVE_TYPEOF
#define foreach_dlist(var, list) \
        for((var)=NULL; ((var)=(typeof(var))(list)->next(var)); )
#else
#define foreach_dlist(var, list) \
    for((var)=NULL; (*((void **)&(var))=(void*)((list)->next(var))); )
#endif

struct dlink {
   void *next;
   void *prev;
};

class dlist : public SMARTALLOC {
   void *head;
   void *tail;
   int16_t loffset;
   uint32_t num_items;
public:
   dlist(void *item, dlink *link);
   dlist(void);
   ~dlist() { destroy(); }
   void init(void *item, dlink *link);
   void init();
   void prepend(void *item);
   void append(void *item);
   void set_prev(void *item, void *prev);
   void set_next(void *item, void *next);
   void *get_prev(void *item);
   void *get_next(void *item);
   dlink *get_link(void *item);
   void insert_before(void *item, void *where);
   void insert_after(void *item, void *where);
   void *binary_insert(void *item, int compare(void *item1, void *item2));
   void *binary_search(void *item, int compare(void *item1, void *item2));
   void binary_insert_multiple(void *item, int compare(void *item1, void *item2));
   void remove(void *item);
   bool empty() const;
   int  size() const;
   void *next(void *item);
   void *prev(void *item);
   void destroy();
   void *first() const;
   void *last() const;
};


/*
 * This allows us to do explicit initialization,
 *   allowing us to mix C++ classes inside malloc'ed
 *   C structures. Define before called in constructor.
 */
inline void dlist::init(void *item, dlink *link)
{
   head = tail = NULL;
   loffset = (int)((char *)link - (char *)item);
   if (loffset < 0 || loffset > 5000) {
      Emsg0(M_ABORT, 0, "Improper dlist initialization.\n");
   }
   num_items = 0;
}

inline void dlist::init()
{
   head = tail = NULL;
   loffset = 0;
   num_items = 0;
}


/*
 * Constructor called with the address of a
 *   member of the list (not the list head), and
 *   the address of the link within that member.
 * If the link is at the beginning of the list member,
 *   then there is no need to specify the link address
 *   since the offset is zero.
 */
inline dlist::dlist(void *item, dlink *link)
{
   init(item, link);
}

/* Constructor with link at head of item */
inline dlist::dlist(void) : head(0), tail(0), loffset(0), num_items(0)
{
}

inline void dlist::set_prev(void *item, void *prev)
{
   ((dlink *)(((char *)item)+loffset))->prev = prev;
}

inline void dlist::set_next(void *item, void *next)
{
   ((dlink *)(((char *)item)+loffset))->next = next;
}

inline void *dlist::get_prev(void *item)
{
   return ((dlink *)(((char *)item)+loffset))->prev;
}

inline void *dlist::get_next(void *item)
{
   return ((dlink *)(((char *)item)+loffset))->next;
}


inline dlink *dlist::get_link(void *item)
{
   return (dlink *)(((char *)item)+loffset);
}



inline bool dlist::empty() const
{
   return head == NULL;
}

inline int dlist::size() const
{
   return num_items;
}



inline void * dlist::first() const
{
   return head;
}

inline void * dlist::last() const
{
   return tail;
}

/*
 * C string helper routines for dlist
 *   The string (char *) is kept in the node
 *
 *   Kern Sibbald, February 2007
 *
 */
class dlistString
{
public:
   char *c_str() { return m_str; };

private:
   dlink m_link;
   char m_str[1];
   /* !!! Don't put anything after this as this space is used
    *     to hold the string in inline
    */
};

extern dlistString *new_dlistString(const char *str, int len);
extern dlistString *new_dlistString(const char *str);
