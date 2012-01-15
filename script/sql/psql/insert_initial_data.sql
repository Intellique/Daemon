INSERT INTO Host VALUES (DEFAULT, 'localhost', NULL, NULL),
       (DEFAULT, 'd65', NULL, '3U test'),
       (DEFAULT, 'taiko', 'intellique.com', '5U court'),
       (DEFAULT, 'kazoo', 'intellique.com', '5U court');

INSERT INTO TapeFormat VALUES (DEFAULT, 'LTO-5', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 1610612736, 8192, 88, TRUE),
       (DEFAULT, 'LTO-4', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 8192, 70, FALSE),
       (DEFAULT, 'LTO-3', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 8192, 68, FALSE),
       (DEFAULT, 'LTO-2', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 8192, 66, FALSE),
       (DEFAULT, 'X23', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 1024, 130, FALSE),
       (DEFAULT, 'DLT', 'data', 'linear', 4096, 4096, 40960, 40960, 1024, 153691136, 1024, 129, FALSE);

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

INSERT INTO Pool VALUES (DEFAULT, 'b9650cc3-12ec-4a0f-88db-d70f0b269a6b', 'storiq', DEFAULT, NULL, DEFAULT, DEFAULT, 5);

INSERT INTO Users VALUES (DEFAULT, 'storiq', '8a6eb1d3b4fecbf8a1d6528a6aecb064e801b1e0', 'cd8c63688e0c2cff', 'storiq', 'storiq@localhost', TRUE, TRUE, TRUE, 1);

