
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
-- Mais y'a des problemes pour les prunes

CREATE TABLE CurrentBackup
(
     Id               serial     not null,
     BackupId         integer    not null,
     FullMark         char(1)    default 0,
     PathId	      integer	 not null,
     FilenameId	      integer	 not null,
     MarkId	      integer	 not null  default 0,
     LStat	      text	 not null,
     md5 	      text	 not null,
     primary key (Id)
);

CREATE INDEX currentbackup_fileid on CurrentBackup (BackupId);
CREATE INDEX currentbackup_idx on currentbackup (FilenameId, PathId);
CREATE INDEX currentbackup_idx1 on currentbackup (FilenameId);
CREATE INDEX currentbackup_idx2 on currentbackup (PathId);


CREATE TEMPORARY TABLE batch (path varchar,
                              name varchar,
                              lstat varchar,
                              md5 varchar);

-- On batch insert dans la table temporaire

-- il faut trouver les fichiers manquant
-- INSERT des nouveaux, UPDATE des anciens, SELECT pour trouver les deletes

UPDATE CurrentBackup SET MarkId = 1 
 WHERE CurrentBackup.Id IN (
       SELECT CurrentBackup.Id 
         FROM      batch 
              JOIN Filename USING (Name) 
              JOIN Path     USING (Path)
              JOIN CurrentBackup 
                ON (Filename.FilenameId = CurrentBackup.FilenameId 
                    AND
                    Path.PathId = CurrentBackup.PathId)
         WHERE CurrentBackup.BackupId = 1
     )

INSERT INTO CurrentBackup (BackupId, FullMark, PathId, FilenameId, LStat, MD5)
  (SELECT CurrentBackup.BackupId, 'F', Path.PathId, Filename.FilenameId, 
	  batch.LStat, batch.MD5
     FROM batch        
          JOIN Path     USING (Path) 
          JOIN Filename USING (Name) 
          LEFT OUTER JOIN CurrentBackup 
                ON (Filename.FilenameId = CurrentBackup.FilenameId 
                    AND Path.PathId = CurrentBackup.PathId 
                    AND CurrentBackup.BackupId = 1)
         WHERE BackupId IS NULL
             
  )

-- il faut trouver les fichiers modifies
-- Le champs LStat n'est plus le meme
SELECT * 
  FROM      batch 
       JOIN Path     USING (Path) 
       JOIN Filename USING (Name) 
       JOIN CurrentBackup USING (FilenameId, PathId)

 WHERE CurrentBackup.LStat != batch.LStat
   AND CurrentBackup.BackupId = 1

-- il faut mettre a jour la liste des fichiers

