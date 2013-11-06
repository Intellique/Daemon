DROP TABLE IF EXISTS Archive CASCADE;
DROP TABLE IF EXISTS ArchiveFile CASCADE;
DROP TABLE IF EXISTS ArchiveFileToArchiveVolume CASCADE;
DROP TABLE IF EXISTS ArchiveFileToChecksumResult CASCADE;
DROP TABLE IF EXISTS ArchiveVolume CASCADE;
DROP TABLE IF EXISTS ArchiveVolumeToChecksumResult CASCADE;
DROP TABLE IF EXISTS Backup CASCADE;
DROP TABLE IF EXISTS BackupVolume CASCADE;
DROP TABLE IF EXISTS Changer CASCADE;
DROP TABLE IF EXISTS ChangerSlot CASCADE;
DROP TABLE IF EXISTS Checksum CASCADE;
DROP TABLE IF EXISTS ChecksumResult CASCADE;
DROP TABLE IF EXISTS Drive CASCADE;
DROP TABLE IF EXISTS DriveFormat CASCADE;
DROP TABLE IF EXISTS DriveFormatSupport CASCADE;
DROP TABLE IF EXISTS Host CASCADE;
DROP TABLE IF EXISTS Job CASCADE;
DROP TABLE IF EXISTS JobRecord CASCADE;
DROP TABLE IF EXISTS JobToSelectedFile CASCADE;
DROP TABLE IF EXISTS JobType CASCADE;
DROP TABLE IF EXISTS Log CASCADE;
DROP TABLE IF EXISTS Media CASCADE;
DROP TABLE IF EXISTS MediaFormat CASCADE;
DROP TABLE IF EXISTS MediaLabel CASCADE;
DROP TABLE IF EXISTS Metadata CASCADE;
DROP TABLE IF EXISTS Pool CASCADE;
DROP TABLE IF EXISTS PoolToChecksum CASCADE;
DROP TABLE IF EXISTS Proxy CASCADE;
DROP TABLE IF EXISTS Report CASCADE;
DROP TABLE IF EXISTS RestoreTo CASCADE;
DROP TABLE IF EXISTS SelectedFile CASCADE;
DROP TABLE IF EXISTS UserEvent CASCADE;
DROP TABLE IF EXISTS UserLog CASCADE;
DROP TABLE IF EXISTS Users CASCADE;
DROP TABLE IF EXISTS Vtl CASCADE;

DROP TYPE IF EXISTS AutoCheckMode;
DROP TYPE IF EXISTS ChangerSlotType;
DROP TYPE IF EXISTS ChangerStatus;
DROP TYPE IF EXISTS DriveStatus;
DROP TYPE IF EXISTS FileType;
DROP TYPE IF EXISTS JobStatus;
DROP TYPE IF EXISTS LogLevel;
DROP TYPE IF EXISTS LogType;
DROP TYPE IF EXISTS MediaFormatDataType;
DROP TYPE IF EXISTS MediaFormatMode;
DROP TYPE IF EXISTS MediaLocation;
DROP TYPE IF EXISTS MediaStatus;
DROP TYPE IF EXISTS MediaType;
DROP TYPE IF EXISTS ProxyStatus;
DROP TYPE IF EXISTS UnbreakableLevel;
