-- Types
CREATE TYPE AutoCheckMode AS ENUM (
    'quick mode',
    'thorough mode',
    'none'
);

CREATE TYPE ChangerSlotType AS ENUM (
    'drive',
    'import / export',
    'storage'
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

CREATE TYPE MediaLocation AS ENUM (
    'offline',
    'online',
    'in drive'
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
    'readonly',
    'rewritable'
);

CREATE TYPE ProxyStatus AS ENUM (
    'todo',
    'running',
    'done',
    'error'
);

CREATE TYPE UnbreakableLevel AS ENUM (
    'archive',
    'file',
    'none'
);


-- Tables
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

CREATE TABLE Pool (
    id SERIAL PRIMARY KEY,

    uuid UUID NOT NULL UNIQUE,
    name VARCHAR(64) NOT NULL,

    mediaFormat INTEGER NOT NULL REFERENCES MediaFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    autocheck AutoCheckMode NOT NULL DEFAULT 'none',
    lockcheck BOOLEAN NOT NULL DEFAULT FALSE,

    growable BOOLEAN NOT NULL DEFAULT FALSE,
    unbreakableLevel UnbreakableLevel NOT NULL DEFAULT 'none',
    rewritable BOOLEAN NOT NULL DEFAULT TRUE,

    metadata TEXT NOT NULL DEFAULT '',
    needproxy BOOLEAN NOT NULL DEFAULT FALSE,

    poolOriginal INTEGER REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    deleted BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE Media (
    id SERIAL PRIMARY KEY,

    uuid UUID NULL UNIQUE,
    label VARCHAR(64),
    mediumSerialNumber VARCHAR(36) UNIQUE,
    name VARCHAR(255) NULL,

    status MediaStatus NOT NULL,
    location MediaLocation NOT NULL,

    firstUsed TIMESTAMP(0) NOT NULL,
    useBefore TIMESTAMP(0) NOT NULL,

    loadCount INTEGER NOT NULL DEFAULT 0 CHECK (loadCount >= 0),
    readCount INTEGER NOT NULL DEFAULT 0 CHECK (readCount >= 0),
    writeCount INTEGER NOT NULL DEFAULT 0 CHECK (writeCount >= 0),
    operationCount INTEGER NOT NULL DEFAULT 0 CHECK (operationCount >= 0),

    nbFiles INTEGER NOT NULL DEFAULT 0 CHECK (nbFiles >= 0),
    blockSize INTEGER NOT NULL DEFAULT 0 CHECK (blockSize >= 0),
    freeBlock INTEGER NOT NULL CHECK (freeblock >= 0),
    totalBlock INTEGER NOT NULL CHECK (totalBlock >= 0),

    hasPartition BOOLEAN NOT NULL DEFAULT FALSE,

    locked BOOLEAN NOT NULL DEFAULT FALSE,
    type MediaType NOT NULL DEFAULT 'rewritable',
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
    write BOOLEAN NOT NULL DEFAULT TRUE,

    PRIMARY KEY (driveFormat, mediaFormat)
);

CREATE TABLE Host (
    id SERIAL PRIMARY KEY,

    name VARCHAR(255) NOT NULL,
    domaine VARCHAR(255) NULL,

    description TEXT,

    UNIQUE (name, domaine)
);

CREATE TABLE Changer (
    id SERIAL PRIMARY KEY,

    device VARCHAR(64),

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

    device VARCHAR(64),
    scsiDevice VARCHAR(64),

    model VARCHAR(64) NOT NULL,
    vendor VARCHAR(64) NOT NULL,
    firmwareRev VARCHAR(64) NOT NULL,
    serialNumber VARCHAR(64) NOT NULL,

    status DriveStatus NOT NULL,
    operationDuration FLOAT(3) NOT NULL DEFAULT 0 CHECK (operationDuration >= 0),
    lastClean TIMESTAMP(0),
    enable BOOLEAN NOT NULL DEFAULT TRUE,

    changer INTEGER NULL REFERENCES Changer(id) ON UPDATE CASCADE ON DELETE CASCADE,
    driveFormat INTEGER NULL REFERENCES DriveFormat(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE ChangerSlot (
    id SERIAL PRIMARY KEY,

    index INTEGER NOT NULL,
    changer INTEGER NOT NULL REFERENCES Changer(id) ON UPDATE CASCADE ON DELETE CASCADE,
    drive INTEGER UNIQUE REFERENCES Drive(id) ON UPDATE CASCADE ON DELETE SET NULL,
    media INTEGER REFERENCES Media(id) ON UPDATE CASCADE ON DELETE SET NULL,
    type ChangerSlotType NOT NULL DEFAULT 'storage',
    enable BOOLEAN NOT NULL DEFAULT TRUE,

    CONSTRAINT unique_slot UNIQUE (index, changer)
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

    uuid UUID NOT NULL UNIQUE,
    name TEXT NOT NULL,

    starttime TIMESTAMP(0) NOT NULL,
    endtime TIMESTAMP(0) NOT NULL,

    checksumok BOOLEAN NOT NULL DEFAULT FALSE,
    checktime TIMESTAMP(0),

    creator INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    owner INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    copyOf BIGINT REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    deleted BOOLEAN NOT NULL DEFAULT FALSE,

    CONSTRAINT archive_id CHECK (id != copyOf),
    CONSTRAINT archive_time CHECK (starttime <= endtime)
);

CREATE TABLE ArchiveFile (
    id BIGSERIAL PRIMARY KEY,

    name TEXT NOT NULL,
    type FileType NOT NULL,
    mimeType VARCHAR(64) NOT NULL,

    ownerId SMALLINT NOT NULL DEFAULT 0,
    owner VARCHAR(255) NOT NULL,
    groupId SMALLINT NOT NULL DEFAULT 0,
    groups VARCHAR(255) NOT NULL,

    perm SMALLINT NOT NULL CHECK (perm >= 0),

    ctime TIMESTAMP(0) NOT NULL,
    mtime TIMESTAMP(0) NOT NULL,

    size BIGINT NOT NULL CHECK (size >= 0),

    parent BIGINT NOT NULL REFERENCES SelectedFile(id) ON UPDATE CASCADE ON DELETE RESTRICT
);

CREATE TABLE ArchiveVolume (
    id BIGSERIAL PRIMARY KEY,

    sequence INTEGER NOT NULL DEFAULT 0 CHECK (sequence >= 0),
    size BIGINT NOT NULL DEFAULT 0 CHECK (size >= 0),

    starttime TIMESTAMP(0) NOT NULL,
    endtime TIMESTAMP(0) NOT NULL,

    checksumok BOOLEAN NOT NULL DEFAULT FALSE,
    checktime TIMESTAMP(0),

    archive BIGINT NOT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE CASCADE,
    media INTEGER NOT NULL REFERENCES Media(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    mediaPosition INTEGER NOT NULL DEFAULT 0 CHECK (mediaPosition >= 0),
    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE RESTRICT,

    CONSTRAINT archiveVolume_time CHECK (starttime <= endtime)
);

CREATE TABLE ArchiveFileToArchiveVolume (
    archiveVolume BIGINT NOT NULL REFERENCES ArchiveVolume(id) ON UPDATE CASCADE ON DELETE CASCADE,
    archiveFile BIGINT NOT NULL REFERENCES ArchiveFile(id) ON UPDATE CASCADE ON DELETE CASCADE,

    blockNumber BIGINT CHECK (blockNumber >= 0) NOT NULL,
    archivetime TIMESTAMP(0) NOT NULL,

    checktime TIMESTAMP(0),
    checksumok BOOLEAN NOT NULL DEFAULT FALSE,

    PRIMARY KEY (archiveVolume, archiveFile)
);

CREATE TABLE Metadata (
    key TEXT NOT NULL,
    value TEXT NOT NULL,

    archive BIGINT NOT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE TABLE Proxy (
    id BIGSERIAL PRIMARY KEY,
    archivefile BIGINT NOT NULL REFERENCES ArchiveFile(id) ON UPDATE CASCADE ON DELETE CASCADE,
    status ProxyStatus NOT NULL DEFAULT 'todo'
);

CREATE TABLE Backup (
    id BIGSERIAL PRIMARY KEY,

    timestamp TIMESTAMP(0) NOT NULL DEFAULT now(),

    nbMedia INTEGER NOT NULL DEFAULT 0 CHECK (nbMedia >= 0),
    nbArchive INTEGER NOT NULL DEFAULT 0 CHECK (nbArchive >= 0)
);

CREATE TABLE BackupVolume (
    id BIGSERIAL PRIMARY KEY,

    sequence INTEGER NOT NULL DEFAULT 0 CHECK (sequence >= 0),
    backup BIGINT NOT NULL REFERENCES Backup(id) ON DELETE CASCADE ON UPDATE CASCADE,
    media INTEGER NOT NULL REFERENCES Media(id) ON DELETE RESTRICT ON UPDATE CASCADE,
    mediaPosition INTEGER NOT NULL DEFAULT 0 CHECK (mediaPosition >= 0),
    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE RESTRICT
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
    status JobStatus NOT NULL DEFAULT 'scheduled',
    update TIMESTAMP(0) NOT NULL DEFAULT NOW(),

    archive BIGINT DEFAULT NULL REFERENCES Archive(id) ON UPDATE CASCADE ON DELETE CASCADE,
    backup BIGINT DEFAULT NULL REFERENCES Backup(id) ON UPDATE CASCADE ON DELETE CASCADE,
    media INTEGER NULL DEFAULT NULL REFERENCES Media(id) ON UPDATE CASCADE ON DELETE CASCADE,
    pool INTEGER NULL DEFAULT NULL REFERENCES Pool(id) ON UPDATE CASCADE ON DELETE SET NULL,

    host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE RESTRICT,
    login INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE CASCADE,

    metadata TEXT NOT NULL DEFAULT '',
    options hstore NOT NULL DEFAULT ''
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
    job BIGINT NOT NULL REFERENCES Job(id) ON UPDATE CASCADE ON DELETE CASCADE
);

CREATE TABLE Report (
    id BIGSERIAL PRIMARY KEY,
    timestamp TIMESTAMP(0) NOT NULL,

    archive BIGINT REFERENCES archive(id) ON UPDATE CASCADE ON DELETE CASCADE,
    job BIGINT NOT NULL REFERENCES job(id) ON UPDATE CASCADE ON DELETE CASCADE,

    data TEXT NOT NULL
);

CREATE TABLE Vtl (
	id SERIAL PRIMARY KEY,

	path VARCHAR(255) NOT NULL,
	prefix VARCHAR(255) NOT NULL,
	nbslots INTEGER NOT NULL CHECK (nbslots > 0),
	nbdrives INTEGER NOT NULL CHECK (nbdrives > 0),

	mediaformat INTEGER NOT NULL REFERENCES MediaFormat(id) ON UPDATE CASCADE ON DELETE CASCADE,
	host INTEGER NOT NULL REFERENCES Host(id) ON UPDATE CASCADE ON DELETE CASCADE,

	deleted BOOLEAN NOT NULL DEFAULT FALSE
);

-- Comments
COMMENT ON COLUMN Archive.starttime IS 'Start time of archive creation';
COMMENT ON COLUMN Archive.endtime IS 'End time of archive creation';
COMMENT ON COLUMN Archive.checktime IS 'Last time of checked time';

COMMENT ON COLUMN ArchiveVolume.starttime IS 'Start time of archive volume creation';
COMMENT ON COLUMN ArchiveVolume.endtime IS 'End time of archive volume creation';
COMMENT ON COLUMN ArchiveVolume.checktime IS 'Last time of checked time';

COMMENT ON TABLE Checksum IS 'Contains only checksum available';

COMMENT ON COLUMN DriveFormat.cleaningInterval IS 'Interval between two cleaning in days';

COMMENT ON TYPE JobStatus IS E'disable => disabled,\nerror => error while running,\nfinished => task finished,\npause => waiting for user action,\nrunning => running,\nscheduled => not yet started or completed,\nstopped => stopped by user,\nwaiting => waiting for a resource';

COMMENT ON COLUMN Media.label IS 'Contains an UUID';

COMMENT ON COLUMN MediaFormat.blockSize IS 'Default block size';
COMMENT ON COLUMN MediaFormat.supportPartition IS 'Is the media can be partitionned';
COMMENT ON COLUMN MediaFormat.supportMAM IS 'MAM: Medium Axiliary Memory, contains some usefull data';

