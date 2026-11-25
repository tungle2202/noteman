#include "database/database.hpp"

#include "database/row_readers.hpp"
#include "database/sql/queries.hpp"
#include "modules/permission.hpp"
#include "modules/uuid.hpp"

namespace noteman::db {

using namespace detail;
namespace permission = modules::permission;

Subject Database::create_subject(const NewSubject &subject) {
    auto s = prepare(sql::kInsertSubject);
    s.bind(1, modules::generate_uuid_v4())
        .bind(2, subject.code)
        .bind(3, subject.name)
        .bind(4, subject.semester)
        .bind(5, subject.owner_id)
        .bind(6, subject.group_id)
        .bind(7, subject.mode);
    return read_required(s, read_subject);
}

std::optional<Subject> Database::get_subject(const std::string &id) {
    auto s = prepare(sql::kSelectSubjectById);
    s.bind(1, id);
    return read_one(s, read_subject);
}

std::vector<Subject> Database::list_subjects_owned_by(const std::string &owner_id) {
    auto s = prepare(sql::kSelectSubjectsByOwner);
    s.bind(1, owner_id);
    return read_all(s, read_subject);
}

std::vector<Subject> Database::list_visible_subjects(const std::string &user_id) {
    auto s = prepare(sql::kSelectVisibleSubjects);
    s.bind(1, user_id);
    return read_all(s, read_subject);
}

bool Database::update_subject(const Subject &subject) {
    prepare(sql::kUpdateSubject)
        .bind(1, subject.id)
        .bind(2, subject.code)
        .bind(3, subject.name)
        .bind(4, subject.semester)
        .bind(5, subject.owner_id)
        .bind(6, subject.group_id)
        .bind(7, subject.mode)
        .run();
    return changes() > 0;
}

bool Database::delete_subject(const std::string &id) {
    return transaction([&] {
        // quotes -> materials is ON DELETE RESTRICT, which SQLite checks
        // immediately. Deleting the threads first removes this subject's own
        // quotes before the materials cascade reaches them; quotes from other
        // subjects' threads still (intentionally) block the delete.
        prepare(sql::kDeleteThreadsBySubject).bind(1, id).run();
        prepare(sql::kDeleteSubject).bind(1, id).run();
        return changes() > 0;
    });
}

bool Database::can_access_subject(const std::string &user_id, const std::string &subject_id,
                                  permission::Access access) {
    auto s = prepare(sql::kSelectSubjectAccess);
    s.bind(1, user_id).bind(2, subject_id);
    if (!s.step()) return false; // no such subject

    const bool is_owner = s.column_bool(0);
    const bool in_group = s.column_bool(1);
    const int mode = static_cast<int>(s.column_int64(2));
    const bool is_admin = s.column_bool(3);

    if (is_admin) return true;
    return permission::allows(mode, permission::resolve_class(is_owner, in_group), access);
}

} // namespace noteman::db
