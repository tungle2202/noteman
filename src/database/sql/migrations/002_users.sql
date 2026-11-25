-- 002_users
-- User accounts and per-user file ownership.
-- Shipped migration: never edit, add a new numbered file instead.

CREATE TABLE users (
    id              TEXT PRIMARY KEY NOT NULL,
    user_id         TEXT NOT NULL GENERATED ALWAYS AS (id) VIRTUAL,
    userid          TEXT NOT NULL GENERATED ALWAYS AS (id) VIRTUAL,
    username        TEXT NOT NULL UNIQUE,
    email           TEXT UNIQUE,
    hashed_password TEXT,
    salt            TEXT,
    created_at      TEXT NOT NULL
);
CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_email    ON users(email);

ALTER TABLE files ADD COLUMN user_id TEXT REFERENCES users(id) ON DELETE SET NULL;
CREATE INDEX idx_files_user_id ON files(user_id);
