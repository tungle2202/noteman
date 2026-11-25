#pragma once

#include <string>

namespace noteman::modules {

// Current UTC time in the project's fixed ISO 8601 format with millisecond
// precision, e.g. "2026-11-25T04:35:12.345Z".
// Matches SQLite's strftime('%Y-%m-%dT%H:%M:%fZ', 'now') used by the schema
// defaults, so values from C++ and from SQL sort and compare consistently.
std::string now_iso8601();

} // namespace noteman::modules
