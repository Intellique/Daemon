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
CREATE TABLE Archive (
    archive_id BIGSERIAL PRIMARY KEY,
    archive_name VARCHAR(255) NOT NULL,
    archive_ctime TIMESTAMP,
    archive_endtime TIMESTAMP,
    CONSTRAINT archive_time CHECK (archive_ctime <= archive_endtime)
);

CREATE TABLE Checksum (
    checksum_id SERIAL PRIMARY KEY,
    checksum_name VARCHAR(64) NOT NULL UNIQUE
);

CREATE TABLE ChecksumResult (
    checksumResult_id BIGSERIAL PRIMARY KEY,
    checksumResult_result VARCHAR(255) NOT NULL,
    checksum_id INTEGER NOT NULL REFERENCES Checksum(checksum_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CONSTRAINT checksumResult_result_key UNIQUE (checksumResult_result, checksum_id)
);

CREATE TABLE ArchiveToChecksumResult (
    archive_id BIGINT NOT NULL REFERENCES Archive(archive_id) ON DELETE CASCADE ON UPDATE CASCADE,
    checksumResult_id BIGINT NOT NULL REFERENCES ChecksumResult(checksumResult_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    PRIMARY KEY (archive_id, checksumResult_id)
);

CREATE TABLE DriveFormat (
    driveFormat_id SERIAL PRIMARY KEY,
    driveFormat_name VARCHAR(64) NOT NULL UNIQUE,
    driveFormat_cleaningInterval INTEGER NOT NULL CHECK (driveFormat_cleaningInterval > 0)
);

CREATE TABLE ArchiveFile (
    archiveFile_id BIGSERIAL PRIMARY KEY,
    archiveFile_name VARCHAR(255) NOT NULL,
    archiveFile_type FileType NOT NULL,
    archiveFile_owner SMALLINT NOT NULL DEFAULT 0,
    archiveFile_group SMALLINT NOT NULL DEFAULT 0,
    archiveFile_perm SMALLINT NOT NULL,
    archiveFile_ctime TIMESTAMP NOT NULL,
    archiveFile_mtime TIMESTAMP NOT NULL,
    archiveFile_size BIGINT NOT NULL CHECK (archiveFile_size >= 0)
);

CREATE TABLE MetaType (
	metaType_id SERIAL PRIMARY KEY,
	metaType_name TEXT NOT NULL UNIQUE
);

CREATE TABLE Meta (
	meta_id BIGSERIAL PRIMARY KEY,
	metaType_id INTEGER NOT NULL REFERENCES MetaType(metaType_id) ON DELETE CASCADE ON UPDATE CASCADE,
	meta_value TEXT NOT NULL
);

CREATE TABLE ArchiveFileToMeta (
	archiveFile_id BIGINT NOT NULL REFERENCES ArchiveFile(archiveFile_id) ON DELETE CASCADE ON UPDATE CASCADE,
	meta_id BIGINT NOT NULL REFERENCES Meta(meta_id) ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE ArchiveFileToChecksumResult (
    archiveFile_id BIGINT NOT NULL REFERENCES ArchiveFile(archiveFile_id) ON DELETE CASCADE ON UPDATE CASCADE,
    checksumResult_id BIGINT NOT NULL REFERENCES ChecksumResult(checksumResult_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    PRIMARY KEY (archiveFile_id, checksumResult_id)
);

CREATE TABLE HomeDir (
    homeDir_id SERIAL PRIMARY KEY,
    homeDir_path VARCHAR(255) NOT NULL
);

CREATE TABLE Host (
    host_id SERIAL PRIMARY KEY,
    host_name VARCHAR(255) NOT NULL UNIQUE,
    host_description TEXT
);

CREATE TABLE Users (
    user_id SERIAL PRIMARY KEY,
    user_login VARCHAR(255) NOT NULL UNIQUE,
    user_password VARCHAR(255) NOT NULL,
    user_fullname VARCHAR(255),
    user_email VARCHAR(255) NOT NULL,
    user_isAdmin BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE UsersToHomeDirToHost (
    user_id INTEGER NOT NULL REFERENCES Users(user_id) ON DELETE CASCADE ON UPDATE CASCADE,
    homeDir_id INTEGER NOT NULL REFERENCES HomeDir(homeDir_id) ON DELETE CASCADE ON UPDATE CASCADE,
    host_id INTEGER NOT NULL REFERENCES Host(host_id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (user_id, homeDir_id, host_id)
);

CREATE TABLE Job (
    job_id BIGSERIAL PRIMARY KEY,
    job_name VARCHAR(255) NOT NULL,
    job_type JobType NOT NULL,
    job_status JobStatus NOT NULL,
    job_start TIMESTAMP NOT NULL,
    job_interval INTEGER CHECK (job_interval > 0),
    job_repetition INTEGER NOT NULL DEFAULT 1 CHECK (job_repetition >= 0),
    job_update TIMESTAMP NOT NULL,
    archive_id INTEGER REFERENCES Archive(archive_id) ON DELETE CASCADE ON UPDATE CASCADE,
    user_id INTEGER NOT NULL REFERENCES Users(user_id) ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE JobToChecksum (
    job_id INTEGER NOT NULL REFERENCES Job(job_id) ON DELETE CASCADE ON UPDATE CASCADE,
    checksum_id INTEGER NOT NULL REFERENCES Checksum(checksum_id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (job_id, checksum_id)
);

CREATE TABLE JobRecord (
    jobRecord_id SERIAL PRIMARY KEY,
    jobRecord_status JobStatus NOT NULL CHECK (jobRecord_status != 'disable'),
    jobRecord_numRun INTEGER NOT NULL DEFAULT 1 CHECK (jobRecord_numrun > 0),
    jobRecord_started TIMESTAMP NOT NULL,
    jobRecord_ended TIMESTAMP,
    jobRecord_message TEXT,
    job_id INTEGER NOT NULL REFERENCES Job(job_id) ON DELETE CASCADE ON UPDATE CASCADE,
    host_id INTEGER NOT NULL REFERENCES Host(host_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CHECK (jobRecord_started <= jobRecord_ended)
);

CREATE TABLE Changer (
    changer_id SERIAL PRIMARY KEY,
    changer_device VARCHAR(64) NOT NULL,
    changer_status ChangerStatus NOT NULL,
    changer_barcode BOOLEAN NOT NULL,
    changer_isVirtual BOOLEAN NOT NULL,
    changer_model VARCHAR(64) NOT NULL,
    changer_vendor VARCHAR(64) NOT NULL,
    changer_firmwareRev VARCHAR(64) NOT NULL,
    changer_update TIMESTAMP NOT NULL,
    host_id INTEGER NOT NULL REFERENCES Host(host_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Drive (
    drive_id SERIAL PRIMARY KEY,
    drive_name VARCHAR(64) NOT NULL,
    drive_device VARCHAR(64) NOT NULL,
    drive_status DriveStatus NOT NULL,
    drive_changerNum INTEGER NOT NULL CHECK (drive_changerNum >= 0),
    drive_operationDuration INTEGER NOT NULL DEFAULT 0 CHECK (drive_operationDuration >= 0),
    drive_lastClean TIMESTAMP NOT NULL,
    drive_model VARCHAR(64) NOT NULL,
    drive_vendor VARCHAR(64) NOT NULL,
    drive_firmwareRev VARCHAR(64) NOT NULL,
    changer_id INTEGER NOT NULL REFERENCES Changer(changer_id) ON DELETE CASCADE ON UPDATE CASCADE,
    driveFormat_id INTEGER NOT NULL REFERENCES DriveFormat(driveFormat_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE SelectedFile (
    selectedFile_id SERIAL PRIMARY KEY,
    selectedFile_path VARCHAR(255) NOT NULL,
    selectedFile_recursive BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE JobToSelectedFile (
    job_id INTEGER NOT NULL REFERENCES Job(job_id) ON DELETE CASCADE ON UPDATE CASCADE,
    selectedFile_id INTEGER NOT NULL REFERENCES SelectedFile(selectedFile_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    PRIMARY KEY (job_id, selectedFile_id)
);

CREATE TABLE TapeFormat (
    tapeFormat_id SERIAL PRIMARY KEY,
    tapeFormat_name VARCHAR(64) NOT NULL UNIQUE,
    tapeFormat_dataType TapeFormatDataType NOT NULL,
    tapeFormat_mode TapeFormatMode NOT NULL,
    tapeFormat_maxLoadCount INTEGER NOT NULL CHECK (tapeFormat_maxLoadCount > 0),
    tapeFormat_maxReadCount INTEGER NOT NULL CHECK (tapeFormat_maxReadCount > 0),
    tapeFormat_maxWriteCount INTEGER NOT NULL CHECK (tapeFormat_maxWriteCount > 0),
    tapeFormat_maxOpCount INTEGER NOT NULL CHECK (tapeFormat_maxOpCount > 0),
    tapeFormat_lifespan INTEGER NOT NULL CHECK (tapeFormat_lifespan > 0),
    tapeFormat_capacity BIGINT NOT NULL CHECK (tapeFormat_capacity > 0),
    tapeFormat_blockSize INTEGER NOT NULL DEFAULT 0 CHECK (tapeFormat_blockSize >= 0),
    tapeFormat_supportPartition BOOLEAN NOT NULL
);

CREATE TABLE DriveFormatSupport (
    driveFormatSupport_format INTEGER NOT NULL REFERENCES DriveFormat(driveFormat_id) ON DELETE CASCADE ON UPDATE CASCADE,
    tapeFormat_id INTEGER NOT NULL REFERENCES TapeFormat(tapeFormat_id) ON DELETE CASCADE ON UPDATE CASCADE,
    driveFormatSupport_read BOOLEAN NOT NULL,
    driveFormatSupport_write BOOLEAN NOT NULL,
    PRIMARY KEY (driveFormatSupport_format, tapeFormat_id)
);

CREATE TABLE Pool (
    pool_id SERIAL PRIMARY KEY,
    pool_name VARCHAR(64) NOT NULL,
    pool_retention INTEGER NOT NULL CHECK (pool_retention > 0),
    pool_retentionLimit TIMESTAMP,
    pool_autoRecycle BOOLEAN NOT NULL,
    tapeFormat_id INTEGER NOT NULL REFERENCES TapeFormat(tapeFormat_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Tape (
    tape_id SERIAL PRIMARY KEY,
    tape_label UUID NOT NULL UNIQUE,
    tape_name VARCHAR(64) NOT NULL,
    tape_status TapeStatus NOT NULL,
    tape_location TapeLocation NOT NULL,
    tape_firstUsed TIMESTAMP,
    tape_useBefore TIMESTAMP,
    tape_loadCount INTEGER NOT NULL DEFAULT 0 CHECK (tape_loadCount >= 0),
    tape_readCount INTEGER NOT NULL DEFAULT 0 CHECK (tape_readCount >= 0),
    tape_writeCount INTEGER NOT NULL DEFAULT 0 CHECK (tape_writeCount >= 0),
    tape_endPos INTEGER NOT NULL DEFAULT 0 CHECK (tape_endPos >= 0),
    tape_nbFiles INTEGER NOT NULL DEFAULT 0 CHECK (tape_nbFiles >= 0),
    tape_hasPartition BOOLEAN NOT NULL DEFAULT FALSE,
    tapeFormat_id INTEGER NOT NULL REFERENCES TapeFormat(tapeFormat_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    pool_id INTEGER REFERENCES Pool(pool_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    CHECK (tape_firstUsed < tape_useBefore)
);

CREATE TABLE ArchiveVolume (
    archiveVolume_id SERIAL PRIMARY KEY,
    archiveVolume_sequence INTEGER NOT NULL,
    archiveVolume_size BIGINT NOT NULL,
    archiveVolume_ctime TIMESTAMP,
    archiveVolume_endtime TIMESTAMP,
    archiveVolume_tapePosition INTEGER NOT NULL,
    archive_id INTEGER NOT NULL REFERENCES Archive(archive_id) ON DELETE CASCADE ON UPDATE CASCADE,
    tape_id INTEGER NOT NULL REFERENCES Tape(tape_id) ON DELETE CASCADE ON UPDATE CASCADE,
    CHECK (archiveVolume_ctime <= archiveVolume_endtime)
);

CREATE TABLE ArchiveVolumeToChecksumResult (
    archiveVolume_id INTEGER NOT NULL REFERENCES ArchiveVolume(archiveVolume_id) ON DELETE CASCADE ON UPDATE CASCADE,
    checksumResult_id INTEGER NOT NULL REFERENCES ChecksumResult(checksumResult_id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (archiveVolume_id, checksumResult_id)
);

CREATE TABLE ArchiveFileToArchiveVolume (
    archiveVolume_id INTEGER NOT NULL REFERENCES ArchiveVolume(archiveVolume_id) ON DELETE CASCADE ON UPDATE CASCADE,
    archiveFile_id INTEGER NOT NULL REFERENCES ArchiveFile(archiveFile_id) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (archiveVolume_id, archiveFile_id)
);

CREATE TABLE ChangerSlot (
    changerSlot_id SERIAL PRIMARY KEY,
    changerSlot_index INTEGER NOT NULL,

    changer_id INTEGER NOT NULL REFERENCES Changer(changer_id) ON DELETE CASCADE ON UPDATE CASCADE,
    tape_id INTEGER REFERENCES Tape(tape_id) ON DELETE SET NULL ON UPDATE CASCADE
);

-- Comments
COMMENT ON COLUMN Archive.archive_ctime IS 'Start time of archive creation';
COMMENT ON COLUMN Archive.archive_endtime IS 'End time of archive creation';

COMMENT ON TABLE Checksum IS 'Contains only checksum available';

COMMENT ON COLUMN DriveFormat.driveFormat_cleaningInterval IS 'Interval between two cleaning in days';

COMMENT ON COLUMN Tape.tape_label IS 'Contains an UUID';

