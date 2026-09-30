use rusqlite::{Connection, Result};

pub const SCHEMA_SQL: &str = r#"
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;

-- Subjects table
CREATE TABLE IF NOT EXISTS subjects (
    id TEXT PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    semester TEXT NOT NULL
);

-- Threads table
CREATE TABLE IF NOT EXISTS threads (
    id TEXT PRIMARY KEY NOT NULL,
    subject_id TEXT NOT NULL,
    title TEXT NOT NULL,
    description TEXT,
    created_at TEXT NOT NULL,
    FOREIGN KEY (subject_id) REFERENCES subjects(id) ON DELETE CASCADE
);

-- Files table
CREATE TABLE IF NOT EXISTS files (
    id TEXT PRIMARY KEY NOT NULL,
    thread_id TEXT NOT NULL,
    file_name TEXT NOT NULL,
    file_type TEXT NOT NULL,
    storage_path TEXT NOT NULL,
    is_material INTEGER NOT NULL DEFAULT 0 CHECK (is_material IN (0, 1)),
    FOREIGN KEY (thread_id) REFERENCES threads(id) ON DELETE CASCADE
);

-- Tasks table
CREATE TABLE IF NOT EXISTS tasks (
    id TEXT PRIMARY KEY NOT NULL,
    thread_id TEXT NOT NULL,
    title TEXT NOT NULL,
    deadline TEXT,
    FOREIGN KEY (thread_id) REFERENCES threads(id) ON DELETE CASCADE
);

-- Indexes for performance and foreign key operations
CREATE INDEX IF NOT EXISTS idx_threads_subject_id ON threads(subject_id);
CREATE INDEX IF NOT EXISTS idx_files_thread_id ON files(thread_id);
CREATE INDEX IF NOT EXISTS idx_tasks_thread_id ON tasks(thread_id);
"#;

pub fn initialize_schema(conn: &Connection) -> Result<()> {
    conn.execute_batch(SCHEMA_SQL)?;
    Ok(())
}
