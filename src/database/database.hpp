#pragma once

// Noteman SQLite database layer.
// Schema source of truth: docs/design/database_design.json

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace noteman::db {

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

class DatabaseError : public std::runtime_error {
public:
    DatabaseError(const std::string &what, int code)
        : std::runtime_error(what), code_(code) {}
    int code() const noexcept { return code_; }

private:
    int code_;
};

// ---------------------------------------------------------------------------
// Row models (one per table)
// ---------------------------------------------------------------------------

struct Subject {
    std::string id; // course code, e.g. "IT3080"
    std::string name;
    std::string semester; // e.g. "20261"
};

struct Thread {
    std::string id; // UUID v4
    std::string subject_id;
    std::string title;
    std::optional<std::string> description;
    std::string created_at; // ISO 8601
};

struct File {
    std::string id; // UUID v4
    std::string thread_id;
    std::string file_name;
    std::string file_type;
    std::string storage_path;
    bool is_material = false;
    std::optional<std::string> user_id;
};

struct Task {
    std::string id; // UUID v4
    std::string thread_id;
    std::string title;
    std::optional<std::string> deadline;
};

struct User {
    std::string id; // UUID v4
    std::string username;
    std::optional<std::string> email;
    std::optional<std::string> hashed_password;
    std::optional<std::string> salt;
    std::string created_at; // ISO 8601
};

struct Migration {
    int version;
    const char *name;
    const char *sql;
};

// ---------------------------------------------------------------------------
// Prepared statement (RAII)
// ---------------------------------------------------------------------------

class Statement {
public:
    Statement(sqlite3 *db, const std::string &sql);
    ~Statement();
    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;
    Statement(Statement &&other) noexcept;
    Statement &operator=(Statement &&other) noexcept;

    // Bind parameters (1-based index, like sqlite3_bind_*).
    Statement &bind(int index, const std::string &value);
    Statement &bind(int index, const char *value);
    Statement &bind(int index, std::int64_t value);
    Statement &bind(int index, int value);
    Statement &bind(int index, bool value);
    Statement &bind(int index, const std::optional<std::string> &value);
    Statement &bind_null(int index);

    // Advance; returns true while a row is available.
    bool step();
    // Execute a statement that returns no rows.
    void run();
    void reset();

    std::string column_text(int col) const;
    std::optional<std::string> column_optional_text(int col) const;
    std::int64_t column_int64(int col) const;
    bool column_is_null(int col) const;

private:
    void check_bind(int rc);

    sqlite3 *db_ = nullptr;
    sqlite3_stmt *stmt_ = nullptr;
};

// ---------------------------------------------------------------------------
// Database connection
// ---------------------------------------------------------------------------

class Database {
public:
    // Opens (creating if needed) the database at `path`, applies the pragmas
    // from the design (foreign_keys=ON, journal_mode=WAL, synchronous=NORMAL)
    // and runs all pending migrations. Use ":memory:" for an in-memory DB.
    explicit Database(const std::string &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    sqlite3 *handle() const noexcept { return db_; }

    void exec(const std::string &sql);
    Statement prepare(const std::string &sql);

    // Runs `fn` inside BEGIN IMMEDIATE / COMMIT; rolls back if `fn` throws.
    void transaction(const std::function<void()> &fn);

    int schema_version();
    static int latest_schema_version();

    // --- subjects ---------------------------------------------------------
    void create_subject(const Subject &subject);
    std::optional<Subject> get_subject(const std::string &id);
    std::vector<Subject> list_subjects();
    void update_subject(const Subject &subject);
    void delete_subject(const std::string &id); // cascades to threads/files/tasks

    // --- threads ----------------------------------------------------------
    // Generates id and created_at; returns the stored row.
    Thread create_thread(const std::string &subject_id, const std::string &title,
                         const std::optional<std::string> &description = std::nullopt);
    std::optional<Thread> get_thread(const std::string &id);
    std::vector<Thread> list_threads(const std::string &subject_id);
    void update_thread(const Thread &thread);
    void delete_thread(const std::string &id);

    // --- files ------------------------------------------------------------
    // If file.id is empty a UUID v4 is generated. Returns the stored row.
    File create_file(File file);
    std::optional<File> get_file(const std::string &id);
    std::vector<File> list_files(const std::string &thread_id);
    std::vector<File> list_files_by_user(const std::string &user_id);
    void update_file(const File &file);
    void delete_file(const std::string &id);

    // --- tasks ------------------------------------------------------------
    Task create_task(const std::string &thread_id, const std::string &title,
                     const std::optional<std::string> &deadline = std::nullopt);
    std::optional<Task> get_task(const std::string &id);
    std::vector<Task> list_tasks(const std::string &thread_id);
    void update_task(const Task &task);
    void delete_task(const std::string &id);

    // --- users ------------------------------------------------------------
    // Password hashing is the caller's responsibility; pass the already
    // computed hash and salt (or nullopt for password-less accounts).
    User create_user(const std::string &username,
                     const std::optional<std::string> &email = std::nullopt,
                     const std::optional<std::string> &hashed_password = std::nullopt,
                     const std::optional<std::string> &salt = std::nullopt);
    std::optional<User> get_user(const std::string &id);
    std::optional<User> get_user_by_username(const std::string &username);
    std::vector<User> list_users();
    void update_user(const User &user);
    void delete_user(const std::string &id); // files.user_id -> NULL

private:
    void apply_pragmas();
    void run_migrations();

    sqlite3 *db_ = nullptr;
};

// Generates a random RFC 4122 version 4 UUID string.
std::string generate_uuid_v4();

// Current UTC time as ISO 8601, e.g. "2026-11-24T04:29:50Z".
std::string now_iso8601();

} // namespace noteman::db
