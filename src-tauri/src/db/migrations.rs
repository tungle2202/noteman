use chrono::Utc;
use rusqlite::{params, Connection};
use crate::db::error::DbError;
use crate::db::models::MigrationInfo;

/// Represents a single, versioned migration step.
pub struct Migration {
    pub version: i32,
    pub name: &'static str,
    pub sql: &'static str,
}

/// Ordered list of migrations. New schema changes must be appended here with incremented version numbers.
pub const MIGRATIONS: &[Migration] = &[
    Migration {
        version: 1,
        name: "001_initial_schema",
        sql: r#"
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

        -- Indexes for query performance and foreign key cascades
        CREATE INDEX IF NOT EXISTS idx_threads_subject_id ON threads(subject_id);
        CREATE INDEX IF NOT EXISTS idx_files_thread_id ON files(thread_id);
        CREATE INDEX IF NOT EXISTS idx_tasks_thread_id ON tasks(thread_id);
        "#,
    },
];

/// Enforces SQLite PRAGMAs, ensures migration audit log table, and executes pending migrations in order.
pub fn run_migrations(conn: &mut Connection) -> Result<i32, DbError> {
    // 1. Enforce engine PRAGMAs and migration tracking table
    conn.execute_batch(
        r#"
        PRAGMA foreign_keys = ON;
        PRAGMA journal_mode = WAL;
        PRAGMA synchronous = NORMAL;

        CREATE TABLE IF NOT EXISTS _migrations (
            version INTEGER PRIMARY KEY NOT NULL,
            name TEXT NOT NULL,
            applied_at TEXT NOT NULL
        );
        "#,
    )?;

    // 2. Query current schema version
    let current_version: i32 = conn.query_row("PRAGMA user_version", [], |r| r.get(0))?;

    // 3. Apply pending migrations sequentially
    for migration in MIGRATIONS.iter() {
        if migration.version > current_version {
            let tx = conn.transaction()?;
            tx.execute_batch(migration.sql)?;
            let now = Utc::now().to_rfc3339();
            tx.execute(
                "INSERT INTO _migrations (version, name, applied_at) VALUES (?1, ?2, ?3)",
                params![migration.version, migration.name, now],
            )?;
            tx.execute_batch(&format!("PRAGMA user_version = {};", migration.version))?;
            tx.commit()?;
        }
    }

    let final_version: i32 = conn.query_row("PRAGMA user_version", [], |r| r.get(0))?;
    Ok(final_version)
}

/// Retrieve the current SQLite user_version pragma value.
pub fn get_schema_version(conn: &Connection) -> Result<i32, DbError> {
    let version: i32 = conn.query_row("PRAGMA user_version", [], |r| r.get(0))?;
    Ok(version)
}

/// Retrieve the historical audit log of all applied migrations from _migrations.
pub fn get_applied_migrations(conn: &Connection) -> Result<Vec<MigrationInfo>, DbError> {
    let table_exists: bool = conn
        .query_row(
            "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='_migrations'",
            [],
            |r| r.get::<_, i64>(0),
        )
        .map(|c| c > 0)
        .unwrap_or(false);

    if !table_exists {
        return Ok(Vec::new());
    }

    let mut stmt = conn.prepare("SELECT version, name, applied_at FROM _migrations ORDER BY version ASC")?;
    let rows = stmt.query_map([], |row| {
        Ok(MigrationInfo {
            version: row.get(0)?,
            name: row.get(1)?,
            applied_at: row.get(2)?,
        })
    })?;

    let mut migrations = Vec::new();
    for m in rows {
        migrations.push(m?);
    }
    Ok(migrations)
}
