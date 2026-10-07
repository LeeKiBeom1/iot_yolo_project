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

CREATE TABLE IF NOT EXISTS sensor_data (
    id INT NOT NULL AUTO_INCREMENT,
    device_id VARCHAR(32) NOT NULL,
    message_id VARCHAR(64) NOT NULL,
    timestamp DATETIME NOT NULL,
    light INT DEFAULT NULL,
    temperature FLOAT DEFAULT NULL,
    humidity FLOAT DEFAULT NULL,
    sound INT DEFAULT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_sensor_message_id (message_id),
    KEY idx_sensor_timestamp (timestamp)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_general_ci;
CREATE TABLE IF NOT EXISTS vision_data (
    id INT NOT NULL AUTO_INCREMENT,
    device_id VARCHAR(32) NOT NULL,
    message_id VARCHAR(64) NOT NULL,
    timestamp DATETIME NOT NULL,
    frame_id BIGINT UNSIGNED NOT NULL,
    timestamp_ms BIGINT NOT NULL,
    class_id INT NOT NULL,
    class_name VARCHAR(50) NOT NULL,
    confidence FLOAT NOT NULL,
    x INT NOT NULL,
    y INT NOT NULL,
    width INT NOT NULL,
    height INT NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uq_vision_message_id (message_id),
    KEY idx_vision_timestamp (timestamp),
    KEY idx_vision_timestamp_ms (timestamp_ms)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_general_ci;
