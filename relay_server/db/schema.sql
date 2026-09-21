CREATE TABLE IF NOT EXISTS sensor_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
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
    timestamp TEXT NOT NULL,
    object TEXT,
    confidence REAL,
    sync_status TEXT NOT NULL DEFAULT 'UNSENT'
);

CREATE INDEX IF NOT EXISTS idx_vision_timestamp
ON vision_data(timestamp);
