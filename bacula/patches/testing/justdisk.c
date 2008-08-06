/*
 * This file is used to test simple I/O operation for accurate mode
 *
 *
 * 1) Loading
 *    fseek()   ?
 *    fwrite()
 *
 * 2) Fetch + Update
 *    fseek(pos1)
 *    fread()
 *    fseek(pos1)
 *    fwrite()
 *
 * 3) Read all
 *    fseek()
 *    fread()
 *    
 */

/*
 cd regress/build
 make
 cd patches/testing
 g++ -g -Wall -I../../src -I../../src/lib -L../../src/lib justdisk.c -lbac -lpthread -lssl -D_TEST_BUF
 ./a.out
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "bacula.h"

typedef struct {
   int32_t path;
   int32_t filename;
   int32_t seen;
   int32_t pad;
   int64_t mtime;
   int64_t ctime;
} AccurateElt;

#define NB_ELT 50000000

typedef struct {
   char *buf;
   rblink *link;
} MY;

static int my_cmp(void *item1, void *item2)
{
   MY *elt1, *elt2;
   elt1 = (MY *) item1;
   elt2 = (MY *) item2;
   printf("cmp(%s, %s)\n", elt1->buf, elt2->buf);
   return strcmp(elt1->buf, elt2->buf);
}

rblist *load_rb(const char *file)
{
   FILE *fp;
   char buffer[1024];
   MY *res;
   rblist *lst;
   lst = New(rblist(res, res->link));

   fp = fopen(file, "r");
   if (!fp) {
      return NULL;
   }
   while (fgets(buffer, sizeof(buffer), fp)) {
      if (*buffer) {
         int len = strlen(buffer);
         if (len > 0) {
            buffer[len-1]=0;       /* zap \n */
         }
         MY *buf = (MY *)malloc(sizeof(MY));
         memset(buf, 0, sizeof(MY));
         buf->buf = bstrdup(buffer);
         res = (MY *)lst->insert(buf, my_cmp);
         if (res != buf) {
            free(buf->buf);
            free(buf);
         }
      }
   }
   fclose(fp);

   return lst;
}

/* buffer used for 4k io operations */
class AccurateBuffer
{
public:
   AccurateBuffer() { 
      _fp=NULL; 
      _nb=-1;
      _max_nb=-1;
      _dirty=0; 
      memset(_buf, 0, sizeof(_buf));
   };
   void *get_elt(int nb);
   void init();
   void destroy();
   void update_elt(int nb);
   ~AccurateBuffer() {
      destroy();
   }
private:
   char _buf[4096];
   int _nb;
   int _max_nb;
   char _dirty;
   FILE *_fp;
};

void AccurateBuffer::init()
{
   _fp = fopen("testfile", "w+");
   if (!_fp) {
      exit(1);
   }
   _nb=-1;
   _max_nb=-1;
}

void AccurateBuffer::destroy()
{
   if (_fp) {
      fclose(_fp);
      _fp = NULL;
   }
}

void *AccurateBuffer::get_elt(int nb)
{
   int page=nb*sizeof(AccurateElt)/sizeof(_buf);

   if (!_fp) {
      init();
   }

   if (page != _nb) {		/* not the same page */
      if (_dirty) {		/* have to sync on disk */
//	 printf("put dirty page on disk %i\n", _nb);
	 if (fseek(_fp, _nb*sizeof(_buf), SEEK_SET) == -1) {
	    perror("bad fseek");
	    exit(3);
	 }
	 if (fwrite(_buf, sizeof(_buf), 1, _fp) != 1) {
	    perror("writing...");
	    exit(2);
	 }
	 _dirty=0;
      }
      if (page <= _max_nb) {	/* we read it only if the page exists */
//       printf("read page from disk %i <= %i\n", page, _max_nb);
	 fseek(_fp, page*sizeof(_buf), SEEK_SET);
	 if (fread(_buf, sizeof(_buf), 1, _fp) != 1) {
//	    printf("memset to zero\n");
	    memset(_buf, 0, sizeof(_buf));
	 }
      } else {
	 memset(_buf, 0, sizeof(_buf));
      }
      _nb = page;
      _max_nb = MAX(_max_nb, page);
   }

   /* compute addr of the element in _buf */
   int addr=(nb%(sizeof(_buf)/sizeof(AccurateElt)))*sizeof(AccurateElt);
// printf("addr=%i\n", addr);
   return (void *) (_buf + addr);
}

void AccurateBuffer::update_elt(int nb)
{
   _dirty = 1;
}


#ifdef _TEST_OPEN
int main()
{
   int fd;
   int i;
   AccurateElt elt;
   char *start_heap = (char *)sbrk(0);

   rblist *rb_file = load_rb("files");
   rblist *rb_path = load_rb("path");

   char *etc = (char *)rb_path->search((void *)"/etc/", my_cmp);

   printf("%p\n", etc);

   fd = open("testfile", O_CREAT | O_RDWR, 0600);
   if (fd<0) {
      perror("E: Can't open testfile ");
      return(1);
   }

   memset(&elt, 0, sizeof(elt));

   /* 1) Loading */
   for (i=0; i<NB_ELT; i++) {
      write(fd, &elt, sizeof(elt));
   }

   lseek(fd, 0, SEEK_SET);	/* rewind */

   /* 2) load and update */
   for (i=0; i<NB_ELT; i++) {
      lseek(fd, i*sizeof(AccurateElt), SEEK_SET);
      read(fd, &elt, sizeof(elt));
      lseek(fd, i*sizeof(AccurateElt), SEEK_SET);
      write(fd, &elt, sizeof(elt));
   }

   lseek(fd, 0, SEEK_SET);	/* rewind */

   /* 3) Fetch all of them */
   for (i=0; i<NB_ELT; i++) {
      read(fd, &elt, sizeof(elt));
   }

   close(fd);

   fprintf(stderr, "heap;%lld\n", (long long)((char *)sbrk(0) - start_heap));
   sleep(50);
   return (0);
}
#else  /* _TEST_OPEN */
#ifdef _TEST_BUF
int main()
{
   char *start_heap = (char *)sbrk(0);

   int i;
   AccurateElt *elt;
   AccurateBuffer *buf = new AccurateBuffer;

   /* 1) Loading */
   printf("Loading...\n");
   for (i=0; i<NB_ELT; i++) {
      elt = (AccurateElt *) buf->get_elt(i);
      elt->mtime = i;
      buf->update_elt(i);
   }

   /* 2) load and update */
   printf("Load and update...\n");
   for (i=0; i<NB_ELT; i++) {
      elt = (AccurateElt *) buf->get_elt(i);
      if (elt->mtime != i) {
	 printf("Something is wrong with elt %i mtime=%lli\n", i, elt->mtime);	 
	 exit (0);
      }
      elt->seen = i;
      buf->update_elt(i);
   }

   /* 3) Fetch all of them */
   printf("Fetch them...\n");
   for (i=0; i<NB_ELT; i++) {
      elt = (AccurateElt *) buf->get_elt(i);
      if (elt->seen != i || elt->mtime != i) {
	 printf("Something is wrong with elt %i mtime=%lli seen=%i\n", i, elt->mtime, elt->seen);
	 exit (0);
      }
   }
   fprintf(stderr, "heap;%lld\n", (long long)((char *)sbrk(0) - start_heap));
   delete buf;

   return(0);
}

#else
int main()
{
   FILE *fd;
   int i;
   AccurateElt elt;
   fd = fopen("testfile", "w+");
   if (!fd) {
      perror("E: Can't open testfile ");
      return(1);
   }

   memset(&elt, 0, sizeof(elt));

   /* 1) Loading */
   printf("Loading...\n");
   for (i=0; i<NB_ELT; i++) {
      fwrite(&elt, sizeof(elt), 1, fd);
   }

   fseek(fd, 0, SEEK_SET);	/* rewind */

   /* 2) load and update */
   printf("Load and update...\n");
   for (i=0; i<NB_ELT; i++) {
      fseek(fd, i*sizeof(AccurateElt), SEEK_SET);
      fread(&elt, sizeof(elt), 1, fd);
      fseek(fd, i*sizeof(AccurateElt), SEEK_SET);
      fwrite(&elt, sizeof(elt), 1, fd);
   }

   fseek(fd, 0, SEEK_SET);	/* rewind */

   /* 3) Fetch all of them */
   printf("Fetch them...\n");
   for (i=0; i<NB_ELT; i++) {
      fread(&elt, sizeof(elt), 1, fd);
   }

   fclose(fd);
   return(0);
}
#endif	/* _TEST_BUF */
#endif	/* _TEST_OPEN */
