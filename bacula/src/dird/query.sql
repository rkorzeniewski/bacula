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
