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

