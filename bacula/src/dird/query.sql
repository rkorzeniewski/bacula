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
:List last 10 Full Backups for a Client:
*Enter Client name:
Select JobId,Client.Name as Client,StartTime,JobFiles,JobBytes
 FROM Client,Job
 WHERE Client.Name="%1"
 AND Client.ClientId=Job.ClientId
 AND Level='F' AND JobStatus='T'
 LIMIT 10;
#
:List Volumes used by selected JobId:
*Enter JobId:
SELECT Job.JobId,VolumeName 
 FROM Job,JobMedia,Media 
 WHERE Job.JobId=%1 
 AND Job.JobId=JobMedia.JobId 
 AND JobMedia.MediaId=Media.MediaId;
#
:List Volumes to Restore All Files:
*Enter Client Name:
!DROP TABLE temp;
!DROP TABLE temp2;
CREATE TABLE temp (JobId INTEGER UNSIGNED NOT NULL,
 JobTDate BIGINT UNSIGNED,
 ClientId INTEGER UNSIGNED,
 StartTime TEXT,
 VolumeName TEXT,
 StartFile INTEGER UNSIGNED, 
 VolSessionId INTEGER UNSIGNED,
 VolSessionTime INTEGER UNSIGNED);
CREATE TABLE temp2 (JobId INTEGER UNSIGNED NOT NULL,
 StartTime TEXT,
 VolumeName TEXT,
 StartFile INTEGER UNSIGNED, 
 VolSessionId INTEGER UNSIGNED,
 VolSessionTime INTEGER UNSIGNED);
INSERT INTO temp SELECT Job.JobId,MAX(JobTDate),Job.ClientId,StartTime,VolumeName,
   JobMedia.StartFile,VolSessionId,VolSessionTime
 FROM Client,Job,JobMedia,Media WHERE Client.Name="%1"
 AND Client.ClientId=Job.ClientId
 AND Level='F' AND JobStatus='T'
 AND JobMedia.JobId=Job.JobId 
 AND JobMedia.MediaId=Media.MediaId
 GROUP BY Job.JobTDate LIMIT 1;
INSERT INTO temp2 SELECT JobId,StartTime,VolumeName,StartFile, 
   VolSessionId,VolSessionTime
 FROM temp;
INSERT INTO temp2 SELECT Job.JobId,Job.StartTime,Media.VolumeName,
   JobMedia.StartFile,Job.VolSessionId,Job.VolSessionTime
 FROM Job,temp,JobMedia,Media
 WHERE Job.JobTDate>temp.JobTDate 
 AND Job.ClientId=temp.ClientId
 AND Level='I' AND JobStatus='T'
 AND JobMedia.JobId=Job.JobId 
 AND JobMedia.MediaId=Media.MediaId
 GROUP BY Job.JobId;
SELECT * from temp;
SELECT * from temp2;
!DROP TABLE temp;
!DROP TABLE temp2;
#
