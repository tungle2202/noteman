use tauri::State;
use crate::db::manager::DbManager;
use crate::db::models::{
    CreateFileInput, CreateSubjectInput, CreateTaskInput, CreateThreadInput, DbStats,
    FileIntegrityItem, FileRecord, MigrationInfo, Subject, Task, Thread, UpdateSubjectInput,
    UpdateTaskInput, UpdateThreadInput,
};

#[tauri::command]
pub fn db_get_stats(db: State<'_, DbManager>) -> Result<DbStats, String> {
    db.get_stats().map_err(|e| e.to_string())
}

// =========================================================================
// SUBJECTS
// =========================================================================

#[tauri::command]
pub fn db_create_subject(
    db: State<'_, DbManager>,
    input: CreateSubjectInput,
) -> Result<Subject, String> {
    db.create_subject(input).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_get_subjects(db: State<'_, DbManager>) -> Result<Vec<Subject>, String> {
    db.get_subjects().map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_get_subject(db: State<'_, DbManager>, id: String) -> Result<Subject, String> {
    db.get_subject(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_update_subject(
    db: State<'_, DbManager>,
    id: String,
    input: UpdateSubjectInput,
) -> Result<Subject, String> {
    db.update_subject(&id, input).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_delete_subject(
    db: State<'_, DbManager>,
    id: String,
    clean_files: Option<bool>,
) -> Result<(), String> {
    db.delete_subject(&id, clean_files.unwrap_or(true))
        .map_err(|e| e.to_string())
}

// =========================================================================
// THREADS
// =========================================================================

#[tauri::command]
pub fn db_create_thread(
    db: State<'_, DbManager>,
    input: CreateThreadInput,
) -> Result<Thread, String> {
    db.create_thread(input).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_get_threads_by_subject(
    db: State<'_, DbManager>,
    subject_id: String,
) -> Result<Vec<Thread>, String> {
    db.get_threads_by_subject(&subject_id)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_get_thread(db: State<'_, DbManager>, id: String) -> Result<Thread, String> {
    db.get_thread(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_update_thread(
    db: State<'_, DbManager>,
    id: String,
    input: UpdateThreadInput,
) -> Result<Thread, String> {
    db.update_thread(&id, input).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_delete_thread(
    db: State<'_, DbManager>,
    id: String,
    clean_files: Option<bool>,
) -> Result<(), String> {
    db.delete_thread(&id, clean_files.unwrap_or(true))
        .map_err(|e| e.to_string())
}

// =========================================================================
// FILES & PACKAGE-MANAGER FILE HANDLING
// =========================================================================

#[tauri::command]
pub fn db_create_file_record(
    db: State<'_, DbManager>,
    input: CreateFileInput,
) -> Result<FileRecord, String> {
    db.create_file_record(input).map_err(|e| e.to_string())
}

/// USE CASE: In-Memory Content Authoring & Editing
/// -----------------------------------------------
/// Use this command when saving user-authored notes created or edited inside the embedded
/// Markdown (.md) or LaTeX (.tex) editors. The payload data resides in frontend memory/state
/// and is transmitted as a byte buffer (`Vec<u8>`).
///
/// WHEN NOT TO USE:
/// Do NOT use this command to import external files from the user's filesystem (such as large
/// lecture PDFs, slide decks, or ZIP archives). Sending large byte arrays across Tauri's JSON-based
/// IPC creates unnecessary memory duplication and serialization latency. Use `db_import_file` instead.
#[tauri::command]
pub fn db_save_file(
    db: State<'_, DbManager>,
    thread_id: String,
    file_name: String,
    file_type: String,
    is_material: bool,
    data: Vec<u8>,
) -> Result<FileRecord, String> {
    db.save_file(&thread_id, &file_name, &file_type, is_material, &data)
        .map_err(|e| e.to_string())
}

/// USE CASE: External File Importing (Zero-IPC-Memory Overhead)
/// ------------------------------------------------------------
/// Use this command when importing external course materials or documents selected via native file
/// dialog (`@tauri-apps/plugin-dialog`) or desktop drag-and-drop (e.g. .pdf textbooks, .pptx slides,
/// .zip archives, .docx specs). The frontend transmits ONLY the local `source_path` string.
/// Rust directly executes a fast, zero-IPC-memory disk copy (`std::fs::copy`) into Noteman's managed storage.
///
/// WHEN NOT TO USE:
/// Do NOT use this command for note content authored in memory inside the webview that does not
/// already exist as a file on the local filesystem. Use `db_save_file` for in-memory editor buffers.
#[tauri::command]
pub fn db_import_file(
    db: State<'_, DbManager>,
    thread_id: String,
    source_path: String,
    file_name: String,
    file_type: String,
    is_material: bool,
) -> Result<FileRecord, String> {
    db.import_file(&thread_id, &source_path, &file_name, &file_type, is_material)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_read_file(db: State<'_, DbManager>, file_id: String) -> Result<Vec<u8>, String> {
    db.read_file(&file_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_get_files_by_thread(
    db: State<'_, DbManager>,
    thread_id: String,
) -> Result<Vec<FileRecord>, String> {
    db.get_files_by_thread(&thread_id)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_get_file(db: State<'_, DbManager>, id: String) -> Result<FileRecord, String> {
    db.get_file(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_delete_file_record(
    db: State<'_, DbManager>,
    id: String,
    remove_physical_file: Option<bool>,
) -> Result<(), String> {
    db.delete_file_record(&id, remove_physical_file.unwrap_or(true))
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_verify_files_integrity(
    db: State<'_, DbManager>,
) -> Result<Vec<FileIntegrityItem>, String> {
    db.verify_files_integrity().map_err(|e| e.to_string())
}

// =========================================================================
// TASKS
// =========================================================================

#[tauri::command]
pub fn db_create_task(db: State<'_, DbManager>, input: CreateTaskInput) -> Result<Task, String> {
    db.create_task(input).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_get_tasks_by_thread(
    db: State<'_, DbManager>,
    thread_id: String,
) -> Result<Vec<Task>, String> {
    db.get_tasks_by_thread(&thread_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_get_task(db: State<'_, DbManager>, id: String) -> Result<Task, String> {
    db.get_task(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_update_task(
    db: State<'_, DbManager>,
    id: String,
    input: UpdateTaskInput,
) -> Result<Task, String> {
    db.update_task(&id, input).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_delete_task(db: State<'_, DbManager>, id: String) -> Result<(), String> {
    db.delete_task(&id).map_err(|e| e.to_string())
}

// =========================================================================
// MIGRATIONS & SYSTEM MAINTENANCE
// =========================================================================

#[tauri::command]
pub fn db_get_schema_version(db: State<'_, DbManager>) -> Result<i32, String> {
    db.get_schema_version().map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_get_applied_migrations(
    db: State<'_, DbManager>,
) -> Result<Vec<MigrationInfo>, String> {
    db.get_applied_migrations().map_err(|e| e.to_string())
}

#[tauri::command]
pub fn db_purge_trash(db: State<'_, DbManager>) -> Result<(), String> {
    db.purge_trash().map_err(|e| e.to_string())
}
