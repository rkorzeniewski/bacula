
CREATE TABLE CurrentBackupId
(
     BackupId          serial     not null,
     ClientId          integer    not null,
     JobName           text       not null,
     FileSetId         integer    not null,
     primary key (BackupId)
);

-- Serait bien de prendre la meme table pour
-- les File et le CurrentBackup...
-- Quand on prune, il faut purger ici aussi

CREATE TABLE CurrentBackup
(
     FileId           integer    not null,
     BackupId         integer    not null,
     FullMark         char(1)    default 0,
     MarkId           integer    default 0,
     primary key (FileId)
);

CREATE INDEX currentbackup_fileid on CurrentBackup (BackupId);

-- On batch insert dans la table temporaire

-- il faut trouver les fichiers manquant
-- INSERT des nouveaux, UPDATE des anciens, SELECT pour trouver les deletes

-- UPDATE CurrentBackup SET MarkId = 1 
--  WHERE CurrentBackup.Id IN (
--        SELECT CurrentBackup.Id 
--          FROM      batch 
--               JOIN Filename USING (Name) 
--               JOIN Path     USING (Path)
--               JOIN CurrentBackup 
--                 ON (Filename.FilenameId = CurrentBackup.FilenameId 
--                     AND
--                     Path.PathId = CurrentBackup.PathId)
--          WHERE CurrentBackup.BackupId = 1
--      )
-- 
-- INSERT INTO CurrentBackup (BackupId, FullMark, PathId, FilenameId, LStat, MD5)
--   (SELECT CurrentBackup.BackupId, 'F', Path.PathId, Filename.FilenameId, 
-- 	  batch.LStat, batch.MD5
--      FROM batch        
--           JOIN Path     USING (Path) 
--           JOIN Filename USING (Name) 
--           LEFT OUTER JOIN CurrentBackup 
--                 ON (Filename.FilenameId = CurrentBackup.FilenameId 
--                     AND Path.PathId = CurrentBackup.PathId 
--                     AND CurrentBackup.BackupId = 1)
--          WHERE BackupId IS NULL
--              
--   )
-- 
-- il faut trouver les fichiers modifies
-- Le champs LStat n'est plus le meme
-- SELECT * 
--   FROM      batch 
--        JOIN Path     USING (Path) 
--        JOIN Filename USING (Name) 
--        JOIN CurrentBackup USING (FilenameId, PathId)
-- 
--  WHERE CurrentBackup.LStat != batch.LStat
--    AND CurrentBackup.BackupId = 1
-- 
-- il faut mettre a jour la liste des fichiers

