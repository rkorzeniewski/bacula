:List Job totals:
SELECT  count(*) AS Jobs, sum(JobFiles) AS Files, 
 sum(JobBytes) AS Bytes, Name AS Job FROM Job GROUP BY Name;
SELECT max(JobId) AS Jobs,sum(JobFiles) AS Files,
 sum(JobBytes) As Bytes FROM Job
#
:List where a file is saved:
*Enter path with trailing slash:
*Enter filename:
*Enter Client name:
SELECT Job.JobId, StartTime AS JobStartTime, VolumeName, Client.Name AS ClientName
 FROM Job,File,Path,Filename,Media,JobMedia,Client
 WHERE File.JobId=Job.JobId
 AND Path.Path="%1"
 AND Filename.Name="%2"
 AND Client.Name="%3"
 AND Path.PathId=File.PathId
 AND Filename.FilenameId=File.FilenameId
 AND JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId
 AND Client.ClientId=Job.ClientId
 GROUP BY Job.JobId;
#
:List where the most recent copies of a file are saved:
*Enter path with trailing slash:
*Enter filename:
*Enter Client name:
SELECT Job.JobId, StartTime AS JobStartTime, VolumeName, Client.Name AS ClientName
 FROM Job,File,Path,Filename,Media,JobMedia,Client
 WHERE File.JobId=Job.JobId
 AND Path.Path="%1"
 AND Filename.Name="%2"
 AND Client.Name="%3"
 AND Path.PathId=File.PathId
 AND Filename.FilenameId=File.FilenameId
 AND JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId
 AND Client.ClientId=Job.ClientId
 ORDER BY Job.StartTime DESC LIMIT 5;
#
:List total files/bytes by Job:
SELECT count(*) AS Jobs, sum(JobFiles) AS Files,
 sum(JobBytes) AS Bytes, Name AS Job
 FROM Job GROUP by Name
#
:List total files/bytes by Volume:
SELECT count(*) AS Jobs, sum(JobFiles) AS Files,
 sum(JobBytes) AS Bytes, VolumeName
 FROM Job,JobMedia,Media
 WHERE JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId
 GROUP by VolumeName;  
#
# create list of files to be deleted
#
:List files older than n days to be dropped:
*Enter retention period:
# First cleanup
drop table if exists retension;
# First create table with all files older than n days
create temporary table retension
  select Job.JobId,File.FileId,Path.PathId,Filename.FilenameId
  from Filename,File,Path,JobMedia,Media,Job
  where JobMedia.JobId=File.JobId 
  and Job.JobId=File.JobId
  and Media.MediaId=JobMedia.MediaId 
  and Filename.FilenameId=File.FilenameId
  and Path.PathId=File.PathId
  and (to_days(current_date) - to_days(EndTime)) > %1;
# Now select entries that have a more recent backup
select retension.JobId,retension.FileId,Path.Path,Filename.Name
  from retension,Filename,File,Path,Job
  where Job.JobId!=retension.JobId
  and (to_days(current_date) - to_days(Job.EndTime)) <= %1
  and Job.JobId=File.JobId
  and Filename.FilenameId=File.FilenameId
  and Path.PathId=File.PathId
  and retension.PathId=File.PathId
  and retension.FilenameId=File.FilenameId;
