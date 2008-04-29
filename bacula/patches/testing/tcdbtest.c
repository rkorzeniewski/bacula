/* 
 export LD_LIBRARY_PATH=/home/eric/dev/bacula/tcdb/lib/
 g++ -I/home/eric/dev/bacula/tcdb/include -o tt -L /home/eric/dev/bacula/tcdb/lib/ -ltokyocabinet -lz -lpthread -lm tcdbtest.c 

./tt $(wc -l src.txt2) 0

 */

#include <tcutil.h>
#include <tchdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

#define NITEMS 5000000



void atend()
{
   unlink("casket.hdb");
}

int64_t get_current_time()
{
   struct timeval tv;
   if (gettimeofday(&tv, NULL) != 0) {
      tv.tv_sec = (long)time(NULL);   /* fall back to old method */
      tv.tv_usec = 0;
   }
   return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
}

int main(int argc, char **argv){

   FILE *fp, *res;
   TCHDB *hdb;
   int ecode;
   char *key, *value;
   int i=0;
   char save_key[512];
   char line[512];
   int64_t ctime, ttime;;
   char result[200];

   if (argc != 4) {
      fprintf(stderr, "Usage: tt count file cache %i\n");
      exit(1);
   }

   atexit(atend);
   signal(15, exit);
   signal(2, exit);

   snprintf(result, sizeof(result), "result.%i", getpid());
   res = fopen(result, "w");
   
   /* create the object */
   hdb = tchdbnew();

   if (atoi(argv[3]) > 0) {
      tchdbsetcache(hdb, atoi(argv[3]));
   }
   fprintf(res, "cache;%i\n", atoi(argv[3]));

   /*
    * apow : 128 (size of stat hash field)
    */
   int opt=0;//HDBTTCBS;
   tchdbtune(hdb, atoll(argv[1]), 7, 16, opt);
   fprintf(res, "bucket;%lli\n", atoll(argv[1]));
   fprintf(res, "compress;%i\n", opt);

   /* open the database */
   if(!tchdbopen(hdb, "casket.hdb", HDBOWRITER | HDBOCREAT)){
      ecode = tchdbecode(hdb);
      fprintf(stderr, "open error: %s\n", tchdberrmsg(ecode));
   }

   ctime = get_current_time();

   /* fill hash with real data (find / > src.txt) */
   fp = fopen(argv[2], "r");
   if (!fp) {
      fprintf(stderr, "open %s file error\n", argv[2]);
      exit (1);
   }
   while (fgets(line, sizeof(line), fp)) {
      char *data = "A AAA B AZS SDSZ EZEZ SSDFS AEZEZEZ ZEZDSDDQe";
      if (!tchdbputasync2(hdb, line, data)) {
	 ecode = tchdbecode(hdb);
         fprintf(stderr, "put error: %s\n", tchdberrmsg(ecode));
      }
      if (i++ == 99) {
         strcpy(save_key, line);
      }
   }
   fclose(fp);

   ttime= get_current_time();
   fprintf(res, "nbelt;%lli\n", i);

   fprintf(stderr, "loading %i file into hash database in %ims\n", 
	   i, (ttime - ctime)/1000);
   fprintf(res, "load;%i\n", (ttime - ctime)/1000);


   /* retrieve records */
   value = tchdbget2(hdb, save_key);
   if(value){
      //printf("%s:%s\n", save_key, value);
      free(value);
   } else {
      ecode = tchdbecode(hdb);
      fprintf(stderr, "get error: %s\n", tchdberrmsg(ecode));
   }

   /* retrieve all records and mark them as seen */
   i=0;
   fp = fopen(argv[2], "r");
   if (!fp) {
      fprintf(stderr, "open %s file error\n", argv[2]);
      exit (1);
   }
   while (fgets(line, sizeof(line), fp)) {
      char *data = "A AAA B AZS SDSZ EZEZ SSDFS AEZEZEZ ZEZDSDDQe";
      if (i++ != 200) {
	 value = tchdbget2(hdb, line);
	 if (value) {
	    *value=0;
	    if (!tchdbputasync2(hdb, line, value)) {
	       ecode = tchdbecode(hdb);
	       fprintf(stderr, "put error: %s\n", tchdberrmsg(ecode));
	    }
	    free(value);
	 } else {
	    fprintf(stderr, "can't find %s in hash\n", line);
	 }
      }
   }
   fclose(fp);

   ctime = get_current_time();
   fprintf(stderr, "marking as seen in %ims\n", (ctime - ttime)/1000);
   fprintf(res, "seen;%i\n", (ctime - ttime)/1000);

  /* traverse records */
  tchdbiterinit(hdb);
  while((key = tchdbiternext2(hdb)) != NULL){
    value = tchdbget2(hdb, key);
    if(value && *value){
       //printf("%s:%s\n", key, value);
      free(value);
    }
    free(key);
  }

   ttime = get_current_time();
   fprintf(stderr, "checking not seen in %ims\n", (ttime - ctime)/1000);
   fprintf(res, "walk;%i\n", (ttime - ctime)/1000);

  /* close the database */
  if(!tchdbclose(hdb)){
    ecode = tchdbecode(hdb);
    fprintf(stderr, "close error: %s\n", tchdberrmsg(ecode));
  }

  /* delete the object */
  tchdbdel(hdb);
  struct stat statp;
  stat("casket.hdb", &statp);
  fprintf(res, "size;%lli\n", statp.st_size);
  unlink("casket.hdb");
  fclose(res);
  return 0;
}
