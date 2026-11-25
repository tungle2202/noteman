// Connection lifecycle, pragmas, transactions and schema migrations.

#include "database/database.hpp"

#include "database/migrations.hpp"
#include "database/sql/queries.hpp"

#include <sqlite3.h>

namespace noteman::db {

using detail::throw_sqlite_error;

Database::Database(const std::string &path) {
    int rc = sqlite3_open_v2(path.c_str(), &db_,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = "cannot open database '" + path +
                          "': " + (db_ ? sqlite3_errmsg(db_) : sqlite3_errstr(rc));
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

Statement Database::prepare(const char *sql) { return Statement(db_, sql); }

int Database::changes() const { return sqlite3_changes(db_); }

// ---------------------------------------------------------------------------
// Transactions
// ---------------------------------------------------------------------------

Database::TransactionScope::TransactionScope(Database &db) : db_(db) {
    if (sqlite3_get_autocommit(db_.db_)) {
        db_.exec("BEGIN IMMEDIATE");
    } else {
        savepoint_ = "nm_sp_" + std::to_string(++db_.savepoint_depth_);
        try {
            db_.exec("SAVEPOINT " + savepoint_);
        } catch (...) {
            --db_.savepoint_depth_;
            throw;
        }
    }
}

void Database::TransactionScope::commit() {
    db_.exec(savepoint_.empty() ? std::string("COMMIT") : "RELEASE " + savepoint_);
    done_ = true;
    if (!savepoint_.empty()) --db_.savepoint_depth_;
}

Database::TransactionScope::~TransactionScope() {
    if (done_) return;
    if (savepoint_.empty()) {
        sqlite3_exec(db_.db_, "ROLLBACK", nullptr, nullptr, nullptr);
    } else {
        const std::string rollback = "ROLLBACK TO " + savepoint_ + "; RELEASE " + savepoint_;
        sqlite3_exec(db_.db_, rollback.c_str(), nullptr, nullptr, nullptr);
        --db_.savepoint_depth_;
    }
}

// ---------------------------------------------------------------------------
// Pragmas & migrations
// ---------------------------------------------------------------------------

void Database::apply_pragmas() {
    exec("PRAGMA foreign_keys = ON");
    exec("PRAGMA journal_mode = WAL");
    exec("PRAGMA synchronous = NORMAL");
}

int Database::schema_version() {
    auto stmt = prepare(sql::kSelectSchemaVersion);
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
            exec(std::string(m.sql));
            prepare(sql::kInsertMigration).bind(1, m.version).bind(2, std::string(m.name)).run();
            exec("PRAGMA user_version = " + std::to_string(m.version));
        });
    }
}

} // namespace noteman::db
