ALTER TABLE sensor_data
    ADD COLUMN device_id VARCHAR(32) NULL AFTER id,
    ADD COLUMN message_id VARCHAR(64) NULL AFTER device_id;

UPDATE sensor_data
SET
    device_id = 'legacy-relay',
    message_id = CONCAT('legacy-sensor-', LPAD(id, 10, '0'));

ALTER TABLE sensor_data
    MODIFY device_id VARCHAR(32) NOT NULL,
    MODIFY message_id VARCHAR(64) NOT NULL,
    ADD UNIQUE KEY uq_sensor_message_id (message_id);

ALTER TABLE vision_data
    ADD COLUMN device_id VARCHAR(32) NULL AFTER id,
    ADD COLUMN message_id VARCHAR(64) NULL AFTER device_id;

UPDATE vision_data
SET
    device_id = 'legacy-relay',
    message_id = CONCAT('legacy-vision-', LPAD(id, 10, '0'));

ALTER TABLE vision_data
    MODIFY device_id VARCHAR(32) NOT NULL,
    MODIFY message_id VARCHAR(64) NOT NULL,
    ADD UNIQUE KEY uq_vision_message_id (message_id);
