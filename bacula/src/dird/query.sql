:List Job totals:
SELECT  count(*) AS Jobs, sum(JobFiles) AS Files, 
 sum(JobBytes) AS Bytes, Name AS Job FROM Job GROUP BY Name;
SELECT max(JobId) AS Jobs,sum(JobFiles) AS Files,
 sum(JobBytes) As Bytes FROM Job;
#
:List where a file is saved:
*Enter path with trailing slash:
*Enter filename:
*Enter Client name:
SELECT Job.JobId,StartTime AS JobStartTime,VolumeName,Client.Name AS ClientName
 FROM Job,File,Path,Filename,Media,JobMedia,Client
 WHERE File.JobId=Job.JobId
 AND Path.Path='%1'
 AND Filename.Name='%2'
 AND Client.Name='%3'
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
SELECT Job.JobId,StartTime AS JobStartTime,VolumeName,Client.Name AS ClientName
 FROM Job,File,Path,Filename,Media,JobMedia,Client
 WHERE File.JobId=Job.JobId
 AND Path.Path='%1'
 AND Filename.Name='%2'
 AND Client.Name='%3'
 AND Path.PathId=File.PathId
 AND Filename.FilenameId=File.FilenameId
 AND JobMedia.JobId=Job.JobId
 AND JobMedia.MediaId=Media.MediaId
 AND Client.ClientId=Job.ClientId
 ORDER BY Job.StartTime DESC LIMIT 5;
#
:List last 20 Full Backups for a Client:
*Enter Client name:
Select Job.JobId,Client.Name as Client,StartTime,JobFiles,JobBytes,
JobMedia.StartFile as VolFile,VolumeName
 FROM Client,Job,JobMedia,Media
 WHERE Client.Name='%1'
 AND Client.ClientId=Job.ClientId
 AND Level='F' AND JobStatus='T'
 AND JobMedia.JobId=Job.JobId AND JobMedia.MediaId=Media.MediaId
 ORDER BY Job.StartTime DESC LIMIT 20;
#
:List all backups for a Client after a specified time
*Enter Client Name:
*Enter time in YYYY-MM-DD HH:MM:SS format:
Select Job.JobId,Client.Name as Client,Level,StartTime,JobFiles,JobBytes,VolumeName
  FROM Client,Job,JobMedia,Media
  WHERE Client.Name='%1'
  AND Client.ClientId=Job.ClientId
  AND JobStatus='T'
  AND JobMedia.JobId=Job.JobId AND JobMedia.MediaId=Media.MediaId
  AND Job.StartTime >= '%2'
  ORDER BY Job.StartTime;
#
:List all backups for a Client
*Enter Client Name:
Select Job.JobId,Client.Name as Client,Level,StartTime,JobFiles,JobBytes,VolumeName
  FROM Client,Job,JobMedia,Media
  WHERE Client.Name='%1'
  AND Client.ClientId=Job.ClientId
  AND JobStatus='T'
  AND JobMedia.JobId=Job.JobId AND JobMedia.MediaId=Media.MediaId
  ORDER BY Job.StartTime;
#
:List Volume Attributes for a selected Volume:
*Enter Volume name:
SELECT Slot,MaxVolBytes,VolCapacityBytes,VolStatus,Recycle,VolRetention,
 VolUseDuration,MaxVolJobs,MaxVolFiles
 FROM Media   
 WHERE Volumename='%1';
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
 Level CHAR,
 StartTime TEXT,
 VolumeName TEXT,
 StartFile INTEGER UNSIGNED, 
 VolSessionId INTEGER UNSIGNED,
 VolSessionTime INTEGER UNSIGNED);
CREATE TABLE temp2 (JobId INTEGER UNSIGNED NOT NULL,
 StartTime TEXT,
 VolumeName TEXT,
 Level CHAR,
 StartFile INTEGER UNSIGNED, 
 VolSessionId INTEGER UNSIGNED,
 VolSessionTime INTEGER UNSIGNED);
# Select last Full save
INSERT INTO temp SELECT Job.JobId,JobTDate,Job.ClientId,Job.Level,
   StartTime,VolumeName,JobMedia.StartFile,VolSessionId,VolSessionTime
 FROM Client,Job,JobMedia,Media WHERE Client.Name='%1'
 AND Client.ClientId=Job.ClientId
 AND Level='F' AND JobStatus='T'
 AND JobMedia.JobId=Job.JobId 
 AND JobMedia.MediaId=Media.MediaId
 ORDER BY Job.JobTDate DESC LIMIT 1;
# Copy into temp 2
INSERT INTO temp2 SELECT JobId,StartTime,VolumeName,Level,StartFile, 
   VolSessionId,VolSessionTime
 FROM temp;
# Now add subsequent incrementals
INSERT INTO temp2 SELECT Job.JobId,Job.StartTime,Media.VolumeName,
   Job.Level,JobMedia.StartFile,Job.VolSessionId,Job.VolSessionTime
 FROM Job,temp,JobMedia,Media
 WHERE Job.JobTDate>temp.JobTDate 
 AND Job.ClientId=temp.ClientId
 AND Job.Level='I' AND JobStatus='T'
 AND JobMedia.JobId=Job.JobId 
 AND JobMedia.MediaId=Media.MediaId
 GROUP BY Job.JobId;
# list results
SELECT * from temp2;
!DROP TABLE temp;
!DROP TABLE temp2;
#
:List Pool Attributes for a selected Pool:
*Enter Pool name:
SELECT Recycle,VolRetention,VolUseDuration,MaxVolJobs,MaxVolFiles,MaxVolBytes
 FROM Pool
 WHERE Name='%1';
#
:List where a File is saved:
*Enter Filename (no path):
SELECT Job.JobId as JobId, Client.Name as Client,
 Path.Path,Filename.Name,
 StartTime,Level,JobFiles,JobBytes
 FROM Client,Job,File,Filename,Path WHERE Client.ClientId=Job.ClientId
 AND JobStatus='T' AND Job.JobId=File.JobId
 AND Path.PathId=File.PathId AND Filename.FilenameId=File.FilenameId
 AND Filename.Name='%1' ORDER BY Job.StartTime LIMIT 20;
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
:List Files for a selected JobId:
*Enter JobId:
SELECT Path.Path,Filename.Name FROM File,
 Filename,Path WHERE File.JobId=%1 
 AND Filename.FilenameId=File.FilenameId 
 AND Path.PathId=File.PathId ORDER BY
 Path.Path,Filename.Name;
