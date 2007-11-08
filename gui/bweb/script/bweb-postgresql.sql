
-- --------------------------------------------------
-- Upgrade from 2.2
-- --------------------------------------------------

CREATE FUNCTION concat (text, text) RETURNS text AS '
DECLARE
result text;
BEGIN
IF $1 is not null THEN
result := $1 || $2;
END IF;

RETURN result;
END;
' LANGUAGE plpgsql;

CREATE AGGREGATE group_concat(
sfunc = concat,
basetype = text,
stype = text,
initcond = ''
);

BEGIN;
CREATE TABLE bweb_user
(
	userid       serial not null,
	username     text not null,
	use_acl      boolean default false,
        comment      text default '',
	passwd       text default '',
	primary key (userid)
);
CREATE UNIQUE INDEX bweb_user_idx on bweb_user (username);

CREATE TABLE bweb_role
(
	roleid       serial not null,
	rolename     text not null,
--	comment      text default '',
	primary key (roleid)
);
CREATE UNIQUE INDEX bweb_role_idx on bweb_role (rolename);

INSERT INTO bweb_role (rolename) VALUES ('r_user_mgnt');
INSERT INTO bweb_role (rolename) VALUES ('r_delete_job');
INSERT INTO bweb_role (rolename) VALUES ('r_prune');
INSERT INTO bweb_role (rolename) VALUES ('r_purge');
INSERT INTO bweb_role (rolename) VALUES ('r_group_mgnt');
INSERT INTO bweb_role (rolename) VALUES ('r_location_mgnt');
INSERT INTO bweb_role (rolename) VALUES ('r_cancel_job');
INSERT INTO bweb_role (rolename) VALUES ('r_run_job');
INSERT INTO bweb_role (rolename) VALUES ('r_configure');
INSERT INTO bweb_role (rolename) VALUES ('r_client_status');
INSERT INTO bweb_role (rolename) VALUES ('r_view_job');

CREATE TABLE  bweb_role_member
(
	roleid       integer not null,
	userid       integer not null,
	primary key (roleid, userid)
);

CREATE TABLE  bweb_client_group_acl
(
	client_group_id       integer not null,
	userid                integer not null,
	primary key (client_group_id, userid)
);
COMMIT;
-- --------------------------------------------------
-- Upgrade from 2.0
-- --------------------------------------------------

BEGIN;
-- Manage Client groups in bweb
-- Works with postgresql and mysql5 

CREATE TABLE client_group
(
    client_group_id             serial	  not null,
    client_group_name	        text	  not null,
    primary key (client_group_id)
);

CREATE UNIQUE INDEX client_group_idx on client_group (client_group_name);

CREATE TABLE client_group_member
(
    client_group_id 		    integer	  not null,
    clientid          integer     not null,
    primary key (client_group_id, clientid)
);

CREATE INDEX client_group_member_idx on client_group_member (client_group_id);

COMMIT;

-- --------------------------------------------------
-- End of upgrade from 2.0
-- --------------------------------------------------

--   -- creer un nouveau group
--   
--   INSERT INTO client_group (client_group_name) VALUES ('SIGMA');
--   
--   -- affecter une machine a un group
--   
--   INSERT INTO client_group_member (client_group_id, clientid) 
--          (SELECT client_group_id, 
--                 (SELECT Clientid FROM Client WHERE Name = 'slps0003-fd')
--             FROM client_group 
--            WHERE client_group_name IN ('SIGMA', 'EXPLOITATION', 'MUTUALISE'));
--        
--   
--   -- modifier l'affectation d'une machine
--   
--   DELETE FROM client_group_member 
--         WHERE clientid = (SELECT ClientId FROM Client WHERE Name = 'slps0003-fd')
--   
--   -- supprimer un groupe
--   
--   DELETE FROM client_group_member 
--         WHERE client_group_id = (SELECT client_group_id FROM client_group WHERE client_group_name = 'EXPLOIT')
--   
--   
--   -- afficher tous les clients du group SIGMA
--   
--   SELECT Name FROM Client JOIN client_group_member using (clientid) 
--                           JOIN client_group using (client_group_id)
--    WHERE client_group_name = 'SIGMA';
--   
--   -- afficher tous les groups
--   
--   SELECT client_group_name FROM client_group ORDER BY client_group_name;
--   
--   -- afficher tous les job du group SIGMA hier
--   
--   SELECT JobId, Job.Name, Client.Name, JobStatus, JobErrors
--     FROM Job JOIN Client              USING(ClientId) 
--              JOIN client_group_member USING (ClientId)
--              JOIN client_group        USING (client_group_id)
--     WHERE client_group_name = 'SIGMA'
--       AND Job.StartTime > '2007-03-20'; 
--   
--   -- donne des stats
--   
--   SELECT count(1) AS nb, sum(JobFiles) AS files,
--          sum(JobBytes) AS size, sum(JobErrors) AS joberrors,
--          JobStatus AS jobstatus, client_group_name
--     FROM Job JOIN Client              USING(ClientId) 
--              JOIN client_group_member USING (ClientId)
--              JOIN client_group        USING (client_group_id)
--     WHERE Job.StartTime > '2007-03-20'
--     GROUP BY JobStatus, client_group_name
--   
--   

CREATE PROCEDURAL LANGUAGE plpgsql;
BEGIN;

CREATE FUNCTION SEC_TO_TIME(timestamp with time zone)
RETURNS timestamp with time zone AS $$
    select date_trunc('second', $1);
$$ LANGUAGE SQL;

CREATE FUNCTION SEC_TO_TIME(bigint)
RETURNS interval AS $$
    select date_trunc('second', $1 * interval '1 second');
$$ LANGUAGE SQL;

CREATE FUNCTION UNIX_TIMESTAMP(timestamp with time zone)
RETURNS double precision AS $$
    select date_part('epoch', $1);
$$ LANGUAGE SQL;

CREATE FUNCTION SEC_TO_INT(interval)
RETURNS double precision AS $$
    select extract(epoch from $1);
$$ LANGUAGE SQL;

CREATE OR REPLACE FUNCTION base64_decode_lstat(int4, varchar) RETURNS int8 AS $$
DECLARE
val int8;
b64 varchar(64);
size varchar(64);
i int;
BEGIN
size := split_part($2, ' ', $1);
b64 := 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
val := 0;
FOR i IN 1..length(size) LOOP
val := val + (strpos(b64, substr(size, i, 1))-1) * (64^(length(size)-i));
END LOOP;
RETURN val;
END;
$$ language 'plpgsql';

COMMIT;


