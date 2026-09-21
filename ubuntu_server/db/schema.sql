CREATE TABLE IF NOT EXISTS sensor_data (
    id INT NOT NULL AUTO_INCREMENT,
    timestamp DATETIME NOT NULL,
    light INT DEFAULT NULL,
    temperature FLOAT DEFAULT NULL,
    humidity FLOAT DEFAULT NULL,
    sound INT DEFAULT NULL,
    PRIMARY KEY (id),
    KEY idx_sensor_timestamp (timestamp)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_general_ci;

CREATE TABLE IF NOT EXISTS vision_data (
    id INT NOT NULL AUTO_INCREMENT,
    timestamp DATETIME NOT NULL,
    object VARCHAR(50) DEFAULT NULL,
    confidence FLOAT DEFAULT NULL,
    PRIMARY KEY (id),
    KEY idx_vision_timestamp (timestamp)
) ENGINE=InnoDB
  DEFAULT CHARSET=utf8mb4
  COLLATE=utf8mb4_general_ci;
