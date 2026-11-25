#include "database/database.hpp"

#include "database/row_readers.hpp"
#include "database/sql/queries.hpp"
#include "modules/uuid.hpp"

namespace noteman::db {

using namespace detail;

User Database::create_user(const NewUser &user) {
    return transaction([&] {
        std::string group_id;
        if (user.primary_group_id && !user.primary_group_id->empty()) {
            group_id = *user.primary_group_id;
        } else {
            group_id = create_group(user.username).id; // private group
        }

        auto s = prepare(sql::kInsertUser);
        s.bind(1, modules::generate_uuid_v4())
            .bind(2, user.username)
            .bind(3, user.email)
            .bind(4, user.password_hash)
            .bind(5, group_id)
            .bind(6, user.is_admin);
        return read_required(s, read_user);
    });
}

std::optional<User> Database::get_user(const std::string &id) {
    auto s = prepare(sql::kSelectUserById);
    s.bind(1, id);
    return read_one(s, read_user);
}

std::optional<User> Database::get_user_by_username(const std::string &username) {
    auto s = prepare(sql::kSelectUserByUsername);
    s.bind(1, username);
    return read_one(s, read_user);
}

std::vector<User> Database::list_users() {
    auto s = prepare(sql::kSelectUsers);
    return read_all(s, read_user);
}

bool Database::update_user(const User &user) {
    prepare(sql::kUpdateUser)
        .bind(1, user.id)
        .bind(2, user.username)
        .bind(3, user.email)
        .bind(4, user.password_hash)
        .bind(5, user.primary_group_id)
        .bind(6, user.is_admin)
        .run();
    return changes() > 0;
}

bool Database::delete_user(const std::string &id) {
    prepare(sql::kDeleteUser).bind(1, id).run();
    return changes() > 0;
}

} // namespace noteman::db
