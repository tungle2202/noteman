#include "database/database.hpp"

#include "database/row_readers.hpp"
#include "database/sql/queries.hpp"
#include "modules/uuid.hpp"

namespace noteman::db {

using namespace detail;

Group Database::create_group(const std::string &name) {
    auto s = prepare(sql::kInsertGroup);
    s.bind(1, modules::generate_uuid_v4()).bind(2, name);
    return read_required(s, read_group);
}

std::optional<Group> Database::get_group(const std::string &id) {
    auto s = prepare(sql::kSelectGroupById);
    s.bind(1, id);
    return read_one(s, read_group);
}

std::optional<Group> Database::get_group_by_name(const std::string &name) {
    auto s = prepare(sql::kSelectGroupByName);
    s.bind(1, name);
    return read_one(s, read_group);
}

std::vector<Group> Database::list_groups() {
    auto s = prepare(sql::kSelectGroups);
    return read_all(s, read_group);
}

bool Database::rename_group(const std::string &id, const std::string &name) {
    prepare(sql::kRenameGroup).bind(1, id).bind(2, name).run();
    return changes() > 0;
}

bool Database::delete_group(const std::string &id) {
    prepare(sql::kDeleteGroup).bind(1, id).run();
    return changes() > 0;
}

void Database::add_user_to_group(const std::string &user_id, const std::string &group_id) {
    prepare(sql::kInsertUserGroup).bind(1, user_id).bind(2, group_id).run();
}

bool Database::remove_user_from_group(const std::string &user_id, const std::string &group_id) {
    prepare(sql::kDeleteUserGroup).bind(1, user_id).bind(2, group_id).run();
    return changes() > 0;
}

std::vector<Group> Database::list_groups_of_user(const std::string &user_id) {
    auto s = prepare(sql::kSelectGroupsOfUser);
    s.bind(1, user_id);
    return read_all(s, read_group);
}

std::vector<User> Database::list_group_members(const std::string &group_id) {
    auto s = prepare(sql::kSelectMembersOfGroup);
    s.bind(1, group_id);
    return read_all(s, read_user);
}

} // namespace noteman::db
