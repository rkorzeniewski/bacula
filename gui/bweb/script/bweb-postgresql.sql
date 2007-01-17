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
