#include "database.hpp"
#include "migrations.hpp"

#include <sqlite3.h>

#include <ctime>
#include <random>

namespace noteman::db {

namespace {

[[noreturn]] void throw_error(sqlite3 *db, int rc, const std::string &context) {
    std::string msg = context + ": ";
    msg += db ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
    throw DatabaseError(msg, rc);
}

Subject read_subject(const Statement &s) {
    return Subject{s.column_text(0), s.column_text(1), s.column_text(2)};
}

Thread read_thread(const Statement &s) {
    return Thread{s.column_text(0), s.column_text(1), s.column_text(2),
                  s.column_optional_text(3), s.column_text(4)};
}

File read_file(const Statement &s) {
    return File{s.column_text(0),       s.column_text(1),
                s.column_text(2),       s.column_text(3),
                s.column_text(4),       s.column_int64(5) != 0,
                s.column_optional_text(6)};
}

Task read_task(const Statement &s) {
    return Task{s.column_text(0), s.column_text(1), s.column_text(2),
                s.column_optional_text(3)};
}

User read_user(const Statement &s) {
    return User{s.column_text(0),          s.column_text(1),
                s.column_optional_text(2), s.column_optional_text(3),
                s.column_optional_text(4), s.column_text(5)};
}

constexpr const char *kSubjectCols = "id, name, semester";
constexpr const char *kThreadCols = "id, subject_id, title, description, created_at";
constexpr const char *kFileCols =
    "id, thread_id, file_name, file_type, storage_path, is_material, user_id";
constexpr const char *kTaskCols = "id, thread_id, title, deadline";
constexpr const char *kUserCols =
    "id, username, email, hashed_password, salt, created_at";

std::string select_from(const char *cols, const char *table, const char *tail) {
    return std::string("SELECT ") + cols + " FROM " + table + " " + tail;
}

} // namespace

// ===========================================================================
// Utilities
// ===========================================================================

std::string generate_uuid_v4() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 255);

    unsigned char bytes[16];
    for (auto &b : bytes) b = static_cast<unsigned char>(dist(rng));
    bytes[6] = (bytes[6] & 0x0F) | 0x40; // version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80; // RFC 4122 variant

    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back('-');
        out.push_back(hex[bytes[i] >> 4]);
        out.push_back(hex[bytes[i] & 0x0F]);
    }
    return out;
}

std::string now_iso8601() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// ===========================================================================
// Statement
// ===========================================================================

Statement::Statement(sqlite3 *db, const std::string &sql) : db_(db) {
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()),
                                &stmt_, nullptr);
    if (rc != SQLITE_OK) throw_error(db_, rc, "prepare failed (" + sql + ")");
}

Statement::~Statement() { sqlite3_finalize(stmt_); }

Statement::Statement(Statement &&other) noexcept
    : db_(other.db_), stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement &Statement::operator=(Statement &&other) noexcept {
    if (this != &other) {
        sqlite3_finalize(stmt_);
        db_ = other.db_;
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

void Statement::check_bind(int rc) {
    if (rc != SQLITE_OK) throw_error(db_, rc, "bind failed");
}

Statement &Statement::bind(int index, const std::string &value) {
    check_bind(sqlite3_bind_text(stmt_, index, value.data(),
                                 static_cast<int>(value.size()), SQLITE_TRANSIENT));
    return *this;
}

Statement &Statement::bind(int index, const char *value) {
    if (!value) return bind_null(index);
    check_bind(sqlite3_bind_text(stmt_, index, value, -1, SQLITE_TRANSIENT));
    return *this;
}

Statement &Statement::bind(int index, std::int64_t value) {
    check_bind(sqlite3_bind_int64(stmt_, index, value));
    return *this;
}

Statement &Statement::bind(int index, int value) {
    return bind(index, static_cast<std::int64_t>(value));
}

Statement &Statement::bind(int index, bool value) {
    return bind(index, static_cast<std::int64_t>(value ? 1 : 0));
}

Statement &Statement::bind(int index, const std::optional<std::string> &value) {
    return value ? bind(index, *value) : bind_null(index);
}

Statement &Statement::bind_null(int index) {
    check_bind(sqlite3_bind_null(stmt_, index));
    return *this;
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw_error(db_, rc, "step failed");
}

void Statement::run() {
    while (step()) {
    }
}

void Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

std::string Statement::column_text(int col) const {
    const auto *text = sqlite3_column_text(stmt_, col);
    if (!text) return {};
    return std::string(reinterpret_cast<const char *>(text),
                       static_cast<std::size_t>(sqlite3_column_bytes(stmt_, col)));
}

std::optional<std::string> Statement::column_optional_text(int col) const {
    if (column_is_null(col)) return std::nullopt;
    return column_text(col);
}

std::int64_t Statement::column_int64(int col) const {
    return sqlite3_column_int64(stmt_, col);
}

bool Statement::column_is_null(int col) const {
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
}

// ===========================================================================
// Database: connection, pragmas, migrations
// ===========================================================================

Database::Database(const std::string &path) {
    int rc = sqlite3_open_v2(path.c_str(), &db_,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                 SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = "cannot open database '" + path + "': " +
                          (db_ ? sqlite3_errmsg(db_) : sqlite3_errstr(rc));
        sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseError(msg, rc);
    }
    sqlite3_busy_timeout(db_, 5000);

    try {
        apply_pragmas();
        run_migrations();
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string &sql) {
    char *err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = "exec failed: ";
        msg += err ? err : sqlite3_errstr(rc);
        sqlite3_free(err);
        throw DatabaseError(msg, rc);
    }
}

Statement Database::prepare(const std::string &sql) { return Statement(db_, sql); }

void Database::transaction(const std::function<void()> &fn) {
    exec("BEGIN IMMEDIATE");
    try {
        fn();
        exec("COMMIT");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        throw;
    }
}

void Database::apply_pragmas() {
    exec("PRAGMA foreign_keys = ON");
    exec("PRAGMA journal_mode = WAL");
    exec("PRAGMA synchronous = NORMAL");
}

int Database::schema_version() {
    auto stmt = prepare("PRAGMA user_version");
    stmt.step();
    return static_cast<int>(stmt.column_int64(0));
}

int Database::latest_schema_version() {
    const auto &all = migrations();
    return all.empty() ? 0 : all.back().version;
}

void Database::run_migrations() {
    const int current = schema_version();
    if (current > latest_schema_version()) {
        throw DatabaseError("database schema version " + std::to_string(current) +
                                " is newer than supported version " +
                                std::to_string(latest_schema_version()),
                            SQLITE_ERROR);
    }

    for (const auto &m : migrations()) {
        if (m.version <= current) continue;

        // DDL + audit log + user_version bump commit atomically.
        transaction([&] {
            exec(m.sql);
            prepare("INSERT INTO _migrations (version, name, applied_at) "
                    "VALUES (?, ?, ?)")
                .bind(1, m.version)
                .bind(2, m.name)
                .bind(3, now_iso8601())
                .run();
            exec("PRAGMA user_version = " + std::to_string(m.version));
        });
    }
}

// ===========================================================================
// subjects
// ===========================================================================

void Database::create_subject(const Subject &subject) {
    prepare("INSERT INTO subjects (id, name, semester) VALUES (?, ?, ?)")
        .bind(1, subject.id)
        .bind(2, subject.name)
        .bind(3, subject.semester)
        .run();
}

std::optional<Subject> Database::get_subject(const std::string &id) {
    auto s = prepare(select_from(kSubjectCols, "subjects", "WHERE id = ?"));
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return read_subject(s);
}

std::vector<Subject> Database::list_subjects() {
    auto s = prepare(select_from(kSubjectCols, "subjects", "ORDER BY semester DESC, id"));
    std::vector<Subject> out;
    while (s.step()) out.push_back(read_subject(s));
    return out;
}

void Database::update_subject(const Subject &subject) {
    prepare("UPDATE subjects SET name = ?, semester = ? WHERE id = ?")
        .bind(1, subject.name)
        .bind(2, subject.semester)
        .bind(3, subject.id)
        .run();
}

void Database::delete_subject(const std::string &id) {
    prepare("DELETE FROM subjects WHERE id = ?").bind(1, id).run();
}

// ===========================================================================
// threads
// ===========================================================================

Thread Database::create_thread(const std::string &subject_id, const std::string &title,
                               const std::optional<std::string> &description) {
    Thread t{generate_uuid_v4(), subject_id, title, description, now_iso8601()};
    prepare("INSERT INTO threads (id, subject_id, title, description, created_at) "
            "VALUES (?, ?, ?, ?, ?)")
        .bind(1, t.id)
        .bind(2, t.subject_id)
        .bind(3, t.title)
        .bind(4, t.description)
        .bind(5, t.created_at)
        .run();
    return t;
}

std::optional<Thread> Database::get_thread(const std::string &id) {
    auto s = prepare(select_from(kThreadCols, "threads", "WHERE id = ?"));
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return read_thread(s);
}

std::vector<Thread> Database::list_threads(const std::string &subject_id) {
    auto s = prepare(
        select_from(kThreadCols, "threads", "WHERE subject_id = ? ORDER BY created_at"));
    s.bind(1, subject_id);
    std::vector<Thread> out;
    while (s.step()) out.push_back(read_thread(s));
    return out;
}

void Database::update_thread(const Thread &thread) {
    prepare("UPDATE threads SET subject_id = ?, title = ?, description = ? WHERE id = ?")
        .bind(1, thread.subject_id)
        .bind(2, thread.title)
        .bind(3, thread.description)
        .bind(4, thread.id)
        .run();
}

void Database::delete_thread(const std::string &id) {
    prepare("DELETE FROM threads WHERE id = ?").bind(1, id).run();
}

// ===========================================================================
// files
// ===========================================================================

File Database::create_file(File file) {
    if (file.id.empty()) file.id = generate_uuid_v4();
    prepare("INSERT INTO files (id, thread_id, file_name, file_type, storage_path, "
            "is_material, user_id) VALUES (?, ?, ?, ?, ?, ?, ?)")
        .bind(1, file.id)
        .bind(2, file.thread_id)
        .bind(3, file.file_name)
        .bind(4, file.file_type)
        .bind(5, file.storage_path)
        .bind(6, file.is_material)
        .bind(7, file.user_id)
        .run();
    return file;
}

std::optional<File> Database::get_file(const std::string &id) {
    auto s = prepare(select_from(kFileCols, "files", "WHERE id = ?"));
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return read_file(s);
}

std::vector<File> Database::list_files(const std::string &thread_id) {
    auto s = prepare(
        select_from(kFileCols, "files", "WHERE thread_id = ? ORDER BY file_name"));
    s.bind(1, thread_id);
    std::vector<File> out;
    while (s.step()) out.push_back(read_file(s));
    return out;
}

std::vector<File> Database::list_files_by_user(const std::string &user_id) {
    auto s = prepare(
        select_from(kFileCols, "files", "WHERE user_id = ? ORDER BY file_name"));
    s.bind(1, user_id);
    std::vector<File> out;
    while (s.step()) out.push_back(read_file(s));
    return out;
}

void Database::update_file(const File &file) {
    prepare("UPDATE files SET thread_id = ?, file_name = ?, file_type = ?, "
            "storage_path = ?, is_material = ?, user_id = ? WHERE id = ?")
        .bind(1, file.thread_id)
        .bind(2, file.file_name)
        .bind(3, file.file_type)
        .bind(4, file.storage_path)
        .bind(5, file.is_material)
        .bind(6, file.user_id)
        .bind(7, file.id)
        .run();
}

void Database::delete_file(const std::string &id) {
    prepare("DELETE FROM files WHERE id = ?").bind(1, id).run();
}

// ===========================================================================
// tasks
// ===========================================================================

Task Database::create_task(const std::string &thread_id, const std::string &title,
                           const std::optional<std::string> &deadline) {
    Task t{generate_uuid_v4(), thread_id, title, deadline};
    prepare("INSERT INTO tasks (id, thread_id, title, deadline) VALUES (?, ?, ?, ?)")
        .bind(1, t.id)
        .bind(2, t.thread_id)
        .bind(3, t.title)
        .bind(4, t.deadline)
        .run();
    return t;
}

std::optional<Task> Database::get_task(const std::string &id) {
    auto s = prepare(select_from(kTaskCols, "tasks", "WHERE id = ?"));
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return read_task(s);
}

std::vector<Task> Database::list_tasks(const std::string &thread_id) {
    // Tasks without a deadline sort last.
    auto s = prepare(select_from(kTaskCols, "tasks",
                                 "WHERE thread_id = ? "
                                 "ORDER BY deadline IS NULL, deadline, title"));
    s.bind(1, thread_id);
    std::vector<Task> out;
    while (s.step()) out.push_back(read_task(s));
    return out;
}

void Database::update_task(const Task &task) {
    prepare("UPDATE tasks SET thread_id = ?, title = ?, deadline = ? WHERE id = ?")
        .bind(1, task.thread_id)
        .bind(2, task.title)
        .bind(3, task.deadline)
        .bind(4, task.id)
        .run();
}

void Database::delete_task(const std::string &id) {
    prepare("DELETE FROM tasks WHERE id = ?").bind(1, id).run();
}

// ===========================================================================
// users
// ===========================================================================

User Database::create_user(const std::string &username,
                           const std::optional<std::string> &email,
                           const std::optional<std::string> &hashed_password,
                           const std::optional<std::string> &salt) {
    User u{generate_uuid_v4(), username, email, hashed_password, salt, now_iso8601()};
    prepare("INSERT INTO users (id, username, email, hashed_password, salt, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?)")
        .bind(1, u.id)
        .bind(2, u.username)
        .bind(3, u.email)
        .bind(4, u.hashed_password)
        .bind(5, u.salt)
        .bind(6, u.created_at)
        .run();
    return u;
}

std::optional<User> Database::get_user(const std::string &id) {
    auto s = prepare(select_from(kUserCols, "users", "WHERE id = ?"));
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return read_user(s);
}

std::optional<User> Database::get_user_by_username(const std::string &username) {
    auto s = prepare(select_from(kUserCols, "users", "WHERE username = ?"));
    s.bind(1, username);
    if (!s.step()) return std::nullopt;
    return read_user(s);
}

std::vector<User> Database::list_users() {
    auto s = prepare(select_from(kUserCols, "users", "ORDER BY username"));
    std::vector<User> out;
    while (s.step()) out.push_back(read_user(s));
    return out;
}

void Database::update_user(const User &user) {
    prepare("UPDATE users SET username = ?, email = ?, hashed_password = ?, salt = ? "
            "WHERE id = ?")
        .bind(1, user.username)
        .bind(2, user.email)
        .bind(3, user.hashed_password)
        .bind(4, user.salt)
        .bind(5, user.id)
        .run();
}

void Database::delete_user(const std::string &id) {
    prepare("DELETE FROM users WHERE id = ?").bind(1, id).run();
}

} // namespace noteman::db
