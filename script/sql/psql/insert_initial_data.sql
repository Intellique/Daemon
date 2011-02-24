INSERT INTO Host VALUES (DEFAULT, 'kazoo', NULL),
	   (DEFAULT, 'd65', '3U test'),
	   (DEFAULT, 'storiq-test-squeeze', '5U court');

INSERT INTO TapeFormat VALUES (DEFAULT, 'LTO-5', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 1610612736, 1024, TRUE);

INSERT INTO Tape VALUES (DEFAULT, 'f5aeecb6-f2aa-4e51-b686-b56753e46d41', 'Test tape 1', 'new', 'offline', NULL, NULL, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, FALSE, 1, NULL),
	   (DEFAULT, '9fb39d47-ebf6-4a4c-a533-073660568467', 'Test tape 2', 'new', 'offline', NULL, NULL, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, FALSE, 1, NULL),
	   (DEFAULT, 'd1b17e74-1fe7-11e0-82a0-dfe908e885ff', 'Real tape 1', 'new', 'offline', NULL, NULL, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, FALSE, 1, NULL),
	   (DEFAULT, '8ed75908-d5ad-4cf9-9a2b-e13759d00209', 'Real tape 2', 'new', 'offline', NULL, NULL, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, FALSE, 1, NULL);

INSERT INTO Changer VALUES (DEFAULT, '', 'idle', FALSE, TRUE, 'Virtual library', 'Virtual vendor', 'v1.0', NOW(), 1),
	   (DEFAULT, '/dev/sg1', 'idle', TRUE, FALSE, 'VXA 1x10 1U', 'EXABYTE', 'A110', NOW(), 2),
	   (DEFAULT, '/dev/changer', 'idle', TRUE, FALSE, 'StorageLoader', 'TANDBERG', '0346', NOW(), 3);

INSERT INTO DriveFormat VALUES (DEFAULT, 'LTO-5', 30);

INSERT INTO DriveFormatSupport VALUES (1, 1, TRUE, TRUE);

INSERT INTO Drive VALUES (DEFAULT, 'file tmp', 'tmp/tape', 'empty-idle', 0, 0, NOW(), 'regular file', 'ext4fs', 'v1.0', 1, 1),
	   (DEFAULT, 'LTO-5 drive', '/dev/nst0', 'empty-idle', 0, 0, NOW(), 'LTO-5 Drive', 'Me', 'v1.0', 2, 1),
	   (DEFAULT, 'Drive n0', '/dev/nst0', 'empty-idle', 0, 0, NOW(), 'Ultrium 5-SCSI', 'HP', 'Z21U', 3, 1);

INSERT INTO ChangerSlot VALUES (DEFAULT, 0, 1, NULL),
	   (DEFAULT, 1, 1, 1),
	   (DEFAULT, 0, 2, NULL),
	   (DEFAULT, 1, 2, NULL),
	   (DEFAULT, 2, 2, 2),
	   (DEFAULT, 3, 2, NULL),
	   (DEFAULT, 4, 2, NULL),
	   (DEFAULT, 0, 3, NULL),
	   (DEFAULT, 1, 3, 3),
	   (DEFAULT, 2, 3, 4),
	   (DEFAULT, 3, 3, NULL),
	   (DEFAULT, 4, 3, NULL),
	   (DEFAULT, 5, 3, NULL),
	   (DEFAULT, 6, 3, NULL),
	   (DEFAULT, 7, 3, NULL),
	   (DEFAULT, 8, 3, NULL),
	   (DEFAULT, 9, 3, NULL);

INSERT INTO CheckSum VALUES (DEFAULT, 'MD5'),
	   (DEFAULT, 'SHA1'),
	   (DEFAULT, 'SHA256'),
	   (DEFAULT, 'SHA512'),
	   (DEFAULT, 'Whirlpool');

INSERT INTO Users VALUES (DEFAULT, 'storiq', '09ea7d58393247aae89bdd9bde93a298fc45ed4d', 'storiq', 'storiq@localhost', FALSE);

INSERT INTO Job VALUES (DEFAULT, 'test', 'save', 'idle', NOW(), NULL, 0, NOW(), NULL, 1),
	   (DEFAULT, 'test-shared', 'save', 'idle', NOW(), NULL, 0, NOW(), NULL, 1),
	   (DEFAULT, 'test-music', 'save', 'idle', NOW(), NULL, DEFAULT, NOW(), NULL, 1);

INSERT INTO SelectedFile VALUES (DEFAULT, 'script', TRUE),
	   (DEFAULT, '/mnt/partage', TRUE),
	   (DEFAULT, '/home/guillaume/music', TRUE);

INSERT INTO JobToSelectedFile VALUES (1, 1), (2, 2), (3, 3);

INSERT INTO JobToChecksum VALUES (1, 1), (1, 2), (1, 3), (1, 4), (1, 5),
	   (2, 1), (2, 2),
	   (3, 1), (3, 2);

