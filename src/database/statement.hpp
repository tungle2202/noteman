#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace noteman::db {

// Prepared statement (RAII). Parameter indexes are 1-based and column
// indexes 0-based, like the sqlite3_bind_* / sqlite3_column_* API.
class Statement {
public:
    Statement(sqlite3 *db, const char *sql);
    ~Statement();
    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;
    Statement(Statement &&other) noexcept;
    Statement &operator=(Statement &&other) noexcept;

    Statement &bind(int index, const std::string &value);
    Statement &bind(int index, const char *value); // nullptr binds NULL
    Statement &bind(int index, std::int64_t value);
    Statement &bind(int index, int value);
    Statement &bind(int index, bool value);
    Statement &bind(int index, double value);
    Statement &bind(int index, const std::optional<std::string> &value);
    Statement &bind(int index, const std::optional<std::int64_t> &value);
    Statement &bind(int index, const std::optional<double> &value);
    Statement &bind_blob(int index, std::string_view bytes);
    Statement &bind_null(int index);

    // Advances; returns true while a row is available.
    bool step();
    // Executes a statement that returns no rows (or ignores them).
    void run();
    void reset();

    std::string column_text(int col) const;
    std::optional<std::string> column_optional_text(int col) const;
    std::int64_t column_int64(int col) const;
    std::optional<std::int64_t> column_optional_int64(int col) const;
    double column_double(int col) const;
    std::optional<double> column_optional_double(int col) const;
    bool column_bool(int col) const;
    std::string column_blob(int col) const; // raw bytes
    bool column_is_null(int col) const;

private:
    void check_bind(int rc);

    sqlite3 *db_ = nullptr;
    sqlite3_stmt *stmt_ = nullptr;
};

} // namespace noteman::db
