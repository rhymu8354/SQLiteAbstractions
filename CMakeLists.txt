# CMakeLists.txt for SQLiteClusterMemberStore

cmake_minimum_required(VERSION 3.8)
set(This SQLiteClusterMemberStore)

set(Headers
    include/SQLiteClusterMemberStore/SQLiteDatabase.hpp
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
    ClusterMemberStore
    SQLite
    StringExtensions
    SystemAbstractions
)

add_subdirectory(test)
