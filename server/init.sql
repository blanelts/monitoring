CREATE TABLE IF NOT EXISTS server_stats (
    id SERIAL PRIMARY KEY,
    hostname VARCHAR(255),
    ip_address VARCHAR(50),
    cpu_usage REAL,
    memory_used INT,
    memory_total INT,
    disk_free INT,
    disk_total INT,
    os_info VARCHAR(100),
    last_update TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

