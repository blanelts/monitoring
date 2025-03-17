CREATE TABLE IF NOT EXISTS server_stats (
    id SERIAL PRIMARY KEY,
    hostname VARCHAR(255),
    ip_address VARCHAR(255),
    cpu_usage DOUBLE PRECISION,
    memory_used INT,
    memory_total INT,
    disk_free INT,
    disk_total INT
);
ALTER TABLE server_stats ADD COLUMN last_update TIMESTAMP DEFAULT CURRENT_TIMESTAMP;
