#pragma once

// Internal: map the current row of a statement to a model.
// Column order must match the NM_*_COLS lists in sql/queries.hpp.

#include "database/error.hpp"
#include "database/models.hpp"
#include "database/statement.hpp"

#include <optional>
#include <vector>

namespace noteman::db::detail {

template <typename T> using RowReader = T (*)(const Statement &);

// First row of `s`, or nullopt when there is none.
template <typename T> std::optional<T> read_one(Statement &s, RowReader<T> reader) {
    if (!s.step()) return std::nullopt;
    return reader(s);
}

// Like read_one, but a missing row is an error (used with RETURNING).
template <typename T> T read_required(Statement &s, RowReader<T> reader) {
    if (!s.step()) throw DatabaseError("statement returned no row", 0);
    return reader(s);
}

template <typename T> std::vector<T> read_all(Statement &s, RowReader<T> reader) {
    std::vector<T> out;
    while (s.step()) out.push_back(reader(s));
    return out;
}

User read_user(const Statement &s);
Group read_group(const Statement &s);
Subject read_subject(const Statement &s);
Thread read_thread(const Statement &s);
Blob read_blob(const Statement &s);
Material read_material(const Statement &s);
Quote read_quote(const Statement &s);
ReadingProgress read_reading_progress(const Statement &s);
Note read_note(const Statement &s);
NoteVersion read_note_version(const Statement &s);
Task read_task(const Statement &s);

} // namespace noteman::db::detail
