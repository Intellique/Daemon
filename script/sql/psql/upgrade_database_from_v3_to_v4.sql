-- Update jobstatus
ALTER TYPE JobStatus RENAME TO JobStatus_old;

CREATE TYPE JobStatus AS ENUM (
    'disable',
	'finished',
    'error',
    'idle',
    'pause',
    'running',
    'stopped',
    'waiting'
);

COMMENT ON TYPE JobStatus IS E'disable => disabled,\nfinish => job finished,\nerror => error while running,\nidle => not yet started or completed,\npause => waiting for user action,\nrunning => running,\nstopped => stopped by user,\nwaiting => waiting for a resource';

ALTER TABLE Job ALTER status DROP DEFAULT;
ALTER TABLE Job ALTER status TYPE JobStatus USING status::TEXT::JobStatus;
ALTER TABLE Job ALTER status SET DEFAULT 'idle';

ALTER TABLE JobRecord DROP CONSTRAINT jobrecord_status_check;
ALTER TABLE JobRecord ALTER status TYPE JobStatus USING status::TEXT::JOBSTATUS;
ALTER TABLE JobRecord ADD CONSTRAINT jobrecord_status_check CHECK (status != 'disable');

DROP TYPE jobstatus_old;

-- Add column deleted from table pool
ALTER TABLE pool ADD COLUMN deleted BOOLEAN NOT NULL DEFAULT FALSE;

-- Add new type TapeType
CREATE TYPE TapeType AS ENUM (
    'read only',
    'rewritable',
    'cleaning'
);
ALTER TABLE tape ADD COLUMN type tapetype NOT NULL DEFAULT 'rewritable';

-- Trim space if needed
-- UPDATE tape SET label = TRIM(FROM label), name = TRIM(FROM name);

-- Set default value 
ALTER TABLE job ALTER metadata SET DEFAULT '';
ALTER TABLE job ALTER options SET DEFAULT '';

-- Create tape label
CREATE TABLE TapeLabel (
    id BIGSERIAL PRIMARY KEY,

    name TEXT NOT NULL,
    tape INTEGER NOT NULL REFERENCES Tape(id) ON UPDATE CASCADE ON DELETE CASCADE
);

