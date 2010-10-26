-- Types
CREATE TYPE Changer_status AS ENUM (
	'error',
	'exporting',
	'idle',
	'importing',
	'loading',
	'unknown',
	'unloading'
);

CREATE TYPE Drive_status AS ENUM (
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

CREATE TYPE File_type AS ENUM (
	'blockDevice',
	'characterDevice',
	'directory',
	'fifo',
	'regularFile',
	'socket',
	'symbolicLink'
);

CREATE TYPE Job_status AS ENUM (
	'disable',
	'error',
	'idle',
	'running'
);

CREATE TYPE Job_type AS ENUM (
	'diffSave',
	'dummy',
	'incSave',
	'integrityCheck',
	'list',
	'restore',
	'save',
	'verify'
);

CREATE TYPE Tape_format_dataType AS ENUM (
	'audio',
	'cleaning',
	'data',
	'video'
);

CREATE TYPE Tape_format_mode AS ENUM (
	'disk',
	'linear',
	'optical'
);

CREATE TYPE Tape_location AS ENUM (
	'offline',
	'online'
);

CREATE TYPE Tape_status AS ENUM (
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
	archive_id SERIAL PRIMARY KEY,
	archive_name VARCHAR(255) NOT NULL,
	archive_ctime TIMESTAMP,
	archive_endtime TIMESTAMP,
	CHECK (archive_ctime <= archive_endtime)
);

CREATE TABLE Checksum (
	checksum_id SERIAL PRIMARY KEY,
	checksum_name VARCHAR(64) NOT NULL UNIQUE
);

CREATE TABLE Checksum_result (
	checksumResult_id SERIAL PRIMARY KEY,
	checksumResult_result VARCHAR(255) NOT NULL,
	checksum_id INTEGER NOT NULL REFERENCES Checksum(checksum_id) ON DELETE RESTRICT ON UPDATE CASCADE,
	CONSTRAINT checksumResult_result_key UNIQUE (checksumResult_result, checksum_id)
);

CREATE TABLE Archive_to_checksum_result (
	archive_id INTEGER NOT NULL REFERENCES Archive(archive_id) ON DELETE CASCADE ON UPDATE CASCADE,
	checksumResult_id INTEGER NOT NULL REFERENCES Checksum_result(checksumResult_id) ON DELETE CASCADE ON UPDATE CASCADE,
	PRIMARY KEY (archive_id, checksumResult_id)
);

CREATE TABLE Drive_format (
	driveFormat_id SERIAL PRIMARY KEY,
	driveFormat_name VARCHAR(64) NOT NULL UNIQUE,
	driveFormat_cleaningInterval INTEGER NOT NULL CHECK (driveFormat_cleaningInterval > 0)
);

CREATE TABLE Drive_format_support (
	driveFormatSupport_format INTEGER NOT NULL REFERENCES Drive_format(driveFormat_id) ON DELETE CASCADE ON UPDATE CASCADE,
	driveFormatSupport_support INTEGER NOT NULL REFERENCES Drive_format(driveFormat_id) ON DELETE CASCADE ON UPDATE CASCADE,
	driveFormatSupport_read BOOLEAN NOT NULL,
	driveFormatSupport_write BOOLEAN NOT NULL,
	PRIMARY KEY (driveFormatSupport_format, driveFormatSupport_support)
);

CREATE TABLE File_set (
	fileSet_id SERIAL PRIMARY KEY,
	fileSet_alias VARCHAR(255) NOT NULL UNIQUE,
	fileSet_path VARCHAR(255) NOT NULL,
	fileSet_patternInclude VARCHAR(255),
	fileSet_patternExclude VARCHAR(255)
);

CREATE TABLE File (
	file_id SERIAL PRIMARY KEY,
	file_name VARCHAR(255) NOT NULL,
	file_type File_type NOT NULL,
	file_owner INTEGER NOT NULL DEFAULT 0,
	file_group INTEGER NOT NULL DEFAULT 0,
	file_perm SMALLINT NOT NULL,
	file_ctime TIMESTAMP NOT NULL,
	file_mtime TIMESTAMP NOT NULL,
	file_size BIGINT NOT NULL CHECK (file_size >= 0),
	CHECK (file_ctime <= file_mtime)
);

CREATE TABLE File_to_checksum_result (
	file_id INTEGER NOT NULL REFERENCES File(file_id) ON DELETE CASCADE ON UPDATE CASCADE,
	checksumResult_id INTEGER NOT NULL REFERENCES Checksum_result(checksumResult_id) ON DELETE CASCADE ON UPDATE CASCADE,
	PRIMARY KEY (file_id, checksumResult_id)
);

CREATE TABLE Host (
	host_id SERIAL PRIMARY KEY,
	host_name VARCHAR(255) NOT NULL UNIQUE
);

CREATE TABLE Job (
	job_id SERIAL PRIMARY KEY,
	job_name VARCHAR(255) NOT NULL,
	job_type Job_type NOT NULL DEFAULT 'dummy',
	job_status Job_status NOT NULL,
	job_start TIMESTAMP NOT NULL,
	job_interval INTEGER CHECK (job_interval > 0),
	job_repetition INTEGER CHECK (job_repetition > 0),
	archive_id INTEGER NOT NULL REFERENCES Archive(archive_id) ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE Job_record (
	jobRecord_id SERIAL PRIMARY KEY,
	jobRecord_status Job_status NOT NULL CHECK (jobRecord_status != 'disable'),
	jobRecord_numrun INTEGER NOT NULL DEFAULT 1 CHECK (jobRecord_numrun > 0),
	jobRecord_started TIMESTAMP NOT NULL,
	jobRecord_ended TIMESTAMP,
	jobRecord_message TEXT,
	job_id INTEGER NOT NULL REFERENCES Job(job_id) ON DELETE CASCADE ON UPDATE CASCADE,
	CHECK (jobRecord_started <= jobRecord_ended)
);

CREATE TABLE Changer (
	changer_id SERIAL PRIMARY KEY,
	changer_device VARCHAR(64) NOT NULL,
	changer_status Changer_status NOT NULL,
	changer_barcode BOOLEAN NOT NULL,
	changer_model VARCHAR(64) NOT NULL,
	changer_vendor VARCHAR(64) NOT NULL,
	changer_firmwareRev VARCHAR(64) NOT NULL,
	host_id INTEGER NOT NULL REFERENCES Host(host_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Drive (
	drive_id SERIAL PRIMARY KEY,
	drive_name VARCHAR(64) NOT NULL,
	drive_device VARCHAR(64) NOT NULL,
	drive_status Drive_status NOT NULL,
	drive_changerNum INTEGER NOT NULL CHECK (drive_changerNum >= 0),
	drive_operationDuration INTEGER NOT NULL DEFAULT 0 CHECK (drive_operationDuration >= 0),
	drive_lastClean TIMESTAMP,
	drive_model VARCHAR(64) NOT NULL,
	drive_vendor VARCHAR(64) NOT NULL,
	drive_firmwareRev VARCHAR(64) NOT NULL,
	changer_id INTEGER NOT NULL REFERENCES Changer(changer_id) ON DELETE CASCADE ON UPDATE CASCADE,
	driveFormat_id INTEGER NOT NULL REFERENCES Drive_format(driveFormat_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Tape_format (
	tapeFormat_id SERIAL PRIMARY KEY,
	tapeFormat_name VARCHAR(64) NOT NULL UNIQUE,
	tapeFormat_dataType Tape_format_dataType NOT NULL,
	tapeFormat_mode Tape_format_mode NOT NULL,
	tapeFormat_maxLoadCount INTEGER NOT NULL CHECK (tapeFormat_maxLoadCount > 0),
	tapeFormat_maxWriteCount INTEGER NOT NULL CHECK (tapeFormat_maxWriteCount > 0),
	tapeFormat_maxOpCount INTEGER NOT NULL CHECK (tapeFormat_maxOpCount > 0),
	tapeFormat_lifespan INTEGER NOT NULL CHECK (tapeFormat_lifespan > 0),
	tapeFormat_capacity BIGINT NOT NULL CHECK (tapeFormat_capacity > 0),
	tapeFormat_blockSize INTEGER NOT NULL DEFAULT 0 CHECK (tapeFormat_blockSize >= 0),
	tapeFormat_supportPartition BOOLEAN NOT NULL
);

CREATE TABLE Pool (
	pool_id SERIAL PRIMARY KEY,
	pool_name VARCHAR(64) NOT NULL,
	pool_retention INTEGER NOT NULL CHECK (pool_retention > 0),
	pool_retentionLimit TIMESTAMP,
	pool_autoRecycle BOOLEAN NOT NULL,
	tapeFormat_id INTEGER NOT NULL REFERENCES Tape_format(tapeFormat_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Tape (
	tape_id SERIAL PRIMARY KEY,
	tape_label CHAR(16) NOT NULL UNIQUE,
	tape_name VARCHAR(64) NOT NULL,
	tape_status Tape_status NOT NULL,
	tape_location Tape_location NOT NULL,
	tape_firstUsed TIMESTAMP,
	tape_useBefore TIMESTAMP,
	tape_loadCount INTEGER NOT NULL DEFAULT 0 CHECK (tape_loadCount >= 0),
	tape_readCount INTEGER NOT NULL DEFAULT 0 CHECK (tape_readCount >= 0),
	tape_writeCount INTEGER NOT NULL DEFAULT 0 CHECK (tape_writeCount >= 0),
	tape_endPos INTEGER NOT NULL DEFAULT 0 CHECK (tape_endPos >= 0),
	tape_hasPartition BOOLEAN NOT NULL DEFAULT FALSE,
	tapeFormat_id INTEGER NOT NULL REFERENCES Tape_format(tapeFormat_id) ON DELETE RESTRICT ON UPDATE CASCADE,
	pool_id INTEGER REFERENCES Pool(pool_id) ON DELETE RESTRICT ON UPDATE CASCADE,
	CHECK (tape_firstUsed < tape_useBefore)
);

CREATE TABLE Archive_volume (
	archiveVolume_id SERIAL PRIMARY KEY,
	archiveVolume_sequence INTEGER NOT NULL,
	archiveVolume_size BIGINT NOT NULL,
	archiveVolume_ctime TIMESTAMP,
	archiveVolume_endtime TIMESTAMP,
	archive_id INTEGER NOT NULL REFERENCES Archive(archive_id) ON DELETE CASCADE ON UPDATE CASCADE,
	tape_id INTEGER NOT NULL REFERENCES Tape(tape_id) ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE Archive_volume_to_checksum_result (
	archiveVolume_id INTEGER NOT NULL REFERENCES Archive_volume(archiveVolume_id) ON DELETE CASCADE ON UPDATE CASCADE,
	checksumResult_id INTEGER NOT NULL REFERENCES Checksum_result(checksumResult_id) ON DELETE CASCADE ON UPDATE CASCADE,
	PRIMARY KEY (archiveVolume_id, checksumResult_id)
);

CREATE TABLE File_to_archive_volume (
	archiveVolume_id INTEGER NOT NULL REFERENCES Archive_volume(archiveVolume_id) ON DELETE CASCADE ON UPDATE CASCADE,
	file_id INTEGER NOT NULL REFERENCES File(file_id) ON DELETE CASCADE ON UPDATE CASCADE,
	PRIMARY KEY (archiveVolume_id, file_id)
);

CREATE TABLE Changer_slot (
	changerSlot_id SERIAL PRIMARY KEY,
	changerSlot_index INTEGER NOT NULL,

	changer_id INTEGER NOT NULL REFERENCES Changer(changer_id) ON DELETE CASCADE ON UPDATE CASCADE,
	tape_id INTEGER REFERENCES Tape(tape_id) ON DELETE SET NULL ON UPDATE CASCADE
);

-- Comments
COMMENT ON COLUMN Archive.archive_ctime IS 'Start time of archive creation';
COMMENT ON COLUMN Archive.archive_endtime IS 'End time of archive creation';

COMMENT ON TABLE Checksum IS 'Contains only checksum available';

COMMENT ON COLUMN Drive_format.driveFormat_cleaningInterval IS 'Interval between two cleaning in days';

COMMENT ON COLUMN Tape.tape_label IS 'Contains an UUID';

