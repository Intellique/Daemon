CREATE TABLE `MediaFormat` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `name` VARCHAR(64) NOT NULL UNIQUE,
    `dataType` ENUM('audio', 'cleaning', 'data', 'video') NOT NULL,
    `mode` ENUM('disk', 'linear', 'optical') NOT NULL,

    `maxLoadCount` INTEGER UNSIGNED NOT NULL,
    `maxReadCount` INTEGER UNSIGNED NOT NULL,
    `maxWriteCount` INTEGER UNSIGNED NOT NULL,
    `maxOpCount` INTEGER UNSIGNED NOT NULL,

    `lifespan` SMALLINT UNSIGNED NOT NULL COMMENT 'Interval in year',

    `capacity` BIGINT UNSIGNED NOT NULL,
    `blockSize` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `densityCode` SMALLINT UNSIGNED NOT NULL,

    `supportPartition` BOOLEAN NOT NULL DEFAULT FALSE,
    `supportMAM` BOOLEAN NOT NULL DEFAULT FALSE
) ENGINE=InnoDB;

CREATE TABLE `Pool` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `uuid` CHAR(32) NOT NULL UNIQUE,
    `name` VARCHAR(64) NOT NULL,

    `mediaFormat` INTEGER UNSIGNED NOT NULL REFERENCES MediaFormat(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,

    `autocheck` ENUM('quick mode', 'thorough mode', 'none') NOT NULL DEFAULT 'none',
    `lockcheck` BOOLEAN NOT NULL DEFAULT FALSE,

    `growable` BOOLEAN NOT NULL DEFAULT FALSE,
    `unbreakableLevel` ENUM('archive', 'file', 'none') NOT NULL DEFAULT 'none',
    `rewritable` BOOLEAN NOT NULL DEFAULT TRUE,

    `metadata` TEXT NOT NULL,
    `needproxy` BOOLEAN NOT NULL DEFAULT FALSE,

    `poolOriginal` INTEGER UNSIGNED REFERENCES Pool(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,

    `deleted` BOOLEAN NOT NULL DEFAULT FALSE
) ENGINE=InnoDB;

CREATE TABLE `Media` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `uuid` CHAR(32) NOT NULL UNIQUE,
    `label` VARCHAR(64) NOT NULL,
    `mediumSerialNumber` VARCHAR(36) UNIQUE,
    `name` VARCHAR(255) NOT NULL,

    `status` ENUM('erasable', 'error', 'foreign', 'in use', 'locked', 'needs replacement', 'new', 'pooled', 'unknown') NOT NULL,
    `location` ENUM('offline', 'online', 'in drive') NOT NULL,

    `firstUsed` TIMESTAMP NOT NULL,
    `useBefore` TIMESTAMP NOT NULL,

    `loadCount` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `readCount` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `writeCount` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `operationCount` INTEGER UNSIGNED NOT NULL DEFAULT 0,

    `nbFiles` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `blockSize` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `freeBlock` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `totalBlock` INTEGER UNSIGNED NOT NULL DEFAULT 0,

    `hasPartition` BOOLEAN NOT NULL DEFAULT FALSE,

    `locked` BOOLEAN NOT NULL DEFAULT FALSE,
    `type` ENUM('cleaning', 'readonly', 'rewritable') DEFAULT 'rewritable',
    `mediaFormat` INTEGER UNSIGNED NOT NULL REFERENCES MediaFormat(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    `pool` INTEGER UNSIGNED REFERENCES Pool(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,

    CHECK (tape_firstUsed < tape_useBefore)
) ENGINE=InnoDB;

CREATE TABLE `MediaLabel` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `name` TEXT NOT NULL,
    `media` INTEGER UNSIGNED NOT NULL REFERENCES Media(`id`) ON UPDATE CASCADE ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `DriveFormat` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `name` VARCHAR(64) NOT NULL UNIQUE,
    `densityCode` SMALLINT UNSIGNED NOT NULL,
    `mode` ENUM('disk', 'linear', 'optical') NOT NULL,

    `cleaningInterval` INTEGER UNSIGNED NOT NULL COMMENT 'interval in weeks'
) ENGINE=InnoDB;

CREATE TABLE `DriveFormatSupport` (
    `driveFormat` INTEGER UNSIGNED NOT NULL REFERENCES DriveFormat(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `mediaFormat` INTEGER UNSIGNED NOT NULL REFERENCES MediaFormat(`id`) ON UPDATE CASCADE ON DELETE CASCADE,

    `read` BOOLEAN NOT NULL DEFAULT TRUE,
    `write` BOOLEAN NOT NULL DEFAULT TRUE,

    PRIMARY KEY (`driveFormat`, `mediaFormat`)
) ENGINE=InnoDB;

CREATE TABLE `Host` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `uuid` CHAR(32) NOT NULL UNIQUE,

    `name` VARCHAR(255) NOT NULL,
    `domaine` VARCHAR(255) NULL,

    `description` TEXT,

    UNIQUE (`name`, `domaine`)
) ENGINE=InnoDB;

CREATE TABLE `Changer` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `device` VARCHAR(64),

    `model` VARCHAR(64) NOT NULL,
    `vendor` VARCHAR(64) NOT NULL,
    `firmwareRev` VARCHAR(64) NOT NULL,
    `serialNumber` VARCHAR(64) NOT NULL,
    `wwn` VARCHAR(64),

    `barcode` BOOLEAN NOT NULL,
    `status` ENUM('error', 'exporting', 'idle', 'importing', 'loading', 'unknown', 'unloading') NOT NULL,
    `enable` BOOLEAN NOT NULL DEFAULT TRUE,

    `host` INTEGER UNSIGNED NOT NULL REFERENCES Host(`id`) ON UPDATE CASCADE ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `Drive` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `device` VARCHAR(64),
    `scsiDevice` VARCHAR(64),

    `model` VARCHAR(64) NOT NULL,
    `vendor` VARCHAR(64) NOT NULL,
    `firmwareRev` VARCHAR(64) NOT NULL,
    `serialNumber` VARCHAR(64) NOT NULL,

    `status` ENUM('cleaning', 'empty idle', 'erasing', 'error', 'loaded idle', 'loading', 'positionning', 'reading', 'rewinding', 'unknown', 'unloading', 'writing') NOT NULL,
    `operationDuration` FLOAT(3) NOT NULL DEFAULT 0 CHECK (operationDuration >= 0),
    `lastClean` TIMESTAMP,
    `enable` BOOLEAN NOT NULL DEFAULT TRUE,

    `changer` INTEGER UNSIGNED NOT NULL REFERENCES Changer(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `driveFormat` INTEGER UNSIGNED NOT NULL REFERENCES DriveFormat(`id`) ON UPDATE CASCADE ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `ChangerSlot` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `index` INTEGER UNSIGNED NOT NULL,
    `changer` INTEGER UNSIGNED NOT NULL REFERENCES Changer(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `drive` INTEGER UNIQUE REFERENCES Drive(`id`) ON UPDATE CASCADE ON DELETE SET NULL,
    `media` INTEGER REFERENCES Media(`id`) ON UPDATE CASCADE ON DELETE SET NULL,
    `type` ENUM('drive', 'import / export', 'storage') NOT NULL DEFAULT 'storage',
    `enable` BOOLEAN NOT NULL DEFAULT TRUE,

    CONSTRAINT unique_slot UNIQUE (`index`, `changer`)
) ENGINE=InnoDB;

CREATE TABLE `SelectedFile` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `path` TEXT NOT NULL
) ENGINE=InnoDB;

CREATE TABLE `Users` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `login` VARCHAR(255) NOT NULL UNIQUE,
    `password` VARCHAR(255) NOT NULL,
    `salt` CHAR(16) NOT NULL,

    `fullname` VARCHAR(255),
    `email` VARCHAR(255) NOT NULL,
    `homeDirectory` TEXT NOT NULL,

    `isAdmin` BOOLEAN NOT NULL DEFAULT FALSE,
    `canArchive` BOOLEAN NOT NULL DEFAULT FALSE,
    `canRestore` BOOLEAN NOT NULL DEFAULT FALSE,

    `disabled` BOOLEAN NOT NULL DEFAULT FALSE,

    `pool` INTEGER NOT NULL REFERENCES Pool(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    `meta` TEXT NOT NULL COMMENT 'json encoded hashtable'
) ENGINE=InnoDB;

CREATE TABLE `UserEvent` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `event` VARCHAR(255) NOT NULL UNIQUE -- Type cannot be Text => see MySQL error 1170
) ENGINE=InnoDB;

CREATE TABLE `UserLog` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `login` INTEGER UNSIGNED NOT NULL REFERENCES Users(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `timestamp` TIMESTAMP NOT NULL DEFAULT NOW(),
    `event` INTEGER UNSIGNED NOT NULL REFERENCES UserEvent(`id`) ON UPDATE CASCADE ON DELETE RESTRICT
) ENGINE=InnoDB;

CREATE TABLE `Archive` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `uuid` CHAR(32) NOT NULL UNIQUE,
    `name` VARCHAR(255) NOT NULL,

    `starttime` TIMESTAMP COMMENT 'Start time of archive creation',
    `endtime` TIMESTAMP COMMENT 'End time of archive creation',

    `checksumok` BOOLEAN NOT NULL DEFAULT FALSE,
    `checktime` TIMESTAMP,

    `creator` INTEGER NOT NULL REFERENCES Users(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    `owner` INTEGER NOT NULL REFERENCES Users(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,

    `copyOf` BIGINT REFERENCES Archive(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,

    `deleted` BOOLEAN NOT NULL DEFAULT FALSE,

    CONSTRAINT `archive_id` CHECK (id != copyOf),
    CONSTRAINT `archive_time` CHECK (archive_ctime <= archive_endtime)
) ENGINE=InnoDB;

CREATE TABLE `ArchiveFile` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `name` TEXT NOT NULL,
    `type` ENUM('block device', 'character device', 'directory', 'fifo', 'regular file', 'socket', 'symbolic link') NOT NULL,
    `mimeType` VARCHAR(64) NOT NULL,

    `ownerId` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `owner` VARCHAR(64) NOT NULL,
    `groupId` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `groups` VARCHAR(64) NOT NULL,

    `perm` SMALLINT UNSIGNED NOT NULL,

    `ctime` TIMESTAMP NOT NULL,
    `mtime` TIMESTAMP NOT NULL,

    `size` BIGINT UNSIGNED NOT NULL,

    `parent` BIGINT UNSIGNED NOT NULL REFERENCES SelectedFile(`id`) ON UPDATE CASCADE ON DELETE RESTRICT
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `JobType` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `name` VARCHAR(255) NOT NULL UNIQUE
) ENGINE=InnoDB;

CREATE TABLE `Backup` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `timestamp` TIMESTAMP NOT NULL DEFAULT NOW(),

    `nbMedia` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `nbArchive` INTEGER UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE `Job` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `name` VARCHAR(255) NOT NULL,
    `type` INTEGER UNSIGNED REFERENCES JobType(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,

    `nextStart` TIMESTAMP NOT NULL DEFAULT NOW(),
    `interval` INTEGER UNSIGNED COMMENT 'interval in minutes',
    `repetition` INTEGER NOT NULL DEFAULT 1,
    `done` FLOAT NOT NULL DEFAULT 0,
    `status` ENUM('disable', 'error', 'finished', 'pause', 'running', 'scheduled', 'stopped', 'unknown', 'waiting') NOT NULL DEFAULT 'scheduled',
    `update` TIMESTAMP NOT NULL,

    `archive` BIGINT UNSIGNED REFERENCES Archive(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `backup` BIGINT UNSIGNED REFERENCES Backup(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `media` INTEGER UNSIGNED REFERENCES Media(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `pool` INTEGER UNSIGNED REFERENCES Pool(`id`) ON UPDATE CASCADE ON DELETE SET NULL,

    `host` INTEGER UNSIGNED NOT NULL REFERENCES Host(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    `login` INTEGER UNSIGNED NOT NULL REFERENCES Users(`id`) ON UPDATE CASCADE ON DELETE CASCADE,

    `metadata` TEXT NOT NULL,
    `options` TEXT NOT NULL
) ENGINE=InnoDB;

CREATE TABLE `JobRun` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `job` BIGINT UNSIGNED NOT NULL REFERENCES Job(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `numRun` INTEGER UNSIGNED NOT NULL DEFAULT 1 CHECK (numRun > 0),

    `starttime` TIMESTAMP NOT NULL,
    `endtime` TIMESTAMP,

    `status` ENUM('disable', 'error', 'finished', 'pause', 'running', 'scheduled', 'stopped', 'unknown', 'waiting') NOT NULL DEFAULT 'running',
    `exitCode` INTEGER NOT NULL DEFAULT 0,
    `stoppedByUser` BOOLEAN NOT NULL DEFAULT FALSE,

    CONSTRAINT `jobRun_time` CHECK (archiveVolume_ctime <= archiveVolume_endtime)
) ENGINE=InnoDB;

CREATE TABLE `ArchiveVolume` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `sequence` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `size` BIGINT UNSIGNED NOT NULL DEFAULT 0,

    `starttime` TIMESTAMP NOT NULL,
    `endtime` TIMESTAMP NOT NULL,

    `checksumok` BOOLEAN NOT NULL DEFAULT FALSE,
    `checktime` TIMESTAMP,

    `archive` BIGINT UNSIGNED NOT NULL REFERENCES Archive(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `media` INTEGER UNSIGNED NOT NULL REFERENCES Media(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    `mediaPosition` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `jobRun` BIGINT REFERENCES JobRun(`id`) ON UPDATE CASCADE ON DELETE SET NULL,

    CONSTRAINT `archiveVolume_time` CHECK (archiveVolume_ctime <= archiveVolume_endtime)
) ENGINE=InnoDB;

CREATE TABLE `BackupVolume` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `sequence` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `backup` BIGINT UNSIGNED NOT NULL REFERENCES Backup(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `media` INTEGER UNSIGNED NOT NULL REFERENCES Media(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    `mediaPosition` INTEGER UNSIGNED NOT NULL DEFAULT 0,

    `jobRun` BIGINT REFERENCES JobRun(`id`) ON UPDATE CASCADE ON DELETE SET NULL
) ENGINE=InnoDB;

CREATE TABLE `ArchiveFileToArchiveVolume` (
    `archiveVolume` BIGINT UNSIGNED NOT NULL REFERENCES ArchiveVolume(`id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `archiveFile` BIGINT UNSIGNED NOT NULL REFERENCES ArchiveFile(`id`) ON DELETE CASCADE ON UPDATE CASCADE,

    `blockNumber` BIGINT UNSIGNED NOT NULL,
    `archivetime` TIMESTAMP NOT NULL,

    `checktime` TIMESTAMP,
    `checksumok` BOOLEAN NOT NULL DEFAULT FALSE,

    PRIMARY KEY (`archiveVolume`, `archiveFile`)
) ENGINE=InnoDB;

CREATE TABLE `Metadata` (
    `id` BIGINT NOT NULL,
    `type` ENUM('archive', 'archiveFile', 'archiveVolume', 'host', 'media', 'pool', 'report', 'users', 'vtl'),

    `key` TEXT NOT NULL,
    `value` TEXT NOT NULL,

    PRIMARY KEY (`id`, `type`)
) ENGINE=InnoDB;

CREATE TABLE `Proxy` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `archivefile` BIGINT UNSIGNED NOT NULL REFERENCES ArchiveFileToMeta(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `status` ENUM('todo', 'running', 'done', 'error') NOT NULL DEFAULT 'todo'
) ENGINE=InnoDB;

CREATE TABLE `Checksum` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `name` VARCHAR(64) NOT NULL UNIQUE,
    `deflt` BOOLEAN NOT NULL DEFAULT FALSE
) ENGINE=InnoDB;

CREATE TABLE `ChecksumResult` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `checksum` INTEGER UNSIGNED NOT NULL REFERENCES Checksum(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    `result` VARCHAR(255) NOT NULL,

    CONSTRAINT `ChecksumResult_checksum_invalid` UNIQUE (`checksum`, `result`)
) ENGINE=InnoDB;

CREATE TABLE `PoolToChecksum` (
    `pool` BIGINT UNSIGNED NOT NULL REFERENCES Pool(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `checksum` INTEGER UNSIGNED NOT NULL REFERENCES Checksum(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (`pool`, `checksum`)
) ENGINE=InnoDB;

CREATE TABLE `ArchiveFileToChecksumResult` (
    `archiveFile` BIGINT UNSIGNED REFERENCES ArchiveFile(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `checksumResult` BIGINT UNSIGNED REFERENCES ChecksumResult(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    PRIMARY KEY (`archiveFile`, `checksumResult`)
) ENGINE=InnoDB;

CREATE TABLE `ArchiveVolumeToChecksumResult` (
    `archiveVolume` BIGINT UNSIGNED REFERENCES ArchiveVolume(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `checksumResult` BIGINT UNSIGNED REFERENCES ChecksumResult(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    PRIMARY KEY (`archiveVolume`, `checksumResult`)
) ENGINE=InnoDB;

CREATE TABLE `JobRecord` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `jobRun` BIGINT REFERENCES JobRun(`id`) ON UPDATE CASCADE ON DELETE SET NULL,
    `status` ENUM('error', 'finished', 'pause', 'running', 'scheduled', 'stopped', 'unknown', 'waiting') NOT NULL,
    `timestamp` TIMESTAMP NOT NULL DEFAULT NOW(),
    `message` TEXT NOT NULL,
    `notif` ENUM('normal', 'important', 'read')
) ENGINE=InnoDB;

CREATE TABLE `JobToSelectedFile` (
    `job` BIGINT UNSIGNED NOT NULL REFERENCES Job(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `selectedFile` BIGINT UNSIGNED NOT NULL REFERENCES SelectedFile(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    PRIMARY KEY (`job`, `selectedFile`)
) ENGINE=InnoDB;

CREATE TABLE `Log` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `type` ENUM('changer', 'daemon', 'drive', 'job', 'plugin checksum', 'plugin db', 'plugin log', 'scheduler', 'ui', 'user message') NOT NULL,
    `level` ENUM('alert', 'critical', 'debug', 'emergency', 'error', 'info', 'notice', 'warning') NOT NULL,
    `time` TIMESTAMP NOT NULL,
    `message` TEXT NOT NULL,
    `host` INTEGER UNSIGNED NOT NULL REFERENCES Host(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `login` INTEGER UNSIGNED REFERENCES Users(`id`) ON UPDATE CASCADE ON DELETE SET NULL
) ENGINE=InnoDB;

CREATE TABLE `RestoreTo` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `path` VARCHAR(255) NOT NULL DEFAULT '/',
    `job` BIGINT NOT NULL REFERENCES Job(`id`) ON UPDATE CASCADE ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE `Report` (
    `id` BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `timestamp` TIMESTAMP NOT NULL DEFAULT NOW(),

    `archive` BIGINT UNSIGNED REFERENCES Archive(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `job` BIGINT UNSIGNED NOT NULL REFERENCES Job(`id`) ON UPDATE CASCADE ON DELETE CASCADE,

    `data` TEXT NOT NULL
) ENGINE=InnoDB;

CREATE TABLE `Script` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `path` VARCHAR(255) NOT NULL UNIQUE
) ENGINE=InnoDB;

CREATE TABLE `Scripts` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    `sequence` INTEGER UNSIGNED NOT NULL,
    `jobType` INTEGER UNSIGNED REFERENCES JobType(`id`) ON UPDATE CASCADE ON DELETE RESTRICT,
    `script` INTEGER NOT NULL REFERENCES Script(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `scriptType` ENUM('on error', 'pre job', 'post job') NOT NULL,
    `pool` INTEGER UNSIGNED REFERENCES Pool(`id`) ON UPDATE CASCADE ON DELETE CASCADE,

    UNIQUE (`sequence`, `jobType`, `script`, `scriptType`, `pool`)
) ENGINE=InnoDB;

CREATE TABLE `Vtl` (
    `id` INTEGER UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    `uuid` CHAR(32) NOT NULL UNIQUE,

    `path` VARCHAR(255) NOT NULL,
    `prefix` VARCHAR(255) NOT NULL,
    `nbslots` INTEGER UNSIGNED NOT NULL CHECK (nbslots > 0),
    `nbdrives` INTEGER UNSIGNED NOT NULL CHECK (nbdrives > 0),

    `mediaFormat` INTEGER UNSIGNED NOT NULL REFERENCES MediaFormat(`id`) ON UPDATE CASCADE ON DELETE CASCADE,
    `host` INTEGER UNSIGNED NOT NULL REFERENCES Host(`id`) ON UPDATE CASCADE ON DELETE CASCADE,

    `deleted` BOOLEAN NOT NULL DEFAULT FALSE,

    CHECK (nbslots >= nbdrives)
) ENGINE=InnoDB;

delimiter //

CREATE TRIGGER update_metadata_archive
AFTER UPDATE ON `Archive`
FOR EACH ROW
BEGIN
    IF OLD.id != NEW.id THEN
        UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = 'archive';
    END IF;
END; //

CREATE TRIGGER delete_metadata_archive
AFTER DELETE ON `Archive`
FOR EACH ROW
BEGIN
    DELETE FROM Metadata WHERE id = OLD.id And type = 'archive';
END; //

CREATE TRIGGER update_metadata_archivefile
AFTER UPDATE ON `ArchiveFile`
FOR EACH ROW
BEGIN
    IF OLD.id != NEW.id THEN
        UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = 'archiveFile';
    END IF;
END; //

CREATE TRIGGER delete_metadata_archivefile
AFTER DELETE ON `ArchiveFile`
FOR EACH ROW
BEGIN
    DELETE FROM Metadata WHERE id = OLD.id And type = 'archiveFile';
END; //

CREATE TRIGGER update_metadata_archivevolume
AFTER UPDATE ON `ArchiveVolume`
FOR EACH ROW
BEGIN
    IF OLD.id != NEW.id THEN
        UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = 'archiveVolume';
    END IF;
END; //

CREATE TRIGGER delete_metadata_archivevolume
AFTER DELETE ON `ArchiveVolume`
FOR EACH ROW
BEGIN
    DELETE FROM Metadata WHERE id = OLD.id And type = 'archiveVolume';
END; //

CREATE TRIGGER update_metadata_host
AFTER UPDATE ON `Host`
FOR EACH ROW
BEGIN
    IF OLD.id != NEW.id THEN
        UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = 'host';
    END IF;
END; //

CREATE TRIGGER delete_metadata_host
AFTER DELETE ON `Host`
FOR EACH ROW
BEGIN
    DELETE FROM Metadata WHERE id = OLD.id And type = 'host';
END; //

CREATE TRIGGER update_metadata_media
AFTER UPDATE ON `Media`
FOR EACH ROW
BEGIN
    IF OLD.id != NEW.id THEN
        UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = 'media';
    END IF;
END; //

CREATE TRIGGER delete_metadata_media
AFTER DELETE ON `Media`
FOR EACH ROW
BEGIN
    DELETE FROM Metadata WHERE id = OLD.id And type = 'media';
END; //

CREATE TRIGGER update_metadata_pool
AFTER UPDATE ON `Pool`
FOR EACH ROW
BEGIN
    IF OLD.id != NEW.id THEN
        UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = 'pool';
    END IF;
END; //

CREATE TRIGGER delete_metadata_pool
AFTER DELETE ON `Pool`
FOR EACH ROW
BEGIN
    DELETE FROM Metadata WHERE id = OLD.id And type = 'pool';
END; //

CREATE TRIGGER update_metadata_report
AFTER UPDATE ON `Report`
FOR EACH ROW
BEGIN
    IF OLD.id != NEW.id THEN
        UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = 'report';
    END IF;
END; //

CREATE TRIGGER delete_metadata_report
AFTER DELETE ON `Report`
FOR EACH ROW
BEGIN
    DELETE FROM Metadata WHERE id = OLD.id And type = 'report';
END; //

CREATE TRIGGER update_metadata_users
AFTER UPDATE ON `Users`
FOR EACH ROW
BEGIN
    IF OLD.id != NEW.id THEN
        UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = 'users';
    END IF;
END; //

CREATE TRIGGER delete_metadata_users
AFTER DELETE ON `Users`
FOR EACH ROW
BEGIN
    DELETE FROM Metadata WHERE id = OLD.id And type = 'users';
END; //

CREATE TRIGGER update_metadata_vtl
AFTER UPDATE ON `Vtl`
FOR EACH ROW
BEGIN
    IF OLD.id != NEW.id THEN
        UPDATE Metadata SET id = NEW.id WHERE id = OLD.id AND type = 'vtl';
    END IF;
END; //

CREATE TRIGGER delete_metadata_vtl
AFTER DELETE ON `Vtl`
FOR EACH ROW
BEGIN
    DELETE FROM Metadata WHERE id = OLD.id And type = 'vtl';
END; //

delimiter ;

