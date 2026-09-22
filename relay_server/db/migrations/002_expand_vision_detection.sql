PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE;

ALTER TABLE vision_data RENAME TO vision_data_legacy;

CREATE TABLE vision_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    message_id TEXT NOT NULL UNIQUE,
    timestamp TEXT NOT NULL,
    frame_id INTEGER NOT NULL,
    timestamp_ms INTEGER NOT NULL,
    class_id INTEGER NOT NULL,
    class_name TEXT NOT NULL,
    confidence REAL NOT NULL,
    x INTEGER NOT NULL,
    y INTEGER NOT NULL,
    width INTEGER NOT NULL,
    height INTEGER NOT NULL,
    sync_status TEXT NOT NULL DEFAULT 'UNSENT'
);

INSERT INTO vision_data (
    id, device_id, message_id, timestamp,
    frame_id, timestamp_ms, class_id, class_name, confidence,
    x, y, width, height, sync_status
)
SELECT
    id, device_id, message_id, timestamp,
    id,
    CAST(strftime('%s', timestamp) AS INTEGER) * 1000,
    -1,
    COALESCE(object, 'unknown'),
    COALESCE(confidence, 0),
    0, 0, 0, 0,
    sync_status
FROM vision_data_legacy;

DROP TABLE vision_data_legacy;

CREATE INDEX idx_vision_timestamp
ON vision_data(timestamp);

CREATE INDEX idx_vision_timestamp_ms
ON vision_data(timestamp_ms);

COMMIT;
PRAGMA foreign_keys = ON;
