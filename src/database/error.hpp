#pragma once

#include <stdexcept>
#include <string>

struct sqlite3;

namespace noteman::db {

// Thrown for every SQLite failure. code() is the SQLite result code
// (e.g. SQLITE_CONSTRAINT for a UNIQUE / FOREIGN KEY / CHECK violation).
class DatabaseError : public std::runtime_error {
public:
    DatabaseError(const std::string &what, int code)
        : std::runtime_error(what), code_(code) {}
    int code() const noexcept { return code_; }

private:
    int code_;
};

namespace detail {

// Throws DatabaseError with `context` and the connection's last error message.
[[noreturn]] void throw_sqlite_error(sqlite3 *db, int rc, const std::string &context);

} // namespace detail

} // namespace noteman::db
