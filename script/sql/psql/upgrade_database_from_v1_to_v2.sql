-- Update Users
ALTER TABLE Users DROP COLUMN nbConnection;
ALTER TABLE Users DROP COLUMN lastConnection;

-- Add UserLog
CREATE TABLE UserLog (
    id BIGSERIAL PRIMARY KEY,
    login INTEGER NOT NULL REFERENCES Users(id) ON UPDATE CASCADE ON DELETE CASCADE,
    timestamp TIMESTAMP(0) NOT NULL DEFAULT NOW()
);

-- Update ArchiveFile
ALTER TABLE ArchiveFile ALTER name TYPE TEXT;
ALTER TABLE ArchiveFile ADD blockNumber BIGINT CHECK (blockNumber >= 0);

-- update Job
-- ALTER TABLE Job ALTER interval TYPE INTERVAL;
CREATE LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION intToInterval(arg integer) RETURNS interval AS
$BODY$
  BEGIN
    return CAST( arg || ' seconds' AS interval );
  END;
$BODY$
LANGUAGE 'plpgsql';

-- As superuser
-- CREATE CAST (INTEGER AS INTERVAL) WITH FUNCTION intToInterval(INTEGER) AS IMPLICIT;

ALTER TABLE job ALTER interval SET DATA TYPE INTERVAL;

-- As superuser
-- DROP CAST intToInterval;

DROP FUNCTION IF EXISTS intToInterval(INTEGER);

-- update Pool
ALTER TABLE pool ALTER metadata TYPE TEXT;

