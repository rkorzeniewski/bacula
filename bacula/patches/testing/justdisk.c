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
g++ -g -Wall -I../../src -I../../src/lib -L../../src/lib justdisk.c -lbac -lpthread -lssl -D_TEST_OPEN
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "bacula.h"

typedef struct {
   int path;
   int filename;
   int mtime;
   int ctime;
   int seen;
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
#endif	/* _TEST_OPEN */
