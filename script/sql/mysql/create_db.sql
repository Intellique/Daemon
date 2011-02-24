CREATE TABLE `Archive` (
    `archive_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    `archive_name` VARCHAR(255) NOT NULL,
    `archive_ctime` TIMESTAMP COMMENT 'Start time of archive creation',
    `archive_endtime` TIMESTAMP COMMENT 'End time of archive creation',
    CHECK (archive_ctime <= archive_endtime)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `Checksum` (
    `checksum_id` SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    `checksum_name` VARCHAR(64) NOT NULL,
    UNIQUE (`checksum_name`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `ChecksumResult` (
    `checksumResult_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    `checksumResult_result` VARCHAR(255) NOT NULL,
    `checksum_id` SMALLINT NOT NULL REFERENCES `Checksum`(checksum_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    UNIQUE `checksumResult_result_key` (`checksumResult_result`,`checksum_id`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `ArchiveToChecksumResult` (
    `archive_id` BIGINT UNSIGNED NOT NULL REFERENCES `Archive`(archive_id) ON DELETE CASCADE ON UPDATE CASCADE,
    `checksumResult_id` BIGINT UNSIGNED NOT NULL REFERENCES `ChecksumResult`(checksumResult_id) ON DELETE RESTRICT ON UPDATE CASCADE,
    PRIMARY KEY (`archive_id`,`checksumResult_id`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `DriveFormat` (
    `driveFormat_id` INTEGER UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    `driveFormat_name` VARCHAR(64) NOT NULL,
    `driveFormat_cleaningInterval` INTEGER NOT NULL,
    UNIQUE `driveFormat_name` (`driveFormat_name`),
    CHECK (driveFormat_cleaningInterval > 0)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `ArchiveFile` (
    `archiveFile_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    `archiveFile_name` VARCHAR(255) NOT NULL,
    `archiveFile_type` ENUM('blockDevice', 'characterDevice', 'directory', 'fifo', 'regularFile', 'socket', 'symbolicLink') NOT NULL,
    `archiveFile_owner` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `archiveFile_group` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `archiveFile_perm` SMALLINT UNSIGNED NOT NULL,
    `archiveFile_ctime` TIMESTAMP NOT NULL,
    `archiveFile_mtime` TIMESTAMP NOT NULL,
    `archiveFile_size` BIGINT UNSIGNED NOT NULL,
    CHECK (archiveFile_size >= 0)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `MetaType` (
    `metaType_id` INTEGER UNSIGNED NOT NULL PRIMARY KEY,
    `metaType_name` TEXT NOT NULL,
    UNIQUE (`MetaType_name`(255))
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `Meta` (
    `meta_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    `metaType_id` INTEGER UNSIGNED NOT NULL REFERENCES MetaType(`metaType_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `meta_value` TEXT NOT NULL
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `ArchiveFileToMeta` (
    `archiveFile_id` BIGINT UNSIGNED NOT NULL REFERENCES ArchiveFile(`archiveFile_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `meta_id` BIGINT UNSIGNED NOT NULL REFERENCES Meta(`meta_id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `ArchiveFileToChecksumResult` (
    `archiveFile_id` BIGINT UNSIGNED NOT NULL REFERENCES ArchiveFile(`archiveFile_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `checksumResult_id` BIGINT UNSIGNED NOT NULL REFERENCES ChecksumResult(`checksumResult_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
    PRIMARY KEY (`archiveFile_id`, `checksumResult_id`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `HomeDir` (
    `homeDir_id` INTEGER UNSIGNED NOT NULL PRIMARY KEY,
    `homeDir_path` VARCHAR(255) NOT NULL
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `Host` (
    `host_id` INTEGER UNSIGNED NOT NULL PRIMARY KEY,
    `host_name` VARCHAR(255) NOT NULL,
    `host_description` TEXT,
    UNIQUE (`host_name`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `Users` (
    `user_id` INTEGER UNSIGNED NOT NULL PRIMARY KEY,
    `user_login` VARCHAR(255) NOT NULL,
    `user_password` VARCHAR(255) NOT NULL,
    `user_fullname` VARCHAR(255),
    `user_email` VARCHAR(255) NOT NULL,
    `user_isAdmin` BOOLEAN NOT NULL DEFAULT FALSE,
    UNIQUE (`user_login`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `UsersToHomeDirToHost` (
    `user_id` INTEGER UNSIGNED NOT NULL REFERENCES Users(`user_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `homeDir_id` INTEGER UNSIGNED NOT NULL REFERENCES HomeDir(`homeDir_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `host_id` INTEGER UNSIGNED NOT NULL REFERENCES Host(`host_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (`user_id`, `homeDir_id`, `host_id`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `Job` (
    `job_id` BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `job_name` VARCHAR(255) NOT NULL,
    `job_type` ENUM('diffSave', 'dummy', 'incSave', 'integrityCheck', 'list', 'restore', 'save', 'verify') NOT NULL,
    `job_status` ENUM('disable', 'error', 'idle', 'pause', 'running') NOT NULL,
    `job_start` TIMESTAMP NOT NULL,
    `job_interval` INTEGER,
    `job_repetition` INTEGER NOT NULL DEFAULT 1,
    `job_update` TIMESTAMP NOT NULL,
    `archive_id` BIGINT UNSIGNED REFERENCES Archive(`archive_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `user_id` INTEGER UNSIGNED NOT NULL REFERENCES Users(`user_id`) ON DELETE CASCADE ON UPDATE CASCADE,
     CHECK (job_interval > 0),
     CHECK (job_repetition >= 0)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `JobToChecksum` (
    `job_id` BIGINT NOT NULL REFERENCES Job(`job_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `checksum_id` INTEGER NOT NULL REFERENCES Checksum(`checksum_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (`job_id`, `checksum_id`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `JobRecord` (
    `jobRecord_id` BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `jobRecord_status` ENUM('disable', 'error', 'idle', 'pause', 'running') NOT NULL,
    `jobRecord_numRun` INTEGER UNSIGNED NOT NULL DEFAULT 1 CHECK (jobRecord_numrun > 0),
    `jobRecord_started` TIMESTAMP NOT NULL,
    `jobRecord_ended` TIMESTAMP,
    `jobRecord_message` TEXT,
    `job_id` BIGINT NOT NULL REFERENCES Job(`job_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `host_id` INTEGER NOT NULL REFERENCES Host(`host_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
    CHECK (jobRecord_status != 'disable'),
    CHECK (jobRecord_started <= jobRecord_ended)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `Changer` (
    `changer_id` INTEGER UNSIGNED NOT NULL PRIMARY KEY,
    `changer_device` VARCHAR(64) NOT NULL,
    `changer_status` ENUM('error', 'exporting', 'idle', 'importing', 'loading', 'unknown', 'unloading') NOT NULL,
    `changer_barcode` BOOLEAN NOT NULL,
    `changer_isVirtual` BOOLEAN NOT NULL,
    `changer_model` VARCHAR(64) NOT NULL,
    `changer_vendor` VARCHAR(64) NOT NULL,
    `changer_firmwareRev` VARCHAR(64) NOT NULL,
    `changer_update` TIMESTAMP NOT NULL,
    `host_id` INTEGER NOT NULL REFERENCES Host(`host_id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `Drive` (
    `drive_id` INTEGER UNSIGNED NOT NULL PRIMARY KEY,
    `drive_name` VARCHAR(64) NOT NULL,
    `drive_device` VARCHAR(64) NOT NULL,
    `drive_status` ENUM('cleaning', 'empty-idle', 'erasing', 'error', 'loaded-idle', 'loading', 'positionning', 'reading', 'unloading', 'writing') NOT NULL,
    `drive_changerNum` INTEGER UNSIGNED NOT NULL,
    `drive_operationDuration` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `drive_lastClean` TIMESTAMP NOT NULL,
    `drive_model` VARCHAR(64) NOT NULL,
    `drive_vendor` VARCHAR(64) NOT NULL,
    `drive_firmwareRev` VARCHAR(64) NOT NULL,
    `changer_id` INTEGER NOT NULL REFERENCES Changer(`changer_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `driveFormat_id` INTEGER NOT NULL REFERENCES DriveFormat(`driveFormat_id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `SelectedFile` (
    `selectedFile_id` BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `selectedFile_path` VARCHAR(255) NOT NULL,
    `selectedFile_recursive` BOOLEAN NOT NULL DEFAULT FALSE
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `JobToSelectedFile` (
    `job_id` BIGINT UNSIGNED NOT NULL REFERENCES Job(`job_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `selectedFile_id` BIGINT UNSIGNED NOT NULL REFERENCES SelectedFile(`selectedFile_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
    PRIMARY KEY (`job_id`, `selectedFile_id`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `TapeFormat` (
    `tapeFormat_id` INTEGER UNSIGNED NOT NULL PRIMARY KEY,
    `tapeFormat_name` VARCHAR(64) NOT NULL,
    `tapeFormat_dataType` ENUM('audio', 'cleaning', 'data', 'video') NOT NULL,
    `tapeFormat_mode` ENUM('disk', 'linear', 'optical') NOT NULL,
    `tapeFormat_maxLoadCount` INTEGER UNSIGNED NOT NULL,
    `tapeFormat_maxReadCount` INTEGER UNSIGNED NOT NULL,
    `tapeFormat_maxWriteCount` INTEGER UNSIGNED NOT NULL,
    `tapeFormat_maxOpCount` INTEGER UNSIGNED NOT NULL,
    `tapeFormat_lifespan` INTEGER UNSIGNED NOT NULL,
    `tapeFormat_capacity` BIGINT UNSIGNED NOT NULL,
    `tapeFormat_blockSize` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `tapeFormat_supportPartition` BOOLEAN NOT NULL,
    UNIQUE (`tapeFormat_name`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `DriveFormatSupport` (
    `driveFormatSupport_format` INTEGER UNSIGNED NOT NULL REFERENCES DriveFormat(`driveFormat_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `tapeFormat_id` INTEGER UNSIGNED NOT NULL REFERENCES TapeFormat(`tapeFormat_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `driveFormatSupport_read` BOOLEAN NOT NULL,
    `driveFormatSupport_write` BOOLEAN NOT NULL,
    PRIMARY KEY (`driveFormatSupport_format`, `tapeFormat_id`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `Pool` (
    `pool_id` INTEGER UNSIGNED NOT NULL PRIMARY KEY,
    `pool_name` VARCHAR(64) NOT NULL,
    `pool_retention` INTEGER UNSIGNED NOT NULL,
    `pool_retentionLimit` TIMESTAMP,
    `pool_autoRecycle` BOOLEAN NOT NULL,
    `tapeFormat_id` INTEGER NOT NULL REFERENCES TapeFormat(`tapeFormat_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
     CHECK (pool_retention > 0)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `Tape` (
    `tape_id` BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `tape_label` VARCHAR(36) NOT NULL UNIQUE,
    `tape_name` VARCHAR(64) NOT NULL,
    `tape_status` ENUM('erasable', 'error', 'foreign', 'in_use', 'locked', 'needs_replacement', 'new', 'pooled') NOT NULL,
    `tape_location` ENUM('offline', 'online') NOT NULL,
    `tape_firstUsed` TIMESTAMP,
    `tape_useBefore` TIMESTAMP,
    `tape_loadCount` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `tape_readCount` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `tape_writeCount` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `tape_endPos` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `tape_nbFiles` INTEGER UNSIGNED NOT NULL DEFAULT 0,
    `tape_hasPartition` BOOLEAN NOT NULL DEFAULT FALSE,
    `tapeFormat_id` INTEGER UNSIGNED NOT NULL REFERENCES TapeFormat(`tapeFormat_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
    `pool_id` INTEGER UNSIGNED REFERENCES Pool(`pool_id`) ON DELETE RESTRICT ON UPDATE CASCADE,
    CHECK (tape_firstUsed < tape_useBefore),
    CHECK (tape_loadCount >= 0),
    CHECK (tape_readCount >= 0),
    CHECK (tape_writeCount >= 0),
    CHECK (tape_endPos >= 0),
    CHECK (tape_nbFiles >= 0)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `ArchiveVolume` (
    `archiveVolume_id` BIGINT UNSIGNED NOT NULL PRIMARY KEY,
    `archiveVolume_sequence` INTEGER UNSIGNED NOT NULL,
    `archiveVolume_size` BIGINT UNSIGNED NOT NULL,
    `archiveVolume_ctime` TIMESTAMP,
    `archiveVolume_endtime` TIMESTAMP,
    `archiveVolume_tapePosition` INTEGER UNSIGNED NOT NULL,
    `archive_id` BIGINT UNSIGNED NOT NULL REFERENCES Archive(`archive_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `tape_id` BIGINT UNSIGNED NOT NULL REFERENCES Tape(`tape_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    CHECK (archiveVolume_ctime <= archiveVolume_endtime)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `ArchiveVolumeToChecksumResult` (
    `archiveVolume_id` BIGINT UNSIGNED NOT NULL REFERENCES ArchiveVolume(`archiveVolume_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `checksumResult_id` BIGINT UNSIGNED NOT NULL REFERENCES ChecksumResult(`checksumResult_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (`archiveVolume_id`, `checksumResult_id`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `ArchiveFileToArchiveVolume` (
    `archiveVolume_id` BIGINT UNSIGNED NOT NULL REFERENCES ArchiveVolume(`archiveVolume_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `archiveFile_id` BIGINT UNSIGNED NOT NULL REFERENCES ArchiveFile(`archiveFile_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    PRIMARY KEY (`archiveVolume_id`, `archiveFile_id`)
) ENGINE=InnoDB CHARACTER SET=utf8;

CREATE TABLE `ChangerSlot` (
    `changerSlot_id` INTEGER UNSIGNED NOT NULL PRIMARY KEY,
    `changerSlot_index` INTEGER UNSIGNED NOT NULL,

    `changer_id` INTEGER UNSIGNED NOT NULL REFERENCES Changer(`changer_id`) ON DELETE CASCADE ON UPDATE CASCADE,
    `tape_id` BIGINT UNSIGNED REFERENCES Tape(`tape_id`) ON DELETE SET NULL ON UPDATE CASCADE
) ENGINE=InnoDB CHARACTER SET=utf8;

