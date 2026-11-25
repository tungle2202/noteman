#include "database/database.hpp"

#include "database/row_readers.hpp"
#include "database/sql/queries.hpp"

namespace noteman::db {

using namespace detail;

ReadingProgress Database::save_reading_progress(const std::string &user_id,
                                                const std::string &material_id,
                                                std::int64_t page, double page_offset,
                                                const std::optional<double> &zoom) {
    auto s = prepare(sql::kUpsertReadingProgress);
    s.bind(1, user_id).bind(2, material_id).bind(3, page).bind(4, page_offset).bind(5, zoom);
    return read_required(s, read_reading_progress);
}

std::optional<ReadingProgress> Database::get_reading_progress(const std::string &user_id,
                                                              const std::string &material_id) {
    auto s = prepare(sql::kSelectReadingProgress);
    s.bind(1, user_id).bind(2, material_id);
    return read_one(s, read_reading_progress);
}

} // namespace noteman::db
