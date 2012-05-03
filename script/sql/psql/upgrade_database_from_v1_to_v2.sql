-- Update ArchiveFile
ALTER TABLE ArchiveFile ALTER name TYPE TEXT;
ALTER TABLE ArchiveFile ADD blockNumber INTEGER CHECK (blockNumber >= 0);

