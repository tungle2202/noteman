#include "migrations.hpp"

namespace noteman::db {

namespace {

// Version 1: core academic structure (subjects -> threads -> files / tasks)
// plus the _migrations audit log.
constexpr const char *kMigration001 = R"sql(
CREATE TABLE IF NOT EXISTS _migrations (
    version    INTEGER PRIMARY KEY NOT NULL,
    name       TEXT    NOT NULL,
    applied_at TEXT    NOT NULL
);

CREATE TABLE subjects (
    id       TEXT PRIMARY KEY NOT NULL,
    name     TEXT NOT NULL,
    semester TEXT NOT NULL
);

CREATE TABLE threads (
    id          TEXT PRIMARY KEY NOT NULL,
    subject_id  TEXT NOT NULL REFERENCES subjects(id) ON DELETE CASCADE,
    title       TEXT NOT NULL,
    description TEXT,
    created_at  TEXT NOT NULL
);
CREATE INDEX idx_threads_subject_id ON threads(subject_id);

CREATE TABLE files (
    id           TEXT    PRIMARY KEY NOT NULL,
    thread_id    TEXT    NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
    file_name    TEXT    NOT NULL,
    file_type    TEXT    NOT NULL,
    storage_path TEXT    NOT NULL,
    is_material  INTEGER NOT NULL DEFAULT 0 CHECK (is_material IN (0, 1))
);
CREATE INDEX idx_files_thread_id ON files(thread_id);

CREATE TABLE tasks (
    id        TEXT PRIMARY KEY NOT NULL,
    thread_id TEXT NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
    title     TEXT NOT NULL,
    deadline  TEXT
);
CREATE INDEX idx_tasks_thread_id ON tasks(thread_id);
)sql";

// Version 2: user accounts and per-user file ownership.
constexpr const char *kMigration002 = R"sql(
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
)sql";

} // namespace

const std::vector<Migration> &migrations() {
    static const std::vector<Migration> kMigrations = {
        {1, "001_initial_schema", kMigration001},
        {2, "002_users", kMigration002},
    };
    return kMigrations;
}

} // namespace noteman::db
