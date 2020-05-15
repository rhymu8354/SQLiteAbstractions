# CMakeLists.txt for SQLiteAbstractions

cmake_minimum_required(VERSION 3.8)
set(This SQLiteAbstractions)

set(Headers
    include/SQLiteAbstractions/SQLiteDatabase.hpp
)

set(Sources
    src/SQLiteDatabase.cpp
)

add_library(${This} STATIC ${Sources} ${Headers})
set_target_properties(${This} PROPERTIES
    FOLDER Libraries
)

target_include_directories(${This} PUBLIC include)

target_link_libraries(${This} PUBLIC
    DatabaseAbstractions
    SQLite
    StringExtensions
    SystemAbstractions
)

add_subdirectory(test)
