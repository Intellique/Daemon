-- Update Archive
ALTER TABLE Archive ADD COLUMN copyOf BIGINT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE RESTRICT;

-- Update ArchiveFile
CREATE TEMP TABLE ArchiveFileToArchiveVolume_tmp (LIKE ArchiveFileToArchiveVolume);
INSERT INTO ArchiveFileToArchiveVolume_tmp SELECT * FROM ArchiveFileToArchiveVolume;
TRUNCATE TABLE ArchiveFileToArchiveVolume;
ALTER TABLE ArchiveFileToArchiveVolume ADD COLUMN blockNumber BIGINT CHECK (blockNumber >= 0);
INSERT INTO ArchiveFileToArchiveVolume SELECT afv.archivevolume, af.id, af.blocknumber FROM archivefiletoarchivevolume_tmp afv LEFT JOIN archivefile af ON afv.archivefile = af.id;
DROP TABLE ArchiveFileToArchiveVolume_tmp;

-- Update Changer
ALTER TABLE Changer ADD COLUMN enable BOOLEAN NOT NULL DEFAULT TRUE;

-- Update Drive
ALTER TABLE Drive ADD COLUMN enable BOOLEAN NOT NULL DEFAULT TRUE;

CREATE TABLE Backup (
    id BIGSERIAL PRIMARY KEY,
    timestamp TIMESTAMP(0) NOT NULL DEFAULT now(),
    nbTape INTEGER NOT NULL DEFAULT 0 CHECK (nbTape >= 0),
    nbArchive INTEGER NOT NULL DEFAULT 0 CHECK (nbArchive >= 0)
);

CREATE TABLE BackupVolume (
    id BIGSERIAL PRIMARY KEY,
    sequence INTEGER NOT NULL DEFAULT 0 CHECK (sequence >= 0),
    backup BIGINT NOT NULL REFERENCES Backup(id) ON DELETE CASCADE ON UPDATE CASCADE,
    tape INTEGER NOT NULL REFERENCES Tape(id) ON DELETE RESTRICT ON UPDATE CASCADE,
    tapePosition INTEGER NOT NULL DEFAULT 0 CHECK (tapePosition >= 0)
);

-- Job
ALTER TABLE Job ADD COLUMN backup BIGINT DEFAULT NULL REFERENCES Backup(id) ON UPDATE CASCADE ON DELETE CASCADE;

-- Host
ALTER TABLE Host ALTER description SET DEFAULT '';

-- Pool
ALTER TABLE Pool ALTER growable SET DEFAULT FALSE;
ALTER TABLE Pool ADD COLUMN rewritable BOOLEAN NOT NULL DEFAULT TRUE;
ALTER TABLE Pool ALTER metadata SET DEFAULT '';

-- UserEvent
CREATE TABLE UserEvent (
    id SERIAL PRIMARY KEY,
    event TEXT NOT NULL UNIQUE
);
INSERT INTO UserEvent(event) VALUES ('connection'), ('deconnection');
ALTER TABLE UserLog ADD COLUMN event INTEGER NOT NULL REFERENCES UserEvent(id) ON UPDATE CASCADE ON DELETE RESTRICT;

