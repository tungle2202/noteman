#include "modules/blob_path.hpp"

#include <stdexcept>
#include <string>

namespace noteman::modules {

bool is_sha256_hex(std::string_view hash) {
    if (hash.size() != 64) return false;
    for (char c : hash) {
        const bool digit = c >= '0' && c <= '9';
        const bool lower_hex = c >= 'a' && c <= 'f';
        if (!digit && !lower_hex) return false;
    }
    return true;
}

std::filesystem::path blob_relative_path(std::string_view hash) {
    if (!is_sha256_hex(hash)) {
        throw std::invalid_argument("not a lowercase SHA-256 hex digest: " + std::string(hash));
    }
    return std::filesystem::path("data") / "blobs" / std::string(hash.substr(0, 2)) /
           std::string(hash.substr(2));
}

} // namespace noteman::modules
