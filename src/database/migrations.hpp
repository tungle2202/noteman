#pragma once

#include "database.hpp"

#include <vector>

namespace noteman::db {

// Ordered, forward-only schema migrations. Versions must be sequential
// (1, 2, 3, ...). Never edit a migration that has shipped; append a new one.
const std::vector<Migration> &migrations();

} // namespace noteman::db
