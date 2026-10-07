-- Additive migration: preserves all existing data and tables.
CREATE TABLE IF NOT EXISTS vehicle_count (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    device_id VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    message_id VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    timestamp DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    frame_id BIGINT UNSIGNED NOT NULL,
    timestamp_ms BIGINT NOT NULL,
    vehicle_count INT NOT NULL,
    UNIQUE KEY uq_vehicle_count_message_id (message_id),
    KEY idx_vehicle_count_timestamp_ms (timestamp_ms),
    CONSTRAINT ck_vehicle_count_nonnegative CHECK (vehicle_count >= 0),
    CONSTRAINT ck_vehicle_count_time CHECK (timestamp_ms > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
