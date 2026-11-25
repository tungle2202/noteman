cmake_minimum_required(VERSION 3.16)
project(noteman LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EmbedMigrations)

# SQLITE

add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite/)
find_package(Threads REQUIRED)
target_link_libraries(sqlite3 PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

# Utility modules (single-purpose helpers, no SQLite dependency)

add_library(noteman_modules STATIC
    src/modules/blob_path.cpp
    src/modules/permission.cpp
    src/modules/sha256.cpp
    src/modules/timestamp.cpp
    src/modules/uuid.cpp
)
target_include_directories(noteman_modules PUBLIC src/)

# Database layer

set(NOTEMAN_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
noteman_embed_migrations(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/database/sql/migrations"
    OUTPUT    "${NOTEMAN_GENERATED_DIR}/noteman/migration_scripts.hpp"
)

add_library(noteman_db STATIC
    src/database/database.cpp
    src/database/error.cpp
    src/database/groups.cpp
    src/database/materials.cpp
    src/database/migrations.cpp
    src/database/models.cpp
    src/database/notes.cpp
    src/database/quotes.cpp
    src/database/reading_progress.cpp
    src/database/row_readers.cpp
    src/database/statement.cpp
    src/database/subjects.cpp
    src/database/tasks.cpp
    src/database/threads.cpp
    src/database/users.cpp
)
target_include_directories(noteman_db
    PUBLIC  src/
    PRIVATE "${NOTEMAN_GENERATED_DIR}"
)
target_link_libraries(noteman_db PUBLIC sqlite3 noteman_modules)

# App

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE noteman_db)
