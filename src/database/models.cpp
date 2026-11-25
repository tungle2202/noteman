#include "database/models.hpp"

#include <stdexcept>
#include <string>

namespace noteman::db {

const char *to_string(QuoteKind kind) {
    switch (kind) {
    case QuoteKind::Full: return "full";
    case QuoteKind::Pages: return "pages";
    case QuoteKind::Region: return "region";
    case QuoteKind::Text: return "text";
    }
    return "full";
}

const char *to_string(NoteFormat format) {
    switch (format) {
    case NoteFormat::Markdown: return "md";
    case NoteFormat::Tex: return "tex";
    }
    return "md";
}

const char *to_string(Compression compression) {
    switch (compression) {
    case Compression::None: return "none";
    case Compression::Zstd: return "zstd";
    }
    return "none";
}

QuoteKind quote_kind_from_string(std::string_view text) {
    if (text == "full") return QuoteKind::Full;
    if (text == "pages") return QuoteKind::Pages;
    if (text == "region") return QuoteKind::Region;
    if (text == "text") return QuoteKind::Text;
    throw std::invalid_argument("unknown quote kind: " + std::string(text));
}

NoteFormat note_format_from_string(std::string_view text) {
    if (text == "md") return NoteFormat::Markdown;
    if (text == "tex") return NoteFormat::Tex;
    throw std::invalid_argument("unknown note format: " + std::string(text));
}

Compression compression_from_string(std::string_view text) {
    if (text == "none") return Compression::None;
    if (text == "zstd") return Compression::Zstd;
    throw std::invalid_argument("unknown compression: " + std::string(text));
}

} // namespace noteman::db
