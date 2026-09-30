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

        let conn = Connection::open(&db_path)?;
        initialize_schema(&conn)?;

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

        let conn = Connection::open_in_memory()?;
        initialize_schema(&conn)?;

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
        if clean_files {
            // Find all files belonging to all threads under this subject to clean them on disk
            let paths: Vec<String> = {
                let conn = self.get_conn()?;
                let mut stmt = conn.prepare(
                    "SELECT f.storage_path FROM files f 
                     JOIN threads t ON f.thread_id = t.id 
                     WHERE t.subject_id = ?1",
                )?;
                let rows = stmt
                    .query_map(params![id], |row| row.get(0))?
                    .filter_map(|r| r.ok())
                    .collect();
                rows
            };

            for rel_path in paths {
                let abs_path = self.resolve_storage_path(&rel_path);
                let _ = std::fs::remove_file(abs_path);
            }
        }

        let conn = self.get_conn()?;
        let affected = conn.execute("DELETE FROM subjects WHERE id = ?1", params![id])?;
        if affected == 0 {
            return Err(DbError::NotFound(format!("Subject '{}' not found", id)));
        }
        Ok(())
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
        if clean_files {
            let paths: Vec<String> = {
                let conn = self.get_conn()?;
                let mut stmt = conn.prepare("SELECT storage_path FROM files WHERE thread_id = ?1")?;
                let rows = stmt
                    .query_map(params![id], |row| row.get(0))?
                    .filter_map(|r| r.ok())
                    .collect();
                rows
            };

            for rel_path in paths {
                let abs_path = self.resolve_storage_path(&rel_path);
                let _ = std::fs::remove_file(abs_path);
            }
        }

        let conn = self.get_conn()?;
        let affected = conn.execute("DELETE FROM threads WHERE id = ?1", params![id])?;
        if affected == 0 {
            return Err(DbError::NotFound(format!("Thread '{}' not found", id)));
        }
        Ok(())
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

    pub fn read_file(&self, file_id: &str) -> Result<Vec<u8>, DbError> {
        let file_record = self.get_file(file_id)?;
        let abs_path = self.resolve_storage_path(&file_record.storage_path);
        let bytes = std::fs::read(abs_path)?;
        Ok(bytes)
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
        if remove_physical_file {
            if let Ok(file_record) = self.get_file(id) {
                let abs_path = self.resolve_storage_path(&file_record.storage_path);
                let _ = std::fs::remove_file(abs_path);
            }
        }

        let conn = self.get_conn()?;
        let affected = conn.execute("DELETE FROM files WHERE id = ?1", params![id])?;
        if affected == 0 {
            return Err(DbError::NotFound(format!("File '{}' not found", id)));
        }
        Ok(())
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

        Ok(DbStats {
            subjects_count,
            threads_count,
            files_count,
            tasks_count,
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

        // 6. Check Stats
        let stats = manager.get_stats().expect("stats");
        assert_eq!(stats.subjects_count, 1);
        assert_eq!(stats.threads_count, 1);
        assert_eq!(stats.files_count, 1);
        assert_eq!(stats.tasks_count, 1);

        // 7. Test Cascade Delete: deleting subject should delete thread, file, task
        manager.delete_subject("IT3080", true).expect("delete subject");
        let stats_after = manager.get_stats().expect("stats after");
        assert_eq!(stats_after.subjects_count, 0);
        assert_eq!(stats_after.threads_count, 0);
        assert_eq!(stats_after.files_count, 0);
        assert_eq!(stats_after.tasks_count, 0);

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
}
