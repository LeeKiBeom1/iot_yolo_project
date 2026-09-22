ALTER TABLE vision_data
    CHANGE COLUMN object class_name VARCHAR(50) NULL,
    ADD COLUMN frame_id BIGINT UNSIGNED NULL AFTER timestamp,
    ADD COLUMN timestamp_ms BIGINT NULL AFTER frame_id,
    ADD COLUMN class_id INT NULL AFTER timestamp_ms,
    ADD COLUMN x INT NULL AFTER confidence,
    ADD COLUMN y INT NULL AFTER x,
    ADD COLUMN width INT NULL AFTER y,
    ADD COLUMN height INT NULL AFTER width;

UPDATE vision_data
SET
    frame_id = id,
    timestamp_ms = UNIX_TIMESTAMP(timestamp) * 1000,
    class_id = -1,
    class_name = COALESCE(class_name, 'unknown'),
    confidence = COALESCE(confidence, 0),
    x = 0,
    y = 0,
    width = 0,
    height = 0;

ALTER TABLE vision_data
    MODIFY frame_id BIGINT UNSIGNED NOT NULL,
    MODIFY timestamp_ms BIGINT NOT NULL,
    MODIFY class_id INT NOT NULL,
    MODIFY class_name VARCHAR(50) NOT NULL,
    MODIFY confidence FLOAT NOT NULL,
    MODIFY x INT NOT NULL,
    MODIFY y INT NOT NULL,
    MODIFY width INT NOT NULL,
    MODIFY height INT NOT NULL,
    ADD KEY idx_vision_timestamp_ms (timestamp_ms);
