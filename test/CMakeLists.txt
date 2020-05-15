# CMakeLists.txt for SQLiteAbstractionsTests

cmake_minimum_required(VERSION 3.8)
set(This SQLiteAbstractionsTests)

set(Sources
    src/SQLiteDatabaseTests.cpp
)

add_executable(${This} ${Sources})
set_target_properties(${This} PROPERTIES
    FOLDER Tests
)

target_include_directories(${This} PRIVATE ..)

target_link_libraries(${This} PUBLIC
    gtest_main
    SQLite
    SQLiteAbstractions
    SystemAbstractions
)

add_test(
    NAME ${This}
    COMMAND ${This}
)
