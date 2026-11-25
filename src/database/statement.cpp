#include "database/statement.hpp"

#include "database/error.hpp"

#include <sqlite3.h>

namespace noteman::db {

using detail::throw_sqlite_error;

Statement::Statement(sqlite3 *db, const char *sql) : db_(db) {
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr);
    if (rc != SQLITE_OK) throw_sqlite_error(db_, rc, std::string("prepare failed (") + sql + ")");
}

Statement::~Statement() { sqlite3_finalize(stmt_); }

Statement::Statement(Statement &&other) noexcept : db_(other.db_), stmt_(other.stmt_) {
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
    if (rc != SQLITE_OK) throw_sqlite_error(db_, rc, "bind failed");
}

Statement &Statement::bind(int index, const std::string &value) {
    check_bind(sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()),
                                 SQLITE_TRANSIENT));
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

Statement &Statement::bind(int index, double value) {
    check_bind(sqlite3_bind_double(stmt_, index, value));
    return *this;
}

Statement &Statement::bind(int index, const std::optional<std::string> &value) {
    return value ? bind(index, *value) : bind_null(index);
}

Statement &Statement::bind(int index, const std::optional<std::int64_t> &value) {
    return value ? bind(index, *value) : bind_null(index);
}

Statement &Statement::bind(int index, const std::optional<double> &value) {
    return value ? bind(index, *value) : bind_null(index);
}

Statement &Statement::bind_blob(int index, std::string_view bytes) {
    // A zero-length blob must still be non-NULL (content is NOT NULL).
    check_bind(sqlite3_bind_blob64(stmt_, index, bytes.empty() ? "" : bytes.data(),
                                   static_cast<sqlite3_uint64>(bytes.size()),
                                   SQLITE_TRANSIENT));
    return *this;
}

Statement &Statement::bind_null(int index) {
    check_bind(sqlite3_bind_null(stmt_, index));
    return *this;
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw_sqlite_error(db_, rc, "step failed");
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

std::int64_t Statement::column_int64(int col) const { return sqlite3_column_int64(stmt_, col); }

std::optional<std::int64_t> Statement::column_optional_int64(int col) const {
    if (column_is_null(col)) return std::nullopt;
    return column_int64(col);
}

double Statement::column_double(int col) const { return sqlite3_column_double(stmt_, col); }

std::optional<double> Statement::column_optional_double(int col) const {
    if (column_is_null(col)) return std::nullopt;
    return column_double(col);
}

bool Statement::column_bool(int col) const { return column_int64(col) != 0; }

std::string Statement::column_blob(int col) const {
    const void *data = sqlite3_column_blob(stmt_, col);
    const int size = sqlite3_column_bytes(stmt_, col);
    if (!data || size <= 0) return {};
    return std::string(static_cast<const char *>(data), static_cast<std::size_t>(size));
}

bool Statement::column_is_null(int col) const {
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
}

} // namespace noteman::db
