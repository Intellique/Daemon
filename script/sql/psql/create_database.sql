-- Types
CREATE TYPE ChangerSlotType AS ENUM (
    'default',
    'drive',
    'import / export'
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

CREATE TYPE JobStatus AS ENUM (
    'disable',
    'finished',
    'error',
    'idle',
    'pause',
    'running',
    'stopped',
    'waiting'
);

CREATE TYPE LogLevel AS ENUM (
    'debug',
    'info',
    'warning',
    'error'
);

CREATE TYPE LogType AS ENUM (
    'changer',
    'checksum',
    'daemon',
    'database',
    'drive',
    'job',
    'plugin checksum',
    'plugin db',
    'plugin log',
    'scheduler',
    'ui',
    'user message'
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
    'online',
    'in drive'
);

CREATE TYPE TapeStatus AS ENUM (
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

CREATE TYPE TapeType AS ENUM (
    'read only',
    'rewritable',
    'cleaning'
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

    lifespan INTERVAL NOT NULL CHECK (lifespan > INTERVAL 'P1Y'),

    capacity BIGINT NOT NULL CHECK (capacity > 0),
    blockSize INTEGER NOT NULL DEFAULT 0 CHECK (blockSize >= 0),
    densityCode SMALLINT NOT NULL,

    supportPartition BOOLEAN NOT NULL DEFAULT FALSE,
    supportMAM BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE Pool (
    id SERIAL PRIMARY KEY,

    uuid UUID NOT NULL UNIQUE,
    name VARCHAR(64) NOT NULL,

    tapeFormat INTEGER NOT NULL REFERENCES TapeFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    growable BOOLEAN NOT NULL DEFAULT FALSE,
    rewritable BOOLEAN NOT NULL DEFAULT TRUE,
    deleted BOOLEAN NOT NULL DEFAULT FALSE,

    metadata TEXT NOT NULL DEFAULT ''
);

CREATE TABLE Tape (
    id SERIAL PRIMARY KEY,

    uuid UUID NULL UNIQUE,
    label VARCHAR(64),
    mediumSerialNumber VARCHAR(32) UNIQUE,
    name VARCHAR(255) NULL,

    status TapeStatus NOT NULL,
    location TapeLocation NOT NULL,

    firstUsed TIMESTAMP(0) NOT NULL,
    useBefore TIMESTAMP(0) NOT NULL,

    loadCount INTEGER NOT NULL DEFAULT 0 CHECK (loadCount >= 0),
    readCount INTEGER NOT NULL DEFAULT 0 CHECK (readCount >= 0),
    writeCount INTEGER NOT NULL DEFAULT 0 CHECK (writeCount >= 0),
    endPos INTEGER NOT NULL DEFAULT 0 CHECK (endPos >= 0),

    nbFiles INTEGER NOT NULL DEFAULT 0 CHECK (nbFiles >= 0),
    blockSize INTEGER NOT NULL DEFAULT 0 CHECK (blockSize >= 0),

    hasPartition BOOLEAN NOT NULL DEFAULT FALSE,
    type tapetype NOT NULL DEFAULT 'rewritable',

    tapeFormat INTEGER NOT NULL REFERENCES TapeFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    pool INTEGER NULL REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE SET NULL,

    CHECK (firstUsed < useBefore)
);

CREATE TABLE TapeLabel (
    id BIGSERIAL PRIMARY KEY,

    name TEXT NOT NULL,
    tape INTEGER NOT NULL REFERENCES Tape(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE TABLE DriveFormat (
    id SERIAL PRIMARY KEY,

    name VARCHAR(64) NOT NULL UNIQUE,
    densityCode SMALLINT NOT NULL CHECK (densityCode > 0),
    mode TapeFormatMode NOT NULL,

    cleaningInterval INTERVAL NOT NULL CHECK (cleaningInterval >= INTERVAL 'P1W')
);

CREATE TABLE DriveFormatSupport (
    driveFormat INTEGER NOT NULL REFERENCES DriveFormat(id) ON UPDATE CASCADE ON DELETE CASCADE,
    tapeFormat INTEGER NOT NULL REFERENCES TapeFormat(id) ON UPDATE CASCADE ON DELETE CASCADE,

    read BOOLEAN NOT NULL DEFAULT TRUE,
    write BOOLEAN NOT NULL DEFAULT TRUE,

    PRIMARY KEY (driveFormat, tapeFormat)
);

CREATE TABLE Host (
    id SERIAL PRIMARY KEY,

    name VARCHAR(255) NOT NULL,
    domaine VARCHAR(255) NULL,

    description TEXT NOT NULL DEFAULT '',

    UNIQUE (name, domaine)
);

CREATE TABLE Changer (
    id SERIAL PRIMARY KEY,

    device VARCHAR(64) NOT NULL,

    model VARCHAR(64) NOT NULL,
    vendor VARCHAR(64) NOT NULL,
    firmwareRev VARCHAR(64) NOT NULL,
    serialNumber VARCHAR(64) NOT NULL,

    barcode BOOLEAN NOT NULL,
    status ChangerStatus NOT NULL,
    enable BOOLEAN NOT NULL DEFAULT TRUE,

    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE Drive (
    id SERIAL PRIMARY KEY,
    changerNum INTEGER NOT NULL CHECK (changerNum >= 0),

    device VARCHAR(64) NOT NULL,
    scsiDevice VARCHAR(64) NOT NULL,

    model VARCHAR(64) NOT NULL,
    vendor VARCHAR(64) NOT NULL,
    firmwareRev VARCHAR(64) NOT NULL,
    serialNumber VARCHAR(64) NOT NULL,

    status DriveStatus NOT NULL,
    operationDuration FLOAT(3) NOT NULL DEFAULT 0 CHECK (operationDuration >= 0),
    lastClean TIMESTAMP(0) NOT NULL,
    enable BOOLEAN NOT NULL DEFAULT TRUE,

    changer INTEGER NULL REFERENCES Changer(id) ON UPDATE CASCADE ON DELETE CASCADE,
    driveFormat INTEGER NULL REFERENCES DriveFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE ChangerSlot (
    id SERIAL PRIMARY KEY,

    index INTEGER NOT NULL,
    changer INTEGER NOT NULL REFERENCES Changer(id) ON UPDATE CASCADE ON DELETE CASCADE,
    tape INTEGER REFERENCES Tape(id) ON UPDATE CASCADE ON DELETE SET NULL,
    type ChangerSlotType NOT NULL DEFAULT 'default',

    CONSTRAINT unique_slot UNIQUE (index, changer)
);

CREATE TABLE SelectedFile (
    id BIGSERIAL PRIMARY KEY,
    path VARCHAR(255) NOT NULL
);

CREATE TABLE Users (
    id SERIAL PRIMARY KEY,

    login VARCHAR(255) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    salt CHAR(16) NOT NULL,

    fullname VARCHAR(255),
    email VARCHAR(255) NOT NULL,

    isAdmin BOOLEAN NOT NULL DEFAULT FALSE,
    canArchive BOOLEAN NOT NULL DEFAULT FALSE,
    canRestore BOOLEAN NOT NULL DEFAULT FALSE,

    disabled BOOLEAN NOT NULL DEFAULT FALSE,

    pool INTEGER NOT NULL REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    meta hstore NOT NULL
);

CREATE TABLE UserEvent (
    id SERIAL PRIMARY KEY,
    event TEXT NOT NULL UNIQUE
);

CREATE TABLE UserLog (
    id BIGSERIAL PRIMARY KEY,

    login INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE CASCADE,
    timestamp TIMESTAMP(0) NOT NULL DEFAULT NOW(),
    event INTEGER NOT NULL REFERENCES UserEvent(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE Archive (
    id BIGSERIAL PRIMARY KEY,

    name TEXT NOT NULL,

    ctime TIMESTAMP(0) NOT NULL,
    endtime TIMESTAMP(0),

    creator INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    owner INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    metadata hstore NOT NULL,
    copyOf BIGINT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    CONSTRAINT archive_time CHECK (ctime <= endtime)
);

CREATE TABLE ArchiveFile (
    id BIGSERIAL PRIMARY KEY,

    name TEXT NOT NULL,
    type FileType NOT NULL,
    mimeType VARCHAR(64) NOT NULL,

    ownerId INTEGER NOT NULL DEFAULT 0,
    owner VARCHAR(255) NOT NULL,
    groupId INTEGER NOT NULL DEFAULT 0,
    groups VARCHAR(255) NOT NULL,

    perm SMALLINT NOT NULL CHECK (perm >= 0),

    ctime TIMESTAMP(0) NOT NULL,
    mtime TIMESTAMP(0) NOT NULL,

    size BIGINT NOT NULL CHECK (size >= 0)
);

CREATE TABLE ArchiveVolume (
    id BIGSERIAL PRIMARY KEY,

    sequence INTEGER NOT NULL DEFAULT 0 CHECK (sequence >= 0),
    size BIGINT NOT NULL DEFAULT 0 CHECK (size >= 0),

    ctime TIMESTAMP(0) NOT NULL,
    endtime TIMESTAMP(0),

    archive BIGINT NOT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE CASCADE,
    tape INTEGER NOT NULL REFERENCES Tape(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    tapePosition INTEGER NOT NULL DEFAULT 0 CHECK (tapePosition >= 0),

    CONSTRAINT archiveVolume_time CHECK (ctime <= endtime)
);

CREATE TABLE ArchiveFileToArchiveVolume (
    archiveVolume BIGINT NOT NULL REFERENCES ArchiveVolume(id) ON UPDATE CASCADE ON DELETE CASCADE,
    archiveFile BIGINT NOT NULL REFERENCES ArchiveFile(id) ON UPDATE CASCADE ON DELETE CASCADE,

    blockNumber BIGINT NOT NULL DEFAULT 0 CHECK (blockNumber >= 0),

    PRIMARY KEY (archiveVolume, archiveFile)
);

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

CREATE TABLE Checksum (
    id SERIAL PRIMARY KEY,
    name VARCHAR(64) NOT NULL UNIQUE
);

CREATE TABLE ChecksumResult (
    id BIGSERIAL PRIMARY KEY,

    checksum INTEGER NOT NULL REFERENCES Checksum(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    result TEXT NOT NULL,

    CONSTRAINT ChecksumResult_checksum_invalid UNIQUE (checksum, result)
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

CREATE TABLE JobType (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL UNIQUE
);

CREATE TABLE Job (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    type INTEGER NOT NULL REFERENCES JobType(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    nextStart TIMESTAMP(0) NOT NULL DEFAULT NOW(),
    interval INTERVAL DEFAULT NULL,
    repetition INTEGER NOT NULL DEFAULT 1,
    done FLOAT NOT NULL DEFAULT 0,
    status JobStatus NOT NULL DEFAULT 'idle',
    update TIMESTAMP(0) NOT NULL DEFAULT NOW(),

    archive BIGINT DEFAULT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE CASCADE,
    backup BIGINT DEFAULT NULL REFERENCES Backup(id) ON UPDATE CASCADE ON DELETE CASCADE,
    tape INTEGER NULL DEFAULT NULL REFERENCES Tape(id) ON UPDATE CASCADE ON DELETE CASCADE,
    pool INTEGER NULL DEFAULT NULL REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE SET NULL,

    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    login INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE CASCADE,

    metadata hstore NOT NULL DEFAULT '',
    options hstore NOT NULL DEFAULT ''
);

CREATE TABLE JobToChecksum (
    job BIGINT NOT NULL REFERENCES Job(id) ON UPDATE CASCADE ON DELETE CASCADE,
    checksum INTEGER NOT NULL REFERENCES Checksum(id) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (job, checksum)
);

CREATE TABLE JobRecord (
    id BIGSERIAL PRIMARY KEY,

    job BIGINT NOT NULL REFERENCES Job(id) ON UPDATE CASCADE ON DELETE CASCADE,
    status JobStatus NOT NULL CHECK (status != 'disable'),
    numRun INTEGER NOT NULL DEFAULT 1 CHECK (numRun > 0),
    timestamp TIMESTAMP(0) NOT NULL DEFAULT NOW(),
    message TEXT
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
    time TIMESTAMP(0) NOT NULL,
    message TEXT NOT NULL,
    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE CASCADE,
    login INTEGER NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE SET NULL
);

CREATE TABLE RestoreTo (
    id BIGSERIAL PRIMARY KEY,

    path VARCHAR(255) NOT NULL DEFAULT '/',
    nbTruncPath INTEGER NOT NULL DEFAULT 0 CHECK (nbTruncPath >= 0),
    job BIGINT NOT NULL REFERENCES Job(id) ON UPDATE CASCADE ON DELETE CASCADE
);

-- Comments
COMMENT ON COLUMN Archive.ctime IS 'Start time of archive creation';
COMMENT ON COLUMN Archive.endtime IS 'End time of archive creation';

COMMENT ON TABLE Checksum IS 'Contains only checksum available';

COMMENT ON COLUMN DriveFormat.cleaningInterval IS 'Interval between two cleaning in days';

COMMENT ON TYPE JobStatus IS E'disable => disabled,\nfinish => job finished,\nerror => error while running,\nidle => not yet started or completed,\npause => waiting for user action,\nrunning => running,\nstopped => stopped by user,\nwaiting => waiting for a resource';

COMMENT ON COLUMN Tape.label IS 'Contains an UUID';

COMMENT ON COLUMN TapeFormat.blockSize IS 'Default block size';
COMMENT ON COLUMN TapeFormat.supportPartition IS 'Is the tape can be partitionned';
COMMENT ON COLUMN TapeFormat.supportMAM IS 'MAM: Medium Axiliary Memory, contains some usefull data';

