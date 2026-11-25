-- 003_permissions_blobs_versions
-- Brings the schema to docs/design/database/ERD.dbml (schema version 3):
--   * groups + user_groups, subject ownership and 6-bit permission mode
--   * content-addressed blobs + materials (replaces files with is_material = 1)
--   * PDF quotes and per-user reading progress
--   * text notes with Git-like version history (replaces files with is_material = 0)
--   * last_update columns kept fresh by touch triggers
--
-- Versions 1-2 were never released with user data, and their rows cannot be
-- mapped onto the new model (subjects keyed by course code without an owner,
-- salted password hashes instead of Argon2id PHC strings, files on disk
-- instead of blobs / note versions). The legacy tables are therefore dropped
-- and rebuilt rather than migrated row by row.
--
-- Runs inside a single transaction together with the _migrations insert and
-- the PRAGMA user_version bump (see Database::run_migrations).

-- ---------------------------------------------------------------------------
-- Drop legacy tables (children first so FK actions have nothing to do)
-- ---------------------------------------------------------------------------

DROP TABLE IF EXISTS files;
DROP TABLE IF EXISTS tasks;
DROP TABLE IF EXISTS threads;
DROP TABLE IF EXISTS subjects;
DROP TABLE IF EXISTS users;

-- ---------------------------------------------------------------------------
-- _migrations: rebuild to add the applied_at default, keeping history
-- ---------------------------------------------------------------------------

CREATE TABLE _migrations_new (
    version    INTEGER PRIMARY KEY NOT NULL,
    name       TEXT    NOT NULL,
    applied_at TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);
INSERT INTO _migrations_new (version, name, applied_at)
    SELECT version, name, applied_at FROM _migrations;
DROP TABLE _migrations;
ALTER TABLE _migrations_new RENAME TO _migrations;

-- ---------------------------------------------------------------------------
-- Identity & groups
-- ---------------------------------------------------------------------------

CREATE TABLE groups (
    id         TEXT PRIMARY KEY NOT NULL,
    name       TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE TABLE users (
    id               TEXT    PRIMARY KEY NOT NULL,
    username         TEXT    NOT NULL UNIQUE,
    email            TEXT    UNIQUE,
    password_hash    TEXT,
    primary_group_id TEXT    NOT NULL REFERENCES groups(id) ON DELETE RESTRICT,
    is_admin         INTEGER NOT NULL DEFAULT 0 CHECK (is_admin IN (0, 1)),
    created_at       TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE TABLE user_groups (
    user_id  TEXT NOT NULL REFERENCES users(id)  ON DELETE CASCADE,
    group_id TEXT NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
    PRIMARY KEY (user_id, group_id)
);
CREATE INDEX idx_user_groups_group_id ON user_groups(group_id);

-- The primary group is always mirrored into user_groups so permission
-- queries only need that table.
CREATE TRIGGER trg_users_primary_group_ai
AFTER INSERT ON users
BEGIN
    INSERT OR IGNORE INTO user_groups (user_id, group_id)
    VALUES (NEW.id, NEW.primary_group_id);
END;

CREATE TRIGGER trg_users_primary_group_au
AFTER UPDATE OF primary_group_id ON users
BEGIN
    INSERT OR IGNORE INTO user_groups (user_id, group_id)
    VALUES (NEW.id, NEW.primary_group_id);
END;

-- A user cannot leave their primary group (change primary_group_id first).
-- Deleting the user itself still cascades: the users row is already gone
-- when the cascade reaches user_groups.
CREATE TRIGGER trg_user_groups_keep_primary_bd
BEFORE DELETE ON user_groups
WHEN EXISTS (
    SELECT 1 FROM users
    WHERE id = OLD.user_id AND primary_group_id = OLD.group_id
)
BEGIN
    SELECT RAISE(ABORT, 'cannot remove a user from their primary group');
END;

-- ---------------------------------------------------------------------------
-- Workspace hierarchy: subject -> thread
-- ---------------------------------------------------------------------------

CREATE TABLE subjects (
    id          TEXT    PRIMARY KEY NOT NULL,
    code        TEXT    NOT NULL,
    name        TEXT    NOT NULL,
    semester    TEXT    NOT NULL,
    owner_id    TEXT    NOT NULL REFERENCES users(id)  ON DELETE RESTRICT,
    group_id    TEXT    NOT NULL REFERENCES groups(id) ON DELETE RESTRICT,
    mode        INTEGER NOT NULL DEFAULT 56 CHECK (mode BETWEEN 0 AND 63),
    created_at  TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    last_update TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);
CREATE UNIQUE INDEX uq_subjects_owner_code_semester ON subjects(owner_id, code, semester);
CREATE INDEX idx_subjects_owner_id ON subjects(owner_id);
CREATE INDEX idx_subjects_group_id ON subjects(group_id);

CREATE TABLE threads (
    id          TEXT PRIMARY KEY NOT NULL,
    subject_id  TEXT NOT NULL REFERENCES subjects(id) ON DELETE CASCADE,
    title       TEXT NOT NULL,
    description TEXT,
    created_by  TEXT REFERENCES users(id) ON DELETE SET NULL,
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    last_update TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);
CREATE INDEX idx_threads_subject_id ON threads(subject_id);

-- ---------------------------------------------------------------------------
-- Binary content store
-- ---------------------------------------------------------------------------

CREATE TABLE blobs (
    hash       TEXT    PRIMARY KEY NOT NULL,
    size       INTEGER NOT NULL CHECK (size >= 0),
    mime_type  TEXT    NOT NULL,
    page_count INTEGER CHECK (page_count IS NULL OR page_count >= 0),
    created_at TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE TABLE materials (
    id           TEXT PRIMARY KEY NOT NULL,
    subject_id   TEXT NOT NULL REFERENCES subjects(id) ON DELETE CASCADE,
    blob_hash    TEXT NOT NULL REFERENCES blobs(hash)  ON DELETE RESTRICT,
    display_name TEXT NOT NULL,
    uploaded_by  TEXT REFERENCES users(id) ON DELETE SET NULL,
    uploaded_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    deleted_at   TEXT
);
CREATE INDEX idx_materials_subject_id ON materials(subject_id);
CREATE INDEX idx_materials_blob_hash ON materials(blob_hash);
CREATE UNIQUE INDEX uq_materials_subject_blob ON materials(subject_id, blob_hash);

-- ---------------------------------------------------------------------------
-- PDF quoting & reading progress
-- ---------------------------------------------------------------------------

CREATE TABLE quotes (
    id             TEXT PRIMARY KEY NOT NULL,
    thread_id      TEXT NOT NULL REFERENCES threads(id)   ON DELETE CASCADE,
    material_id    TEXT NOT NULL REFERENCES materials(id) ON DELETE RESTRICT,
    kind           TEXT NOT NULL CHECK (kind IN ('full', 'pages', 'region', 'text')),
    spec_json      TEXT NOT NULL CHECK (json_valid(spec_json)),
    extracted_text TEXT,
    note           TEXT,
    author_id      TEXT REFERENCES users(id) ON DELETE SET NULL,
    created_at     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    last_update    TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);
CREATE INDEX idx_quotes_thread_id ON quotes(thread_id);
CREATE INDEX idx_quotes_material_id ON quotes(material_id);

CREATE TABLE reading_progress (
    user_id     TEXT    NOT NULL REFERENCES users(id)     ON DELETE CASCADE,
    material_id TEXT    NOT NULL REFERENCES materials(id) ON DELETE CASCADE,
    page        INTEGER NOT NULL CHECK (page >= 1),
    page_offset REAL    NOT NULL DEFAULT 0 CHECK (page_offset BETWEEN 0.0 AND 1.0),
    zoom        REAL,
    updated_at  TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    PRIMARY KEY (user_id, material_id)
);

-- ---------------------------------------------------------------------------
-- Text notes with version history
-- ---------------------------------------------------------------------------

CREATE TABLE notes (
    id                 TEXT PRIMARY KEY NOT NULL,
    thread_id          TEXT NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
    file_name          TEXT NOT NULL,
    format             TEXT NOT NULL CHECK (format IN ('md', 'tex')),
    current_version_id TEXT REFERENCES note_versions(id) ON DELETE SET NULL
                            DEFERRABLE INITIALLY DEFERRED,
    created_by         TEXT REFERENCES users(id) ON DELETE SET NULL,
    created_at         TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    last_update        TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);
CREATE INDEX idx_notes_thread_id ON notes(thread_id);
CREATE UNIQUE INDEX uq_notes_thread_file_name ON notes(thread_id, file_name);

CREATE TABLE note_versions (
    id           TEXT    PRIMARY KEY NOT NULL,
    note_id      TEXT    NOT NULL REFERENCES notes(id) ON DELETE CASCADE,
    parent_id    TEXT    REFERENCES note_versions(id) ON DELETE SET NULL,
    author_id    TEXT    REFERENCES users(id) ON DELETE SET NULL,
    content      BLOB    NOT NULL,
    compression  TEXT    NOT NULL DEFAULT 'zstd' CHECK (compression IN ('none', 'zstd')),
    content_hash TEXT    NOT NULL,
    size         INTEGER NOT NULL CHECK (size >= 0),
    message      TEXT,
    pinned       INTEGER NOT NULL DEFAULT 0 CHECK (pinned IN (0, 1)),
    created_at   TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);
CREATE INDEX idx_note_versions_note_created ON note_versions(note_id, created_at);
CREATE INDEX idx_note_versions_content_hash ON note_versions(content_hash);

-- ---------------------------------------------------------------------------
-- Tasks
-- ---------------------------------------------------------------------------

CREATE TABLE tasks (
    id           TEXT PRIMARY KEY NOT NULL,
    thread_id    TEXT NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
    title        TEXT NOT NULL,
    deadline     TEXT,
    completed_at TEXT,
    created_by   TEXT REFERENCES users(id) ON DELETE SET NULL,
    created_at   TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    last_update  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);
CREATE INDEX idx_tasks_thread_id ON tasks(thread_id);
CREATE INDEX idx_tasks_deadline ON tasks(deadline);

-- ---------------------------------------------------------------------------
-- Touch triggers: keep last_update fresh and bubble changes up
--   notes / quotes / tasks -> threads -> subjects, materials -> subjects
--
-- Each AFTER UPDATE trigger only fires when the statement did not set
-- last_update itself (WHEN NEW.last_update IS OLD.last_update). Children
-- therefore touch both their thread and its subject directly, and the
-- touches they cause do not re-trigger anything. This also keeps the
-- triggers loop-free if PRAGMA recursive_triggers is ever enabled.
-- ---------------------------------------------------------------------------

-- subjects ------------------------------------------------------------------

CREATE TRIGGER trg_subjects_touch_au
AFTER UPDATE ON subjects
WHEN NEW.last_update IS OLD.last_update
BEGIN
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.id;
END;

-- threads -------------------------------------------------------------------

CREATE TRIGGER trg_threads_touch_ai
AFTER INSERT ON threads
BEGIN
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.subject_id;
END;

CREATE TRIGGER trg_threads_touch_au
AFTER UPDATE ON threads
WHEN NEW.last_update IS OLD.last_update
BEGIN
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.id;
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id IN (OLD.subject_id, NEW.subject_id);
END;

CREATE TRIGGER trg_threads_touch_ad
AFTER DELETE ON threads
BEGIN
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = OLD.subject_id;
END;

-- materials (no last_update of their own) -----------------------------------

CREATE TRIGGER trg_materials_touch_ai
AFTER INSERT ON materials
BEGIN
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.subject_id;
END;

CREATE TRIGGER trg_materials_touch_au
AFTER UPDATE ON materials
BEGIN
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id IN (OLD.subject_id, NEW.subject_id);
END;

CREATE TRIGGER trg_materials_touch_ad
AFTER DELETE ON materials
BEGIN
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = OLD.subject_id;
END;

-- notes ---------------------------------------------------------------------

CREATE TRIGGER trg_notes_touch_ai
AFTER INSERT ON notes
BEGIN
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.thread_id;
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = (SELECT subject_id FROM threads WHERE id = NEW.thread_id);
END;

CREATE TRIGGER trg_notes_touch_au
AFTER UPDATE ON notes
WHEN NEW.last_update IS OLD.last_update
BEGIN
    UPDATE notes SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.id;
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id IN (OLD.thread_id, NEW.thread_id);
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id IN (SELECT subject_id FROM threads WHERE id IN (OLD.thread_id, NEW.thread_id));
END;

CREATE TRIGGER trg_notes_touch_ad
AFTER DELETE ON notes
BEGIN
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = OLD.thread_id;
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = (SELECT subject_id FROM threads WHERE id = OLD.thread_id);
END;

-- quotes --------------------------------------------------------------------

CREATE TRIGGER trg_quotes_touch_ai
AFTER INSERT ON quotes
BEGIN
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.thread_id;
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = (SELECT subject_id FROM threads WHERE id = NEW.thread_id);
END;

CREATE TRIGGER trg_quotes_touch_au
AFTER UPDATE ON quotes
WHEN NEW.last_update IS OLD.last_update
BEGIN
    UPDATE quotes SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.id;
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id IN (OLD.thread_id, NEW.thread_id);
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id IN (SELECT subject_id FROM threads WHERE id IN (OLD.thread_id, NEW.thread_id));
END;

CREATE TRIGGER trg_quotes_touch_ad
AFTER DELETE ON quotes
BEGIN
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = OLD.thread_id;
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = (SELECT subject_id FROM threads WHERE id = OLD.thread_id);
END;

-- tasks ---------------------------------------------------------------------

CREATE TRIGGER trg_tasks_touch_ai
AFTER INSERT ON tasks
BEGIN
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.thread_id;
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = (SELECT subject_id FROM threads WHERE id = NEW.thread_id);
END;

CREATE TRIGGER trg_tasks_touch_au
AFTER UPDATE ON tasks
WHEN NEW.last_update IS OLD.last_update
BEGIN
    UPDATE tasks SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = NEW.id;
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id IN (OLD.thread_id, NEW.thread_id);
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id IN (SELECT subject_id FROM threads WHERE id IN (OLD.thread_id, NEW.thread_id));
END;

CREATE TRIGGER trg_tasks_touch_ad
AFTER DELETE ON tasks
BEGIN
    UPDATE threads SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = OLD.thread_id;
    UPDATE subjects SET last_update = strftime('%Y-%m-%dT%H:%M:%fZ', 'now')
    WHERE id = (SELECT subject_id FROM threads WHERE id = OLD.thread_id);
END;
