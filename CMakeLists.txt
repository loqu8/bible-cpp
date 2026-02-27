cmake_minimum_required(VERSION 3.20)
project(bible-cpp
    VERSION 0.2.0
    DESCRIPTION "Pure C++20 parallel-text scripture engine"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(BIBLE_BUILD_TESTS "Build tests" ON)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

add_library(bible STATIC
    src/bible.cpp
    src/reference.cpp
    src/strongs_parser.cpp
    src/stats.cpp
)

add_library(bible::bible ALIAS bible)

target_include_directories(bible
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(bible PUBLIC cxx_std_20)

# SQLite: find system or use vendored amalgamation
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    set(BIBLE_SQLITE_TARGET SQLite::SQLite3)
    message(STATUS "Using system SQLite3 ${SQLite3_VERSION}")
else()
    message(STATUS "SQLite3 not found; using vendored amalgamation")
    add_library(sqlite3_vendored STATIC third_party/sqlite/sqlite3.c)
    target_include_directories(sqlite3_vendored PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(sqlite3_vendored PRIVATE
        SQLITE_ENABLE_FTS5
        SQLITE_DQS=0
        SQLITE_OMIT_DEPRECATED
        SQLITE_THREADSAFE=0
    )
    if(UNIX AND NOT APPLE)
        target_link_libraries(sqlite3_vendored PRIVATE dl pthread)
    endif()
    set(BIBLE_SQLITE_TARGET sqlite3_vendored)
endif()

target_link_libraries(bible PRIVATE ${BIBLE_SQLITE_TARGET})

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------

include(GNUInstallDirs)

set(BIBLE_INSTALL_TARGETS bible)
if(TARGET sqlite3_vendored)
    list(APPEND BIBLE_INSTALL_TARGETS sqlite3_vendored)
endif()

install(TARGETS ${BIBLE_INSTALL_TARGETS}
    EXPORT BibleCppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/bible
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT BibleCppTargets
    FILE BibleCppTargets.cmake
    NAMESPACE bible::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bible-cpp
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(BIBLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
