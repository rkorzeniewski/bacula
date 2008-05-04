#include <sys/mtio.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifndef FTAPE
#include "faketape.h"

#define write	faketape_write
#define open    faketape_open
#define read    faketape_read
#define close   faketape_close
#define ioctl   faketape_ioctl
#endif

static int fd;
void print_pos()
{
   struct mtget mt_get;
   ioctl(fd, MTIOCGET, &mt_get);
   printf("file:block %i:%i\n", mt_get.mt_fileno, mt_get.mt_blkno);
}

int main()
{
   int r1, r2;
   char c[200];
   struct mtop mt_com;

   fd  = open("/dev/lto2", O_CREAT | O_RDWR, 0700);

   /* rewind */
   mt_com.mt_count = 1;
   mt_com.mt_op = MTREW;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   printf("rewind\n");
   print_pos();

   /* write something */
   write(fd, "abcdefghijklmnopqrstuvwyz", strlen("abcdefghijklmnopqrstuvwyz")+1);
   write(fd, "abcdefghijklmnopqrstuvwyz", strlen("abcdefghijklmnopqrstuvwyz")+1);
   write(fd, "abcdefghijklmnopqrstuvwyz", strlen("abcdefghijklmnopqrstuvwyz")+1);
   printf("write something (3 writes)\n");
   print_pos();

   /* rewind */
   mt_com.mt_count = 1;
   mt_com.mt_op = MTREW;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   printf("rewind\n");

   /* read something with error */
   errno=0;
   r1 = read(fd, c, 2);
   c[r1] = 0;
   printf("read c=%s len=%i errno=%i\n", c, r1, errno);
   perror("");
   print_pos();

   /* read something */
   errno=0;
   r1 = read(fd, c, 200);
   c[r1] = 0;
   printf("read c=%s len=%i\n", c, r1);
   print_pos();

   /* write something */
   printf("write something\n");
   write(fd, "abcdefghijklmnopqrstuvwyz", strlen("abcdefghijklmnopqrstuvwyz")+1);
   print_pos();
   
   /* rewind */
   mt_com.mt_count = 1;
   mt_com.mt_op = MTREW;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   r1 = read(fd, c, 200);
   c[r1] = '\0';
   printf("read c=%s len=%i\n", c, r1);
   r1 = read(fd, c, 200);
   c[r1] = '\0';
   printf("read c=%s len=%i\n", c, r1);
 
   /* write EOF */
   printf("WEOF\n");
   mt_com.mt_op = MTWEOF;
   mt_com.mt_count = 1;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   print_pos();

   /* FSF */
   mt_com.mt_op = MTFSF;
   mt_com.mt_count = 1;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   printf("fsf r=%i\n", r1);
   print_pos();

   /* write something */
   printf("write something\n");
   write(fd, "abcdefghijklmnopqrstuvwyz", strlen("abcdefghijklmnopqrstuvwyz")+1);
   print_pos();

   /* FSF */
   mt_com.mt_op = MTFSF;
   mt_com.mt_count = 1;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   printf("fsf r=%i\n", r1);
   print_pos();

   /* WEOF */
   mt_com.mt_op = MTWEOF;
   mt_com.mt_count = 3;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   printf("weof 3 r=%i\n", r1);
   print_pos();

   /* rewind */
   mt_com.mt_count = 1;
   mt_com.mt_op = MTREW;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   printf("rewind\n");
   print_pos();

   /* FSR */
   mt_com.mt_op = MTFSR;
   mt_com.mt_count = 10;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   printf("fsr r=%i\n", r1);
   print_pos();

   /* eom */
   mt_com.mt_count = 1;
   mt_com.mt_op = MTEOM;
   r1 = ioctl(fd, MTIOCTOP, &mt_com);
   printf("goto eom\n");
   print_pos();
   
   close(fd);
   return(0);
}
