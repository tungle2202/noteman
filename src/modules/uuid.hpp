#pragma once

#include <string>

namespace noteman::modules {

// Generates a random RFC 4122 version 4 UUID, lowercase canonical form,
// e.g. "550e8400-e29b-41d4-a716-446655440000".
std::string generate_uuid_v4();

} // namespace noteman::modules
