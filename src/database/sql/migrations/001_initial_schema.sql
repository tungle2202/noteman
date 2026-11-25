-- 001_initial_schema
-- Core academic structure (subjects -> threads -> files / tasks) plus the
-- _migrations audit log.
-- Shipped migration: never edit, add a new numbered file instead.

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
