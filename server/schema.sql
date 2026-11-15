-- AutoCargo database schema (MySQL / MariaDB)
CREATE DATABASE IF NOT EXISTS autocargo CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE autocargo;

CREATE TABLE IF NOT EXISTS devices (
    id   INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(32) NOT NULL UNIQUE,   -- case-insensitive because of the collation
    ip   VARCHAR(45) NOT NULL
);

CREATE TABLE IF NOT EXISTS messages (
    id        INT AUTO_INCREMENT PRIMARY KEY,
    sender    VARCHAR(32)  NOT NULL,
    recipient VARCHAR(32)  NOT NULL,
    message   VARCHAR(500) NOT NULL,
    timestamp DATETIME     NOT NULL,
    INDEX idx_timestamp (timestamp)
);

-- Dedicated database user with only the rights the server needs (never use root).
-- Replace the password before running this.
-- CREATE USER 'autocargo'@'localhost' IDENTIFIED BY 'choose-a-strong-password';
-- GRANT SELECT, INSERT, DELETE ON autocargo.* TO 'autocargo'@'localhost';
