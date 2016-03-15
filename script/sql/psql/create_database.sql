-- Types
CREATE TYPE AutoCheckMode AS ENUM (
    'quick mode',
    'thorough mode',
    'none'
);

CREATE TYPE ChangerAction AS ENUM (
    'none',
    'put online',
    'put offline'
);

CREATE TYPE ChangerStatus AS ENUM (
    'error',
    'exporting',
    'go offline',
    'go online',
    'idle',
    'importing',
    'loading',
    'offline',
    'unknown',
    'unloading'
);

CREATE TYPE DriveStatus AS ENUM (
    'cleaning',
    'empty idle',
    'erasing',
    'error',
    'loaded idle',
    'loading',
    'positioning',
    'reading',
    'rewinding',
    'unknown',
    'unloading',
    'writing'
);

CREATE TYPE FileType AS ENUM (
    'block device',
    'character device',
    'directory',
    'fifo',
    'regular file',
    'socket',
    'symbolic link'
);

CREATE TYPE JobRecordNotif AS ENUM (
    'normal',
    'important',
    'read'
);

CREATE TYPE JobRunStep AS ENUM (
    'job',
    'on error',
    'pre job',
    'post job',
    'warm up'
);

CREATE TYPE JobStatus AS ENUM (
    'disable',
    'error',
    'finished',
    'pause',
    'running',
    'scheduled',
    'stopped',
    'unknown',
    'waiting'
);

CREATE TYPE LogLevel AS ENUM (
    'alert',
    'critical',
    'debug',
    'emergency',
    'error',
    'info',
    'notice',
    'warning'
);

CREATE TYPE LogType AS ENUM (
    'changer',
    'daemon',
    'drive',
    'job',
    'logger',
    'plugin checksum',
    'plugin db',
    'plugin log',
    'scheduler',
    'ui',
    'user message'
);

CREATE TYPE MediaFormatDataType AS ENUM (
    'audio',
    'cleaning',
    'data',
    'video'
);

CREATE TYPE MediaFormatMode AS ENUM (
    'disk',
    'linear',
    'optical'
);

CREATE TYPE MediaStatus AS ENUM (
    'erasable',
    'error',
    'foreign',
    'in use',
    'locked',
    'needs replacement',
    'new',
    'pooled',
    'unknown'
);

CREATE TYPE MediaType AS ENUM (
    'cleaning',
    'rewritable',
    'worm'
);

CREATE TYPE MetaType AS ENUM (
    'archive',
    'archivefile',
    'archivevolume',
    'host',
    'media',
    'pool',
    'report',
    'users',
    'vtl'
);

CREATE TYPE ProxyStatus AS ENUM (
    'todo',
    'running',
    'done',
    'error'
);

CREATE TYPE ScriptType AS ENUM (
    'on error',
    'pre job',
    'post job'
);

CREATE TYPE UnbreakableLevel AS ENUM (
    'archive',
    'file',
    'none'
);


-- Tables
CREATE TABLE ArchiveFormat (
    id SERIAL PRIMARY KEY,

    name VARCHAR(32) NOT NULL UNIQUE,

    readable BOOLEAN NOT NULL,
    writable BOOLEAN NOT NULL
);

CREATE TABLE MediaFormat (
    id SERIAL PRIMARY KEY,

    name VARCHAR(64) NOT NULL UNIQUE,
    dataType MediaFormatDataType NOT NULL,
    mode MediaFormatMode NOT NULL,

    maxLoadCount INTEGER NOT NULL CHECK (maxLoadCount > 0),
    maxReadCount INTEGER NOT NULL CHECK (maxReadCount > 0),
    maxWriteCount INTEGER NOT NULL CHECK (maxWriteCount > 0),
    maxOpCount INTEGER NOT NULL CHECK (maxOpCount > 0),

    lifespan INTERVAL NOT NULL CHECK (lifespan > INTERVAL 'P1Y'),

    capacity BIGINT NOT NULL CHECK (capacity > 0),
    blockSize INTEGER NOT NULL DEFAULT 0 CHECK (blockSize >= 0),
    densityCode SMALLINT NOT NULL,

    supportPartition BOOLEAN NOT NULL DEFAULT FALSE,
    supportMAM BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE PoolMirror (
    id SERIAL PRIMARY KEY,

    uuid UUID NOT NULL UNIQUE,
    name VARCHAR(64) NOT NULL,
    synchronized BOOLEAN NOT NULL
);

CREATE TABLE ArchiveMirror (
    id SERIAL PRIMARY KEY,
    poolMirror INTEGER REFERENCES PoolMirror(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE Pool (
    id SERIAL PRIMARY KEY,

    uuid UUID NOT NULL UNIQUE,
    name VARCHAR(64) NOT NULL,

    archiveFormat INTEGER NOT NULL REFERENCES ArchiveFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    mediaFormat INTEGER NOT NULL REFERENCES MediaFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    autocheck AutoCheckMode NOT NULL DEFAULT 'none',
    lockcheck BOOLEAN NOT NULL DEFAULT FALSE,

    growable BOOLEAN NOT NULL DEFAULT FALSE,
    unbreakableLevel UnbreakableLevel NOT NULL DEFAULT 'none',
    rewritable BOOLEAN NOT NULL DEFAULT TRUE,

    metadata JSON NOT NULL DEFAULT '{}',
    backupPool BOOLEAN NOT NULL DEFAULT FALSE,

    poolOriginal INTEGER REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    poolMirror INTEGER REFERENCES PoolMirror(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    deleted BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE PoolGroup (
    id SERIAL PRIMARY KEY,

    uuid UUID NOT NULL UNIQUE,
    name VARCHAR(64) NOT NULL
);

CREATE TABLE PoolToPoolGroup (
    pool INTEGER NOT NULL REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    poolgroup INTEGER NOT NULL REFERENCES PoolGroup(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE INDEX ON PoolToPoolGroup(pool);
CREATE INDEX ON PoolToPoolGroup(poolgroup);

CREATE TABLE PoolTemplate (
    id SERIAL PRIMARY KEY,

    name VARCHAR(64) NOT NULL UNIQUE,

    autocheck AutoCheckMode NOT NULL DEFAULT 'none',
    lockcheck BOOLEAN NOT NULL DEFAULT FALSE,

    growable BOOLEAN NOT NULL DEFAULT FALSE,
    unbreakableLevel UnbreakableLevel NOT NULL DEFAULT 'none',
    rewritable BOOLEAN NOT NULL DEFAULT TRUE,

    metadata JSON NOT NULL DEFAULT '{}',
    createProxy BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE Media (
    id SERIAL PRIMARY KEY,

    uuid UUID NULL UNIQUE,
    label VARCHAR(64),
    mediumSerialNumber VARCHAR(36) UNIQUE,
    name VARCHAR(255) NULL,

    status MediaStatus NOT NULL,

    firstUsed TIMESTAMP(3) WITH TIME ZONE NOT NULL,
    useBefore TIMESTAMP(3) WITH TIME ZONE NOT NULL,
    lastRead TIMESTAMP(3) WITH TIME ZONE,
    lastWrite TIMESTAMP(3) WITH TIME ZONE,

    loadCount INTEGER NOT NULL DEFAULT 0 CHECK (loadCount >= 0),
    readCount INTEGER NOT NULL DEFAULT 0 CHECK (readCount >= 0),
    writeCount INTEGER NOT NULL DEFAULT 0 CHECK (writeCount >= 0),
    operationCount INTEGER NOT NULL DEFAULT 0 CHECK (operationCount >= 0),

    nbTotalBlockRead BIGINT NOT NULL DEFAULT 0 CHECK (nbTotalBlockRead >= 0),
    nbTotalBlockWrite BIGINT NOT NULL DEFAULT 0 CHECK (nbTotalBlockWrite >= 0),

    nbReadError INTEGER NOT NULL DEFAULT 0 CHECK (nbReadError >= 0),
    nbWriteError INTEGER NOT NULL DEFAULT 0 CHECK (nbWriteError >= 0),

    nbFiles INTEGER NOT NULL DEFAULT 0 CHECK (nbFiles >= 0),
    blockSize INTEGER NOT NULL DEFAULT 0 CHECK (blockSize >= 0),
    freeBlock BIGINT NOT NULL CHECK (freeblock >= 0),
    totalBlock BIGINT NOT NULL CHECK (totalBlock >= 0),

    hasPartition BOOLEAN NOT NULL DEFAULT FALSE,
    append BOOLEAN NOT NULL DEFAULT TRUE,

    type MediaType NOT NULL DEFAULT 'rewritable',
    writeLock BOOLEAN NOT NULL DEFAULT FALSE,

    archiveFormat INTEGER REFERENCES ArchiveFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    mediaFormat INTEGER NOT NULL REFERENCES MediaFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    pool INTEGER NULL REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE SET NULL,

    CHECK (firstUsed < useBefore)
);

CREATE TABLE MediaLabel (
    id BIGSERIAL PRIMARY KEY,

    name TEXT NOT NULL,
    media INTEGER NOT NULL REFERENCES Media(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE DriveFormat (
    id SERIAL PRIMARY KEY,

    name VARCHAR(64) NOT NULL UNIQUE,
    densityCode SMALLINT NOT NULL CHECK (densityCode > 0),
    mode MediaFormatMode NOT NULL,

    cleaningInterval INTERVAL NOT NULL CHECK (cleaningInterval >= INTERVAL 'P1W')
);

CREATE TABLE DriveFormatSupport (
    driveFormat INTEGER NOT NULL REFERENCES DriveFormat(id) ON UPDATE CASCADE ON DELETE CASCADE,
    mediaFormat INTEGER NOT NULL REFERENCES MediaFormat(id) ON UPDATE CASCADE ON DELETE CASCADE,

    read BOOLEAN NOT NULL DEFAULT TRUE,
    write BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE TABLE Host (
    id SERIAL PRIMARY KEY,
    uuid UUID NOT NULL UNIQUE,

    name VARCHAR(255) NOT NULL,
    domaine VARCHAR(255) NULL,

    description TEXT,

    daemonVersion TEXT NOT NULL,
    updated TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT NOW(),

    UNIQUE (name, domaine)
);

CREATE TABLE Changer (
    id SERIAL PRIMARY KEY,

    model VARCHAR(64) NOT NULL,
    vendor VARCHAR(64) NOT NULL,
    firmwareRev VARCHAR(64) NOT NULL,
    serialNumber VARCHAR(64) NOT NULL,
    wwn VARCHAR(64),

    barcode BOOLEAN NOT NULL,
    status ChangerStatus NOT NULL,

    isonline BOOLEAN NOT NULL DEFAULT TRUE,
    action ChangerAction NOT NULL DEFAULT 'none',

    enable BOOLEAN NOT NULL DEFAULT TRUE,

    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE Drive (
    id SERIAL PRIMARY KEY,

    model VARCHAR(64) NOT NULL,
    vendor VARCHAR(64) NOT NULL,
    firmwareRev VARCHAR(64) NOT NULL,
    serialNumber VARCHAR(64) NOT NULL,

    status DriveStatus NOT NULL,
    operationDuration FLOAT(3) NOT NULL DEFAULT 0 CHECK (operationDuration >= 0),
    lastClean TIMESTAMP(3) WITH TIME ZONE,
    enable BOOLEAN NOT NULL DEFAULT TRUE,

    changer INTEGER NULL REFERENCES Changer(id) ON UPDATE CASCADE ON DELETE CASCADE,
    driveFormat INTEGER NULL REFERENCES DriveFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE ChangerSlot (
    changer INTEGER NOT NULL REFERENCES Changer(id) ON UPDATE CASCADE ON DELETE CASCADE,
    index INTEGER NOT NULL,
    drive INTEGER UNIQUE REFERENCES Drive(id) ON UPDATE CASCADE ON DELETE SET NULL,

    media INTEGER REFERENCES Media(id) ON UPDATE CASCADE ON DELETE SET NULL,

    isieport BOOLEAN NOT NULL DEFAULT FALSE,
    enable BOOLEAN NOT NULL DEFAULT TRUE,

    PRIMARY KEY (changer, index)
);

CREATE TABLE SelectedFile (
    id BIGSERIAL PRIMARY KEY,
    path TEXT NOT NULL
);

CREATE TABLE Users (
    id SERIAL PRIMARY KEY,

    login VARCHAR(255) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    salt CHAR(16) NOT NULL,

    fullname VARCHAR(255),
    email VARCHAR(255) NOT NULL,
    homeDirectory TEXT NOT NULL,

    isAdmin BOOLEAN NOT NULL DEFAULT FALSE,
    canArchive BOOLEAN NOT NULL DEFAULT FALSE,
    canRestore BOOLEAN NOT NULL DEFAULT FALSE,

    meta JSON NOT NULL,

    poolgroup INTEGER REFERENCES PoolGroup(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    disabled BOOLEAN NOT NULL DEFAULT FALSE
);
CREATE UNIQUE INDEX ON users (LOWER(login));

CREATE TABLE UserEvent (
    id SERIAL PRIMARY KEY,
    event TEXT NOT NULL UNIQUE
);

CREATE TABLE UserLog (
    id BIGSERIAL PRIMARY KEY,

    login INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE CASCADE,
    timestamp TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT NOW(),
    event INTEGER NOT NULL REFERENCES UserEvent(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE Archive (
    id BIGSERIAL PRIMARY KEY,

    uuid UUID NOT NULL UNIQUE,
    name TEXT NOT NULL,

    creator INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    owner INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    canAppend BOOLEAN NOT NULL DEFAULT TRUE,
    deleted BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE ArchiveFile (
    id BIGSERIAL PRIMARY KEY,

    name TEXT NOT NULL,
    type FileType NOT NULL,
    mimeType VARCHAR(255) NOT NULL,

    ownerId INTEGER NOT NULL DEFAULT 0,
    owner VARCHAR(255) NOT NULL,
    groupId INTEGER NOT NULL DEFAULT 0,
    groups VARCHAR(255) NOT NULL,

    perm SMALLINT NOT NULL CHECK (perm >= 0),

    ctime TIMESTAMP(3) WITH TIME ZONE NOT NULL,
    mtime TIMESTAMP(3) WITH TIME ZONE NOT NULL,

    size BIGINT NOT NULL CHECK (size >= 0),

    parent BIGINT NOT NULL REFERENCES SelectedFile(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE Backup (
    id BIGSERIAL PRIMARY KEY,

    timestamp TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT now(),

    nbMedia INTEGER NOT NULL DEFAULT 0 CHECK (nbMedia >= 0),
    nbArchive INTEGER NOT NULL DEFAULT 0 CHECK (nbArchive >= 0)
);

CREATE TABLE JobType (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL UNIQUE
);

CREATE TABLE Job (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    type INTEGER NOT NULL REFERENCES JobType(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    nextStart TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT NOW(),
    interval INTERVAL DEFAULT NULL,
    repetition INTEGER NOT NULL DEFAULT 1,

    status JobStatus NOT NULL DEFAULT 'scheduled',
    update TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT NOW(),

    archive BIGINT DEFAULT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE CASCADE,
    backup BIGINT DEFAULT NULL REFERENCES Backup(id) ON UPDATE CASCADE ON DELETE CASCADE,
    media INTEGER NULL DEFAULT NULL REFERENCES Media(id) ON UPDATE CASCADE ON DELETE CASCADE,
    pool INTEGER NULL DEFAULT NULL REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE SET NULL,

    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    login INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE CASCADE,

    metadata JSON NOT NULL DEFAULT '{}',
    options JSON NOT NULL DEFAULT '{}'
);

CREATE TABLE JobRun (
    id BIGSERIAL PRIMARY KEY,
    job BIGINT NOT NULL REFERENCES Job(id) ON UPDATE CASCADE ON DELETE CASCADE,
    numRun INTEGER NOT NULL DEFAULT 1 CHECK (numRun > 0),

    starttime TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT NOW(),
    endtime TIMESTAMP(3) WITH TIME ZONE,

    status JobStatus NOT NULL DEFAULT 'running',
    step JobRunStep NOT NULL DEFAULT 'pre job',
    done FLOAT NOT NULL DEFAULT 0,

    exitcode INTEGER NOT NULL DEFAULT 0,
    stoppedbyuser BOOLEAN NOT NULL DEFAULT FALSE,

    CHECK (starttime <= endtime)
);

CREATE TABLE ArchiveToArchiveMirror (
    archive BIGINT NOT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE CASCADE,
    archivemirror INTEGER NOT NULL REFERENCES ArchiveMirror(id) ON UPDATE CASCADE ON DELETE CASCADE,

    lastUpdate TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT NOW(),
    jobrun BIGINT NOT NULL REFERENCES JobRun(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE TABLE ArchiveVolume (
    id BIGSERIAL PRIMARY KEY,

    sequence INTEGER NOT NULL DEFAULT 0 CHECK (sequence >= 0),
    size BIGINT NOT NULL DEFAULT 0 CHECK (size >= 0),

    starttime TIMESTAMP(3) WITH TIME ZONE NOT NULL,
    endtime TIMESTAMP(3) WITH TIME ZONE NOT NULL,

    checktime TIMESTAMP(3) WITH TIME ZONE,
    checksumok BOOLEAN NOT NULL DEFAULT FALSE,

    archive BIGINT NOT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE CASCADE,
    media INTEGER NOT NULL REFERENCES Media(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    mediaPosition INTEGER NOT NULL DEFAULT 0 CHECK (mediaPosition >= 0),
    jobrun BIGINT REFERENCES JobRun(id) ON UPDATE CASCADE ON DELETE SET NULL,
    purged BIGINT REFERENCES JobRun(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    CONSTRAINT archiveVolume_time CHECK (starttime <= endtime)
);

CREATE TABLE BackupVolume (
    id BIGSERIAL PRIMARY KEY,

    sequence INTEGER NOT NULL DEFAULT 0 CHECK (sequence >= 0),
    size BIGINT NOT NULL DEFAULT 0 CHECK (size >= 0),

    media INTEGER NOT NULL REFERENCES Media(id) ON DELETE RESTRICT ON UPDATE CASCADE,
    mediaPosition INTEGER NOT NULL DEFAULT 0 CHECK (mediaPosition >= 0),

    checktime TIMESTAMP(3) WITH TIME ZONE,
    checksumok BOOLEAN NOT NULL DEFAULT FALSE,

    backup BIGINT NOT NULL REFERENCES Backup(id) ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE ArchiveFileToArchiveVolume (
    archiveVolume BIGINT NOT NULL REFERENCES ArchiveVolume(id) ON UPDATE CASCADE ON DELETE CASCADE,
    archiveFile BIGINT NOT NULL REFERENCES ArchiveFile(id) ON UPDATE CASCADE ON DELETE CASCADE,

    blockNumber BIGINT CHECK (blockNumber >= 0) NOT NULL,
    archivetime TIMESTAMP(3) WITH TIME ZONE NOT NULL,

    checktime TIMESTAMP(3) WITH TIME ZONE,
    checksumok BOOLEAN NOT NULL DEFAULT FALSE,

    PRIMARY KEY (archiveVolume, archiveFile)
);

ALTER TABLE Backup ADD jobrun BIGINT NOT NULL REFERENCES JobRun(id) ON UPDATE CASCADE ON DELETE SET NULL;

CREATE TABLE Metadata (
    id BIGINT NOT NULL,
    type MetaType NOT NULL,

    key TEXT NOT NULL,
    value JSONB NOT NULL,

    login INTEGER NOT NULL REFERENCES users(id) ON UPDATE CASCADE ON DELETE CASCADE,

    PRIMARY KEY (id, type, key)
);

CREATE TABLE MetadataLog (
    id BIGINT NOT NULL,
    type MetaType NOT NULL,

    key TEXT NOT NULL,
    value JSONB NOT NULL,

    login INTEGER NOT NULL REFERENCES users(id) ON UPDATE CASCADE ON DELETE CASCADE,

    timestamp TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT NOW(),
    updated BOOLEAN NOT NULL
);
CREATE INDEX ON MetadataLog (id, type, key);

CREATE TABLE Proxy (
    id BIGSERIAL PRIMARY KEY,
    archivefile BIGINT NOT NULL REFERENCES ArchiveFile(id) ON UPDATE CASCADE ON DELETE CASCADE,
    status ProxyStatus NOT NULL DEFAULT 'todo'
);

CREATE TABLE Checksum (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL UNIQUE,
    deflt BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE ChecksumResult (
    id BIGSERIAL PRIMARY KEY,

    checksum INTEGER NOT NULL REFERENCES Checksum(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    result TEXT NOT NULL,

    CONSTRAINT ChecksumResult_checksum_invalid UNIQUE (checksum, result)
);

CREATE TABLE PoolToChecksum (
    pool INTEGER NOT NULL REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE CASCADE,
    checksum INTEGER NOT NULL REFERENCES Checksum(id) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (pool, checksum)
);

CREATE TABLE PoolTemplateToChecksum (
    poolTemplate INTEGER NOT NULL REFERENCES PoolTemplate(id) ON UPDATE CASCADE ON DELETE CASCADE,
    checksum INTEGER NOT NULL REFERENCES Checksum(id) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (poolTemplate, checksum)
);

CREATE TABLE ArchiveFileToChecksumResult (
    archiveFile BIGINT NOT NULL REFERENCES ArchiveFile(id) ON UPDATE CASCADE ON DELETE CASCADE,
    checksumResult BIGINT NOT NULL REFERENCES ChecksumResult(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    PRIMARY KEY (archiveFile, checksumResult)
);

CREATE TABLE ArchiveVolumeToChecksumResult (
    archiveVolume BIGINT NOT NULL REFERENCES ArchiveVolume(id) ON UPDATE CASCADE ON DELETE CASCADE,
    checksumResult BIGINT NOT NULL REFERENCES ChecksumResult(id) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (archiveVolume, checksumResult)
);

CREATE TABLE BackupVolumeToChecksumResult (
    backupVolume BIGINT NOT NULL REFERENCES BackupVolume(id) ON UPDATE CASCADE ON DELETE CASCADE,
    checksumResult BIGINT NOT NULL REFERENCES ChecksumResult(id) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (backupVolume, checksumResult)
);

CREATE TABLE JobRecord (
    id BIGSERIAL PRIMARY KEY,
    jobrun BIGINT NOT NULL REFERENCES Jobrun(id) ON UPDATE CASCADE ON DELETE CASCADE,

    timestamp TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT NOW(),
    status JobStatus NOT NULL CHECK (status != 'disable'),

    level LogLevel NOT NULL,
    message TEXT NOT NULL,

    notif JobRecordNotif NOT NULL DEFAULT 'normal'
);

CREATE TABLE JobToSelectedFile (
    job BIGINT NOT NULL REFERENCES Job(id) ON UPDATE CASCADE ON DELETE CASCADE,
    selectedFile BIGINT NOT NULL REFERENCES SelectedFile(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    PRIMARY KEY (job, selectedFile)
);

CREATE TABLE Log (
    id BIGSERIAL PRIMARY KEY,

    type LogType NOT NULL,
    level LogLevel NOT NULL,
    time TIMESTAMP(3) WITH TIME ZONE NOT NULL,
    message TEXT NOT NULL,
    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE CASCADE,
    login INTEGER REFERENCES Users(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE TABLE RestoreTo (
    id BIGSERIAL PRIMARY KEY,

    path VARCHAR(255) NOT NULL DEFAULT '/',
    job BIGINT NOT NULL REFERENCES Job(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE TABLE Report (
    id BIGSERIAL PRIMARY KEY,
    timestamp TIMESTAMP(3) WITH TIME ZONE NOT NULL DEFAULT NOW(),

    archive BIGINT REFERENCES archive(id) ON UPDATE CASCADE ON DELETE CASCADE,
    media BIGINT REFERENCES Media(id) ON UPDATE CASCADE ON DELETE CASCADE,
    jobrun BIGINT NOT NULL REFERENCES jobrun(id) ON UPDATE CASCADE ON DELETE CASCADE,

    data JSON NOT NULL
);

CREATE TABLE Reports (
    id BIGSERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    report BIGINT NOT NULL REFERENCES report(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE TABLE Script (
    id SERIAL PRIMARY KEY,
    path TEXT NOT NULL UNIQUE
);

CREATE TABLE Scripts (
    id SERIAL PRIMARY KEY,

    sequence INTEGER NOT NULL CHECK (sequence >= 0),
    jobType INTEGER NOT NULL REFERENCES JobType(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    script INTEGER REFERENCES Script(id) ON UPDATE CASCADE ON DELETE CASCADE,
    scriptType ScriptType NOT NULL,
    pool INTEGER REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE CASCADE,

    UNIQUE (sequence, jobType, script, scriptType, pool)
);

CREATE TABLE Vtl (
    id SERIAL PRIMARY KEY,
    uuid UUID NOT NULL UNIQUE,

    path VARCHAR(255) NOT NULL,
    prefix VARCHAR(255) NOT NULL,
    nbslots INTEGER NOT NULL CHECK (nbslots > 0),
    nbdrives INTEGER NOT NULL CHECK (nbdrives > 0),

    mediaformat INTEGER NOT NULL REFERENCES MediaFormat(id) ON UPDATE CASCADE ON DELETE CASCADE,
    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE CASCADE,

    deleted BOOLEAN NOT NULL DEFAULT FALSE,

    CHECK (nbslots >= nbdrives)
);

-- Functions
CREATE OR REPLACE FUNCTION "json_object_set_key"("json" json, "key_to_set" TEXT, "value_to_set" anyelement)
  RETURNS json
  LANGUAGE sql
  IMMUTABLE
  STRICT
AS $function$
SELECT CONCAT('{', STRING_AGG(TO_JSON("key") || ':' || "value", ','), '}')::JSON
  FROM (SELECT *
          FROM JSON_EACH("json")
         WHERE "key" <> "key_to_set"
         UNION ALL
        SELECT "key_to_set", TO_JSON("value_to_set")) AS "fields"
$function$;

-- Triggers
CREATE OR REPLACE FUNCTION check_metadata() RETURNS TRIGGER AS $body$
    BEGIN
        IF TG_OP = 'UPDATE' AND OLD.id != NEW.id THEN
            UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = TG_TABLE_NAME::MetaType;
        ELSIF TG_OP = 'DELETE' THEN
            DELETE FROM Metadata WHERE id = OLD.id AND type = TG_TABLE_NAME::MetaType;
        END IF;
        RETURN NEW;
    END;
$body$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION log_metadata() RETURNS TRIGGER AS $body$
    BEGIN
        IF TG_OP = 'UPDATE' AND OLD.type != NEW.type THEN
            RAISE EXCEPTION 'type of metadata should not be modified' USING ERRCODE = '09000';
        ELSIF TG_OP = 'DELETE' OR OLD != NEW THEN
            INSERT INTO MetadataLog(id, type, key, value, login, updated)
                VALUES (OLD.id, OLD.type, OLD.key, OLD.value, OLD.login, TG_OP != 'UPDATE');
        END IF;
        RETURN NEW;
    END;
$body$ LANGUAGE plpgsql;

CREATE TRIGGER update_metadata
AFTER UPDATE OR DELETE ON Archive
FOR EACH ROW EXECUTE PROCEDURE check_metadata();

CREATE TRIGGER update_metadata
AFTER UPDATE OR DELETE ON ArchiveFile
FOR EACH ROW EXECUTE PROCEDURE check_metadata();

CREATE TRIGGER update_metadata
AFTER UPDATE OR DELETE ON ArchiveVolume
FOR EACH ROW EXECUTE PROCEDURE check_metadata();

CREATE TRIGGER update_metadata
AFTER UPDATE OR DELETE ON Host
FOR EACH ROW EXECUTE PROCEDURE check_metadata();

CREATE TRIGGER update_metadata
AFTER UPDATE OR DELETE ON Media
FOR EACH ROW EXECUTE PROCEDURE check_metadata();

CREATE TRIGGER update_metadata
AFTER UPDATE OR DELETE ON Pool
FOR EACH ROW EXECUTE PROCEDURE check_metadata();

CREATE TRIGGER update_metadata
AFTER UPDATE OR DELETE ON Report
FOR EACH ROW EXECUTE PROCEDURE check_metadata();

CREATE TRIGGER update_metadata
AFTER UPDATE OR DELETE ON Users
FOR EACH ROW EXECUTE PROCEDURE check_metadata();

CREATE TRIGGER update_metadata
AFTER UPDATE OR DELETE ON Vtl
FOR EACH ROW EXECUTE PROCEDURE check_metadata();

CREATE TRIGGER log_metadata
BEFORE UPDATE OR DELETE ON Metadata
FOR EACH ROW EXECUTE PROCEDURE log_metadata();

-- Comments
COMMENT ON COLUMN ArchiveVolume.starttime IS 'Start time of archive volume creation';
COMMENT ON COLUMN ArchiveVolume.endtime IS 'End time of archive volume creation';
COMMENT ON COLUMN ArchiveVolume.checktime IS 'Last time of checked time';

COMMENT ON TABLE Checksum IS 'Contains only checksum available';

COMMENT ON COLUMN DriveFormat.cleaningInterval IS 'Interval between two cleaning in days';

COMMENT ON TYPE JobStatus IS E'disable => disabled,\nerror => error while running,\nfinished => task finished,\npause => waiting for user action,\nrunning => running,\nscheduled => not yet started or completed,\nstopped => stopped by user,\nwaiting => waiting for a resource';

COMMENT ON COLUMN Media.label IS 'Contains an UUID';
COMMENT ON COLUMN Media.append IS 'Can add file into this media';
COMMENT ON COLUMN Media.writeLock IS 'Media is write protected';

COMMENT ON COLUMN MediaFormat.blockSize IS 'Default block size';
COMMENT ON COLUMN MediaFormat.supportPartition IS 'Is the media can be partitionned';
COMMENT ON COLUMN MediaFormat.supportMAM IS 'MAM: Medium Axiliary Memory, contains some usefull data';

