-- Functions
CREATE FUNCTION unix_to_timestamp(IN unix NUMERIC) RETURNS timestamp with time zone AS
$BODY$select timestamp with time zone 'epoch' + $1 * interval '1 s';$BODY$
LANGUAGE sql IMMUTABLE;

-- Types
CREATE TYPE ChangerSlotType AS ENUM (
    'default',
    'io'
);

CREATE TYPE ChangerStatus AS ENUM (
    'error',
    'exporting',
    'idle',
    'importing',
    'loading',
    'unknown',
    'unloading'
);

CREATE TYPE DriveStatus AS ENUM (
    'cleaning',
    'empty-idle',
    'erasing',
    'error',
    'loaded-idle',
    'loading',
    'positionning',
    'reading',
    'unloading',
    'writing'
);

CREATE TYPE FileType AS ENUM (
    'blockDevice',
    'characterDevice',
    'directory',
    'fifo',
    'regularFile',
    'socket',
    'symbolicLink'
);

CREATE TYPE JobStatus AS ENUM (
    'disable',
    'error',
    'idle',
    'pause',
    'running'
);

CREATE TYPE JobType AS ENUM (
    'diffSave',
    'dummy',
    'incSave',
    'integrityCheck',
    'list',
    'restore',
    'save',
    'verify'
);

CREATE TYPE TapeFormatDataType AS ENUM (
    'audio',
    'cleaning',
    'data',
    'video'
);

CREATE TYPE TapeFormatMode AS ENUM (
    'disk',
    'linear',
    'optical'
);

CREATE TYPE TapeLocation AS ENUM (
    'offline',
    'online'
);

CREATE TYPE TapeStatus AS ENUM (
    'erasable',
    'error',
    'foreign',
    'in_use',
    'locked',
    'needs_replacement',
    'new',
    'pooled'
);


-- Tables
CREATE TABLE TapeFormat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL UNIQUE,
    dataType TapeFormatDataType NOT NULL,
    mode TapeFormatMode NOT NULL,
    maxLoadCount INTEGER NOT NULL CHECK (maxLoadCount > 0),
    maxReadCount INTEGER NOT NULL CHECK (maxReadCount > 0),
    maxWriteCount INTEGER NOT NULL CHECK (maxWriteCount > 0),
    maxOpCount INTEGER NOT NULL CHECK (maxOpCount > 0),
    lifespan INTEGER NOT NULL CHECK (lifespan > 0),
    capacity BIGINT NOT NULL CHECK (capacity > 0),
    blockSize INTEGER NOT NULL DEFAULT 0 CHECK (blockSize >= 0),
    supportPartition BOOLEAN NOT NULL
);

CREATE TABLE Pool (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    retention INTEGER NOT NULL CHECK (retention > 0),
    retentionLimit TIMESTAMP,
    autoRecycle BOOLEAN NOT NULL,
    tapeFormat_id INTEGER NOT NULL REFERENCES TapeFormat(id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Tape (
    id SERIAL PRIMARY KEY,
    label UUID NULL UNIQUE,
    name VARCHAR(64) NOT NULL,
    status TapeStatus NOT NULL,
    location TapeLocation NOT NULL,
    firstUsed TIMESTAMP(0),
    useBefore TIMESTAMP(0),
    loadCount INTEGER NOT NULL DEFAULT 0 CHECK (loadCount >= 0),
    readCount INTEGER NOT NULL DEFAULT 0 CHECK (readCount >= 0),
    writeCount INTEGER NOT NULL DEFAULT 0 CHECK (writeCount >= 0),
    endPos INTEGER NOT NULL DEFAULT 0 CHECK (endPos >= 0),
    nbFiles INTEGER NOT NULL DEFAULT 0 CHECK (nbFiles >= 0),
    hasPartition BOOLEAN NOT NULL DEFAULT FALSE,
    tapeFormat INTEGER NOT NULL REFERENCES TapeFormat(id) ON DELETE RESTRICT ON UPDATE CASCADE,
    pool INTEGER REFERENCES Pool(id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CHECK (firstUsed < useBefore)
);

CREATE TABLE DriveFormat (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL UNIQUE,
    cleaningInterval INTEGER NOT NULL CHECK (cleaningInterval > 0)
);

CREATE TABLE DriveFormatSupport (
    driveFormat INTEGER NOT NULL REFERENCES DriveFormat(id) ON DELETE CASCADE ON UPDATE CASCADE,
    tapeFormat INTEGER NOT NULL REFERENCES TapeFormat(id) ON DELETE CASCADE ON UPDATE CASCADE,
    read BOOLEAN NOT NULL,
    write BOOLEAN NOT NULL,
    PRIMARY KEY (driveFormat, tapeFormat)
);

CREATE TABLE Host (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL UNIQUE,
    description TEXT
);

CREATE TABLE Changer (
    id SERIAL PRIMARY KEY,
    device VARCHAR(64) NOT NULL,
    status ChangerStatus NOT NULL,
    barcode BOOLEAN NOT NULL,
    isVirtual BOOLEAN NOT NULL,
    model VARCHAR(64) NOT NULL,
    vendor VARCHAR(64) NOT NULL,
    firmwareRev VARCHAR(64) NOT NULL,
    update TIMESTAMP(0) NOT NULL,
    host INTEGER NOT NULL REFERENCES Host(id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Drive (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL,
    device VARCHAR(64) NOT NULL,
    status DriveStatus NOT NULL,
    changerNum INTEGER NOT NULL CHECK (changerNum >= 0),
    operationDuration INTEGER NOT NULL DEFAULT 0 CHECK (operationDuration >= 0),
    lastClean TIMESTAMP(0) NOT NULL,
    model VARCHAR(64) NOT NULL,
    vendor VARCHAR(64) NOT NULL,
    firmwareRev VARCHAR(64) NOT NULL,
    changer INTEGER NOT NULL REFERENCES Changer(id) ON DELETE CASCADE ON UPDATE CASCADE,
    driveFormat INTEGER NOT NULL REFERENCES DriveFormat(id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE ChangerSlot (
    id SERIAL PRIMARY KEY,
    index INTEGER NOT NULL,

    changer INTEGER NOT NULL REFERENCES Changer(id) ON DELETE CASCADE ON UPDATE CASCADE,
    tape INTEGER REFERENCES Tape(id) ON DELETE SET NULL ON UPDATE CASCADE
);

CREATE TABLE Archive (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    ctime TIMESTAMP(0),
    endtime TIMESTAMP(0),
    CONSTRAINT archive_time CHECK (ctime <= endtime)
);

CREATE TABLE ArchiveFile (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    type FileType NOT NULL,
    owner SMALLINT NOT NULL DEFAULT 0,
    groups SMALLINT NOT NULL DEFAULT 0,
    perm SMALLINT NOT NULL,
    ctime TIMESTAMP(0) NOT NULL,
    mtime TIMESTAMP(0) NOT NULL,
    size BIGINT NOT NULL CHECK (size >= 0)
);

CREATE TABLE ArchiveVolume (
    id SERIAL PRIMARY KEY,
    sequence INTEGER NOT NULL,
    size BIGINT NOT NULL,
    ctime TIMESTAMP(0),
    endtime TIMESTAMP(0),
    tapePosition INTEGER NOT NULL,
    archive INTEGER NOT NULL REFERENCES Archive(id) ON DELETE CASCADE ON UPDATE CASCADE,
    tape INTEGER NOT NULL REFERENCES Tape(id) ON DELETE CASCADE ON UPDATE CASCADE,
    CHECK (ctime <= endtime)
);

CREATE TABLE ArchiveFileToArchiveVolume (
    archiveVolume INTEGER NOT NULL REFERENCES ArchiveVolume(id) ON DELETE CASCADE ON UPDATE CASCADE,
    archiveFile INTEGER NOT NULL REFERENCES ArchiveFile(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (archiveVolume, archiveFile)
);

CREATE TABLE Checksum (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL UNIQUE
);

CREATE TABLE ChecksumResult (
    id BIGSERIAL PRIMARY KEY,
    result VARCHAR(255) NOT NULL,
    checksum INTEGER NOT NULL REFERENCES Checksum(id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT ChecksumResult_checksum_invalid UNIQUE (result, checksum)
);

CREATE TABLE ArchiveVolumeToChecksumResult (
    archiveVolume INTEGER NOT NULL REFERENCES ArchiveVolume(id) ON DELETE CASCADE ON UPDATE CASCADE,
    checksumResult INTEGER NOT NULL REFERENCES ChecksumResult(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (archiveVolume, checksumResult)
);

CREATE TABLE ArchiveFileToChecksumResult (
    archiveFile BIGINT NOT NULL REFERENCES ArchiveFile(id) ON DELETE CASCADE ON UPDATE CASCADE,
    checksumResult BIGINT NOT NULL REFERENCES ChecksumResult(id) ON DELETE RESTRICT ON UPDATE CASCADE,
    PRIMARY KEY (archiveFile, checksumResult)
);

CREATE TABLE MetaType (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE Meta (
    id BIGSERIAL PRIMARY KEY,
    metaType INTEGER NOT NULL REFERENCES MetaType(id) ON DELETE CASCADE ON UPDATE CASCADE,
    meta_value TEXT NOT NULL
);

CREATE TABLE ArchiveFileToMeta (
    archiveFile BIGINT NOT NULL REFERENCES ArchiveFile(id) ON DELETE CASCADE ON UPDATE CASCADE,
    meta BIGINT NOT NULL REFERENCES Meta(id) ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE HomeDir (
    id SERIAL PRIMARY KEY,
    path VARCHAR(255) NOT NULL
);

CREATE TABLE Users (
    id SERIAL PRIMARY KEY,
    login VARCHAR(255) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    fullname VARCHAR(255),
    email VARCHAR(255) NOT NULL,
    isAdmin BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE UsersToHomeDirToHost (
    login INTEGER NOT NULL REFERENCES Users(id) ON DELETE CASCADE ON UPDATE CASCADE,
    homeDir INTEGER NOT NULL REFERENCES HomeDir(id) ON DELETE CASCADE ON UPDATE CASCADE,
    host INTEGER NOT NULL REFERENCES Host(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (login, homeDir, host)
);

CREATE TABLE SelectedFile (
    id SERIAL PRIMARY KEY,
    path VARCHAR(255) NOT NULL,
    recursive BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE Job (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    type JobType NOT NULL,
    status JobStatus NOT NULL,
    start TIMESTAMP(0) NOT NULL,
    interval INTEGER CHECK (interval > 0),
    repetition INTEGER NOT NULL DEFAULT 1 CHECK (repetition >= 0),
    update TIMESTAMP(0) NOT NULL,
    archive INTEGER REFERENCES Archive(id) ON DELETE CASCADE ON UPDATE CASCADE,
    login INTEGER NOT NULL REFERENCES Users(id) ON DELETE CASCADE ON UPDATE CASCADE,
    pool INTEGER NULL REFERENCES Pool(id) ON DELETE SET NULL ON UPDATE CASCADE
);

CREATE TABLE JobToChecksum (
    job INTEGER NOT NULL REFERENCES Job(id) ON DELETE CASCADE ON UPDATE CASCADE,
    checksum INTEGER NOT NULL REFERENCES Checksum(id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (job, checksum)
);

CREATE TABLE JobRecord (
    id SERIAL PRIMARY KEY,
    status JobStatus NOT NULL CHECK (status != 'disable'),
    numRun INTEGER NOT NULL DEFAULT 1 CHECK (numRun > 0),
    started TIMESTAMP(0) NOT NULL,
    ended TIMESTAMP(0),
    message TEXT,
    job INTEGER NOT NULL REFERENCES Job(id) ON DELETE CASCADE ON UPDATE CASCADE,
    host INTEGER NOT NULL REFERENCES Host(id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CHECK (started <= ended)
);

CREATE TABLE JobToSelectedFile (
    job INTEGER NOT NULL REFERENCES Job(id) ON DELETE CASCADE ON UPDATE CASCADE,
    selectedFile INTEGER NOT NULL REFERENCES SelectedFile(id) ON DELETE RESTRICT ON UPDATE CASCADE,
    PRIMARY KEY (job, selectedFile)
);

-- Comments
COMMENT ON COLUMN Archive.ctime IS 'Start time of archive creation';
COMMENT ON COLUMN Archive.endtime IS 'End time of archive creation';

COMMENT ON TABLE Checksum IS 'Contains only checksum available';

COMMENT ON COLUMN DriveFormat.cleaningInterval IS 'Interval between two cleaning in days';

COMMENT ON COLUMN Tape.label IS 'Contains an UUID';

