INSERT INTO Host VALUES (DEFAULT, 'localhost', NULL, NULL),
       (DEFAULT, 'd65', NULL, '3U test'),
       (DEFAULT, 'taiko', 'intellique.com', '5U court'),
       (DEFAULT, 'kazoo', 'intellique.com', '5U court'),
	   (DEFAULT, 'storiq-stone', 'intellique.com', '2U Test stone');

INSERT INTO TapeFormat VALUES (DEFAULT, 'LTO-5', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 1676052267008, 32768, 88, TRUE, TRUE),
       (DEFAULT, 'LTO-4', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 838026133504, 32768, 70, FALSE, TRUE),
       (DEFAULT, 'LTO-3', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 419013066752, 8192, 68, FALSE, TRUE),
       (DEFAULT, 'LTO-2', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 209506533376, 8192, 66, FALSE, TRUE),
       (DEFAULT, 'X23', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 1024, 130, FALSE, FALSE),
       (DEFAULT, 'DLT', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 1024, 129, FALSE, FALSE);

INSERT INTO DriveFormat VALUES (DEFAULT, 'LTO-5', 88, 30),
       (DEFAULT, 'LTO-4', 70, 30),
       (DEFAULT, 'LTO-3', 68, 30),
       (DEFAULT, 'LTO-2', 66, 30),
       (DEFAULT, 'VXA-3', 130, 30),
       (DEFAULT, 'DLT', 129, 30);

INSERT INTO DriveFormatSupport VALUES (1, 1, TRUE, TRUE),
       (1, 2, TRUE, TRUE),
       (1, 3, TRUE, FALSE),
       (1, 4, FALSE, FALSE),
       (2, 2, TRUE, TRUE),
       (2, 3, TRUE, TRUE),
       (2, 4, TRUE, FALSE),
       (3, 3, TRUE, TRUE),
       (3, 4, TRUE, TRUE),
       (3, 5, TRUE, FALSE),
       (4, 4, TRUE, TRUE),
       (5, 5, TRUE, TRUE),
       (5, 6, TRUE, TRUE),
       (6, 6, TRUE, TRUE);

INSERT INTO Pool VALUES (DEFAULT, 'b9650cc3-12ec-4a0f-88db-d70f0b269a6b', 'storiq', DEFAULT, 1, '{}'),
	   (DEFAULT, 'd9f976d4-e087-4d0a-ab79-96267f6613f0', 'Stone_Db_Backup', DEFAULT, 1, '{}');

INSERT INTO Users VALUES (DEFAULT, 'storiq', '8a6eb1d3b4fecbf8a1d6528a6aecb064e801b1e0', 'cd8c63688e0c2cff', 'storiq', 'storiq@localhost', TRUE, TRUE, TRUE, DEFAULT, 1, hstore('step', '0'));

