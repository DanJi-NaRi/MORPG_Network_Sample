CREATE DATABASE IF NOT EXISTS yuno_auth
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_0900_ai_ci;

USE yuno_auth;

SET FOREIGN_KEY_CHECKS = 0;
DROP TABLE IF EXISTS inventory_items;
DROP TABLE IF EXISTS characters;
DROP TABLE IF EXISTS login_tokens;
DROP TABLE IF EXISTS items;
DROP TABLE IF EXISTS users;
DROP TABLE IF EXISTS accounts;
SET FOREIGN_KEY_CHECKS = 1;

CREATE TABLE users (
  user_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  username VARCHAR(30) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  status SMALLINT NOT NULL DEFAULT 1,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  last_login_at TIMESTAMP NULL DEFAULT NULL,
  PRIMARY KEY (user_id),
  UNIQUE KEY uq_users_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE login_tokens (
  token_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  user_id BIGINT UNSIGNED NOT NULL,
  token_hash VARCHAR(128) NOT NULL,
  ip_address VARCHAR(64) NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  expires_at TIMESTAMP NOT NULL,
  revoked_at TIMESTAMP NULL DEFAULT NULL,
  PRIMARY KEY (token_id),
  UNIQUE KEY uq_login_tokens_user_id (user_id),
  UNIQUE KEY uq_login_tokens_token_hash (token_hash),
  KEY idx_login_tokens_expires_at (expires_at),
  CONSTRAINT fk_login_tokens_users
    FOREIGN KEY (user_id) REFERENCES users(user_id)
    ON DELETE CASCADE,
  CONSTRAINT chk_login_tokens_expires_after_created
    CHECK (expires_at > created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE characters (
  character_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  user_id BIGINT UNSIGNED NOT NULL,
  name VARCHAR(20) NOT NULL,
  class_code VARCHAR(30) NOT NULL,
  level INT NOT NULL DEFAULT 1,
  exp BIGINT NOT NULL DEFAULT 0,
  gold BIGINT NOT NULL DEFAULT 0,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  deleted_at TIMESTAMP NULL DEFAULT NULL,
  PRIMARY KEY (character_id),
  UNIQUE KEY uq_characters_user_id (user_id),
  UNIQUE KEY uq_characters_name (name),
  CONSTRAINT fk_characters_users
    FOREIGN KEY (user_id) REFERENCES users(user_id)
    ON DELETE CASCADE,
  CONSTRAINT chk_characters_level
    CHECK (level >= 1),
  CONSTRAINT chk_characters_exp
    CHECK (exp >= 0),
  CONSTRAINT chk_characters_gold
    CHECK (gold >= 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE items (
  item_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  item_code VARCHAR(50) NOT NULL,
  item_name VARCHAR(100) NOT NULL,
  item_type VARCHAR(30) NOT NULL,
  rarity VARCHAR(20) NOT NULL,
  max_stack INT NOT NULL DEFAULT 1,
  sell_price BIGINT NOT NULL DEFAULT 0,
  is_tradeable BOOLEAN NOT NULL DEFAULT TRUE,
  is_active BOOLEAN NOT NULL DEFAULT TRUE,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (item_id),
  UNIQUE KEY uq_items_item_code (item_code),
  KEY idx_items_item_type (item_type),
  KEY idx_items_is_active (is_active),
  CONSTRAINT chk_items_max_stack
    CHECK (max_stack >= 1),
  CONSTRAINT chk_items_sell_price
    CHECK (sell_price >= 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

CREATE TABLE inventory_items (
  inventory_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  character_id BIGINT UNSIGNED NOT NULL,
  slot_no INT NOT NULL,
  item_id BIGINT UNSIGNED NOT NULL,
  quantity INT NOT NULL DEFAULT 1,
  is_bound BOOLEAN NOT NULL DEFAULT FALSE,
  acquired_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (inventory_id),
  UNIQUE KEY uq_inventory_character_slot (character_id, slot_no),
  KEY idx_inventory_character_id (character_id),
  KEY idx_inventory_item_id (item_id),
  CONSTRAINT fk_inventory_characters
    FOREIGN KEY (character_id) REFERENCES characters(character_id)
    ON DELETE CASCADE,
  CONSTRAINT fk_inventory_items
    FOREIGN KEY (item_id) REFERENCES items(item_id),
  CONSTRAINT chk_inventory_slot_no
    CHECK (slot_no >= 0),
  CONSTRAINT chk_inventory_quantity
    CHECK (quantity >= 1)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
