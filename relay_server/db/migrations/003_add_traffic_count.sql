CREATE TABLE IF NOT EXISTS traffic_count (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    message_id TEXT NOT NULL UNIQUE,
    timestamp TEXT NOT NULL,
    period_start_ms INTEGER NOT NULL,
    period_end_ms INTEGER NOT NULL,
    car_count INTEGER NOT NULL,
    motorcycle_count INTEGER NOT NULL,
    bus_count INTEGER NOT NULL,
    truck_count INTEGER NOT NULL,
    sync_status TEXT NOT NULL DEFAULT 'UNSENT'
);

CREATE INDEX IF NOT EXISTS idx_traffic_count_period_start
ON traffic_count(period_start_ms);
