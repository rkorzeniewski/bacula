\c bacula

begin;

alter table media rename column endblock to endblock_old;
alter table media add column endblock bigint;
update media set endblock = endblock_old;
alter table media alter column endblock set not null;
alter table media drop column endblock_old;

commit;

vacuum;
