#pragma once

#include <optional>
#include <string>
#include <string_view>

// Unix-like permission mode for subjects (see docs/design/database/ERD.dbml).
//
// 6-bit mask, 2 bits (read, write) per class:
//   bit 5 = owner r (32), bit 4 = owner w (16)
//   bit 3 = group r (8),  bit 2 = group w (4)
//   bit 1 = other r (2),  bit 0 = other w (1)
//
// Check order is owner -> group -> other; only the first matching class
// applies (an owner without the owner-read bit cannot read even if "other"
// may). Admins bypass the check entirely; that is decided by the caller.
namespace noteman::modules::permission {

enum class Class { Owner, Group, Other };
enum class Access { Read, Write };

inline constexpr int kOwnerRead = 32;
inline constexpr int kOwnerWrite = 16;
inline constexpr int kGroupRead = 8;
inline constexpr int kGroupWrite = 4;
inline constexpr int kOtherRead = 2;
inline constexpr int kOtherWrite = 1;

inline constexpr int kMinMode = 0;
inline constexpr int kMaxMode = 63;

// Must match the DEFAULT of subjects.mode in the schema.
inline constexpr int kDefaultMode = 56; // rwr---

constexpr bool is_valid(int mode) { return mode >= kMinMode && mode <= kMaxMode; }

// Bit that grants `access` to `cls`.
int bit(Class cls, Access access);

// Picks the single class that applies to a user (owner -> group -> other).
Class resolve_class(bool is_owner, bool in_group);

// True if `mode` grants `access` to `cls`.
bool allows(int mode, Class cls, Access access);

// "rwr---" style rendering (owner, group, other).
std::string to_string(int mode);

// Parses "rwr---" style strings. Returns nullopt on malformed input.
std::optional<int> parse(std::string_view text);

} // namespace noteman::modules::permission
