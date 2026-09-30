pub mod db;

use tauri::Manager;

// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/
#[tauri::command]
fn greet(name: &str) -> String {
    format!("Hello, {}! You've been greeted from Rust!", name)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .setup(|app| {
            let app_data_dir = app
                .path()
                .app_data_dir()
                .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;

            let db_dir = app_data_dir.join("db");
            std::fs::create_dir_all(&db_dir)
                .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;
            let db_path = db_dir.join("noteman.db");
            let db_manager = db::DbManager::new(db_path, app_data_dir)
                .map_err(|e| Box::new(e) as Box<dyn std::error::Error>)?;

            app.manage(db_manager);

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            greet,
            db::db_get_stats,
            db::db_create_subject,
            db::db_get_subjects,
            db::db_get_subject,
            db::db_update_subject,
            db::db_delete_subject,
            db::db_create_thread,
            db::db_get_threads_by_subject,
            db::db_get_thread,
            db::db_update_thread,
            db::db_delete_thread,
            db::db_create_file_record,
            db::db_save_file,
            db::db_read_file,
            db::db_get_files_by_thread,
            db::db_get_file,
            db::db_delete_file_record,
            db::db_verify_files_integrity,
            db::db_create_task,
            db::db_get_tasks_by_thread,
            db::db_get_task,
            db::db_update_task,
            db::db_delete_task,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
