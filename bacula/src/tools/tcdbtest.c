#include <tcutil.h>
#include <tchdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NITEMS 5000000

int main(int argc, char **argv){

  TCHDB *hdb;
  int ecode;
  char *key, *value;
  int i;
  char save_key[200];

  /* create the object */
  hdb = tchdbnew();

  tchdbsetcache(hdb, 5000000);
  tchdbtune(hdb, 9000000, -1, 16, 0);

  /* open the database */
  if(!tchdbopen(hdb, "casket.hdb", HDBOWRITER | HDBOCREAT)){
    ecode = tchdbecode(hdb);
    fprintf(stderr, "open error: %s\n", tchdberrmsg(ecode));
  }

   for (i=0; i<NITEMS; i++) {
      char mkey[200];
      char data[200];
      sprintf(mkey, "This is htable item %d", i);
      sprintf(data, "Data for key %d", i);
      if (!tchdbputasync2(hdb, mkey, data)) {
         ecode = tchdbecode(hdb);
         fprintf(stderr, "put error: %s\n", tchdberrmsg(ecode));
      }
      if (i == 99) {
         strcpy(save_key, mkey);
      }
   }

  /* retrieve records */
  value = tchdbget2(hdb, save_key);
  if(value){
    printf("%s\n", value);
    free(value);
  } else {
    ecode = tchdbecode(hdb);
    fprintf(stderr, "get error: %s\n", tchdberrmsg(ecode));
  }

  /* traverse records */
  tchdbiterinit(hdb);
  while((key = tchdbiternext2(hdb)) != NULL){
    value = tchdbget2(hdb, key);
    if(value){
      printf("%s:%s\n", key, value);
      free(value);
    }
    free(key);
  }

  /* close the database */
  if(!tchdbclose(hdb)){
    ecode = tchdbecode(hdb);
    fprintf(stderr, "close error: %s\n", tchdberrmsg(ecode));
  }

  /* delete the object */
  tchdbdel(hdb);

  return 0;
}
