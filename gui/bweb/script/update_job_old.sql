-- First, create JobHistory table
-- CREATE TABLE JobHistory (LIKE Job);
-- 
-- then put this on your crontab
-- */20 * * * * psql -f /opt/bacula/etc/update_job_old.sql > /home/bacula/update_job_old.log
INSERT INTO JobHistory
  (SELECT * FROM Job WHERE JobStatus in ('T', 'f', 'A') AND JobId NOT IN (SELECT JobId FROM JobHistory) );

