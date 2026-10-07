CREATE TABLE IF NOT EXISTS sensor_data (
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

CREATE INDEX IF NOT EXISTS idx_sensor_timestamp
ON sensor_data(timestamp);

CREATE TABLE IF NOT EXISTS vision_data (
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

CREATE INDEX IF NOT EXISTS idx_vision_timestamp
ON vision_data(timestamp);

CREATE INDEX IF NOT EXISTS idx_vision_timestamp_ms
ON vision_data(timestamp_ms);
