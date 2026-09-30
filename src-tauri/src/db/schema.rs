use rusqlite::Connection;
use crate::db::error::DbError;
use crate::db::migrations::run_migrations;

/// Initializes database schema by delegating to the versioned migration engine.
pub fn initialize_schema(conn: &mut Connection) -> Result<i32, DbError> {
    run_migrations(conn)
}
