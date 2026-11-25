// Text notes (.md, .tex) and their Git-like version history.

#include "database/database.hpp"

#include "database/row_readers.hpp"
#include "database/sql/queries.hpp"
#include "modules/sha256.hpp"
#include "modules/uuid.hpp"

#include <sqlite3.h>

namespace noteman::db {

using namespace detail;

namespace {

// Content is stored uncompressed until a zstd dependency is added; the
// compression column records this per version so both can coexist later.
constexpr Compression kStoreCompression = Compression::None;

void insert_version(Database &db, const std::string &note_id, const std::string &version_id,
                    const std::optional<std::string> &parent_id,
                    const std::optional<std::string> &author_id, const std::string &text,
                    const std::string &content_hash, const std::optional<std::string> &message) {
    db.prepare(sql::kInsertNoteVersion)
        .bind(1, version_id)
        .bind(2, note_id)
        .bind(3, parent_id)
        .bind(4, author_id)
        .bind_blob(5, text)
        .bind(6, to_string(kStoreCompression))
        .bind(7, content_hash)
        .bind(8, static_cast<std::int64_t>(text.size()))
        .bind(9, message)
        .run();
}

} // namespace

// ---------------------------------------------------------------------------
// notes
// ---------------------------------------------------------------------------

Note Database::create_note(const std::string &thread_id, const std::string &file_name,
                           NoteFormat format, const std::string &initial_text,
                           const std::optional<std::string> &created_by) {
    return transaction([&] {
        const std::string note_id = modules::generate_uuid_v4();
        const std::string version_id = modules::generate_uuid_v4();

        // current_version_id is DEFERRABLE INITIALLY DEFERRED, so the note can
        // point at the version inserted right after it.
        prepare(sql::kInsertNote)
            .bind(1, note_id)
            .bind(2, thread_id)
            .bind(3, file_name)
            .bind(4, to_string(format))
            .bind(5, version_id)
            .bind(6, created_by)
            .run();
        insert_version(*this, note_id, version_id, std::nullopt, created_by, initial_text,
                       modules::sha256_hex(initial_text), std::nullopt);

        auto s = prepare(sql::kSelectNoteById);
        s.bind(1, note_id);
        return read_required(s, read_note);
    });
}

std::optional<Note> Database::get_note(const std::string &id) {
    auto s = prepare(sql::kSelectNoteById);
    s.bind(1, id);
    return read_one(s, read_note);
}

std::vector<Note> Database::list_notes(const std::string &thread_id) {
    auto s = prepare(sql::kSelectNotesByThread);
    s.bind(1, thread_id);
    return read_all(s, read_note);
}

bool Database::rename_note(const std::string &id, const std::string &file_name) {
    prepare(sql::kRenameNote).bind(1, id).bind(2, file_name).run();
    return changes() > 0;
}

bool Database::delete_note(const std::string &id) {
    prepare(sql::kDeleteNote).bind(1, id).run();
    return changes() > 0;
}

// ---------------------------------------------------------------------------
// Saving & history
// ---------------------------------------------------------------------------

NoteSaveResult Database::save_note(const std::string &note_id,
                                   const std::optional<std::string> &parent_version_id,
                                   const std::string &text,
                                   const std::optional<std::string> &author_id,
                                   const std::optional<std::string> &message) {
    return transaction([&]() -> NoteSaveResult {
        auto head = prepare(sql::kSelectNoteHead);
        head.bind(1, note_id);
        if (!head.step()) throw DatabaseError("note not found: " + note_id, SQLITE_NOTFOUND);
        const auto current_id = head.column_optional_text(0);
        const auto current_hash = head.column_optional_text(1);

        if (current_id != parent_version_id) return {SaveStatus::Conflict, current_id};

        const std::string hash = modules::sha256_hex(text);
        if (current_hash && *current_hash == hash) return {SaveStatus::Unchanged, current_id};

        // Advance the head first (deferred FK) so a lost race writes nothing.
        const std::string version_id = modules::generate_uuid_v4();
        prepare(sql::kAdvanceNoteHead)
            .bind(1, note_id)
            .bind(2, version_id)
            .bind(3, parent_version_id)
            .run();
        if (changes() == 0) return {SaveStatus::Conflict, current_id};

        insert_version(*this, note_id, version_id, parent_version_id, author_id, text, hash,
                       message);
        return {SaveStatus::Saved, version_id};
    });
}

NoteSaveResult Database::restore_note_version(const std::string &note_id,
                                              const std::string &version_id,
                                              const std::optional<std::string> &author_id,
                                              const std::optional<std::string> &message) {
    return transaction([&] {
        auto s = prepare(sql::kSelectNoteVersionContentOfNote);
        s.bind(1, version_id).bind(2, note_id);
        const auto content = read_version_content(s);
        if (!content) {
            throw DatabaseError("version " + version_id + " does not belong to note " + note_id,
                                SQLITE_NOTFOUND);
        }

        const auto note = get_note(note_id);
        return save_note(note_id, note->current_version_id, *content, author_id,
                         message ? message : "Restored from version " + version_id);
    });
}

std::optional<NoteVersion> Database::get_note_version(const std::string &version_id) {
    auto s = prepare(sql::kSelectNoteVersionById);
    s.bind(1, version_id);
    return read_one(s, read_note_version);
}

std::vector<NoteVersion> Database::list_note_versions(const std::string &note_id) {
    auto s = prepare(sql::kSelectNoteVersionsByNote);
    s.bind(1, note_id);
    return read_all(s, read_note_version);
}

std::optional<std::string> Database::read_note_version_content(const std::string &version_id) {
    auto s = prepare(sql::kSelectNoteVersionContent);
    s.bind(1, version_id);
    return read_version_content(s);
}

std::optional<std::string> Database::read_note_content(const std::string &note_id) {
    const auto note = get_note(note_id);
    if (!note || !note->current_version_id) return std::nullopt;
    return read_note_version_content(*note->current_version_id);
}

bool Database::set_note_version_pinned(const std::string &version_id, bool pinned) {
    prepare(sql::kSetNoteVersionPinned).bind(1, version_id).bind(2, pinned).run();
    return changes() > 0;
}

// Expects a statement selecting (compression, content).
std::optional<std::string> Database::read_version_content(Statement &stmt) {
    if (!stmt.step()) return std::nullopt;
    if (compression_from_string(stmt.column_text(0)) != Compression::None) {
        throw DatabaseError("zstd-compressed note content is not supported yet", SQLITE_ERROR);
    }
    return stmt.column_blob(1);
}

} // namespace noteman::db
