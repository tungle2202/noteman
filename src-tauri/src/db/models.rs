use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Subject {
    pub id: String,
    pub name: String,
    pub semester: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CreateSubjectInput {
    pub id: String,
    pub name: String,
    pub semester: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct UpdateSubjectInput {
    pub name: Option<String>,
    pub semester: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Thread {
    pub id: String,
    pub subject_id: String,
    pub title: String,
    pub description: Option<String>,
    pub created_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CreateThreadInput {
    pub id: Option<String>,
    pub subject_id: String,
    pub title: String,
    pub description: Option<String>,
    pub created_at: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct UpdateThreadInput {
    pub title: Option<String>,
    pub description: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct FileRecord {
    pub id: String,
    pub thread_id: String,
    pub file_name: String,
    pub file_type: String,
    pub storage_path: String,
    pub is_material: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CreateFileInput {
    pub id: Option<String>,
    pub thread_id: String,
    pub file_name: String,
    pub file_type: String,
    pub storage_path: String,
    pub is_material: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct Task {
    pub id: String,
    pub thread_id: String,
    pub title: String,
    pub deadline: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CreateTaskInput {
    pub id: Option<String>,
    pub thread_id: String,
    pub title: String,
    pub deadline: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct UpdateTaskInput {
    pub title: Option<String>,
    pub deadline: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FileIntegrityItem {
    pub file_id: String,
    pub thread_id: String,
    pub file_name: String,
    pub storage_path: String,
    pub exists_on_disk: bool,
    pub file_size_bytes: Option<u64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DbStats {
    pub subjects_count: i64,
    pub threads_count: i64,
    pub files_count: i64,
    pub tasks_count: i64,
    pub db_path: String,
    pub app_dir: String,
}
