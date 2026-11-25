#include "database/database.hpp"

#include "database/row_readers.hpp"
#include "database/sql/queries.hpp"
#include "modules/uuid.hpp"

namespace noteman::db {

using namespace detail;

Quote Database::create_quote(const NewQuote &quote) {
    auto s = prepare(sql::kInsertQuote);
    s.bind(1, modules::generate_uuid_v4())
        .bind(2, quote.thread_id)
        .bind(3, quote.material_id)
        .bind(4, to_string(quote.kind))
        .bind(5, quote.spec_json)
        .bind(6, quote.extracted_text)
        .bind(7, quote.note)
        .bind(8, quote.author_id);
    return read_required(s, read_quote);
}

std::optional<Quote> Database::get_quote(const std::string &id) {
    auto s = prepare(sql::kSelectQuoteById);
    s.bind(1, id);
    return read_one(s, read_quote);
}

std::vector<Quote> Database::list_quotes_by_thread(const std::string &thread_id) {
    auto s = prepare(sql::kSelectQuotesByThread);
    s.bind(1, thread_id);
    return read_all(s, read_quote);
}

std::vector<Quote> Database::list_quotes_by_material(const std::string &material_id) {
    auto s = prepare(sql::kSelectQuotesByMaterial);
    s.bind(1, material_id);
    return read_all(s, read_quote);
}

bool Database::update_quote(const Quote &quote) {
    prepare(sql::kUpdateQuote)
        .bind(1, quote.id)
        .bind(2, to_string(quote.kind))
        .bind(3, quote.spec_json)
        .bind(4, quote.extracted_text)
        .bind(5, quote.note)
        .run();
    return changes() > 0;
}

bool Database::delete_quote(const std::string &id) {
    prepare(sql::kDeleteQuote).bind(1, id).run();
    return changes() > 0;
}

} // namespace noteman::db
