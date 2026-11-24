cmake_minimum_required(VERSION 3.16)
project(noteman LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SQLITE

add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite/)
find_package(Threads REQUIRED)
target_link_libraries(sqlite3 PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

# App

add_executable(my_app src/main.cpp)
target_link_libraries(my_app PRIVATE sqlite3)
