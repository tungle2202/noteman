#include "database/error.hpp"

#include <sqlite3.h>

namespace noteman::db::detail {

void throw_sqlite_error(sqlite3 *db, int rc, const std::string &context) {
    std::string msg = context + ": ";
    msg += db ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
    throw DatabaseError(msg, rc);
}

} // namespace noteman::db::detail
