#pragma once

// Noteman SQLite database layer.
//
//   Schema design : docs/design/database/ERD.dbml
//   Schema DDL    : src/database/sql/migrations/NNN_name.sql (embedded at build time)
//   DML statements: src/database/sql/queries.hpp
//
// The Database methods are implemented per domain:
//   database.cpp (connection, transactions, migrations), users.cpp, groups.cpp,
//   subjects.cpp, threads.cpp, materials.cpp, quotes.cpp, reading_progress.cpp,
//   notes.cpp, tasks.cpp.

#include "database/error.hpp"
#include "database/models.hpp"
#include "database/statement.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct sqlite3;

namespace noteman::db {

class Database {
public:
    // Opens (creating if needed) the database at `path`, applies the pragmas
    // (foreign_keys=ON, journal_mode=WAL, synchronous=NORMAL) and runs all
    // pending migrations. Use ":memory:" for an in-memory database.
    explicit Database(const std::string &path);
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    sqlite3 *handle() const noexcept { return db_; }

    void exec(const std::string &sql);
    Statement prepare(const char *sql);
    Statement prepare(const std::string &sql) { return prepare(sql.c_str()); }

    // Rows changed by the most recent INSERT / UPDATE / DELETE (triggers excluded).
    int changes() const;

    // Runs `fn` atomically and returns its result. The outermost call uses
    // BEGIN IMMEDIATE / COMMIT; nested calls use SAVEPOINTs, so API methods can
    // be composed inside a caller's transaction. Rolls back if `fn` throws.
    template <typename Fn>
    auto transaction(Fn &&fn) -> std::invoke_result_t<Fn &> {
        TransactionScope scope(*this);
        if constexpr (std::is_void_v<std::invoke_result_t<Fn &>>) {
            fn();
            scope.commit();
        } else {
            auto result = fn();
            scope.commit();
            return result;
        }
    }

    int schema_version();
    static int latest_schema_version();

    // --- users --------------------------------------------------------------
    // Password hashing (Argon2id, PHC string) is the caller's responsibility.
    // Without primary_group_id a private group named after the user is created.
    // The primary group is mirrored into user_groups by a trigger.
    User create_user(const NewUser &user);
    std::optional<User> get_user(const std::string &id);
    std::optional<User> get_user_by_username(const std::string &username);
    std::vector<User> list_users();
    bool update_user(const User &user); // username, email, password_hash, primary_group_id, is_admin
    bool delete_user(const std::string &id); // fails while the user owns subjects

    // --- groups & membership --------------------------------------------------
    Group create_group(const std::string &name);
    std::optional<Group> get_group(const std::string &id);
    std::optional<Group> get_group_by_name(const std::string &name);
    std::vector<Group> list_groups();
    bool rename_group(const std::string &id, const std::string &name);
    bool delete_group(const std::string &id); // fails while it is a primary group or owns subjects

    void add_user_to_group(const std::string &user_id, const std::string &group_id);
    // Fails for the user's primary group.
    bool remove_user_from_group(const std::string &user_id, const std::string &group_id);
    std::vector<Group> list_groups_of_user(const std::string &user_id);
    std::vector<User> list_group_members(const std::string &group_id);

    // --- subjects -------------------------------------------------------------
    Subject create_subject(const NewSubject &subject);
    std::optional<Subject> get_subject(const std::string &id);
    std::vector<Subject> list_subjects_owned_by(const std::string &owner_id);
    // Subjects `user_id` can read (admins: all).
    std::vector<Subject> list_visible_subjects(const std::string &user_id);
    bool update_subject(const Subject &subject); // code, name, semester, owner_id, group_id, mode
    // Cascades to threads, notes, quotes, tasks and materials. Fails while one
    // of its materials is quoted from another subject's thread.
    bool delete_subject(const std::string &id);

    // --- permissions ----------------------------------------------------------
    // owner -> group -> other, first matching class applies; admins bypass.
    // Threads, notes, quotes and tasks inherit from their subject.
    bool can_access_subject(const std::string &user_id, const std::string &subject_id,
                            modules::permission::Access access);
    bool can_read_subject(const std::string &user_id, const std::string &subject_id) {
        return can_access_subject(user_id, subject_id, modules::permission::Access::Read);
    }
    bool can_write_subject(const std::string &user_id, const std::string &subject_id) {
        return can_access_subject(user_id, subject_id, modules::permission::Access::Write);
    }

    // --- threads --------------------------------------------------------------
    Thread create_thread(const std::string &subject_id, const std::string &title,
                         const std::optional<std::string> &description = std::nullopt,
                         const std::optional<std::string> &created_by = std::nullopt);
    std::optional<Thread> get_thread(const std::string &id);
    std::vector<Thread> list_threads(const std::string &subject_id);
    bool update_thread(const Thread &thread); // subject_id, title, description
    bool delete_thread(const std::string &id);

    // --- blobs ----------------------------------------------------------------
    // Registers content already written to modules::blob_relative_path(hash).
    // Idempotent: an existing blob with the same hash is returned unchanged.
    Blob register_blob(const std::string &hash, std::int64_t size, const std::string &mime_type,
                       const std::optional<std::int64_t> &page_count = std::nullopt);
    std::optional<Blob> get_blob(const std::string &hash);
    // Deletes blob rows no material references; returns their hashes so the
    // caller can remove the files from disk.
    std::vector<std::string> delete_unreferenced_blobs();

    // --- materials ------------------------------------------------------------
    Material add_material(const std::string &subject_id, const std::string &blob_hash,
                          const std::string &display_name,
                          const std::optional<std::string> &uploaded_by = std::nullopt);
    std::optional<Material> get_material(const std::string &id);
    std::vector<Material> list_materials(const std::string &subject_id,
                                         bool include_deleted = false);
    bool rename_material(const std::string &id, const std::string &display_name);
    bool soft_delete_material(const std::string &id); // hidden, quotes still resolve
    bool restore_material(const std::string &id);
    bool delete_material(const std::string &id); // fails while quoted

    // --- quotes ---------------------------------------------------------------
    Quote create_quote(const NewQuote &quote);
    std::optional<Quote> get_quote(const std::string &id);
    std::vector<Quote> list_quotes_by_thread(const std::string &thread_id);
    std::vector<Quote> list_quotes_by_material(const std::string &material_id);
    bool update_quote(const Quote &quote); // kind, spec_json, extracted_text, note
    bool delete_quote(const std::string &id);

    // --- reading progress -----------------------------------------------------
    ReadingProgress save_reading_progress(const std::string &user_id,
                                          const std::string &material_id, std::int64_t page,
                                          double page_offset = 0.0,
                                          const std::optional<double> &zoom = std::nullopt);
    std::optional<ReadingProgress> get_reading_progress(const std::string &user_id,
                                                        const std::string &material_id);

    // --- notes ----------------------------------------------------------------
    // Creates the note and its first version in one transaction.
    Note create_note(const std::string &thread_id, const std::string &file_name,
                     NoteFormat format, const std::string &initial_text = {},
                     const std::optional<std::string> &created_by = std::nullopt);
    std::optional<Note> get_note(const std::string &id);
    std::vector<Note> list_notes(const std::string &thread_id);
    bool rename_note(const std::string &id, const std::string &file_name);
    bool delete_note(const std::string &id); // cascades to its versions

    // Saves `text` as a new version on top of `parent_version_id` (the version
    // the editor started from). Optimistic concurrency: if the note's current
    // version is no longer the parent, nothing is written and Conflict is
    // returned with the current version id (3-way merge base = parent).
    // Saving text identical to the current version returns Unchanged.
    NoteSaveResult save_note(const std::string &note_id,
                             const std::optional<std::string> &parent_version_id,
                             const std::string &text,
                             const std::optional<std::string> &author_id = std::nullopt,
                             const std::optional<std::string> &message = std::nullopt);

    // Rollback: saves the content of `version_id` as a new version on top of
    // the current one (history is kept and the rollback has an author).
    NoteSaveResult restore_note_version(const std::string &note_id,
                                        const std::string &version_id,
                                        const std::optional<std::string> &author_id = std::nullopt,
                                        const std::optional<std::string> &message = std::nullopt);

    std::optional<NoteVersion> get_note_version(const std::string &version_id);
    std::vector<NoteVersion> list_note_versions(const std::string &note_id); // newest first
    std::optional<std::string> read_note_version_content(const std::string &version_id);
    std::optional<std::string> read_note_content(const std::string &note_id); // current version
    bool set_note_version_pinned(const std::string &version_id, bool pinned);

    // --- tasks ----------------------------------------------------------------
    Task create_task(const std::string &thread_id, const std::string &title,
                     const std::optional<std::string> &deadline = std::nullopt,
                     const std::optional<std::string> &created_by = std::nullopt);
    std::optional<Task> get_task(const std::string &id);
    std::vector<Task> list_tasks(const std::string &thread_id); // open first, by deadline
    bool update_task(const Task &task); // thread_id, title, deadline
    bool set_task_completed(const std::string &id, bool completed);
    bool delete_task(const std::string &id);

private:
    class TransactionScope {
    public:
        explicit TransactionScope(Database &db);
        ~TransactionScope();
        TransactionScope(const TransactionScope &) = delete;
        TransactionScope &operator=(const TransactionScope &) = delete;
        void commit();

    private:
        Database &db_;
        std::string savepoint_; // empty for the outermost BEGIN IMMEDIATE
        bool done_ = false;
    };

    void apply_pragmas();
    void run_migrations();
    std::optional<std::string> read_version_content(Statement &stmt);

    sqlite3 *db_ = nullptr;
    int savepoint_depth_ = 0;
};

} // namespace noteman::db
