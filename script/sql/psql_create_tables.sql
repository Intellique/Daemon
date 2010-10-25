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


CREATE TABLE Checksum (
	checksum_id SERIAL PRIMARY KEY,
	checksum_name VARCHAR(64) NOT NULL UNIQUE
);

CREATE TABLE Checksum_result (
	checksumResult_id SERIAL PRIMARY KEY,
	checksumResult_result VARCHAR(255) NOT NULL UNIQUE,
	checksum_id INTEGER REFERENCES Checksum(checksum_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Drive_format (
	driveFormat_id SERIAL PRIMARY KEY,
	driveFormat_name VARCHAR(64) NOT NULL UNIQUE,
	driveFormat_cleaningInterval INTEGER NOT NULL
);

CREATE TABLE Drive_format_support (
	driveFormatSupport_format INTEGER REFERENCES Drive_format(driveFormat_id) ON DELETE CASCADE ON UPDATE CASCADE,
	driveFormatSupport_support INTEGER REFERENCES Drive_format(driveFormat_id) ON DELETE CASCADE ON UPDATE CASCADE,
	driveFormatSupport_read BOOLEAN NOT NULL,
	driveFormatSupport_write BOOLEAN NOT NULL,
	PRIMARY KEY (driveFormatSupport_format, driveFormatSupport_support)
);

CREATE TABLE File_set (
	fileSet_id SERIAL PRIMARY KEY,
	fileSet_alias VARCHAR(255) NOT NULL UNIQUE,
	fileSet_path VARCHAR(255) NOT NULL,
	fileSet_patternInclude VARCHAR(255) NOT NULL,
	fileSet_patternExclude VARCHAR(255) NOT NULL
);

CREATE TABLE Files (
	files_id SERIAL PRIMARY KEY,
	files_name VARCHAR(255) NOT NULL,
	files_type File_type NOT NULL,
	files_owner INTEGER NOT NULL DEFAULT 0,
	files_group INTEGER NOT NULL DEFAULT 0,
	files_perm SMALLINT NOT NULL,
	files_ctime TIMESTAMP NOT NULL,
	files_mtime TIMESTAMP NOT NULL,
	files_size BIGINT NOT NULL
);

CREATE TABLE Host (
	host_id SERIAL PRIMARY KEY,
	host_name VARCHAR(255) NOT NULL UNIQUE
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
	drive_changerNum INTEGER NOT NULL,
	drive_operationDuration INTEGER NOT NULL DEFAULT 0,
	drive_lastClean TIMESTAMP,
	drive_model VARCHAR(64) NOT NULL,
	drive_vendor VARCHAR(64) NOT NULL,
	drive_firmwareRev VARCHAR(64) NOT NULL,
	changer_id INTEGER REFERENCES Changer(changer_id) ON DELETE CASCADE ON UPDATE CASCADE,
	driveFormat_id INTEGER REFERENCES Drive_format(driveFormat_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Tape_format (
	tapeFormat_id SERIAL PRIMARY KEY,
	tapeFormat_name VARCHAR(64) NOT NULL UNIQUE,
	tapeFormat_dataType Tape_format_dataType NOT NULL,
	tapeFormat_mode Tape_format_mode NOT NULL,
	tapeFormat_maxLoadCount INTEGER NOT NULL,
	tapeFormat_maxWriteCount INTEGER NOT NULL,
	tapeFormat_OpCount INTEGER NOT NULL,
	tapeFormat_lifespan INTEGER NOT NULL,
	tapeFormat_capacity INTEGER NOT NULL,
	tapeFormat_blockSize INTEGER NOT NULL DEFAULT 0,
	tapeFormat_supportPartition BOOLEAN NOT NULL
);

CREATE TABLE Pool (
	pool_id SERIAL PRIMARY KEY,
	pool_name VARCHAR(64) NOT NULL,
	pool_retention INTEGER NOT NULL,
	pool_retentionLimit TIMESTAMP,
	pool_autoRecycle BOOLEAN NOT NULL,
	tapeFormat_id INTEGER REFERENCES Tape_format(tapeFormat_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Tape (
	tape_id SERIAL PRIMARY KEY,
	tape_label CHAR(16) NOT NULL UNIQUE,
	tape_name VARCHAR(64) NOT NULL,
	tape_status Tape_status NOT NULL,
	tape_location Tape_location NOT NULL,
	tape_firstUsed TIMESTAMP,
	tape_useBefore TIMESTAMP,
	tape_loadCount INTEGER NOT NULL DEFAULT 0,
	tape_readCount INTEGER NOT NULL DEFAULT 0,
	tape_writeCount INTEGER NOT NULL DEFAULT 0,
	tape_endPos INTEGER NOT NULL DEFAULT 0,
	tape_hasPartition BOOLEAN NOT NULL DEFAULT FALSE,
	tapeFormat_id INTEGER REFERENCES Tape_format(tapeFormat_id) ON DELETE RESTRICT ON UPDATE CASCADE,
	pool_id INTEGER REFERENCES Pool(pool_id) ON DELETE RESTRICT ON UPDATE CASCADE
);

CREATE TABLE Changer_slot (
	changerSlot_id SERIAL PRIMARY KEY,
	changerSlot_index INTEGER NOT NULL,

	changer_id INTEGER REFERENCES Changer(changer_id) ON DELETE CASCADE ON UPDATE CASCADE,
	tape_id INTEGER REFERENCES Tape(tape_id) ON DELETE SET NULL ON UPDATE CASCADE
);

