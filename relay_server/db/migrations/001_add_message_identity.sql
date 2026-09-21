PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE;

ALTER TABLE sensor_data RENAME TO sensor_data_legacy;

CREATE TABLE sensor_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    message_id TEXT NOT NULL UNIQUE,
    timestamp TEXT NOT NULL,
    light INTEGER,
    temperature REAL,
    humidity REAL,
    sound INTEGER,
    sync_status TEXT NOT NULL DEFAULT 'UNSENT'
);

INSERT INTO sensor_data (
    id, device_id, message_id, timestamp,
    light, temperature, humidity, sound, sync_status
)
SELECT
    id,
    'legacy-relay',
    'legacy-sensor-' || printf('%010d', id),
    timestamp,
    light, temperature, humidity, sound, sync_status
FROM sensor_data_legacy;

DROP TABLE sensor_data_legacy;

CREATE INDEX idx_sensor_timestamp
ON sensor_data(timestamp);

ALTER TABLE vision_data RENAME TO vision_data_legacy;

CREATE TABLE vision_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    message_id TEXT NOT NULL UNIQUE,
    timestamp TEXT NOT NULL,
    object TEXT,
    confidence REAL,
    sync_status TEXT NOT NULL DEFAULT 'UNSENT'
);

INSERT INTO vision_data (
    id, device_id, message_id, timestamp,
    object, confidence, sync_status
)
SELECT
    id,
    'legacy-relay',
    'legacy-vision-' || printf('%010d', id),
    timestamp,
    object, confidence, sync_status
FROM vision_data_legacy;

DROP TABLE vision_data_legacy;

CREATE INDEX idx_vision_timestamp
ON vision_data(timestamp);

COMMIT;
PRAGMA foreign_keys = ON;
