INSERT INTO Host VALUES (DEFAULT, 'localhost', NULL);

INSERT INTO TapeFormat VALUES (DEFAULT, 'LTO-5', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 1610612736, 8192, 88, TRUE),
       (DEFAULT, 'LTO-4', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 8192, 70, FALSE),
       (DEFAULT, 'LTO-3', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 8192, 68, FALSE),
       (DEFAULT, 'LTO-2', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 8192, 66, FALSE),
       (DEFAULT, 'X23', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 1024, 130, FALSE);

INSERT INTO DriveFormat VALUES (DEFAULT, 'LTO-5', 88, 30),
	   (DEFAULT, 'LTO-4', 70, 30),
	   (DEFAULT, 'LTO-3', 68, 30),
	   (DEFAULT, 'LTO-2', 66, 30),
       (DEFAULT, 'VXA-3', 130, 30);

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
	   (5, 5, TRUE, TRUE);

INSERT INTO CheckSum VALUES (DEFAULT, 'MD5'),
	   (DEFAULT, 'SHA1'),
	   (DEFAULT, 'SHA256'),
	   (DEFAULT, 'SHA512');

INSERT INTO Users VALUES (DEFAULT, 'storiq', '09ea7d58393247aae89bdd9bde93a298fc45ed4d', 'storiq', 'storiq@localhost', FALSE);

