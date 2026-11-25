#pragma once

#include <filesystem>
#include <string_view>

namespace noteman::modules {

// True if `hash` is a SHA-256 digest in 64 lowercase hex characters.
bool is_sha256_hex(std::string_view hash);

// Location of a blob in the content-addressed store, relative to the app's
// data root: data/blobs/{hash[0:2]}/{hash[2:]}
// Throws std::invalid_argument if `hash` is not a lowercase SHA-256 hex digest.
std::filesystem::path blob_relative_path(std::string_view hash);

} // namespace noteman::modules
