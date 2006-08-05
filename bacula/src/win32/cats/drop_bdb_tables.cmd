rem 
rem  shell script to Delete the Bacula database (same as deleting 
rem   the tables)
rem 

del /f @working_dir@/control.db
del /f @working_dir@/jobs.db
del /f @working_dir@/pools.db
del /f @working_dir@/media.db
del /f @working_dir@/jobmedia.db
del /f @working_dir@/client.db
del /f @working_dir@/fileset.db
