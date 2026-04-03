USE yuno_auth;

DELIMITER $$

DROP PROCEDURE IF EXISTS sp_auth_validate_user_credentials$$
CREATE PROCEDURE sp_auth_validate_user_credentials(
  IN p_username VARCHAR(30),
  IN p_password_plain VARCHAR(255)
)
BEGIN
  SELECT
    user_id,
    password_hash,
    (password_hash = SHA2(p_password_plain, 256)) AS legacy_sha256_match,
    (password_hash = p_password_plain) AS legacy_plaintext_match
  FROM users
  WHERE username = p_username
    AND status = 1
  LIMIT 1;
END$$

DROP PROCEDURE IF EXISTS sp_auth_migrate_user_password$$
CREATE PROCEDURE sp_auth_migrate_user_password(
  IN p_user_id BIGINT UNSIGNED,
  IN p_password_hash VARCHAR(255)
)
BEGIN
  UPDATE users
  SET password_hash = p_password_hash
  WHERE user_id = p_user_id;
END$$

DROP PROCEDURE IF EXISTS sp_auth_create_user$$
CREATE PROCEDURE sp_auth_create_user(
  IN p_username VARCHAR(30),
  IN p_password_hash VARCHAR(255)
)
BEGIN
  INSERT INTO users(username, password_hash, status, created_at)
  VALUES (p_username, p_password_hash, 1, NOW());
END$$

DROP PROCEDURE IF EXISTS sp_auth_upsert_login_token$$
CREATE PROCEDURE sp_auth_upsert_login_token(
  IN p_user_id BIGINT UNSIGNED,
  IN p_token_plain VARCHAR(255),
  IN p_expires_at_epoch BIGINT UNSIGNED
)
BEGIN
  INSERT INTO login_tokens(user_id, token_hash, ip_address, created_at, expires_at)
  VALUES (
    p_user_id,
    SHA2(p_token_plain, 256),
    NULL,
    NOW(),
    FROM_UNIXTIME(p_expires_at_epoch)
  )
  ON DUPLICATE KEY UPDATE
    token_hash = VALUES(token_hash),
    ip_address = VALUES(ip_address),
    expires_at = VALUES(expires_at),
    created_at = VALUES(created_at),
    revoked_at = NULL;
END$$

DROP PROCEDURE IF EXISTS sp_auth_touch_last_login$$
CREATE PROCEDURE sp_auth_touch_last_login(
  IN p_user_id BIGINT UNSIGNED
)
BEGIN
  UPDATE users
  SET last_login_at = NOW()
  WHERE user_id = p_user_id;
END$$

DROP PROCEDURE IF EXISTS sp_auth_has_active_login_token$$
CREATE PROCEDURE sp_auth_has_active_login_token(
  IN p_user_id BIGINT UNSIGNED
)
BEGIN
  SELECT token_id
  FROM login_tokens
  WHERE user_id = p_user_id
    AND revoked_at IS NULL
    AND expires_at > NOW()
  LIMIT 1;
END$$

DROP PROCEDURE IF EXISTS sp_auth_revoke_login_token$$
CREATE PROCEDURE sp_auth_revoke_login_token(
  IN p_user_id BIGINT UNSIGNED
)
BEGIN
  UPDATE login_tokens
  SET revoked_at = NOW()
  WHERE user_id = p_user_id
    AND revoked_at IS NULL;
END$$

DROP PROCEDURE IF EXISTS sp_auth_revoke_login_token_by_hash$$
CREATE PROCEDURE sp_auth_revoke_login_token_by_hash(
  IN p_token_plain VARCHAR(255)
)
BEGIN
  UPDATE login_tokens
  SET revoked_at = NOW()
  WHERE (token_hash = SHA2(p_token_plain, 256) OR token_hash = p_token_plain)
    AND revoked_at IS NULL;
END$$

DROP PROCEDURE IF EXISTS sp_auth_validate_login_token$$
CREATE PROCEDURE sp_auth_validate_login_token(
  IN p_token_plain VARCHAR(255)
)
BEGIN
  SELECT
    lt.user_id,
    (lt.token_hash = p_token_plain) AS legacy_plaintext_match
  FROM login_tokens lt
  INNER JOIN users u ON u.user_id = lt.user_id
  WHERE (lt.token_hash = SHA2(p_token_plain, 256) OR lt.token_hash = p_token_plain)
    AND lt.revoked_at IS NULL
    AND lt.expires_at > NOW()
    AND u.status = 1
  LIMIT 1;
END$$

DROP PROCEDURE IF EXISTS sp_auth_migrate_login_token_hash$$
CREATE PROCEDURE sp_auth_migrate_login_token_hash(
  IN p_user_id BIGINT UNSIGNED,
  IN p_token_plain VARCHAR(255)
)
BEGIN
  UPDATE login_tokens
  SET token_hash = SHA2(p_token_plain, 256)
  WHERE user_id = p_user_id
    AND token_hash = p_token_plain;
END$$

DELIMITER ;
