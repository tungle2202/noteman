use std::path::PathBuf;
use std::sync::Mutex;
use rusqlite::{params, Connection, OptionalExtension};
use uuid::Uuid;
use chrono::Utc;

use crate::db::error::DbError;
use crate::db::models::{
    CreateFileInput, CreateSubjectInput, CreateTaskInput, CreateThreadInput, DbStats,
    FileIntegrityItem, FileRecord, Subject, Task, Thread, UpdateSubjectInput, UpdateTaskInput,
    UpdateThreadInput,
};
use crate::db::schema::initialize_schema;

pub struct DbManager {
    conn: Mutex<Connection>,
    pub db_path: PathBuf,
    pub app_dir: PathBuf,
}

impl DbManager {
    /// Standard production subdirectories according to production_directory_tree.json
    const MANAGED_DIRS: &'static [&'static str] = &[
        "db",
        "notes/markdown",
        "notes/latex",
        "notes/misc",
        "materials/pdf",
        "materials/slides",
        "materials/docs",
        "materials/archives",
        "media/images",
        "media/audio",
        "cache/katex",
        "cache/previews",
        "cache/latex_builds",
        "cache/exports",
        "cache/trash",
        "config/templates/markdown",
        "config/templates/latex",
        "logs",
    ];

    /// Initialize a new DbManager with a file-backed SQLite database.
    pub fn new(db_path: PathBuf, app_dir: PathBuf) -> Result<Self, DbError> {
        if let Some(parent) = db_path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        std::fs::create_dir_all(&app_dir)?;

        for dir in Self::MANAGED_DIRS {
            std::fs::create_dir_all(app_dir.join(dir))?;
        }

        // Startup sweep: clean up any stale staged files from previous abrupt crashes
        Self::purge_trash_dir(&app_dir);

        let mut conn = Connection::open(&db_path)?;
        initialize_schema(&mut conn)?;

        Ok(Self {
            conn: Mutex::new(conn),
            db_path,
            app_dir,
        })
    }

    /// Initialize an in-memory DbManager (ideal for testing).
    pub fn new_in_memory(app_dir: PathBuf) -> Result<Self, DbError> {
        std::fs::create_dir_all(&app_dir)?;
        for dir in Self::MANAGED_DIRS {
            std::fs::create_dir_all(app_dir.join(dir))?;
        }

        Self::purge_trash_dir(&app_dir);

        let mut conn = Connection::open_in_memory()?;
        initialize_schema(&mut conn)?;

        Ok(Self {
            conn: Mutex::new(conn),
            db_path: PathBuf::from(":memory:"),
            app_dir,
        })
    }

    fn get_conn(&self) -> Result<std::sync::MutexGuard<'_, Connection>, DbError> {
        self.conn
            .lock()
            .map_err(|e| DbError::LockPoisoned(e.to_string()))
    }

    /// Resolve relative storage_path safely against the application directory.
    pub fn resolve_storage_path(&self, relative_path: &str) -> PathBuf {
        let clean_path = relative_path.trim_start_matches(['/', '\\']);
        self.app_dir.join(clean_path)
    }

    /// Startup & maintenance sweeper: purges all files and subdirectories inside cache/trash.
    pub fn purge_trash_dir(app_dir: &std::path::Path) {
        let trash_dir = app_dir.join("cache/trash");
        if trash_dir.exists() {
            if let Ok(entries) = std::fs::read_dir(&trash_dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.is_dir() {
                        let _ = std::fs::remove_dir_all(path);
                    } else {
                        let _ = std::fs::remove_file(path);
                    }
                }
            }
        }
    }

    /// Manually trigger a purge of the cache/trash staging directory.
    pub fn purge_trash(&self) -> Result<(), DbError> {
        Self::purge_trash_dir(&self.app_dir);
        Ok(())
    }

    /// Two-phase staging helper: atomically moves files to cache/trash/{op_uuid}/.
    /// Returns (trash_operation_dir, staged_moves_list) on success.
    /// If any file move fails, already-staged files are rolled back to their original locations.
    fn stage_files_to_trash(
        &self,
        op_uuid: &str,
        relative_paths: &[String],
    ) -> Result<(PathBuf, Vec<(PathBuf, PathBuf)>), DbError> {
        let trash_dir = self.app_dir.join("cache/trash").join(op_uuid);
        std::fs::create_dir_all(&trash_dir)?;

        let mut staged_moves = Vec::new();

        for rel_path in relative_paths {
            let abs_src = self.resolve_storage_path(rel_path);
            if abs_src.exists() {
                let abs_dest = trash_dir.join(rel_path);
                if let Some(parent) = abs_dest.parent() {
                    std::fs::create_dir_all(parent)?;
                }

                if let Err(err) = std::fs::rename(&abs_src, &abs_dest) {
                    if err.kind() == std::io::ErrorKind::NotFound {
                        // File was unlinked concurrently, treat as non-fatal
                        continue;
                    }

                    // Staging failed: rollback all previously staged files
                    Self::restore_staged_files(&staged_moves, &trash_dir);
                    return Err(DbError::Io(err));
                }

                staged_moves.push((abs_src, abs_dest));
            }
        }

        Ok((trash_dir, staged_moves))
    }

    /// Restores staged files back to their original source paths and deletes the trash op folder.
    fn restore_staged_files(staged_moves: &[(PathBuf, PathBuf)], trash_dir: &std::path::Path) {
        for (src, dest) in staged_moves.iter().rev() {
            if dest.exists() {
                if let Some(parent) = src.parent() {
                    let _ = std::fs::create_dir_all(parent);
                }
                let _ = std::fs::rename(dest, src);
            }
        }
        let _ = std::fs::remove_dir_all(trash_dir);
    }

    /// Cleans up empty parent directories left behind after successful file removal.
    fn clean_empty_parent_dirs(&self, staged_moves: &[(PathBuf, PathBuf)]) {
        for (abs_src, _) in staged_moves {
            if let Some(parent) = abs_src.parent() {
                if parent.starts_with(&self.app_dir) && parent != self.app_dir {
                    let is_empty = std::fs::read_dir(parent)
                        .map(|mut it| it.next().is_none())
                        .unwrap_or(false);
                    if is_empty {
                        let _ = std::fs::remove_dir(parent);
                    }
                }
            }
        }
    }

    /// Cleans up thread-specific directories across all managed storage categories if empty.
    fn clean_thread_managed_dirs(&self, thread_id: &str) {
        let categories = [
            "notes/markdown",
            "notes/latex",
            "notes/misc",
            "materials/pdf",
            "materials/slides",
            "materials/docs",
            "materials/archives",
            "media/images",
            "media/audio",
        ];
        for cat in categories {
            let thread_dir = self.app_dir.join(cat).join(thread_id);
            if thread_dir.exists() {
                let is_empty = std::fs::read_dir(&thread_dir)
                    .map(|mut it| it.next().is_none())
                    .unwrap_or(false);
                if is_empty {
                    let _ = std::fs::remove_dir(&thread_dir);
                }
            }
        }
    }

    /// Current SQLite schema version from PRAGMA user_version.
    pub fn get_schema_version(&self) -> Result<i32, DbError> {
        let conn = self.get_conn()?;
        crate::db::migrations::get_schema_version(&conn)
    }

    /// Audit list of all applied migrations.
    pub fn get_applied_migrations(&self) -> Result<Vec<crate::db::models::MigrationInfo>, DbError> {
        let conn = self.get_conn()?;
        crate::db::migrations::get_applied_migrations(&conn)
    }

    // =========================================================================
    // SUBJECTS CRUD
    // =========================================================================

    pub fn create_subject(&self, input: CreateSubjectInput) -> Result<Subject, DbError> {
        if input.id.trim().is_empty() {
            return Err(DbError::Validation("Subject ID cannot be empty".to_string()));
        }
        if input.name.trim().is_empty() {
            return Err(DbError::Validation("Subject name cannot be empty".to_string()));
        }

        let conn = self.get_conn()?;
        conn.execute(
            "INSERT INTO subjects (id, name, semester) VALUES (?1, ?2, ?3)",
            params![input.id, input.name, input.semester],
        )?;

        Ok(Subject {
            id: input.id,
            name: input.name,
            semester: input.semester,
        })
    }

    pub fn get_subjects(&self) -> Result<Vec<Subject>, DbError> {
        let conn = self.get_conn()?;
        let mut stmt = conn.prepare("SELECT id, name, semester FROM subjects ORDER BY id ASC")?;
        let rows = stmt.query_map([], |row| {
            Ok(Subject {
                id: row.get(0)?,
                name: row.get(1)?,
                semester: row.get(2)?,
            })
        })?;

        let mut subjects = Vec::new();
        for subject in rows {
            subjects.push(subject?);
        }
        Ok(subjects)
    }

    pub fn get_subject(&self, id: &str) -> Result<Subject, DbError> {
        let conn = self.get_conn()?;
        let mut stmt = conn.prepare("SELECT id, name, semester FROM subjects WHERE id = ?1")?;
        let subject = stmt
            .query_row(params![id], |row| {
                Ok(Subject {
                    id: row.get(0)?,
                    name: row.get(1)?,
                    semester: row.get(2)?,
                })
            })
            .optional()?;

        subject.ok_or_else(|| DbError::NotFound(format!("Subject '{}' not found", id)))
    }

    pub fn update_subject(&self, id: &str, input: UpdateSubjectInput) -> Result<Subject, DbError> {
        let current = self.get_subject(id)?;
        let new_name = input.name.unwrap_or(current.name);
        let new_semester = input.semester.unwrap_or(current.semester);

        let conn = self.get_conn()?;
        conn.execute(
            "UPDATE subjects SET name = ?1, semester = ?2 WHERE id = ?3",
            params![new_name, new_semester, id],
        )?;

        Ok(Subject {
            id: id.to_string(),
            name: new_name,
            semester: new_semester,
        })
    }

    pub fn delete_subject(&self, id: &str, clean_files: bool) -> Result<(), DbError> {
        let _ = self.get_subject(id)?;

        if !clean_files {
            let conn = self.get_conn()?;
            let affected = conn.execute("DELETE FROM subjects WHERE id = ?1", params![id])?;
            if affected == 0 {
                return Err(DbError::NotFound(format!("Subject '{}' not found", id)));
            }
            return Ok(());
        }

        // 1. Query child file paths and thread IDs before SQL delete
        let (paths, thread_ids): (Vec<String>, Vec<String>) = {
            let conn = self.get_conn()?;
            let mut stmt_files = conn.prepare(
                "SELECT f.storage_path FROM files f 
                 JOIN threads t ON f.thread_id = t.id 
                 WHERE t.subject_id = ?1",
            )?;
            let paths = stmt_files
                .query_map(params![id], |row| row.get(0))?
                .filter_map(|r| r.ok())
                .collect();

            let mut stmt_threads = conn.prepare("SELECT id FROM threads WHERE subject_id = ?1")?;
            let thread_ids = stmt_threads
                .query_map(params![id], |row| row.get(0))?
                .filter_map(|r| r.ok())
                .collect();

            (paths, thread_ids)
        };

        // 2. Stage files in cache/trash
        let op_uuid = Uuid::new_v4().to_string();
        let (trash_dir, staged) = self.stage_files_to_trash(&op_uuid, &paths)?;

        // 3. Transactional DB deletion (cascades threads -> files, tasks)
        let res = (|| -> Result<(), DbError> {
            let mut conn = self.get_conn()?;
            let tx = conn.transaction()?;
            let affected = tx.execute("DELETE FROM subjects WHERE id = ?1", params![id])?;
            if affected == 0 {
                return Err(DbError::NotFound(format!("Subject '{}' not found", id)));
            }
            tx.commit()?;
            Ok(())
        })();

        match res {
            Ok(()) => {
                let _ = std::fs::remove_dir_all(&trash_dir);
                self.clean_empty_parent_dirs(&staged);
                for thread_id in thread_ids {
                    self.clean_thread_managed_dirs(&thread_id);
                }
                Ok(())
            }
            Err(e) => {
                Self::restore_staged_files(&staged, &trash_dir);
                Err(e)
            }
        }
    }

    // =========================================================================
    // THREADS CRUD
    // =========================================================================

    pub fn create_thread(&self, input: CreateThreadInput) -> Result<Thread, DbError> {
        let id = input.id.unwrap_or_else(|| Uuid::new_v4().to_string());
        let created_at = input
            .created_at
            .unwrap_or_else(|| Utc::now().to_rfc3339());

        if input.title.trim().is_empty() {
            return Err(DbError::Validation("Thread title cannot be empty".to_string()));
        }

        let conn = self.get_conn()?;
        conn.execute(
            "INSERT INTO threads (id, subject_id, title, description, created_at) VALUES (?1, ?2, ?3, ?4, ?5)",
            params![id, input.subject_id, input.title, input.description, created_at],
        )?;

        Ok(Thread {
            id,
            subject_id: input.subject_id,
            title: input.title,
            description: input.description,
            created_at,
        })
    }

    pub fn get_threads_by_subject(&self, subject_id: &str) -> Result<Vec<Thread>, DbError> {
        let conn = self.get_conn()?;
        let mut stmt = conn.prepare(
            "SELECT id, subject_id, title, description, created_at FROM threads WHERE subject_id = ?1 ORDER BY created_at DESC",
        )?;
        let rows = stmt.query_map(params![subject_id], |row| {
            Ok(Thread {
                id: row.get(0)?,
                subject_id: row.get(1)?,
                title: row.get(2)?,
                description: row.get(3)?,
                created_at: row.get(4)?,
            })
        })?;

        let mut threads = Vec::new();
        for thread in rows {
            threads.push(thread?);
        }
        Ok(threads)
    }

    pub fn get_thread(&self, id: &str) -> Result<Thread, DbError> {
        let conn = self.get_conn()?;
        let mut stmt = conn.prepare(
            "SELECT id, subject_id, title, description, created_at FROM threads WHERE id = ?1",
        )?;
        let thread = stmt
            .query_row(params![id], |row| {
                Ok(Thread {
                    id: row.get(0)?,
                    subject_id: row.get(1)?,
                    title: row.get(2)?,
                    description: row.get(3)?,
                    created_at: row.get(4)?,
                })
            })
            .optional()?;

        thread.ok_or_else(|| DbError::NotFound(format!("Thread '{}' not found", id)))
    }

    pub fn update_thread(&self, id: &str, input: UpdateThreadInput) -> Result<Thread, DbError> {
        let current = self.get_thread(id)?;
        let new_title = input.title.unwrap_or(current.title);
        let new_description = match input.description {
            Some(desc) => Some(desc),
            None => current.description,
        };

        let conn = self.get_conn()?;
        conn.execute(
            "UPDATE threads SET title = ?1, description = ?2 WHERE id = ?3",
            params![new_title, new_description, id],
        )?;

        Ok(Thread {
            id: id.to_string(),
            subject_id: current.subject_id,
            title: new_title,
            description: new_description,
            created_at: current.created_at,
        })
    }

    pub fn delete_thread(&self, id: &str, clean_files: bool) -> Result<(), DbError> {
        let _ = self.get_thread(id)?;

        if !clean_files {
            let conn = self.get_conn()?;
            let affected = conn.execute("DELETE FROM threads WHERE id = ?1", params![id])?;
            if affected == 0 {
                return Err(DbError::NotFound(format!("Thread '{}' not found", id)));
            }
            return Ok(());
        }

        // 1. Query child file paths before SQL delete
        let paths: Vec<String> = {
            let conn = self.get_conn()?;
            let mut stmt = conn.prepare("SELECT storage_path FROM files WHERE thread_id = ?1")?;
            let rows = stmt
                .query_map(params![id], |row| row.get(0))?
                .filter_map(|r| r.ok())
                .collect();
            rows
        };

        // 2. Stage files in cache/trash
        let op_uuid = Uuid::new_v4().to_string();
        let (trash_dir, staged) = self.stage_files_to_trash(&op_uuid, &paths)?;

        // 3. Transactional DB deletion (cascades files and tasks)
        let res = (|| -> Result<(), DbError> {
            let mut conn = self.get_conn()?;
            let tx = conn.transaction()?;
            let affected = tx.execute("DELETE FROM threads WHERE id = ?1", params![id])?;
            if affected == 0 {
                return Err(DbError::NotFound(format!("Thread '{}' not found", id)));
            }
            tx.commit()?;
            Ok(())
        })();

        match res {
            Ok(()) => {
                let _ = std::fs::remove_dir_all(&trash_dir);
                self.clean_empty_parent_dirs(&staged);
                self.clean_thread_managed_dirs(id);
                Ok(())
            }
            Err(e) => {
                Self::restore_staged_files(&staged, &trash_dir);
                Err(e)
            }
        }
    }

    // =========================================================================
    // FILES CRUD & PACKAGE-MANAGER-STYLE FILE STORAGE
    // =========================================================================

    pub fn create_file_record(&self, input: CreateFileInput) -> Result<FileRecord, DbError> {
        let id = input.id.unwrap_or_else(|| Uuid::new_v4().to_string());
        let is_material_int = if input.is_material { 1 } else { 0 };

        let conn = self.get_conn()?;
        conn.execute(
            "INSERT INTO files (id, thread_id, file_name, file_type, storage_path, is_material) 
             VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![
                id,
                input.thread_id,
                input.file_name,
                input.file_type,
                input.storage_path,
                is_material_int
            ],
        )?;

        Ok(FileRecord {
            id,
            thread_id: input.thread_id,
            file_name: input.file_name,
            file_type: input.file_type,
            storage_path: input.storage_path,
            is_material: input.is_material,
        })
    }

    /// Determine strict destination directory path based on file_type and is_material,
    /// adhering to the Linux package manager architecture in production_directory_tree.json.
    pub fn compute_storage_path(
        thread_id: &str,
        file_id: &str,
        file_name: &str,
        file_type: &str,
        is_material: bool,
    ) -> String {
        let ext = file_type.trim_start_matches('.').to_lowercase();
        let target_dir = if ["png", "jpg", "jpeg", "webp", "svg", "gif"].contains(&ext.as_str()) {
            "media/images"
        } else if ["mp3", "wav", "m4a", "ogg", "aac"].contains(&ext.as_str()) {
            "media/audio"
        } else if is_material {
            match ext.as_str() {
                "pdf" => "materials/pdf",
                "pptx" | "ppt" | "odp" | "key" => "materials/slides",
                "zip" | "tar" | "gz" | "xz" | "7z" | "rar" => "materials/archives",
                _ => "materials/docs",
            }
        } else {
            match ext.as_str() {
                "md" | "markdown" => "notes/markdown",
                "tex" | "bib" | "sty" | "cls" => "notes/latex",
                _ => "notes/misc",
            }
        };

        format!("{}/{}/{}_{}", target_dir, thread_id, file_id, file_name)
    }

    /// Linux-package-manager inspired: writes physical file content into managed storage
    /// and records the file metadata in the SQLite database atomically.
    pub fn save_file(
        &self,
        thread_id: &str,
        file_name: &str,
        file_type: &str,
        is_material: bool,
        data: &[u8],
    ) -> Result<FileRecord, DbError> {
        // Verify thread exists
        let _ = self.get_thread(thread_id)?;

        let file_id = Uuid::new_v4().to_string();
        let relative_path = Self::compute_storage_path(thread_id, &file_id, file_name, file_type, is_material);
        let abs_path = self.resolve_storage_path(&relative_path);

        if let Some(parent) = abs_path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        std::fs::write(&abs_path, data)?;

        let record_res = self.create_file_record(CreateFileInput {
            id: Some(file_id),
            thread_id: thread_id.to_string(),
            file_name: file_name.to_string(),
            file_type: file_type.to_string(),
            storage_path: relative_path,
            is_material,
        });

        match record_res {
            Ok(record) => Ok(record),
            Err(e) => {
                // Atomic cleanup: remove written file if DB insertion failed
                let _ = std::fs::remove_file(abs_path);
                Err(e)
            }
        }
    }

    /// Fast zero-IPC-memory disk copy: imports an existing external file from the filesystem
    /// (e.g. large PDFs, slides, archives) directly into Noteman's managed storage.
    pub fn import_file(
        &self,
        thread_id: &str,
        source_path: &str,
        file_name: &str,
        file_type: &str,
        is_material: bool,
    ) -> Result<FileRecord, DbError> {
        // Verify thread exists
        let _ = self.get_thread(thread_id)?;

        let src = PathBuf::from(source_path);
        if !src.is_file() {
            return Err(DbError::NotFound(format!(
                "Source file '{}' not found",
                source_path
            )));
        }

        let file_id = Uuid::new_v4().to_string();
        let relative_path = Self::compute_storage_path(thread_id, &file_id, file_name, file_type, is_material);
        let abs_dest = self.resolve_storage_path(&relative_path);

        if let Some(parent) = abs_dest.parent() {
            std::fs::create_dir_all(parent)?;
        }

        // Fast zero-IPC disk copy
        std::fs::copy(&src, &abs_dest)?;

        let record_res = self.create_file_record(CreateFileInput {
            id: Some(file_id),
            thread_id: thread_id.to_string(),
            file_name: file_name.to_string(),
            file_type: file_type.to_string(),
            storage_path: relative_path,
            is_material,
        });

        match record_res {
            Ok(record) => Ok(record),
            Err(e) => {
                let _ = std::fs::remove_file(abs_dest);
                Err(e)
            }
        }
    }

    pub fn read_file(&self, file_id: &str) -> Result<Vec<u8>, DbError> {
        let file_record = self.get_file(file_id)?;
        let abs_path = self.resolve_storage_path(&file_record.storage_path);
        let bytes = std::fs::read(abs_path)?;
        Ok(bytes)
    }

    /// Atomically renames a file on disk and updates its file_name and storage_path in SQLite.
    ///
    /// - Strictly recomputes storage_path according to production routing matrix rules.
    /// - If the new_name does not supply an extension, preserves the existing file extension.
    /// - Verifies that the source physical file exists on disk.
    /// - Prevents overwriting distinct files on disk.
    /// - Transactional: if physical rename fails, DB is not modified; if DB update/commit fails,
    ///   the physical file rename is reverted back to the original path.
    /// - Cleans up empty parent directories if the file moved across categories.
    pub fn rename_file(&self, file_id: &str, new_name: &str) -> Result<FileRecord, DbError> {
        let clean_name = new_name.trim();
        let clean_name = clean_name
            .strip_prefix(&format!("{}_", file_id))
            .unwrap_or(clean_name);

        if clean_name.is_empty() {
            return Err(DbError::Validation("File name cannot be empty".to_string()));
        }

        if clean_name.contains('/')
            || clean_name.contains('\\')
            || clean_name.contains('\0')
            || clean_name == "."
            || clean_name == ".."
        {
            return Err(DbError::Validation(
                "File name contains invalid characters or path separators".to_string(),
            ));
        }

        let mut conn = self.get_conn()?;

        // Retrieve existing file record
        let current_file: FileRecord = {
            let mut stmt = conn.prepare(
                "SELECT id, thread_id, file_name, file_type, storage_path, is_material 
                 FROM files WHERE id = ?1",
            )?;
            let file = stmt
                .query_row(params![file_id], |row| {
                    let is_material_int: i32 = row.get(5)?;
                    Ok(FileRecord {
                        id: row.get(0)?,
                        thread_id: row.get(1)?,
                        file_name: row.get(2)?,
                        file_type: row.get(3)?,
                        storage_path: row.get(4)?,
                        is_material: is_material_int == 1,
                    })
                })
                .optional()?;

            file.ok_or_else(|| DbError::NotFound(format!("File '{}' not found", file_id)))?
        };

        // Determine effective file_name and file_type
        let (effective_name, effective_type) = if !current_file.file_type.is_empty()
            && clean_name
                .to_lowercase()
                .ends_with(&current_file.file_type.to_lowercase())
        {
            (clean_name.to_string(), current_file.file_type.clone())
        } else {
            match std::path::Path::new(clean_name)
                .extension()
                .and_then(|e| e.to_str())
            {
                Some(ext) => (clean_name.to_string(), format!(".{}", ext)),
                None => {
                    if !current_file.file_type.is_empty() {
                        let ext = if current_file.file_type.starts_with('.') {
                            current_file.file_type.clone()
                        } else {
                            format!(".{}", current_file.file_type)
                        };
                        (format!("{}{}", clean_name, ext), ext)
                    } else {
                        (clean_name.to_string(), current_file.file_type.clone())
                    }
                }
            }
        };

        let old_abs_path = self.resolve_storage_path(&current_file.storage_path);
        let new_storage_path = Self::compute_storage_path(
            &current_file.thread_id,
            &current_file.id,
            &effective_name,
            &effective_type,
            current_file.is_material,
        );
        let new_abs_path = self.resolve_storage_path(&new_storage_path);

        // Check if nothing changed
        if current_file.file_name == effective_name && current_file.storage_path == new_storage_path {
            if !old_abs_path.exists() {
                return Err(DbError::NotFound(format!(
                    "Physical file not found on disk at '{}'",
                    current_file.storage_path
                )));
            }
            return Ok(current_file);
        }

        // Source file must exist on disk
        if !old_abs_path.exists() {
            return Err(DbError::NotFound(format!(
                "Physical file not found on disk at '{}'",
                current_file.storage_path
            )));
        }

        // Avoid overwriting a different file
        if new_abs_path.exists() && new_abs_path != old_abs_path {
            return Err(DbError::Validation(format!(
                "Target file already exists on disk: '{}'",
                new_storage_path
            )));
        }

        if let Some(parent) = new_abs_path.parent() {
            std::fs::create_dir_all(parent)?;
        }

        // 1. Rename on disk
        std::fs::rename(&old_abs_path, &new_abs_path)?;

        // 2. Transactional DB update
        let tx_result = (|| -> Result<(), DbError> {
            let tx = conn.transaction()?;
            let affected = tx.execute(
                "UPDATE files SET file_name = ?1, file_type = ?2, storage_path = ?3 WHERE id = ?4",
                params![effective_name, effective_type, new_storage_path, file_id],
            )?;
            if affected == 0 {
                return Err(DbError::NotFound(format!("File '{}' not found", file_id)));
            }
            tx.commit()?;
            Ok(())
        })();

        match tx_result {
            Ok(()) => {
                // If old directory is empty and different from new directory, clean it up
                if let Some(old_parent) = old_abs_path.parent() {
                    if let Some(new_parent) = new_abs_path.parent() {
                        if old_parent != new_parent
                            && old_parent.starts_with(&self.app_dir)
                            && old_parent != self.app_dir
                        {
                            let is_empty = std::fs::read_dir(old_parent)
                                .map(|mut it| it.next().is_none())
                                .unwrap_or(false);
                            if is_empty {
                                let _ = std::fs::remove_dir(old_parent);
                            }
                        }
                    }
                }

                Ok(FileRecord {
                    id: current_file.id,
                    thread_id: current_file.thread_id,
                    file_name: effective_name,
                    file_type: effective_type,
                    storage_path: new_storage_path,
                    is_material: current_file.is_material,
                })
            }
            Err(e) => {
                // Atomic rollback on failure: move physical file back to old location
                let _ = std::fs::rename(&new_abs_path, &old_abs_path);
                Err(e)
            }
        }
    }

    pub fn get_files_by_thread(&self, thread_id: &str) -> Result<Vec<FileRecord>, DbError> {
        let conn = self.get_conn()?;
        let mut stmt = conn.prepare(
            "SELECT id, thread_id, file_name, file_type, storage_path, is_material 
             FROM files WHERE thread_id = ?1 ORDER BY file_name ASC",
        )?;
        let rows = stmt.query_map(params![thread_id], |row| {
            let is_material_int: i32 = row.get(5)?;
            Ok(FileRecord {
                id: row.get(0)?,
                thread_id: row.get(1)?,
                file_name: row.get(2)?,
                file_type: row.get(3)?,
                storage_path: row.get(4)?,
                is_material: is_material_int == 1,
            })
        })?;

        let mut files = Vec::new();
        for file in rows {
            files.push(file?);
        }
        Ok(files)
    }

    pub fn get_file(&self, id: &str) -> Result<FileRecord, DbError> {
        let conn = self.get_conn()?;
        let mut stmt = conn.prepare(
            "SELECT id, thread_id, file_name, file_type, storage_path, is_material 
             FROM files WHERE id = ?1",
        )?;
        let file = stmt
            .query_row(params![id], |row| {
                let is_material_int: i32 = row.get(5)?;
                Ok(FileRecord {
                    id: row.get(0)?,
                    thread_id: row.get(1)?,
                    file_name: row.get(2)?,
                    file_type: row.get(3)?,
                    storage_path: row.get(4)?,
                    is_material: is_material_int == 1,
                })
            })
            .optional()?;

        file.ok_or_else(|| DbError::NotFound(format!("File '{}' not found", id)))
    }

    pub fn delete_file_record(&self, id: &str, remove_physical_file: bool) -> Result<(), DbError> {
        let file = self.get_file(id)?;

        if !remove_physical_file {
            let conn = self.get_conn()?;
            let affected = conn.execute("DELETE FROM files WHERE id = ?1", params![id])?;
            if affected == 0 {
                return Err(DbError::NotFound(format!("File '{}' not found", id)));
            }
            return Ok(());
        }

        let op_uuid = Uuid::new_v4().to_string();
        let (trash_dir, staged) = self.stage_files_to_trash(&op_uuid, &[file.storage_path])?;

        let res = (|| -> Result<(), DbError> {
            let mut conn = self.get_conn()?;
            let tx = conn.transaction()?;
            let affected = tx.execute("DELETE FROM files WHERE id = ?1", params![id])?;
            if affected == 0 {
                return Err(DbError::NotFound(format!("File '{}' not found", id)));
            }
            tx.commit()?;
            Ok(())
        })();

        match res {
            Ok(()) => {
                let _ = std::fs::remove_dir_all(&trash_dir);
                self.clean_empty_parent_dirs(&staged);
                Ok(())
            }
            Err(e) => {
                Self::restore_staged_files(&staged, &trash_dir);
                Err(e)
            }
        }
    }

    /// Package manager style integrity check: verifies every database file record
    /// corresponds to a real file on disk.
    pub fn verify_files_integrity(&self) -> Result<Vec<FileIntegrityItem>, DbError> {
        let conn = self.get_conn()?;
        let mut stmt = conn.prepare(
            "SELECT id, thread_id, file_name, storage_path FROM files ORDER BY id ASC",
        )?;
        let rows = stmt.query_map([], |row| {
            Ok((
                row.get::<_, String>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
                row.get::<_, String>(3)?,
            ))
        })?;

        let mut reports = Vec::new();
        for item in rows {
            let (file_id, thread_id, file_name, storage_path) = item?;
            let abs_path = self.resolve_storage_path(&storage_path);
            let exists = abs_path.is_file();
            let size = if exists {
                std::fs::metadata(&abs_path).map(|m| m.len()).ok()
            } else {
                None
            };

            reports.push(FileIntegrityItem {
                file_id,
                thread_id,
                file_name,
                storage_path,
                exists_on_disk: exists,
                file_size_bytes: size,
            });
        }
        Ok(reports)
    }

    // =========================================================================
    // TASKS CRUD
    // =========================================================================

    pub fn create_task(&self, input: CreateTaskInput) -> Result<Task, DbError> {
        let id = input.id.unwrap_or_else(|| Uuid::new_v4().to_string());
        if input.title.trim().is_empty() {
            return Err(DbError::Validation("Task title cannot be empty".to_string()));
        }

        let conn = self.get_conn()?;
        conn.execute(
            "INSERT INTO tasks (id, thread_id, title, deadline) VALUES (?1, ?2, ?3, ?4)",
            params![id, input.thread_id, input.title, input.deadline],
        )?;

        Ok(Task {
            id,
            thread_id: input.thread_id,
            title: input.title,
            deadline: input.deadline,
        })
    }

    pub fn get_tasks_by_thread(&self, thread_id: &str) -> Result<Vec<Task>, DbError> {
        let conn = self.get_conn()?;
        let mut stmt = conn.prepare(
            "SELECT id, thread_id, title, deadline FROM tasks WHERE thread_id = ?1 ORDER BY rowid ASC",
        )?;
        let rows = stmt.query_map(params![thread_id], |row| {
            Ok(Task {
                id: row.get(0)?,
                thread_id: row.get(1)?,
                title: row.get(2)?,
                deadline: row.get(3)?,
            })
        })?;

        let mut tasks = Vec::new();
        for task in rows {
            tasks.push(task?);
        }
        Ok(tasks)
    }

    pub fn get_task(&self, id: &str) -> Result<Task, DbError> {
        let conn = self.get_conn()?;
        let mut stmt = conn.prepare("SELECT id, thread_id, title, deadline FROM tasks WHERE id = ?1")?;
        let task = stmt
            .query_row(params![id], |row| {
                Ok(Task {
                    id: row.get(0)?,
                    thread_id: row.get(1)?,
                    title: row.get(2)?,
                    deadline: row.get(3)?,
                })
            })
            .optional()?;

        task.ok_or_else(|| DbError::NotFound(format!("Task '{}' not found", id)))
    }

    pub fn update_task(&self, id: &str, input: UpdateTaskInput) -> Result<Task, DbError> {
        let current = self.get_task(id)?;
        let new_title = input.title.unwrap_or(current.title);
        let new_deadline = match input.deadline {
            Some(d) => Some(d),
            None => current.deadline,
        };

        let conn = self.get_conn()?;
        conn.execute(
            "UPDATE tasks SET title = ?1, deadline = ?2 WHERE id = ?3",
            params![new_title, new_deadline, id],
        )?;

        Ok(Task {
            id: id.to_string(),
            thread_id: current.thread_id,
            title: new_title,
            deadline: new_deadline,
        })
    }

    pub fn delete_task(&self, id: &str) -> Result<(), DbError> {
        let conn = self.get_conn()?;
        let affected = conn.execute("DELETE FROM tasks WHERE id = ?1", params![id])?;
        if affected == 0 {
            return Err(DbError::NotFound(format!("Task '{}' not found", id)));
        }
        Ok(())
    }

    // =========================================================================
    // SYSTEM STATS & HEALTH
    // =========================================================================

    pub fn get_stats(&self) -> Result<DbStats, DbError> {
        let conn = self.get_conn()?;
        let subjects_count: i64 =
            conn.query_row("SELECT COUNT(*) FROM subjects", [], |r| r.get(0))?;
        let threads_count: i64 =
            conn.query_row("SELECT COUNT(*) FROM threads", [], |r| r.get(0))?;
        let files_count: i64 =
            conn.query_row("SELECT COUNT(*) FROM files", [], |r| r.get(0))?;
        let tasks_count: i64 =
            conn.query_row("SELECT COUNT(*) FROM tasks", [], |r| r.get(0))?;
        let schema_version: i32 =
            crate::db::migrations::get_schema_version(&conn)?;

        Ok(DbStats {
            subjects_count,
            threads_count,
            files_count,
            tasks_count,
            schema_version,
            db_path: self.db_path.to_string_lossy().to_string(),
            app_dir: self.app_dir.to_string_lossy().to_string(),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_db_manager_crud_and_cascades() {
        let temp_dir = std::env::temp_dir().join(format!("noteman_test_{}", Uuid::new_v4()));
        let manager = DbManager::new_in_memory(temp_dir.clone()).expect("init db");

        // 1. Create Subject
        let subject = manager
            .create_subject(CreateSubjectInput {
                id: "IT3080".to_string(),
                name: "Mạng máy tính".to_string(),
                semester: "20261".to_string(),
            })
            .expect("create subject");
        assert_eq!(subject.id, "IT3080");

        let subjects = manager.get_subjects().expect("get subjects");
        assert_eq!(subjects.len(), 1);

        // 2. Create Thread
        let thread = manager
            .create_thread(CreateThreadInput {
                id: Some("thread-1".to_string()),
                subject_id: "IT3080".to_string(),
                title: "Buổi 2: Rust và Tauri".to_string(),
                description: Some("Short description about the lesson".to_string()),
                created_at: None,
            })
            .expect("create thread");
        assert_eq!(thread.title, "Buổi 2: Rust và Tauri");

        // 3. Save File Content (package manager style)
        let sample_content = b"# Rust and Tauri notes\nThis is a test note.";
        let file_rec = manager
            .save_file("thread-1", "notes.md", ".md", false, sample_content)
            .expect("save file");
        assert_eq!(file_rec.file_name, "notes.md");
        assert_eq!(file_rec.file_type, ".md");
        assert!(!file_rec.is_material);

        let read_back = manager.read_file(&file_rec.id).expect("read file");
        assert_eq!(read_back, sample_content);

        // 4. File Integrity check
        let integrity = manager.verify_files_integrity().expect("verify integrity");
        assert_eq!(integrity.len(), 1);
        assert!(integrity[0].exists_on_disk);

        // 5. Create Task
        let task = manager
            .create_task(CreateTaskInput {
                id: Some("task-1".to_string()),
                thread_id: "thread-1".to_string(),
                title: "Làm bài tập mạng 1".to_string(),
                deadline: Some("2026-10-05T23:59:59Z".to_string()),
            })
            .expect("create task");
        assert_eq!(task.title, "Làm bài tập mạng 1");

        // 6. Check Stats & Schema Version
        let stats = manager.get_stats().expect("stats");
        assert_eq!(stats.subjects_count, 1);
        assert_eq!(stats.threads_count, 1);
        assert_eq!(stats.files_count, 1);
        assert_eq!(stats.tasks_count, 1);
        assert_eq!(stats.schema_version, 1);

        // 7. Test Cascade Delete with Two-Phase Staging: deleting subject should delete thread, file, task
        manager.delete_subject("IT3080", true).expect("delete subject");
        let stats_after = manager.get_stats().expect("stats after");
        assert_eq!(stats_after.subjects_count, 0);
        assert_eq!(stats_after.threads_count, 0);
        assert_eq!(stats_after.files_count, 0);
        assert_eq!(stats_after.tasks_count, 0);

        // Verify trash staging directory was completely purged
        let trash_dir = temp_dir.join("cache/trash");
        let trash_empty = if trash_dir.exists() {
            std::fs::read_dir(&trash_dir).map(|mut it| it.next().is_none()).unwrap_or(true)
        } else {
            true
        };
        assert!(trash_empty);

        // Cleanup temp dir
        let _ = std::fs::remove_dir_all(temp_dir);
    }

    #[test]
    fn test_foreign_key_constraints() {
        let temp_dir = std::env::temp_dir().join(format!("noteman_test_fk_{}", Uuid::new_v4()));
        let manager = DbManager::new_in_memory(temp_dir.clone()).expect("init db");

        // Attempt to create thread without subject should fail due to foreign key
        let res = manager.create_thread(CreateThreadInput {
            id: Some("thread-nonexistent".to_string()),
            subject_id: "NO_SUCH_SUBJECT".to_string(),
            title: "Test invalid subject".to_string(),
            description: None,
            created_at: None,
        });
        assert!(res.is_err());

        // Cleanup
        let _ = std::fs::remove_dir_all(temp_dir);
    }

    #[test]
    fn test_updates_and_integrity_check() {
        let temp_dir = std::env::temp_dir().join(format!("noteman_test_upd_{}", Uuid::new_v4()));
        let manager = DbManager::new_in_memory(temp_dir.clone()).expect("init db");

        // Create subject
        manager
            .create_subject(CreateSubjectInput {
                id: "MATH101".to_string(),
                name: "Calculus I".to_string(),
                semester: "20261".to_string(),
            })
            .expect("create subject");

        // Update subject
        let updated_subj = manager
            .update_subject(
                "MATH101",
                UpdateSubjectInput {
                    name: Some("Calculus I - Advanced".to_string()),
                    semester: None,
                },
            )
            .expect("update subject");
        assert_eq!(updated_subj.name, "Calculus I - Advanced");
        assert_eq!(updated_subj.semester, "20261");

        // Create thread
        let thread = manager
            .create_thread(CreateThreadInput {
                id: Some("thread-math-1".to_string()),
                subject_id: "MATH101".to_string(),
                title: "Derivatives".to_string(),
                description: Some("Lesson on derivatives".to_string()),
                created_at: None,
            })
            .expect("create thread");

        // Update thread
        let updated_thread = manager
            .update_thread(
                &thread.id,
                UpdateThreadInput {
                    title: Some("Derivatives & Integrals".to_string()),
                    description: None,
                },
            )
            .expect("update thread");
        assert_eq!(updated_thread.title, "Derivatives & Integrals");

        // Create task and update task
        let task = manager
            .create_task(CreateTaskInput {
                id: None,
                thread_id: thread.id.clone(),
                title: "Problem set 1".to_string(),
                deadline: None,
            })
            .expect("create task");

        let updated_task = manager
            .update_task(
                &task.id,
                UpdateTaskInput {
                    title: Some("Problem set 1 - Final".to_string()),
                    deadline: Some("2026-10-10".to_string()),
                },
            )
            .expect("update task");
        assert_eq!(updated_task.title, "Problem set 1 - Final");
        assert_eq!(updated_task.deadline.as_deref(), Some("2026-10-10"));

        // Save a file
        let file = manager
            .save_file(&thread.id, "homework.tex", ".tex", true, b"\\documentclass{article}")
            .expect("save file");
        assert!(file.is_material);

        // Delete physical file manually to test integrity report
        let abs_path = manager.resolve_storage_path(&file.storage_path);
        std::fs::remove_file(&abs_path).expect("remove file manually");

        let integrity = manager.verify_files_integrity().expect("integrity check");
        assert_eq!(integrity.len(), 1);
        assert!(!integrity[0].exists_on_disk);

        // Cleanup
        let _ = std::fs::remove_dir_all(temp_dir);
    }

    #[test]
    fn test_storage_routing_matrix() {
        let path_md = DbManager::compute_storage_path("t1", "f1", "notes.md", ".md", false);
        assert_eq!(path_md, "notes/markdown/t1/f1_notes.md");

        let path_tex = DbManager::compute_storage_path("t1", "f2", "paper.tex", ".tex", false);
        assert_eq!(path_tex, "notes/latex/t1/f2_paper.tex");

        let path_pdf = DbManager::compute_storage_path("t1", "f3", "slide.pdf", ".pdf", true);
        assert_eq!(path_pdf, "materials/pdf/t1/f3_slide.pdf");

        let path_slides = DbManager::compute_storage_path("t1", "f4", "lecture.pptx", ".pptx", true);
        assert_eq!(path_slides, "materials/slides/t1/f4_lecture.pptx");

        let path_doc = DbManager::compute_storage_path("t1", "f5", "spec.docx", ".docx", true);
        assert_eq!(path_doc, "materials/docs/t1/f5_spec.docx");

        let path_archive = DbManager::compute_storage_path("t1", "f6", "code.zip", ".zip", true);
        assert_eq!(path_archive, "materials/archives/t1/f6_code.zip");

        let path_img = DbManager::compute_storage_path("t1", "f7", "diagram.png", ".png", false);
        assert_eq!(path_img, "media/images/t1/f7_diagram.png");

        let path_audio = DbManager::compute_storage_path("t1", "f8", "recording.mp3", ".mp3", true);
        assert_eq!(path_audio, "media/audio/t1/f8_recording.mp3");
    }

    #[test]
    fn test_two_phase_file_deletion_and_missing_file_tolerance() {
        let temp_dir = std::env::temp_dir().join(format!("noteman_test_del_{}", Uuid::new_v4()));
        let manager = DbManager::new_in_memory(temp_dir.clone()).expect("init db");

        manager
            .create_subject(CreateSubjectInput {
                id: "PHY101".to_string(),
                name: "Physics I".to_string(),
                semester: "20261".to_string(),
            })
            .expect("create subject");

        let thread = manager
            .create_thread(CreateThreadInput {
                id: Some("thread-phy-1".to_string()),
                subject_id: "PHY101".to_string(),
                title: "Mechanics".to_string(),
                description: None,
                created_at: None,
            })
            .expect("create thread");

        // 1. Normal file creation and two-phase deletion
        let file1 = manager
            .save_file(&thread.id, "lab1.md", ".md", false, b"# Lab 1")
            .expect("save file 1");
        let abs_path1 = manager.resolve_storage_path(&file1.storage_path);
        assert!(abs_path1.exists());

        manager.delete_file_record(&file1.id, true).expect("delete file 1");
        assert!(!abs_path1.exists());
        assert!(manager.get_file(&file1.id).is_err());

        // 2. Missing file tolerance: delete physical file externally, then call delete_file_record
        let file2 = manager
            .save_file(&thread.id, "lab2.md", ".md", false, b"# Lab 2")
            .expect("save file 2");
        let abs_path2 = manager.resolve_storage_path(&file2.storage_path);
        assert!(abs_path2.exists());

        // Remove physically beforehand to simulate missing file
        std::fs::remove_file(&abs_path2).expect("remove file manually");
        assert!(!abs_path2.exists());

        // Must succeed without error (NotFound is non-fatal)
        manager.delete_file_record(&file2.id, true).expect("delete file with missing disk file");
        assert!(manager.get_file(&file2.id).is_err());

        // Cleanup
        let _ = std::fs::remove_dir_all(temp_dir);
    }

    #[test]
    fn test_import_file_from_external_path() {
        let temp_dir = std::env::temp_dir().join(format!("noteman_test_imp_{}", Uuid::new_v4()));
        let manager = DbManager::new_in_memory(temp_dir.clone()).expect("init db");

        manager
            .create_subject(CreateSubjectInput {
                id: "CS101".to_string(),
                name: "CS Intro".to_string(),
                semester: "20261".to_string(),
            })
            .expect("create subject");

        let thread = manager
            .create_thread(CreateThreadInput {
                id: Some("thread-cs-1".to_string()),
                subject_id: "CS101".to_string(),
                title: "Intro".to_string(),
                description: None,
                created_at: None,
            })
            .expect("create thread");

        // Create an external file (e.g. in /tmp or external directory)
        let external_file = temp_dir.join("external_syllabus.pdf");
        std::fs::write(&external_file, b"%PDF-1.4 sample syllabus content").expect("write external");

        // Import the file directly via disk copy
        let imported = manager
            .import_file(
                &thread.id,
                external_file.to_str().unwrap(),
                "syllabus.pdf",
                ".pdf",
                true,
            )
            .expect("import file");

        assert_eq!(imported.file_name, "syllabus.pdf");
        assert_eq!(imported.file_type, ".pdf");
        assert!(imported.is_material);

        let abs_imported = manager.resolve_storage_path(&imported.storage_path);
        assert!(abs_imported.exists());
        assert_eq!(
            std::fs::read(&abs_imported).expect("read imported"),
            b"%PDF-1.4 sample syllabus content"
        );

        // Original external file remains untouched
        assert!(external_file.exists());

        // Cleanup
        let _ = std::fs::remove_dir_all(temp_dir);
    }

    #[test]
    fn test_migrations_and_trash_sweeper() {
        let temp_dir = std::env::temp_dir().join(format!("noteman_test_mig_{}", Uuid::new_v4()));
        let manager = DbManager::new_in_memory(temp_dir.clone()).expect("init db");

        // Schema version check
        let version = manager.get_schema_version().expect("get schema version");
        assert_eq!(version, 1);

        let applied = manager.get_applied_migrations().expect("get applied migrations");
        assert_eq!(applied.len(), 1);
        assert_eq!(applied[0].version, 1);
        assert_eq!(applied[0].name, "001_initial_schema");

        // Test trash sweeper
        let fake_stale_trash = temp_dir.join("cache/trash/stale-op-123");
        std::fs::create_dir_all(&fake_stale_trash).expect("create fake trash");
        std::fs::write(fake_stale_trash.join("orphan.txt"), b"stale").expect("write orphan");
        assert!(fake_stale_trash.exists());

        manager.purge_trash().expect("purge trash");
        assert!(!fake_stale_trash.exists());

        // Cleanup
        let _ = std::fs::remove_dir_all(temp_dir);
    }

    #[test]
    fn test_rename_file_basic_and_omitted_extension() {
        let temp_dir = std::env::temp_dir().join(format!("noteman_test_rn_{}", Uuid::new_v4()));
        let manager = DbManager::new_in_memory(temp_dir.clone()).expect("init db");

        manager
            .create_subject(CreateSubjectInput {
                id: "CS201".to_string(),
                name: "Data Structures".to_string(),
                semester: "20261".to_string(),
            })
            .expect("create subject");

        let thread = manager
            .create_thread(CreateThreadInput {
                id: Some("thread-rn-1".to_string()),
                subject_id: "CS201".to_string(),
                title: "Trees".to_string(),
                description: None,
                created_at: None,
            })
            .expect("create thread");

        let sample_content = b"# Binary Search Trees\nRoot, left, right.";
        let file = manager
            .save_file(&thread.id, "trees.md", ".md", false, sample_content)
            .expect("save file");

        let old_abs_path = manager.resolve_storage_path(&file.storage_path);
        assert!(old_abs_path.exists());

        // 1. Basic rename with explicit extension
        let renamed = manager
            .rename_file(&file.id, "bst_notes.md")
            .expect("rename with extension");

        assert_eq!(renamed.id, file.id);
        assert_eq!(renamed.file_name, "bst_notes.md");
        assert_eq!(renamed.file_type, ".md");
        assert_eq!(
            renamed.storage_path,
            format!("notes/markdown/{}/{}_bst_notes.md", thread.id, file.id)
        );

        // Old file must no longer exist, new file must exist with same content
        assert!(!old_abs_path.exists());
        let new_abs_path = manager.resolve_storage_path(&renamed.storage_path);
        assert!(new_abs_path.exists());
        assert_eq!(
            std::fs::read(&new_abs_path).expect("read new file"),
            sample_content
        );

        // Verify SQLite database was updated
        let fetched = manager.get_file(&file.id).expect("get file from db");
        assert_eq!(fetched.file_name, "bst_notes.md");
        assert_eq!(fetched.storage_path, renamed.storage_path);

        // 2. Rename without extension (should preserve existing .md extension)
        let renamed_no_ext = manager
            .rename_file(&file.id, "bst_final")
            .expect("rename without extension");

        assert_eq!(renamed_no_ext.file_name, "bst_final.md");
        assert_eq!(renamed_no_ext.file_type, ".md");
        assert!(!new_abs_path.exists());
        let final_abs_path = manager.resolve_storage_path(&renamed_no_ext.storage_path);
        assert!(final_abs_path.exists());

        // 3. Rename to same name is a no-op
        let same = manager
            .rename_file(&file.id, "bst_final.md")
            .expect("rename to same name");
        assert_eq!(same.file_name, "bst_final.md");
        assert!(final_abs_path.exists());

        // Cleanup
        let _ = std::fs::remove_dir_all(temp_dir);
    }

    #[test]
    fn test_rename_file_type_change_and_routing() {
        let temp_dir = std::env::temp_dir().join(format!("noteman_test_rn_cat_{}", Uuid::new_v4()));
        let manager = DbManager::new_in_memory(temp_dir.clone()).expect("init db");

        manager
            .create_subject(CreateSubjectInput {
                id: "CS202".to_string(),
                name: "Algorithms".to_string(),
                semester: "20261".to_string(),
            })
            .expect("create subject");

        let thread = manager
            .create_thread(CreateThreadInput {
                id: Some("thread-rn-2".to_string()),
                subject_id: "CS202".to_string(),
                title: "Sorting".to_string(),
                description: None,
                created_at: None,
            })
            .expect("create thread");

        // Save a misc text note
        let file = manager
            .save_file(&thread.id, "sorting.txt", ".txt", false, b"quicksort")
            .expect("save misc file");
        assert!(file.storage_path.starts_with("notes/misc/"));
        let old_abs = manager.resolve_storage_path(&file.storage_path);
        assert!(old_abs.exists());

        // Rename from .txt to .md (should move to notes/markdown/)
        let renamed = manager
            .rename_file(&file.id, "sorting.md")
            .expect("rename to markdown");
        assert_eq!(renamed.file_name, "sorting.md");
        assert_eq!(renamed.file_type, ".md");
        assert!(renamed.storage_path.starts_with("notes/markdown/"));

        assert!(!old_abs.exists());
        let new_abs = manager.resolve_storage_path(&renamed.storage_path);
        assert!(new_abs.exists());
        assert_eq!(std::fs::read(new_abs).expect("read"), b"quicksort");

        // Cleanup
        let _ = std::fs::remove_dir_all(temp_dir);
    }

    #[test]
    fn test_rename_file_validation_and_safety_checks() {
        let temp_dir = std::env::temp_dir().join(format!("noteman_test_rn_err_{}", Uuid::new_v4()));
        let manager = DbManager::new_in_memory(temp_dir.clone()).expect("init db");

        manager
            .create_subject(CreateSubjectInput {
                id: "CS203".to_string(),
                name: "OS".to_string(),
                semester: "20261".to_string(),
            })
            .expect("create subject");

        let thread = manager
            .create_thread(CreateThreadInput {
                id: Some("thread-rn-3".to_string()),
                subject_id: "CS203".to_string(),
                title: "Processes".to_string(),
                description: None,
                created_at: None,
            })
            .expect("create thread");

        let file = manager
            .save_file(&thread.id, "process.md", ".md", false, b"# Processes")
            .expect("save file");

        // 1. Nonexistent file ID
        let err_not_found = manager.rename_file("non-existent-id", "test.md");
        assert!(matches!(err_not_found, Err(DbError::NotFound(_))));

        // 2. Empty file name
        let err_empty = manager.rename_file(&file.id, "   ");
        assert!(matches!(err_empty, Err(DbError::Validation(_))));

        // 3. Invalid characters or path traversal
        assert!(matches!(
            manager.rename_file(&file.id, "../escape.md"),
            Err(DbError::Validation(_))
        ));
        assert!(matches!(
            manager.rename_file(&file.id, "sub/dir.md"),
            Err(DbError::Validation(_))
        ));
        assert!(matches!(
            manager.rename_file(&file.id, "sub\\dir.md"),
            Err(DbError::Validation(_))
        ));
        assert!(matches!(
            manager.rename_file(&file.id, "."),
            Err(DbError::Validation(_))
        ));
        assert!(matches!(
            manager.rename_file(&file.id, ".."),
            Err(DbError::Validation(_))
        ));

        // 4. Target already exists on disk
        let target_path = DbManager::compute_storage_path(&thread.id, &file.id, "existing.md", ".md", false);
        let abs_target = manager.resolve_storage_path(&target_path);
        if let Some(p) = abs_target.parent() {
            std::fs::create_dir_all(p).unwrap();
        }
        std::fs::write(&abs_target, b"occupied").unwrap();

        let err_collision = manager.rename_file(&file.id, "existing.md");
        assert!(matches!(err_collision, Err(DbError::Validation(_))));

        // 5. Missing source file on disk
        let file_missing = manager
            .save_file(&thread.id, "ghost.md", ".md", false, b"ghost")
            .expect("save file");
        let abs_ghost = manager.resolve_storage_path(&file_missing.storage_path);
        std::fs::remove_file(abs_ghost).expect("delete ghost");

        let err_missing = manager.rename_file(&file_missing.id, "revived.md");
        assert!(matches!(err_missing, Err(DbError::NotFound(_))));

        // Cleanup
        let _ = std::fs::remove_dir_all(temp_dir);
    }
}
