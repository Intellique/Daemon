-- Update ArchiveFile
ALTER TABLE ArchiveFile ALTER name TYPE TEXT;
ALTER TABLE ArchiveFile ADD blockNumber BIGINT CHECK (blockNumber >= 0);

