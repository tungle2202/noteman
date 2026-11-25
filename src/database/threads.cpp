#include "database/database.hpp"

#include "database/row_readers.hpp"
#include "database/sql/queries.hpp"
#include "modules/uuid.hpp"

namespace noteman::db {

using namespace detail;

Thread Database::create_thread(const std::string &subject_id, const std::string &title,
                               const std::optional<std::string> &description,
                               const std::optional<std::string> &created_by) {
    auto s = prepare(sql::kInsertThread);
    s.bind(1, modules::generate_uuid_v4())
        .bind(2, subject_id)
        .bind(3, title)
        .bind(4, description)
        .bind(5, created_by);
    return read_required(s, read_thread);
}

std::optional<Thread> Database::get_thread(const std::string &id) {
    auto s = prepare(sql::kSelectThreadById);
    s.bind(1, id);
    return read_one(s, read_thread);
}

std::vector<Thread> Database::list_threads(const std::string &subject_id) {
    auto s = prepare(sql::kSelectThreadsBySubject);
    s.bind(1, subject_id);
    return read_all(s, read_thread);
}

bool Database::update_thread(const Thread &thread) {
    prepare(sql::kUpdateThread)
        .bind(1, thread.id)
        .bind(2, thread.subject_id)
        .bind(3, thread.title)
        .bind(4, thread.description)
        .run();
    return changes() > 0;
}

bool Database::delete_thread(const std::string &id) {
    prepare(sql::kDeleteThread).bind(1, id).run();
    return changes() > 0;
}

} // namespace noteman::db
