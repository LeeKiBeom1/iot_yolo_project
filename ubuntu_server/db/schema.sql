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
