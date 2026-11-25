#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace noteman::modules {

// Incremental SHA-256 (FIPS 180-4). Used for blob content addressing and
// note_versions.content_hash.
//
//   Sha256 h;
//   h.update(chunk1);
//   h.update(chunk2);
//   std::string hex = h.finish_hex();
class Sha256 {
public:
    using Digest = std::array<std::uint8_t, 32>;

    Sha256();

    void update(const void *data, std::size_t size);
    void update(std::string_view data) { update(data.data(), data.size()); }

    // Finalizes the hash. The object must not be updated afterwards.
    Digest finish();
    std::string finish_hex(); // 64 lowercase hex characters

private:
    void transform(const std::uint8_t *block);

    std::uint32_t state_[8];
    std::uint64_t total_bytes_ = 0;
    std::uint8_t buffer_[64];
    std::size_t buffer_size_ = 0;
};

// One-shot helper: SHA-256 of `data` as 64 lowercase hex characters.
std::string sha256_hex(std::string_view data);

} // namespace noteman::modules
