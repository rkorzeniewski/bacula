-- First, create job_old table
-- CREATE TABLE job_old (LIKE Job);
-- 
-- then put this on your crontab
-- */20 * * * * psql -f /opt/bacula/etc/update_job_old.sql > /home/bacula/update_job_old.log
INSERT INTO job_old
  (SELECT * FROM Job WHERE JobStatus in ('T', 'f', 'A') AND JobId NOT IN (SELECT JobId FROM job_old) );

