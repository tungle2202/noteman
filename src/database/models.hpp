#pragma once

// Row models, one per table in docs/design/database/ERD.dbml.
// Timestamps are ISO 8601 UTC strings: "2026-11-25T04:35:12.345Z".

#include "modules/permission.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace noteman::db {

// ---------------------------------------------------------------------------
// Enumerated TEXT columns
// ---------------------------------------------------------------------------

enum class QuoteKind { Full, Pages, Region, Text };    // quotes.kind
enum class NoteFormat { Markdown, Tex };               // notes.format
enum class Compression { None, Zstd };                 // note_versions.compression

const char *to_string(QuoteKind kind);                 // "full", "pages", "region", "text"
const char *to_string(NoteFormat format);              // "md", "tex"
const char *to_string(Compression compression);        // "none", "zstd"

// Throw std::invalid_argument on unknown values.
QuoteKind quote_kind_from_string(std::string_view text);
NoteFormat note_format_from_string(std::string_view text);
Compression compression_from_string(std::string_view text);

// ---------------------------------------------------------------------------
// Identity & groups
// ---------------------------------------------------------------------------

struct User {
    std::string id;
    std::string username;
    std::optional<std::string> email;
    std::optional<std::string> password_hash; // Argon2id PHC string
    std::string primary_group_id;
    bool is_admin = false;
    std::string created_at;
};

struct NewUser {
    std::string username;
    std::optional<std::string> email;
    std::optional<std::string> password_hash; // already hashed by the caller
    // Empty: a private group named after the user is created and used.
    std::optional<std::string> primary_group_id;
    bool is_admin = false;
};

struct Group {
    std::string id;
    std::string name;
    std::string created_at;
};

// ---------------------------------------------------------------------------
// Workspace hierarchy
// ---------------------------------------------------------------------------

struct Subject {
    std::string id;
    std::string code;     // e.g. "IT3080"
    std::string name;     // e.g. "Mạng máy tính"
    std::string semester; // e.g. "20261"
    std::string owner_id;
    std::string group_id;
    int mode = modules::permission::kDefaultMode;
    std::string created_at;
    std::string last_update;
};

struct NewSubject {
    std::string code;
    std::string name;
    std::string semester;
    std::string owner_id;
    std::optional<std::string> group_id; // empty: owner's primary group
    int mode = modules::permission::kDefaultMode;
};

struct Thread {
    std::string id;
    std::string subject_id;
    std::string title;
    std::optional<std::string> description;
    std::optional<std::string> created_by;
    std::string created_at;
    std::string last_update;
};

// ---------------------------------------------------------------------------
// Binary content
// ---------------------------------------------------------------------------

struct Blob {
    std::string hash; // SHA-256 hex
    std::int64_t size = 0;
    std::string mime_type;
    std::optional<std::int64_t> page_count;
    std::string created_at;
};

struct Material {
    std::string id;
    std::string subject_id;
    std::string blob_hash;
    std::string display_name;
    std::optional<std::string> uploaded_by;
    std::string uploaded_at;
    std::optional<std::string> deleted_at; // soft delete
};

// ---------------------------------------------------------------------------
// PDF quoting & reading progress
// ---------------------------------------------------------------------------

struct Quote {
    std::string id;
    std::string thread_id;
    std::string material_id;
    QuoteKind kind = QuoteKind::Full;
    std::string spec_json = "{}"; // PDF user-space units (points)
    std::optional<std::string> extracted_text;
    std::optional<std::string> note;
    std::optional<std::string> author_id;
    std::string created_at;
    std::string last_update;
};

struct NewQuote {
    std::string thread_id;
    std::string material_id;
    QuoteKind kind = QuoteKind::Full;
    std::string spec_json = "{}";
    std::optional<std::string> extracted_text;
    std::optional<std::string> note;
    std::optional<std::string> author_id;
};

struct ReadingProgress {
    std::string user_id;
    std::string material_id;
    std::int64_t page = 1;    // 1-based
    double page_offset = 0.0; // 0.0 (top) .. 1.0 (bottom)
    std::optional<double> zoom;
    std::string updated_at;
};

// ---------------------------------------------------------------------------
// Notes & versions
// ---------------------------------------------------------------------------

struct Note {
    std::string id;
    std::string thread_id;
    std::string file_name;
    NoteFormat format = NoteFormat::Markdown;
    std::optional<std::string> current_version_id;
    std::optional<std::string> created_by;
    std::string created_at;
    std::string last_update;
};

// Version metadata; content is fetched separately (read_note_version_content).
struct NoteVersion {
    std::string id;
    std::string note_id;
    std::optional<std::string> parent_id;
    std::optional<std::string> author_id;
    Compression compression = Compression::None;
    std::string content_hash; // SHA-256 of the uncompressed text
    std::int64_t size = 0;    // uncompressed bytes
    std::optional<std::string> message;
    bool pinned = false;
    std::string created_at;
};

enum class SaveStatus {
    Saved,     // new version created, note now points to it
    Unchanged, // text identical to the current version, nothing written
    Conflict,  // expected parent is no longer current: merge and retry
};

struct NoteSaveResult {
    SaveStatus status = SaveStatus::Saved;
    // Saved: the new version. Unchanged / Conflict: the note's current version.
    std::optional<std::string> version_id;
};

// ---------------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------------

struct Task {
    std::string id;
    std::string thread_id;
    std::string title;
    std::optional<std::string> deadline; // ISO 8601 UTC or YYYY-MM-DD
    std::optional<std::string> completed_at;
    std::optional<std::string> created_by;
    std::string created_at;
    std::string last_update;
};

} // namespace noteman::db
