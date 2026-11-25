#pragma once

#include <string_view>
#include <vector>

namespace noteman::db {

struct Migration {
    int version;           // sequential: 1, 2, 3, ...
    std::string_view name; // file name without extension, e.g. "003_permissions_blobs_versions"
    std::string_view sql;  // full script
};

// Ordered, forward-only schema migrations, embedded at build time from
// src/database/sql/migrations/NNN_name.sql. Never edit a migration that has
// shipped; add a new numbered file instead (re-run CMake to pick it up).
const std::vector<Migration> &migrations();

} // namespace noteman::db
