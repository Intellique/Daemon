INSERT INTO MediaFormat(name, dataType, mode, maxLoadCount, maxReadCount, maxWriteCount, maxOpCount, lifespan, capacity, blockSize, densityCode, supportPartition, supportMAM) VALUES
	('LTO-7', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P10Y', 5999999057920, 131072, 92, TRUE, TRUE),
	('LTO-6', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P10Y', 2620446998528, 131072, 90, TRUE, TRUE),
	('LTO-5', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P10Y', 1529931104256, 131072, 88, TRUE, TRUE),
	('LTO-4', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P8Y', 764965552128, 131072, 70, FALSE, TRUE),
	('LTO-3', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 382482776064, 131072, 68, FALSE, TRUE),
	('LTO-2', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 191241388032, 131072, 66, FALSE, TRUE),
	('LTO-CLEANING', 'data', 'linear', 75, 75, 75, 75, INTERVAL 'P6Y', 1, 0, 0, FALSE, FALSE);

INSERT INTO DriveFormat(name, densityCode, mode, cleaningInterval) VALUES
	('LTO-7', 92, 'linear', INTERVAL 'P1M'),
	('LTO-6', 90, 'linear', INTERVAL 'P1M'),
	('LTO-5', 88, 'linear', INTERVAL 'P1M'),
	('LTO-4', 70, 'linear', INTERVAL 'P1M'),
	('LTO-3', 68, 'linear', INTERVAL 'P1M'),
	('LTO-2', 66, 'linear', INTERVAL 'P1M');

INSERT INTO DriveFormatSupport(driveFormat, mediaFormat, read, write) VALUES
	(1, 1, TRUE, TRUE),
	(1, 2, TRUE, TRUE),
	(1, 3, TRUE, FALSE),
	(2, 2, TRUE, TRUE),
	(2, 3, TRUE, TRUE),
	(2, 4, TRUE, FALSE),
	(3, 3, TRUE, TRUE),
	(3, 4, TRUE, TRUE),
	(3, 5, TRUE, FALSE),
	(4, 4, TRUE, TRUE),
	(4, 5, TRUE, TRUE),
	(4, 6, TRUE, FALSE),
	(5, 5, TRUE, TRUE),
	(5, 6, TRUE, TRUE),
	(6, 6, TRUE, TRUE);

INSERT INTO Application(name) VALUES
    ('StoriqOne Changer'),
    ('StoriqOne Daemon'),
    ('StoriqOne Drive'),
    ('StoriqOne Job'),
    ('StoriqOne Logger');

INSERT INTO ArchiveFormat(name, readable, writable) VALUES
    ('Storiq One (TAR)', TRUE, TRUE),
    ('Storiq One (Backup)', TRUE, TRUE);

INSERT INTO Pool(uuid, name, archiveFormat, mediaFormat, backupPool) VALUES
	('b9650cc3-12ec-4a0f-88db-d70f0b269a6b', 'storiq', 1, 1, FALSE),
	('d9f976d4-e087-4d0a-ab79-96267f6613f0', 'Stone_Db_Backup', 2, 1, TRUE);

INSERT INTO PoolGroup(uuid, name) VALUES
    ('bbaaf022-2e1b-4a5e-a49f-aa1e219340f3', 'storiq');

INSERT INTO PoolToPoolGroup VALUES
    (1, 1);

INSERT INTO Users(login, password, salt, fullname, email, homedirectory, isAdmin, canArchive, canRestore, poolgroup, meta) VALUES
	('storiq', '8a6eb1d3b4fecbf8a1d6528a6aecb064e801b1e0', 'cd8c63688e0c2cff', 'storiq', 'storiq@localhost', '/mnt/raid', TRUE, TRUE, TRUE, 1, '{"step":5,"showHelp":true}');

INSERT INTO UserEvent(event) VALUES
	('connection'),
	('disconnection');
