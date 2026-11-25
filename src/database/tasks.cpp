#include "database/database.hpp"

#include "database/row_readers.hpp"
#include "database/sql/queries.hpp"
#include "modules/uuid.hpp"

namespace noteman::db {

using namespace detail;

Task Database::create_task(const std::string &thread_id, const std::string &title,
                           const std::optional<std::string> &deadline,
                           const std::optional<std::string> &created_by) {
    auto s = prepare(sql::kInsertTask);
    s.bind(1, modules::generate_uuid_v4())
        .bind(2, thread_id)
        .bind(3, title)
        .bind(4, deadline)
        .bind(5, created_by);
    return read_required(s, read_task);
}

std::optional<Task> Database::get_task(const std::string &id) {
    auto s = prepare(sql::kSelectTaskById);
    s.bind(1, id);
    return read_one(s, read_task);
}

std::vector<Task> Database::list_tasks(const std::string &thread_id) {
    auto s = prepare(sql::kSelectTasksByThread);
    s.bind(1, thread_id);
    return read_all(s, read_task);
}

bool Database::update_task(const Task &task) {
    prepare(sql::kUpdateTask)
        .bind(1, task.id)
        .bind(2, task.thread_id)
        .bind(3, task.title)
        .bind(4, task.deadline)
        .run();
    return changes() > 0;
}

bool Database::set_task_completed(const std::string &id, bool completed) {
    prepare(sql::kSetTaskCompleted).bind(1, id).bind(2, completed).run();
    return changes() > 0;
}

bool Database::delete_task(const std::string &id) {
    prepare(sql::kDeleteTask).bind(1, id).run();
    return changes() > 0;
}

} // namespace noteman::db
