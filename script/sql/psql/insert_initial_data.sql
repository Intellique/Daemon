INSERT INTO Host(name, domaine, description) VALUES
	('localhost', NULL, DEFAULT),
	('d65', NULL, '3U test'),
	('taiko', 'intellique.com', '5U court'),
	('kazoo', 'intellique.com', 'shuttle'),
	('storiq-stone', 'intellique.com', '2U Test stone');

INSERT INTO MediaFormat(name, dataType, mode, maxLoadCount, maxReadCount, maxWriteCount, maxOpCount, lifespan, capacity, blockSize, densityCode, supportPartition, supportMAM) VALUES
	('LTO-5', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P10Y', 1529931104256, 32768, 88, TRUE, TRUE),
	('LTO-4', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P8Y', 764965552128, 32768, 70, FALSE, TRUE),
	('LTO-3', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 382482776064, 32768, 68, FALSE, TRUE),
	('LTO-2', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 191241388032, 32768, 66, FALSE, TRUE),
	('X23', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 153691136, 1024, 130, FALSE, FALSE),
	('DLT', 'data', 'linear', 4096, 40960, 40960, 40960, INTERVAL 'P6Y', 153691136, 1024, 129, FALSE, FALSE);

INSERT INTO DriveFormat(name, densityCode, cleaningInterval) VALUES
	('LTO-5', 88, INTERVAL 'P1W'),
	('LTO-4', 70, INTERVAL 'P1W'),
	('LTO-3', 68, INTERVAL 'P1W'),
	('LTO-2', 66, INTERVAL 'P1W'),
	('VXA-3', 130, INTERVAL 'P1W'),
	('DLT', 129, INTERVAL 'P1W');

INSERT INTO DriveFormatSupport(driveFormat, mediaFormat, read, write) VALUES
	(1, 1, TRUE, TRUE),
	(1, 2, TRUE, TRUE),
	(1, 3, TRUE, FALSE),
	(1, 4, FALSE, FALSE),
	(2, 2, TRUE, TRUE),
	(2, 3, TRUE, TRUE),
	(2, 4, TRUE, FALSE),
	(3, 3, TRUE, TRUE),
	(3, 4, TRUE, TRUE),
	(4, 4, TRUE, TRUE),
	(5, 5, TRUE, TRUE),
	(5, 6, TRUE, TRUE),
	(6, 6, TRUE, TRUE);

INSERT INTO Pool(uuid, name, mediaFormat) VALUES
	('b9650cc3-12ec-4a0f-88db-d70f0b269a6b', 'storiq', 1),
	('d9f976d4-e087-4d0a-ab79-96267f6613f0', 'Stone_Db_Backup', 1);

INSERT INTO Users(login, password, salt, fullname, email, isAdmin, canArchive, canRestore, pool, meta) VALUES
	('storiq', '8a6eb1d3b4fecbf8a1d6528a6aecb064e801b1e0', 'cd8c63688e0c2cff', 'storiq', 'storiq@localhost', TRUE, TRUE, TRUE, 1, hstore('step', '0') || hstore('showHelp', '1'));

