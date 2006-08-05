ALTER TABLE media ADD COLUMN DeviceId integer;
UPDATE media SET DeviceId=0;
ALTER TABLE media ADD COLUMN MediaTypeId integer;
UPDATE media SET MediaTypeId=0;
ALTER TABLE media ADD COLUMN LocationId integer;
UPDATE media SET LocationId=0;
ALTER TABLE media ADD COLUMN RecycleCount integer;
UPDATE media SET RecycleCount=0;
ALTER TABLE media ADD COLUMN InitialWrite timestamp without time zone;
ALTER TABLE media ADD COLUMN scratchpoolid integer;
UPDATE media SET scratchpoolid=0;
ALTER TABLE media ADD COLUMN recyclepoolid integer;
UPDATE media SET recyclepoolid=0;
ALTER TABLE media ADD COLUMN enabled integer;
UPDATE media SET enabled=1;
ALTER TABLE media ADD COLUMN Comment TEXT;

ALTER TABLE job ADD COLUMN RealEndTime timestamp without time zone;
ALTER TABLE job ADD COLUMN PriorJobId integer;
UPDATE job SET PriorJobId=0;

ALTER TABLE jobmedia DROP COLUMN Stripe;

CREATE TABLE Location (
   LocationId SERIAL NOT NULL,
   Location TEXT NOT NULL,
   Cost integer default 0,
   Enabled integer,
   PRIMARY KEY (LocationId)
);

CREATE TABLE LocationLog (
   LocLogId SERIAL NOT NULL,
   Date timestamp   without time zone,
   Comment TEXT NOT NULL,
   MediaId INTEGER DEFAULT 0,
   LocationId INTEGER DEFAULT 0,
   newvolstatus text not null
	check (newvolstatus in ('Full','Archive','Append',
	      'Recycle','Purged','Read-Only','Disabled',
	      'Error','Busy','Used','Cleaning','Scratch')),
   newenabled smallint,
   PRIMARY KEY(LocLogId)
);


CREATE TABLE Log
(
    LogId	      serial	  not null,
    JobId	      integer	  not null,
    Time	      timestamp   without time zone,
    LogText	      text	  not null,
    primary key (LogId)
);
create index log_name_idx on Log (JobId);


DELETE FROM version;
INSERT INTO version (versionId) VALUES (10);

vacuum;
