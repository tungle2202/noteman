#include "modules/uuid.hpp"

#include <array>
#include <cstdint>
#include <random>

namespace noteman::modules {

namespace {

std::mt19937_64 &rng() {
    // Seed the full engine state from several random_device draws so that
    // independent processes do not share a 32-bit seed space.
    static thread_local std::mt19937_64 engine = [] {
        std::random_device rd;
        std::array<std::uint32_t, 8> seed{};
        for (auto &word : seed) word = rd();
        std::seed_seq seq(seed.begin(), seed.end());
        return std::mt19937_64(seq);
    }();
    return engine;
}

} // namespace

std::string generate_uuid_v4() {
    std::uniform_int_distribution<std::uint64_t> dist;
    const std::uint64_t hi = dist(rng());
    const std::uint64_t lo = dist(rng());

    unsigned char bytes[16];
    for (int i = 0; i < 8; ++i) {
        bytes[i] = static_cast<unsigned char>(hi >> (56 - 8 * i));
        bytes[8 + i] = static_cast<unsigned char>(lo >> (56 - 8 * i));
    }
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40); // version 4
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80); // RFC 4122 variant

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(36);
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back('-');
        out.push_back(kHex[bytes[i] >> 4]);
        out.push_back(kHex[bytes[i] & 0x0F]);
    }
    return out;
}

} // namespace noteman::modules
