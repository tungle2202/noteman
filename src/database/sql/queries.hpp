#pragma once

// Every DML statement used by the database layer.
// DDL (tables, indexes, triggers) lives in sql/migrations/*.sql.
//
// The column order of each NM_*_COLS list is what the row readers in
// row_readers.cpp expect: keep them in sync when adding a column.
// Parameters are numbered (?1, ?2, ...) and documented per statement.

namespace noteman::db::sql {

#define NM_NOW "strftime('%Y-%m-%dT%H:%M:%fZ', 'now')"

#define NM_USER_COLS "id, username, email, password_hash, primary_group_id, is_admin, created_at"
#define NM_GROUP_COLS "id, name, created_at"
#define NM_SUBJECT_COLS \
    "id, code, name, semester, owner_id, group_id, mode, created_at, last_update"
#define NM_THREAD_COLS "id, subject_id, title, description, created_by, created_at, last_update"
#define NM_BLOB_COLS "hash, size, mime_type, page_count, created_at"
#define NM_MATERIAL_COLS \
    "id, subject_id, blob_hash, display_name, uploaded_by, uploaded_at, deleted_at"
#define NM_QUOTE_COLS                                                                      \
    "id, thread_id, material_id, kind, spec_json, extracted_text, note, author_id, " \
    "created_at, last_update"
#define NM_PROGRESS_COLS "user_id, material_id, page, page_offset, zoom, updated_at"
#define NM_NOTE_COLS \
    "id, thread_id, file_name, format, current_version_id, created_by, created_at, last_update"
#define NM_VERSION_COLS                                                                  \
    "id, note_id, parent_id, author_id, compression, content_hash, size, message, pinned, " \
    "created_at"
#define NM_TASK_COLS \
    "id, thread_id, title, deadline, completed_at, created_by, created_at, last_update"

// ---------------------------------------------------------------------------
// Schema / migrations
// ---------------------------------------------------------------------------

inline constexpr const char *kSelectSchemaVersion = "PRAGMA user_version";

// ?1 version, ?2 name. applied_at is explicit because the version-1 table
// has no default for it.
inline constexpr const char *kInsertMigration =
    "INSERT INTO _migrations (version, name, applied_at) VALUES (?1, ?2, " NM_NOW ")";

// ---------------------------------------------------------------------------
// users
// ---------------------------------------------------------------------------

// ?1 id, ?2 username, ?3 email, ?4 password_hash, ?5 primary_group_id, ?6 is_admin
inline constexpr const char *kInsertUser =
    "INSERT INTO users (id, username, email, password_hash, primary_group_id, is_admin) "
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6) RETURNING " NM_USER_COLS;

inline constexpr const char *kSelectUserById =
    "SELECT " NM_USER_COLS " FROM users WHERE id = ?1";

inline constexpr const char *kSelectUserByUsername =
    "SELECT " NM_USER_COLS " FROM users WHERE username = ?1";

inline constexpr const char *kSelectUsers =
    "SELECT " NM_USER_COLS " FROM users ORDER BY username";

// ?1 id, ?2 username, ?3 email, ?4 password_hash, ?5 primary_group_id, ?6 is_admin
inline constexpr const char *kUpdateUser =
    "UPDATE users SET username = ?2, email = ?3, password_hash = ?4, "
    "primary_group_id = ?5, is_admin = ?6 WHERE id = ?1";

inline constexpr const char *kDeleteUser = "DELETE FROM users WHERE id = ?1";

// ---------------------------------------------------------------------------
// groups & membership
// ---------------------------------------------------------------------------

// ?1 id, ?2 name
inline constexpr const char *kInsertGroup =
    "INSERT INTO groups (id, name) VALUES (?1, ?2) RETURNING " NM_GROUP_COLS;

inline constexpr const char *kSelectGroupById =
    "SELECT " NM_GROUP_COLS " FROM groups WHERE id = ?1";

inline constexpr const char *kSelectGroupByName =
    "SELECT " NM_GROUP_COLS " FROM groups WHERE name = ?1";

inline constexpr const char *kSelectGroups =
    "SELECT " NM_GROUP_COLS " FROM groups ORDER BY name";

// ?1 id, ?2 name
inline constexpr const char *kRenameGroup = "UPDATE groups SET name = ?2 WHERE id = ?1";

inline constexpr const char *kDeleteGroup = "DELETE FROM groups WHERE id = ?1";

// ?1 user_id, ?2 group_id
inline constexpr const char *kInsertUserGroup =
    "INSERT OR IGNORE INTO user_groups (user_id, group_id) VALUES (?1, ?2)";

// ?1 user_id, ?2 group_id
inline constexpr const char *kDeleteUserGroup =
    "DELETE FROM user_groups WHERE user_id = ?1 AND group_id = ?2";

// ?1 user_id
inline constexpr const char *kSelectGroupsOfUser =
    "SELECT g.id, g.name, g.created_at FROM groups g "
    "JOIN user_groups ug ON ug.group_id = g.id "
    "WHERE ug.user_id = ?1 ORDER BY g.name";

// ?1 group_id
inline constexpr const char *kSelectMembersOfGroup =
    "SELECT u.id, u.username, u.email, u.password_hash, u.primary_group_id, u.is_admin, "
    "u.created_at FROM users u "
    "JOIN user_groups ug ON ug.user_id = u.id "
    "WHERE ug.group_id = ?1 ORDER BY u.username";

// ---------------------------------------------------------------------------
// subjects
// ---------------------------------------------------------------------------

// ?1 id, ?2 code, ?3 name, ?4 semester, ?5 owner_id,
// ?6 group_id (NULL = owner's primary group), ?7 mode
inline constexpr const char *kInsertSubject =
    "INSERT INTO subjects (id, code, name, semester, owner_id, group_id, mode) "
    "VALUES (?1, ?2, ?3, ?4, ?5, "
    "COALESCE(?6, (SELECT primary_group_id FROM users WHERE id = ?5)), ?7) "
    "RETURNING " NM_SUBJECT_COLS;

inline constexpr const char *kSelectSubjectById =
    "SELECT " NM_SUBJECT_COLS " FROM subjects WHERE id = ?1";

// ?1 owner_id
inline constexpr const char *kSelectSubjectsByOwner =
    "SELECT " NM_SUBJECT_COLS " FROM subjects WHERE owner_id = ?1 "
    "ORDER BY semester DESC, code";

// Subjects user ?1 may read (owner -> group -> other, first match wins;
// admins see everything).
inline constexpr const char *kSelectVisibleSubjects =
    "SELECT " NM_SUBJECT_COLS " FROM subjects s "
    "WHERE EXISTS (SELECT 1 FROM users WHERE id = ?1 AND is_admin = 1) "
    "   OR (s.owner_id = ?1 AND (s.mode & 32) != 0) "
    "   OR (s.owner_id != ?1 AND (s.mode & 8) != 0 AND EXISTS ("
    "         SELECT 1 FROM user_groups ug WHERE ug.user_id = ?1 AND ug.group_id = s.group_id)) "
    "   OR (s.owner_id != ?1 AND (s.mode & 2) != 0 AND NOT EXISTS ("
    "         SELECT 1 FROM user_groups ug WHERE ug.user_id = ?1 AND ug.group_id = s.group_id)) "
    "ORDER BY s.semester DESC, s.code";

// ?1 id, ?2 code, ?3 name, ?4 semester, ?5 owner_id, ?6 group_id, ?7 mode
inline constexpr const char *kUpdateSubject =
    "UPDATE subjects SET code = ?2, name = ?3, semester = ?4, owner_id = ?5, "
    "group_id = ?6, mode = ?7 WHERE id = ?1";

inline constexpr const char *kDeleteSubject = "DELETE FROM subjects WHERE id = ?1";

// Facts needed to decide access of user ?1 to subject ?2:
// is_owner, in_group, mode, is_admin.
inline constexpr const char *kSelectSubjectAccess =
    "SELECT s.owner_id = ?1, "
    "EXISTS (SELECT 1 FROM user_groups ug WHERE ug.user_id = ?1 AND ug.group_id = s.group_id), "
    "s.mode, "
    "COALESCE((SELECT is_admin FROM users WHERE id = ?1), 0) "
    "FROM subjects s WHERE s.id = ?2";

// ---------------------------------------------------------------------------
// threads
// ---------------------------------------------------------------------------

// ?1 id, ?2 subject_id, ?3 title, ?4 description, ?5 created_by
inline constexpr const char *kInsertThread =
    "INSERT INTO threads (id, subject_id, title, description, created_by) "
    "VALUES (?1, ?2, ?3, ?4, ?5) RETURNING " NM_THREAD_COLS;

inline constexpr const char *kSelectThreadById =
    "SELECT " NM_THREAD_COLS " FROM threads WHERE id = ?1";

// ?1 subject_id
inline constexpr const char *kSelectThreadsBySubject =
    "SELECT " NM_THREAD_COLS " FROM threads WHERE subject_id = ?1 ORDER BY created_at";

// ?1 subject_id
inline constexpr const char *kDeleteThreadsBySubject =
    "DELETE FROM threads WHERE subject_id = ?1";

// ?1 id, ?2 subject_id, ?3 title, ?4 description
inline constexpr const char *kUpdateThread =
    "UPDATE threads SET subject_id = ?2, title = ?3, description = ?4 WHERE id = ?1";

inline constexpr const char *kDeleteThread = "DELETE FROM threads WHERE id = ?1";

// ---------------------------------------------------------------------------
// blobs
// ---------------------------------------------------------------------------

// ?1 hash, ?2 size, ?3 mime_type, ?4 page_count. Deduplicated by hash.
inline constexpr const char *kInsertBlob =
    "INSERT INTO blobs (hash, size, mime_type, page_count) VALUES (?1, ?2, ?3, ?4) "
    "ON CONFLICT (hash) DO NOTHING";

inline constexpr const char *kSelectBlobByHash =
    "SELECT " NM_BLOB_COLS " FROM blobs WHERE hash = ?1";

// Garbage collection: blobs no material references any more.
inline constexpr const char *kDeleteUnreferencedBlobs =
    "DELETE FROM blobs WHERE hash NOT IN (SELECT blob_hash FROM materials) RETURNING hash";

// ---------------------------------------------------------------------------
// materials
// ---------------------------------------------------------------------------

// ?1 id, ?2 subject_id, ?3 blob_hash, ?4 display_name, ?5 uploaded_by
inline constexpr const char *kInsertMaterial =
    "INSERT INTO materials (id, subject_id, blob_hash, display_name, uploaded_by) "
    "VALUES (?1, ?2, ?3, ?4, ?5) RETURNING " NM_MATERIAL_COLS;

inline constexpr const char *kSelectMaterialById =
    "SELECT " NM_MATERIAL_COLS " FROM materials WHERE id = ?1";

// ?1 subject_id, ?2 include_deleted (0/1)
inline constexpr const char *kSelectMaterialsBySubject =
    "SELECT " NM_MATERIAL_COLS " FROM materials "
    "WHERE subject_id = ?1 AND (?2 OR deleted_at IS NULL) ORDER BY display_name";

// ?1 id, ?2 display_name
inline constexpr const char *kRenameMaterial =
    "UPDATE materials SET display_name = ?2 WHERE id = ?1";

inline constexpr const char *kSoftDeleteMaterial =
    "UPDATE materials SET deleted_at = COALESCE(deleted_at, " NM_NOW ") WHERE id = ?1";

inline constexpr const char *kRestoreMaterial =
    "UPDATE materials SET deleted_at = NULL WHERE id = ?1";

inline constexpr const char *kDeleteMaterial = "DELETE FROM materials WHERE id = ?1";

// ---------------------------------------------------------------------------
// quotes
// ---------------------------------------------------------------------------

// ?1 id, ?2 thread_id, ?3 material_id, ?4 kind, ?5 spec_json,
// ?6 extracted_text, ?7 note, ?8 author_id
inline constexpr const char *kInsertQuote =
    "INSERT INTO quotes (id, thread_id, material_id, kind, spec_json, extracted_text, note, "
    "author_id) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8) RETURNING " NM_QUOTE_COLS;

inline constexpr const char *kSelectQuoteById =
    "SELECT " NM_QUOTE_COLS " FROM quotes WHERE id = ?1";

// ?1 thread_id
inline constexpr const char *kSelectQuotesByThread =
    "SELECT " NM_QUOTE_COLS " FROM quotes WHERE thread_id = ?1 ORDER BY created_at";

// ?1 material_id (reverse lookup: where is this PDF quoted?)
inline constexpr const char *kSelectQuotesByMaterial =
    "SELECT " NM_QUOTE_COLS " FROM quotes WHERE material_id = ?1 ORDER BY created_at";

// ?1 id, ?2 kind, ?3 spec_json, ?4 extracted_text, ?5 note
inline constexpr const char *kUpdateQuote =
    "UPDATE quotes SET kind = ?2, spec_json = ?3, extracted_text = ?4, note = ?5 WHERE id = ?1";

inline constexpr const char *kDeleteQuote = "DELETE FROM quotes WHERE id = ?1";

// ---------------------------------------------------------------------------
// reading_progress
// ---------------------------------------------------------------------------

// ?1 user_id, ?2 material_id, ?3 page, ?4 page_offset, ?5 zoom
inline constexpr const char *kUpsertReadingProgress =
    "INSERT INTO reading_progress (user_id, material_id, page, page_offset, zoom) "
    "VALUES (?1, ?2, ?3, ?4, ?5) "
    "ON CONFLICT (user_id, material_id) DO UPDATE SET "
    "page = excluded.page, page_offset = excluded.page_offset, zoom = excluded.zoom, "
    "updated_at = " NM_NOW " "
    "RETURNING " NM_PROGRESS_COLS;

// ?1 user_id, ?2 material_id
inline constexpr const char *kSelectReadingProgress =
    "SELECT " NM_PROGRESS_COLS " FROM reading_progress WHERE user_id = ?1 AND material_id = ?2";

// ---------------------------------------------------------------------------
// notes
// ---------------------------------------------------------------------------

// ?1 id, ?2 thread_id, ?3 file_name, ?4 format, ?5 current_version_id, ?6 created_by
// current_version_id is DEFERRABLE: the first version is inserted right after
// in the same transaction.
inline constexpr const char *kInsertNote =
    "INSERT INTO notes (id, thread_id, file_name, format, current_version_id, created_by) "
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6)";

inline constexpr const char *kSelectNoteById =
    "SELECT " NM_NOTE_COLS " FROM notes WHERE id = ?1";

// ?1 thread_id
inline constexpr const char *kSelectNotesByThread =
    "SELECT " NM_NOTE_COLS " FROM notes WHERE thread_id = ?1 ORDER BY file_name";

// ?1 id, ?2 file_name
inline constexpr const char *kRenameNote = "UPDATE notes SET file_name = ?2 WHERE id = ?1";

inline constexpr const char *kDeleteNote = "DELETE FROM notes WHERE id = ?1";

// ?1 note_id -> current_version_id, content_hash of the current version
inline constexpr const char *kSelectNoteHead =
    "SELECT n.current_version_id, v.content_hash FROM notes n "
    "LEFT JOIN note_versions v ON v.id = n.current_version_id WHERE n.id = ?1";

// Optimistic concurrency: ?1 note_id, ?2 new version id, ?3 expected parent.
// 0 rows changed -> someone saved first.
inline constexpr const char *kAdvanceNoteHead =
    "UPDATE notes SET current_version_id = ?2 WHERE id = ?1 AND current_version_id IS ?3";

// ---------------------------------------------------------------------------
// note_versions
// ---------------------------------------------------------------------------

// ?1 id, ?2 note_id, ?3 parent_id, ?4 author_id, ?5 content, ?6 compression,
// ?7 content_hash, ?8 size, ?9 message
inline constexpr const char *kInsertNoteVersion =
    "INSERT INTO note_versions (id, note_id, parent_id, author_id, content, compression, "
    "content_hash, size, message) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)";

inline constexpr const char *kSelectNoteVersionById =
    "SELECT " NM_VERSION_COLS " FROM note_versions WHERE id = ?1";

// ?1 note_id, newest first (rowid breaks ties within one millisecond)
inline constexpr const char *kSelectNoteVersionsByNote =
    "SELECT " NM_VERSION_COLS " FROM note_versions WHERE note_id = ?1 "
    "ORDER BY created_at DESC, rowid DESC";

// ?1 version id -> compression, content
inline constexpr const char *kSelectNoteVersionContent =
    "SELECT compression, content FROM note_versions WHERE id = ?1";

// ?1 version id, ?2 note_id -> compression, content (version must belong to the note)
inline constexpr const char *kSelectNoteVersionContentOfNote =
    "SELECT compression, content FROM note_versions WHERE id = ?1 AND note_id = ?2";

// ?1 id, ?2 pinned (0/1)
inline constexpr const char *kSetNoteVersionPinned =
    "UPDATE note_versions SET pinned = ?2 WHERE id = ?1";

// ---------------------------------------------------------------------------
// tasks
// ---------------------------------------------------------------------------

// ?1 id, ?2 thread_id, ?3 title, ?4 deadline, ?5 created_by
inline constexpr const char *kInsertTask =
    "INSERT INTO tasks (id, thread_id, title, deadline, created_by) "
    "VALUES (?1, ?2, ?3, ?4, ?5) RETURNING " NM_TASK_COLS;

inline constexpr const char *kSelectTaskById =
    "SELECT " NM_TASK_COLS " FROM tasks WHERE id = ?1";

// ?1 thread_id. Open tasks first, then by deadline (no deadline last).
inline constexpr const char *kSelectTasksByThread =
    "SELECT " NM_TASK_COLS " FROM tasks WHERE thread_id = ?1 "
    "ORDER BY completed_at IS NOT NULL, deadline IS NULL, deadline, title";

// ?1 id, ?2 thread_id, ?3 title, ?4 deadline
inline constexpr const char *kUpdateTask =
    "UPDATE tasks SET thread_id = ?2, title = ?3, deadline = ?4 WHERE id = ?1";

// ?1 id, ?2 completed (0/1). Re-completing keeps the original timestamp.
inline constexpr const char *kSetTaskCompleted =
    "UPDATE tasks SET completed_at = CASE WHEN ?2 THEN COALESCE(completed_at, " NM_NOW
    ") ELSE NULL END WHERE id = ?1";

inline constexpr const char *kDeleteTask = "DELETE FROM tasks WHERE id = ?1";

#undef NM_NOW
#undef NM_USER_COLS
#undef NM_GROUP_COLS
#undef NM_SUBJECT_COLS
#undef NM_THREAD_COLS
#undef NM_BLOB_COLS
#undef NM_MATERIAL_COLS
#undef NM_QUOTE_COLS
#undef NM_PROGRESS_COLS
#undef NM_NOTE_COLS
#undef NM_VERSION_COLS
#undef NM_TASK_COLS

} // namespace noteman::db::sql
