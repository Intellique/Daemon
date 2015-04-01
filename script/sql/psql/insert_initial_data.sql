INSERT INTO MediaFormat(name, dataType, mode, maxLoadCount, maxReadCount, maxWriteCount, maxOpCount, lifespan, capacity, blockSize, densityCode, supportPartition, supportMAM) VALUES
	('LTO-6', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P10Y', 2620446998528, 32768, 90, TRUE, TRUE),
	('LTO-5', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P10Y', 1529931104256, 32768, 88, TRUE, TRUE),
	('LTO-4', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P8Y', 764965552128, 32768, 70, FALSE, TRUE),
	('LTO-3', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 382482776064, 32768, 68, FALSE, TRUE),
	('LTO-2', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 191241388032, 32768, 66, FALSE, TRUE),
	('X23', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 153691136, 1024, 130, FALSE, FALSE),
	('DLT', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 153691136, 1024, 129, FALSE, FALSE);

INSERT INTO DriveFormat(name, densityCode, mode, cleaningInterval) VALUES
    ('LTO-6', 90, 'linear', INTERVAL 'P1W'),
	('LTO-5', 88, 'linear', INTERVAL 'P1W'),
	('LTO-4', 70, 'linear', INTERVAL 'P1W'),
	('LTO-3', 68, 'linear', INTERVAL 'P1W'),
	('LTO-2', 66, 'linear', INTERVAL 'P1W'),
	('VXA-3', 130, 'linear', INTERVAL 'P1W'),
	('DLT', 129, 'linear', INTERVAL 'P1W');

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
	(5, 5, TRUE, TRUE),
	(5, 6, TRUE, TRUE),
	(6, 6, TRUE, TRUE);

INSERT INTO Pool(uuid, name, mediaFormat) VALUES
	('b9650cc3-12ec-4a0f-88db-d70f0b269a6b', 'storiq', 1),
	('d9f976d4-e087-4d0a-ab79-96267f6613f0', 'Stone_Db_Backup', 1);

INSERT INTO PoolGroup(uuid, name) VALUES
	('7a9102a2-6f4d-c85f-1553-e8d769569558', 'storiq');

INSERT INTO Users(login, password, salt, fullname, email, homedirectory, isAdmin, canArchive, canRestore, poolgroup, meta) VALUES
	('storiq', '8a6eb1d3b4fecbf8a1d6528a6aecb064e801b1e0', 'cd8c63688e0c2cff', 'storiq', 'storiq@localhost', '/mnt/raid', TRUE, TRUE, TRUE, 1, hstore('step', '0') || hstore('showHelp', '1'));

INSERT INTO UserEvent(event) VALUES
	('connection'),
	('disconnection');
