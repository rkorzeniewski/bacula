#!/bin/sh

# Run tests using differents catalog databases

PWD=`pwd`

CATALOG_MODE="postgresql postgresql-batchinsert postgresql-dbi postgresql-dbi-batchinsert \
              mysql mysql-batchinsert mysql-dbi mysql-dbi-batchinsert"

for CT in $CATALOG_MODE ; do

   CATALOG=`echo $CT | cut -d"-" -f1`
   DISABLE_BATCH_INSERT=`echo $CT | grep batchinsert`
   WITHOUT_DBI=`echo $CT | grep dbi`

   if test "$DISABLE_BATCH_INSERT" = "" ; then
      ENABLE_BATCH_INSERT="--disable-batch-insert"
   else
      ENABLE_BATCH_INSERT="--enable-batch-insert"
   fi

   if test "$WITHOUT_DBI" = "" ; then

     _WHICHDB="WHICHDB=\"--disable-nls --with-${CATALOG} ${ENABLE_BATCH_INSERT}\""
     _LIBDBI="#LIBDBI"
   else 
     if test "$CATALOG" = "mysql" ; then
        DBPORT=3306
     else
        DBPORT=5432
     fi

     _WHICHDB="WHICHDB=\"--disable-nls --with-dbi --with-dbi-driver=${CATALOG} ${ENABLE_BATCH_INSERT} --with-db-port=${DBPORT}\""
     _LIBDBI="LIBDBI=\"dbdriver = dbi:${CATALOG}; dbaddress = 127.0.0.1; dbport = ${DBPORT}\""

   fi

   _SITE_NAME="SITE_NAME=joaohf-bacula-${CT}"   
   
   # subustitute config values
   cp -a ${PWD}/config ${PWD}/config.tmp

   mkdir -p tmp

   echo "/^SITE_NAME/c $_SITE_NAME" >> tmp/config_sed
   echo "/^WHICHDB/c $_WHICHDB"  >> tmp/config_sed
   echo "/^#LIBDBI/c $_LIBDBI" >> tmp/config_sed
   echo "/^LIBDBI/c $_LIBDBI" >> tmp/config_sed

   sed -f tmp/config_sed ${PWD}/config.tmp > ${PWD}/config
   rm tmp/config_sed

   make setup
   ./experimental-disk
   echo "  ==== Submiting ${_SITE_NAME} ====" >> test.out
done




