TRUNCATE TABLE Archive RESTART IDENTITY CASCADE;
TRUNCATE TABLE ArchiveFile RESTART IDENTITY CASCADE;
TRUNCATE TABLE ArchiveFileToArchiveVolume RESTART IDENTITY CASCADE;
TRUNCATE TABLE ArchiveFileToChecksumResult RESTART IDENTITY CASCADE;
TRUNCATE TABLE ArchiveFileToMeta RESTART IDENTITY CASCADE;
TRUNCATE TABLE ArchiveToChecksumResult RESTART IDENTITY CASCADE;
TRUNCATE TABLE ArchiveVolume RESTART IDENTITY CASCADE;
TRUNCATE TABLE ArchiveVolumeToChecksumResult RESTART IDENTITY CASCADE;
TRUNCATE TABLE Changer RESTART IDENTITY CASCADE;
TRUNCATE TABLE ChangerSlot RESTART IDENTITY CASCADE;
TRUNCATE TABLE Checksum RESTART IDENTITY CASCADE;
TRUNCATE TABLE ChecksumResult RESTART IDENTITY CASCADE;
TRUNCATE TABLE Drive RESTART IDENTITY CASCADE;
TRUNCATE TABLE DriveFormat RESTART IDENTITY CASCADE;
TRUNCATE TABLE DriveFormatSupport RESTART IDENTITY CASCADE;
TRUNCATE TABLE HomeDir RESTART IDENTITY CASCADE;
TRUNCATE TABLE Host RESTART IDENTITY CASCADE;
TRUNCATE TABLE Job RESTART IDENTITY CASCADE;
TRUNCATE TABLE JobRecord RESTART IDENTITY CASCADE;
TRUNCATE TABLE JobToChecksum RESTART IDENTITY CASCADE;
TRUNCATE TABLE JobToSelectedFile RESTART IDENTITY CASCADE;
TRUNCATE TABLE Meta RESTART IDENTITY CASCADE;
TRUNCATE TABLE MetaType RESTART IDENTITY CASCADE;
TRUNCATE TABLE Pool RESTART IDENTITY CASCADE;
TRUNCATE TABLE SelectedFile RESTART IDENTITY CASCADE;
TRUNCATE TABLE Tape RESTART IDENTITY CASCADE;
TRUNCATE TABLE TapeFormat RESTART IDENTITY CASCADE;
TRUNCATE TABLE Users RESTART IDENTITY CASCADE;
TRUNCATE TABLE UsersToHomeDirToHost RESTART IDENTITY CASCADE;
