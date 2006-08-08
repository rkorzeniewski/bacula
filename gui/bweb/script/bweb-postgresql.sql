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

COMMIT;